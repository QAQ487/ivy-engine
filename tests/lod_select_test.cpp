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
