// settings.render.shadowMode id / clamp tests (engine/scene/shadow_mode.h).
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/shadow_mode_test tests/shadow_mode_test.cpp

#include "engine/scene/shadow_mode.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

int main() {
    using namespace shadow_mode;
    Expect(FromId("csm") == ShadowMode::Csm, "csm id");
    Expect(FromId("volume") == ShadowMode::Volume, "volume id");
    Expect(FromId("none") == ShadowMode::None, "none id");
    Expect(FromId("") == ShadowMode::Csm, "empty id falls back to csm");
    Expect(FromId("Volume") == ShadowMode::Csm, "ids are case-sensitive (JSON contract)");
    Expect(FromId("stencil") == ShadowMode::Csm, "unknown id falls back to csm");

    const ShadowMode all[] = { ShadowMode::Csm, ShadowMode::Volume, ShadowMode::None };
    for (ShadowMode m : all) {
        Expect(FromId(Id(m)) == m, "Id round-trips through FromId");
        Expect(std::strlen(Label(m)) > 0, "every mode has a label");
    }

    Expect(ClampVolumeStrength(0.3f) == 0.3f, "in-range strength kept");
    Expect(ClampVolumeStrength(-1.0f) == 0.0f, "strength clamped to 0");
    Expect(ClampVolumeStrength(2.0f) == 1.0f, "strength clamped to 1");
    Expect(ClampVolumeStrength(std::numeric_limits<float>::quiet_NaN()) == kDefaultVolumeStrength,
        "NaN strength resets to the default");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("PASS: shadow mode ids and volume strength clamp\n");
    return 0;
}
