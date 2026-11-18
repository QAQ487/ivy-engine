// Culling / shadow-range setting helper tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/cull_settings_test tests/cull_settings_test.cpp

#include "engine/rendering/culling/cull_settings_math.h"

#include <cmath>
#include <cstdio>

using namespace cull_settings;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
    // Cull distance scale: clamped, NaN-safe, default 1.
    Expect(Near(ClampCullDistanceScale(1.0f), 1.0f), "scale 1 unchanged");
    Expect(Near(ClampCullDistanceScale(0.0f), kMinCullDistanceScale), "scale clamps low");
    Expect(Near(ClampCullDistanceScale(-3.0f), kMinCullDistanceScale), "negative scale clamps low");
    Expect(Near(ClampCullDistanceScale(1000.0f), kMaxCullDistanceScale), "scale clamps high");
    Expect(Near(ClampCullDistanceScale(NAN), 1.0f), "NaN scale -> 1");
    Expect(Near(ClampCullDistanceScale(INFINITY), 1.0f), "inf scale -> 1");

    // Scaled cull distance: multiplies, and "never" (0) stays never.
    Expect(Near(ScaledCullDistance(100.0f, 2.0f), 200.0f), "scale 2 doubles the distance");
    Expect(Near(ScaledCullDistance(100.0f, 0.5f), 50.0f), "scale 0.5 halves the distance");
    Expect(Near(ScaledCullDistance(100.0f, 1.0f), 100.0f), "scale 1 keeps the distance");
    Expect(ScaledCullDistance(0.0f, 5.0f) == 0.0f, "0 stays 0 (never cull)");
    Expect(ScaledCullDistance(-1.0f, 5.0f) == 0.0f, "negative distance means never");
    Expect(Near(ScaledCullDistance(100.0f, 99.0f), 1000.0f), "huge scale is clamped to 10x");

    // Shadow distance: 0 = preset, positive overrides within [5, 1000].
    Expect(ClampShadowDistanceOverride(0.0f) == 0.0f, "0 means preset");
    Expect(ClampShadowDistanceOverride(-10.0f) == 0.0f, "negative means preset");
    Expect(ClampShadowDistanceOverride(NAN) == 0.0f, "NaN means preset");
    Expect(Near(ClampShadowDistanceOverride(2.0f), kMinShadowDistance), "tiny override clamps to 5");
    Expect(Near(ClampShadowDistanceOverride(150.0f), 150.0f), "override kept");
    Expect(Near(ClampShadowDistanceOverride(1e9f), kMaxShadowDistance), "huge override clamps to 1000");
    Expect(Near(ResolveShadowDistance(70.0f, 0.0f), 70.0f), "no override uses the preset");
    Expect(Near(ResolveShadowDistance(70.0f, 120.0f), 120.0f), "override replaces the preset");
    Expect(Near(ResolveShadowDistance(40.0f, 1.0f), kMinShadowDistance), "override below min is clamped");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("cull_settings_test: PASS");
    return 0;
}
