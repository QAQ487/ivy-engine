// editor_ui_scale_math_test.cpp
// Linux-runnable unit tests for editor UI scale math (no compounding / DPI / clamp).
//
// Prefer: ./tools/run_editor_ui_scale_math_check.sh

#include "engine/editor/editor_ui_scale_math.h"

#include <cmath>
#include <cstdio>

static int g_failed = 0;

static void Expect(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void ExpectNear(float a, float b, float eps, const char* msg)
{
    if (std::fabs(a - b) > eps) {
        std::fprintf(stderr, "FAIL: %s (got %f expected %f)\n", msg, a, b);
        ++g_failed;
    }
}

static void TestClamp()
{
    Expect(ClampUiScalePercent(100) == 100, "100 stays");
    Expect(ClampUiScalePercent(50) == 50, "50 min inclusive");
    Expect(ClampUiScalePercent(300) == 300, "300 max inclusive");
    Expect(ClampUiScalePercent(49) == 50, "below min → 50");
    Expect(ClampUiScalePercent(301) == 300, "above max → 300");
    Expect(ClampUiScalePercent(0) == 50, "0 → 50");
    Expect(ClampUiScalePercent(-10) == 50, "negative → 50");
}

static void TestDpiNormalize()
{
    ExpectNear(NormalizeDpiScale(1.0f), 1.0f, 0.0001f, "1.0");
    ExpectNear(NormalizeDpiScale(1.25f), 1.25f, 0.0001f, "1.25");
    ExpectNear(NormalizeDpiScale(0.0f), 1.0f, 0.0001f, "0 → 1");
    ExpectNear(NormalizeDpiScale(-1.0f), 1.0f, 0.0001f, "neg → 1");
    ExpectNear(NormalizeDpiScale(NAN), 1.0f, 0.0001f, "nan → 1");
    ExpectNear(NormalizeDpiScale(0.25f), 0.5f, 0.0001f, "tiny floored");
    ExpectNear(NormalizeDpiScale(8.0f), 4.0f, 0.0001f, "huge capped");
}

static void TestEffectiveScale()
{
    ExpectNear(ComputeEffectiveUiScale(100, 1.0f), 1.0f, 0.0001f, "100% @1x");
    ExpectNear(ComputeEffectiveUiScale(200, 1.0f), 2.0f, 0.0001f, "200% @1x");
    ExpectNear(ComputeEffectiveUiScale(50, 1.0f), 0.5f, 0.0001f, "50% @1x");
    ExpectNear(ComputeEffectiveUiScale(100, 1.5f), 1.5f, 0.0001f, "100% @1.5x dpi");
    ExpectNear(ComputeEffectiveUiScale(200, 1.5f), 3.0f, 0.0001f, "200% @1.5x dpi");
    ExpectNear(ComputeEffectiveUiScale(150, 2.0f), 3.0f, 0.0001f, "150% @2x dpi");
    // Invalid percent still clamps before combining.
    ExpectNear(ComputeEffectiveUiScale(10, 1.0f), 0.5f, 0.0001f, "invalid low clamps");
    ExpectNear(ComputeEffectiveUiScale(999, 1.0f), 3.0f, 0.0001f, "invalid high clamps");
    ExpectNear(ComputeEffectiveUiScale(100, 0.0f), 1.0f, 0.0001f, "bad dpi → user only");
}

static void TestNoCompounding()
{
    // Recomputing from the same unscaled inputs never multiplies prior results.
    Expect(EffectiveUiScaleIsIdempotent(200, 1.25f), "idempotent 200/1.25");
    Expect(EffectiveUiScaleIsIdempotent(50, 2.0f), "idempotent 50/2");

    const float once = ComputeEffectiveUiScale(200, 1.0f);
    // Mistaken compounding would be once * once = 4; correct is still 2.
    const float again = ComputeEffectiveUiScale(200, 1.0f);
    ExpectNear(once, 2.0f, 0.0001f, "first apply 200%");
    ExpectNear(again, once, 0.0001f, "second apply same (no compound)");
    Expect(again != once * once, "not squared");
}

int main()
{
    TestClamp();
    TestDpiNormalize();
    TestEffectiveScale();
    TestNoCompounding();

    if (g_failed != 0) {
        std::fprintf(stderr, "editor_ui_scale_math_test: %d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("editor_ui_scale_math_test: PASS\n");
    return 0;
}
