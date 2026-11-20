// inspector_layout_math_test.cpp
// Pure layout rules of the Properties panel (engine/editor/inspector_layout_math.h).
// Build:
//   g++ -std=c++20 -I. tests/inspector_layout_math_test.cpp -o /tmp/inspector_layout_math_test
#include "engine/editor/inspector_layout_math.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main()
{
    using namespace inspector_layout;
    const float font = 16.0f, sp = 8.0f;

    // Wide panel: word labels, label column capped at 7 font sizes.
    RowLayout wide = ComputeRowLayout(600.0f, font, sp, true);
    CHECK(wide.showText);
    CHECK(wide.labelW == font * 7.0f);
    CHECK(wide.labelW + sp + wide.fieldW == 600.0f);

    // Narrow panel with an icon: icon-only label.
    RowLayout narrow = ComputeRowLayout(180.0f, font, sp, true);
    CHECK(!narrow.showText);
    CHECK(narrow.labelW == font * 1.6f);
    CHECK(narrow.labelW + sp + narrow.fieldW <= 180.0f);

    // Narrow panel without an icon keeps the word.
    CHECK(ComputeRowLayout(180.0f, font, sp, false).showText);

    // Tiny panel: label never above half, field never below 1 px.
    RowLayout tiny = ComputeRowLayout(20.0f, font, sp, false);
    CHECK(tiny.labelW <= 10.0f);
    CHECK(tiny.fieldW >= 1.0f);

    // Threshold scales with font size (UI zoom).
    CHECK(ComputeRowLayout(300.0f, 16.0f, sp, true).showText);
    CHECK(!ComputeRowLayout(300.0f, 32.0f, sp, true).showText);

    CHECK(SplitWidth(100.0f, 3, 5.0f) == 30.0f);
    CHECK(SplitWidth(100.0f, 0, 5.0f) == 0.0f);
    CHECK(SplitWidth(2.0f, 4, 5.0f) == 1.0f);

    CHECK(!StackVecAxes(300.0f, 3, font, 4.0f));
    CHECK(StackVecAxes(120.0f, 3, font, 4.0f));

    CHECK(WrapColumns(400.0f, 40.0f, 8.0f, 4) == 4);
    CHECK(WrapColumns(100.0f, 40.0f, 8.0f, 4) == 2);
    CHECK(WrapColumns(10.0f, 40.0f, 8.0f, 4) == 1);
    CHECK(WrapColumns(400.0f, 40.0f, 8.0f, 0) == 1);

    CHECK(IsLightBackground(0.95f, 0.95f, 0.95f));
    CHECK(!IsLightBackground(0.1f, 0.12f, 0.1f));

    if (g_fail == 0)
        std::printf("inspector_layout_math_test: all passed\n");
    return g_fail == 0 ? 0 : 1;
}
