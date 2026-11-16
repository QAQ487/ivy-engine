// editor_docking_math_test.cpp
// Linux-runnable checks for docking default splits, ini path, and rect conversion.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/editor_docking_math_test tests/editor_docking_math_test.cpp
//   /tmp/editor_docking_math_test

#include "engine/editor/editor_docking_math.h"

#include <cmath>
#include <cstdio>
#include <string>

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

static void TestDefaultPanelSizesMatchLegacyLook()
{
    const EditorDefaultPanelSizes s = ComputeDefaultPanelSizes(1920.0f, 1080.0f - 58.0f, 1.0f);
    ExpectNear(s.leftWidth, 300.0f, 0.01f, "left ~300 at 100% UI");
    ExpectNear(s.rightWidth, 350.0f, 0.01f, "right ~350 at 100% UI");
    // max(340, 0.34 * hostH); hostH ~= 1022 → ~347.5
    Expect(s.bottomHeight >= 340.0f, "bottom at least Ui(340)");
    Expect(s.bottomHeight <= 1022.0f * 0.34f + 1.0f, "bottom near 34% when larger");

    const EditorDefaultPanelSizes z = ComputeDefaultPanelSizes(1920.0f, 1022.0f, 2.0f);
    ExpectNear(z.leftWidth, 600.0f, 0.01f, "left scales with UI zoom");
    ExpectNear(z.rightWidth, 700.0f, 0.01f, "right scales with UI zoom");
}

static void TestSplitRatiosStayInRange()
{
    const EditorDockSplitRatios r = ComputeDefaultDockSplitRatios(1920.0f, 1022.0f, 1.0f);
    Expect(r.left > 0.0f && r.left < 0.5f, "left ratio sane");
    Expect(r.right > 0.0f && r.right < 0.5f, "right ratio sane");
    Expect(r.bottom > 0.0f && r.bottom < 0.7f, "bottom ratio sane");

    const EditorDockSplitRatios tiny = ComputeDefaultDockSplitRatios(100.0f, 80.0f, 1.0f);
    Expect(tiny.left >= 0.05f, "tiny host left floored");
    Expect(tiny.bottom >= 0.10f, "tiny host bottom floored");
}

static void TestIniPathResolution()
{
    Expect(ResolveEditorImGuiLayoutIniPath("").empty(), "empty root → empty path");
    Expect(ResolveEditorImGuiLayoutIniPath("C:/Games/MyProj") ==
               "C:/Games/MyProj/rom/editor/imgui_layout.ini",
           "ascii project ini path");

    // UTF-8: "專案" (project)
    const char kCjk[] = "D:/\xE5\xB0\x88\xE6\xA1\x88/Game";
    const std::string got = ResolveEditorImGuiLayoutIniPath(kCjk);
    Expect(got == std::string(kCjk) + "/rom/editor/imgui_layout.ini",
           "CJK project ini path stays UTF-8");
}

static void TestClientToBackbuffer()
{
    EditorClientRect c{100.0f, 50.0f, 800.0f, 450.0f};
    // 1:1
    EditorBackbufferRect bb = ClientRectToBackbuffer(c, 1920.0f, 1080.0f, 1920.0f, 1080.0f);
    ExpectNear(bb.x, 100.0f, 0.01f, "1:1 x");
    ExpectNear(bb.y, 50.0f, 0.01f, "1:1 y");
    ExpectNear(bb.w, 800.0f, 0.01f, "1:1 w");
    ExpectNear(bb.h, 450.0f, 0.01f, "1:1 h");

    // 2x DPI-style scale
    bb = ClientRectToBackbuffer(c, 960.0f, 540.0f, 1920.0f, 1080.0f);
    ExpectNear(bb.x, 200.0f, 0.01f, "2x x");
    ExpectNear(bb.y, 100.0f, 0.01f, "2x y");
    ExpectNear(bb.w, 1600.0f, 0.01f, "2x w");
    ExpectNear(bb.h, 900.0f, 0.01f, "2x h");

    // Zero client size must not divide-by-zero / produce NaN
    bb = ClientRectToBackbuffer(c, 0.0f, 0.0f, 1920.0f, 1080.0f);
    Expect(std::isfinite(bb.x) && std::isfinite(bb.w), "zero client finite");
    Expect(bb.w >= 1.0f && bb.h >= 1.0f, "min size enforced");
}

static void TestUsableViewport()
{
    Expect(!IsUsableViewportRect({0, 0, 10, 10}), "too small rejected");
    Expect(IsUsableViewportRect({0, 0, 64, 120}), "min size accepted");
}

int main()
{
    TestDefaultPanelSizesMatchLegacyLook();
    TestSplitRatiosStayInRange();
    TestIniPathResolution();
    TestClientToBackbuffer();
    TestUsableViewport();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("editor_docking_math_test: OK\n");
    return 0;
}
