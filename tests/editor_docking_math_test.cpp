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

static void TestDefaultSplitRatiosMatchPreDockingLayout()
{
    // Pre-docking editor_layout used 300 / 350 / max(340, 0.34*H) at 100% UI.
    // Host below ~58px chrome on 1080p → H=1022 (same as legacy test host).
    constexpr float kHostW = 1920.0f;
    constexpr float kHostH = 1022.0f;
    const EditorDockSplitRatios r = ComputeDefaultDockSplitRatios(kHostW, kHostH, 1.0f);

    // left = 300/1920; right = 350/(1920-300); bottom = max(340, 0.34*1022)/1022
    ExpectNear(r.left, 300.0f / kHostW, 0.001f, "left ratio 300/1920");
    ExpectNear(r.right, 350.0f / (kHostW - 300.0f), 0.001f, "right ratio 350/afterLeft");
    const float bottomPx = (std::max)(340.0f, kHostH * 0.34f);
    ExpectNear(r.bottom, bottomPx / kHostH, 0.001f, "bottom ratio ~34%");
    Expect(kEditorDefaultBottomTabCount == 2, "bottom has Console+Asset Browser tabs");
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

static bool RectInside(const EditorClientRect& inner, const EditorClientRect& outer, float eps)
{
    return inner.x + eps >= outer.x
        && inner.y + eps >= outer.y
        && inner.x + inner.w <= outer.x + outer.w + eps
        && inner.y + inner.h <= outer.y + outer.h + eps;
}

static void TestFit16x9()
{
    // Wide node → pillarbox (fit height, side bars).
    {
        const EditorClientRect node{100.0f, 50.0f, 1920.0f, 800.0f};
        const EditorClientRect r = FitEditor16x9Viewport(node);
        ExpectNear(r.h, 800.0f, 0.01f, "wide: height fills node");
        ExpectNear(r.w, 800.0f * 16.0f / 9.0f, 0.05f, "wide: width is 16:9");
        ExpectNear(r.y, 50.0f, 0.01f, "wide: y flush top");
        Expect(r.x > node.x, "wide: left pillarbox");
        Expect(r.x + r.w < node.x + node.w, "wide: right pillarbox");
        Expect(RectInside(r, node, 0.01f), "wide: stays inside");
        ExpectNear(r.w / r.h, 16.0f / 9.0f, 1e-4f, "wide: aspect 16:9");
    }

    // Tall node → letterbox (fit width, top/bottom bars).
    {
        const EditorClientRect node{0.0f, 0.0f, 1280.0f, 1200.0f};
        const EditorClientRect r = FitEditor16x9Viewport(node);
        ExpectNear(r.w, 1280.0f, 0.01f, "tall: width fills node");
        ExpectNear(r.h, 1280.0f * 9.0f / 16.0f, 0.05f, "tall: height is 16:9");
        ExpectNear(r.x, 0.0f, 0.01f, "tall: x flush left");
        Expect(r.y > node.y, "tall: top letterbox");
        Expect(r.y + r.h < node.y + node.h, "tall: bottom letterbox");
        Expect(RectInside(r, node, 0.01f), "tall: stays inside");
        ExpectNear(r.w / r.h, 16.0f / 9.0f, 1e-4f, "tall: aspect 16:9");
    }

    // Exact 16:9 → fills the node.
    {
        const EditorClientRect node{10.0f, 20.0f, 1600.0f, 900.0f};
        const EditorClientRect r = FitEditor16x9Viewport(node);
        ExpectNear(r.x, 10.0f, 0.01f, "exact: x");
        ExpectNear(r.y, 20.0f, 0.01f, "exact: y");
        ExpectNear(r.w, 1600.0f, 0.01f, "exact: w");
        ExpectNear(r.h, 900.0f, 0.01f, "exact: h");
    }

    // Degenerate / zero sizes → min 1x1, finite, no crash.
    {
        const EditorClientRect zero{5.0f, 7.0f, 0.0f, 0.0f};
        const EditorClientRect r = FitEditor16x9Viewport(zero);
        Expect(std::isfinite(r.x) && std::isfinite(r.y), "zero: finite pos");
        Expect(std::isfinite(r.w) && std::isfinite(r.h), "zero: finite size");
        Expect(r.w >= 1.0f && r.h >= 1.0f, "zero: min size");
    }
    {
        const EditorClientRect neg{0.0f, 0.0f, -10.0f, -5.0f};
        const EditorClientRect r = FitEditor16x9Viewport(neg);
        Expect(r.w >= 1.0f && r.h >= 1.0f, "neg: min size");
        Expect(std::isfinite(r.w / r.h), "neg: finite aspect");
        // Bounds collapse to 1x1 — aspect may be sacrificed to stay inside.
        Expect(RectInside(r, EditorClientRect{0, 0, 1, 1}, 0.01f), "neg: inside 1x1 hole");
    }

    // Non-integer sizes stay inside the node.
    {
        const EditorClientRect node{0.5f, 1.25f, 1000.333f, 501.777f};
        const EditorClientRect r = FitEditor16x9Viewport(node);
        Expect(RectInside(r, node, 0.02f), "fractional: stays inside");
        ExpectNear(r.w / r.h, 16.0f / 9.0f, 1e-3f, "fractional: aspect");
    }
}

int main()
{
    TestDefaultPanelSizesMatchLegacyLook();
    TestSplitRatiosStayInRange();
    TestDefaultSplitRatiosMatchPreDockingLayout();
    TestIniPathResolution();
    TestClientToBackbuffer();
    TestUsableViewport();
    TestFit16x9();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("editor_docking_math_test: OK\n");
    return 0;
}
