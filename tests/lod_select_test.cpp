// LOD selection math tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/lod_select_test tests/lod_select_test.cpp

#include "engine/rendering/culling/lod_select_math.h"

#include <cmath>
#include <cstdio>

using namespace lod_select;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static int Pick(float d, int prev = kNoPrevious, float bias = 1.0f, float cull = 0.0f,
                float hyst = 0.0f) {
    const float t[] = {30.0f, 80.0f};
    return SelectLevel(d, t, 2, prev, hyst, bias, cull);
}

int main() {
    // Basic bands (no hysteresis).
    Expect(Pick(0.0f) == 0, "distance 0 -> level 0");
    Expect(Pick(29.9f) == 0, "just below first threshold -> level 0");
    Expect(Pick(30.0f) == 1, "at first threshold -> level 1");
    Expect(Pick(79.9f) == 1, "just below second threshold -> level 1");
    Expect(Pick(80.0f) == 2, "at second threshold -> level 2");
    Expect(Pick(10000.0f) == 2, "far -> last level");

    // Empty / null threshold list always yields level 0.
    Expect(SelectLevel(500.0f, nullptr, 0, kNoPrevious, 0.0f, 1.0f, 0.0f) == 0,
           "no lods -> level 0");
    {
        const float t[] = {10.0f};
        Expect(SelectLevel(500.0f, t, 0, kNoPrevious, 0.0f, 1.0f, 0.0f) == 0,
               "zero count -> level 0");
    }

    // Bias scales distance for level choice.
    Expect(Pick(20.0f, kNoPrevious, 2.0f) == 1, "bias 2 switches sooner");
    Expect(Pick(50.0f, kNoPrevious, 0.5f) == 0, "bias 0.5 switches later");
    Expect(ClampBias(0.0f) == kMinBias, "bias clamps low");
    Expect(ClampBias(1000.0f) == kMaxBias, "bias clamps high");
    Expect(ClampBias(NAN) == 1.0f, "NaN bias -> 1");

    // Cull distance uses the raw (unbiased) distance.
    Expect(Pick(100.0f, kNoPrevious, 1.0f, 150.0f) == 2, "inside cull distance");
    Expect(Pick(151.0f, kNoPrevious, 1.0f, 150.0f) == kCulled, "beyond cull distance");
    Expect(Pick(140.0f, kNoPrevious, 10.0f, 150.0f) == 2, "bias does not move cull distance");
    Expect(Pick(1e6f, kNoPrevious, 1.0f, 0.0f) == 2, "cullDistance 0 disables cull");

    // Hysteresis: no flicker around a boundary.
    {
        const float h = 0.05f;
        int level = Pick(29.0f, kNoPrevious, 1.0f, 0.0f, h);
        Expect(level == 0, "start level 0");
        level = Pick(30.5f, level, 1.0f, 0.0f, h);
        Expect(level == 0, "stays 0 inside upper hysteresis band");
        level = Pick(31.6f, level, 1.0f, 0.0f, h);
        Expect(level == 1, "switches to 1 past 30*1.05");
        level = Pick(29.0f, level, 1.0f, 0.0f, h);
        Expect(level == 1, "stays 1 inside lower hysteresis band");
        level = Pick(28.4f, level, 1.0f, 0.0f, h);
        Expect(level == 0, "returns to 0 below 30*0.95");
    }

    // Large jump skips levels.
    Expect(Pick(500.0f, 0, 1.0f, 0.0f, 0.05f) == 2, "teleport far -> last level");
    Expect(Pick(1.0f, 2, 1.0f, 0.0f, 0.05f) == 0, "teleport near -> level 0");

    // Out-of-range prev falls back to raw selection.
    Expect(Pick(50.0f, 9) == 1, "invalid prev -> raw selection");

    // Bad distance values.
    Expect(Pick(NAN) == 0, "NaN distance -> level 0");
    Expect(Pick(-5.0f) == 0, "negative distance -> level 0");

    // Threshold normalization.
    {
        float t[] = {80.0f, -1.0f, 30.0f};
        NormalizeThresholds(t, 3);
        Expect(t[0] == 0.0f && t[1] == 30.0f && t[2] == 80.0f, "sorted, negatives clamped");
        float inf[] = {INFINITY};
        NormalizeThresholds(inf, 1);
        Expect(inf[0] == 0.0f, "non-finite clamped");
        NormalizeThresholds(nullptr, 3); // must not crash
    }

    // Screen-size thresholds convert to distances.
    {
        const float tanHalf = std::tan(0.5f * 60.0f * 0.017453292519943295f); // 60 deg fov
        // A 2 m object fills 12% of the screen height at 2 / (2 * tan30 * 0.12) m.
        const float d = ScreenHeightToDistance(2.0f, tanHalf, 0.12f);
        Expect(std::fabs(d - 2.0f / (2.0f * tanHalf * 0.12f)) < 1e-3f, "distance formula");
        Expect(std::fabs(2.0f / (2.0f * d * tanHalf) - 0.12f) < 1e-4f, "projected height round trip");
        Expect(ScreenHeightToDistance(4.0f, tanHalf, 0.12f) > 1.99f * d, "twice the size switches twice as far");
        Expect(ScreenHeightToDistance(2.0f, tanHalf, 0.06f) > 1.99f * d, "half the screen height switches twice as far");
        Expect(ScreenHeightToDistance(0.0f, tanHalf, 0.1f) == kNeverSwitch, "no extent never switches");
        Expect(ScreenHeightToDistance(-1.0f, tanHalf, 0.1f) == kNeverSwitch, "negative extent never switches");
        Expect(ScreenHeightToDistance(2.0f, 0.0f, 0.1f) == kNeverSwitch, "bad fov never switches");
        Expect(ScreenHeightToDistance(2.0f, tanHalf, 0.0f) == kNeverSwitch, "screenHeight 0 never switches");
        Expect(ScreenHeightToDistance(NAN, tanHalf, 0.1f) == kNeverSwitch, "NaN extent never switches");
        // End to end with the distance selector: descending screen heights give ascending distances.
        float t[3] = { ScreenHeightToDistance(2.0f, tanHalf, 0.12f),
                       ScreenHeightToDistance(2.0f, tanHalf, 0.06f),
                       ScreenHeightToDistance(2.0f, tanHalf, 0.03f) };
        Expect(t[0] < t[1] && t[1] < t[2], "descending screen heights -> ascending distances");
        Expect(SelectLevel(t[0] * 0.9f, t, 3, kNoPrevious, 0.0f, 1.0f, 0.0f) == 0, "close -> LOD0");
        Expect(SelectLevel(t[1] * 1.1f, t, 3, kNoPrevious, 0.0f, 1.0f, 0.0f) == 2, "mid -> LOD2");
        Expect(SelectLevel(t[2] * 1.1f, t, 3, kNoPrevious, 0.0f, 1.0f, 0.0f) == 3, "far -> LOD3");
        // A "never" threshold keeps lower levels unreachable.
        float tn[2] = { t[0], kNeverSwitch };
        Expect(SelectLevel(1.0e6f, tn, 2, kNoPrevious, 0.0f, 1.0f, 0.0f) == 1, "never-switch level stays out of reach");
    }

    // Forced level (debug override).
    Expect(ForcedLevel(2, -1, 3) == 2, "force off keeps the automatic level");
    Expect(ForcedLevel(0, 2, 3) == 2, "force picks that level");
    Expect(ForcedLevel(3, 0, 3) == 0, "force 0 overrides a far level");
    Expect(ForcedLevel(0, 5, 3) == 3, "force is limited to the last level");
    Expect(ForcedLevel(0, 5, 0) == 0, "no lods -> level 0");
    Expect(ForcedLevel(kCulled, 2, 3) == kCulled, "culled stays culled");
    Expect(ForcedLevel(1, -9, 3) == 1, "very negative force means off");
    Expect(ClampForcedLevel(99) == kMaxForcedLevel, "force clamps high");

    // Shadow caster level offset.
    Expect(ShadowCasterLevel(0, 0, 3) == 0, "offset 0 keeps the camera level");
    Expect(ShadowCasterLevel(0, 1, 3) == 1, "offset 1 goes one level lower");
    Expect(ShadowCasterLevel(1, 2, 3) == 3, "offset adds to the camera level");
    Expect(ShadowCasterLevel(2, 3, 3) == 3, "offset never passes the last level");
    Expect(ShadowCasterLevel(0, 3, 0) == 0, "no lods -> level 0");
    Expect(ShadowCasterLevel(kCulled, 2, 3) == kCulled, "culled stays culled");
    Expect(ShadowCasterLevel(0, -4, 3) == 0, "negative offset clamps to 0");
    Expect(ShadowCasterLevel(0, 99, 8) == kMaxShadowLodOffset, "offset clamps to the max");
    Expect(ClampShadowLodOffset(5) == kMaxShadowLodOffset, "clamp high");
    Expect(ShadowCasterLevel(3, 1, 2) == 3, "camera level above count is not reduced");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("lod_select_test: PASS");
    return 0;
}
