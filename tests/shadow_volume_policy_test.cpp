// Shadow-volume pass decisions (engine/rendering/lighting/shadow_volume_policy.h)
// and their interplay with off-screen animation culling (anim_cull_math.h).
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/shadow_volume_policy_test tests/shadow_volume_policy_test.cpp

#include "engine/rendering/culling/anim_cull_math.h"
#include "engine/rendering/lighting/shadow_volume_policy.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace shadow_volume_policy;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

// One Visual's frame state, mirroring GameObject::DrawVisual.
struct Visual {
    bool shadowVolume = true;
    uint64_t considered = anim_cull::kNever;
    uint64_t drawn = anim_cull::kNever;
    uint64_t distanceCulled = kNever;
};

// Main pass + volume pass for one frame; returns whether the volume was drawn.
static bool RunFrame(Visual& v, uint64_t serial, bool onScreen, bool beyondCullDistance) {
    v.considered = serial;  // main pass looked at it
    if (beyondCullDistance)
        v.distanceCulled = serial;
    else if (onScreen)
        v.drawn = serial;   // main pass drew it
    const bool cast = ShouldCast(v.shadowVolume, true, v.distanceCulled, serial);
    if (cast && StampsDrawn(true))
        v.drawn = serial;
    return cast;
}

int main() {
    // ShouldCast basics.
    Expect(!ShouldCast(false, true, kNever, 5), "not opted in: no volume");
    Expect(ShouldCast(true, true, kNever, 5), "opted in, never culled: volume");
    Expect(!ShouldCast(true, true, 5, 5), "culled by distance this frame: no volume");
    Expect(ShouldCast(true, true, 4, 5), "culled last frame only: volume again");
    Expect(ShouldCast(true, false, 5, 5), "edit view (proxy, no LOD) ignores the stamp");
    Expect(StampsDrawn(true) && !StampsDrawn(false), "only the real-model pass stamps drawn");

    // V1: off-screen animated caster whose shadow is on screen keeps animating.
    {
        Visual v;
        const bool cast = RunFrame(v, 10, /*onScreen*/ false, /*beyond*/ false);
        Expect(cast, "off-screen caster still casts");
        Expect(!anim_cull::ShouldSkipPose(true, v.considered, v.drawn, 10),
            "volume drawn -> pose is evaluated next update (shadow does not freeze)");
    }
    // Without a volume (not opted in), the same off-screen Visual may skip its pose.
    {
        Visual v;
        v.shadowVolume = false;
        RunFrame(v, 11, false, false);
        Expect(anim_cull::ShouldSkipPose(true, v.considered, v.drawn, 11),
            "off-screen non-caster still skips its pose");
    }
    // V2: beyond cullDistance -> neither drawn nor casting, and may skip its pose.
    {
        Visual v;
        const bool cast = RunFrame(v, 12, true, true);
        Expect(!cast, "hidden by cullDistance: no shadow volume");
        Expect(anim_cull::ShouldSkipPose(true, v.considered, v.drawn, 12),
            "hidden by cullDistance: pose may be skipped");
        // Next frame back within range: casts again.
        Expect(RunFrame(v, 13, true, false), "back in range: casts again");
    }

    // V3: finite extrusion length.
    Expect(ExtrusionLength(1000.0f) == 4000.0f, "4x far clip");
    Expect(ExtrusionLength(5.0f) == kMinExtrusion, "small far clip -> minimum");
    Expect(ExtrusionLength(1.0e9f) == kMaxExtrusion, "huge far clip -> maximum");
    Expect(ExtrusionLength(0.0f) == 1000.0f, "zero far clip -> fallback");
    Expect(ExtrusionLength(-3.0f) == 1000.0f, "negative far clip -> fallback");
    Expect(ExtrusionLength(std::numeric_limits<float>::quiet_NaN()) == 1000.0f, "NaN -> fallback");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("PASS: shadow volume pass policy (cull distance, anim stamp, extrusion length)\n");
    return 0;
}
