// terrain_brush_test.cpp
// Linux-runnable unit tests for engine/terrain/terrain_brush.{h,cpp} (ADR 0011).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/terrain_brush_test tests/terrain_brush_test.cpp engine/terrain/terrain_brush.cpp engine/terrain/terrain_heightmap.cpp
//   /tmp/terrain_brush_test

#include "engine/terrain/terrain_brush.h"

#include <cmath>
#include <cstdio>

using namespace terrain;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

// 65 x 65 samples over 64 x 64 m, heights 0..100 m.
static Layout TestLayout() {
    Layout l;
    l.sizeX = 64.0f;
    l.sizeZ = 64.0f;
    l.heightScale = 100.0f;
    l.heightOffset = 0.0f;
    return l;
}

static Heightmap FlatAt(const Layout& l, float height) {
    Heightmap m;
    CreateFlat(65, HeightToSample(l, height), m);
    return m;
}

static float HeightAt(const Layout& l, const Heightmap& m, int x, int z) {
    return SampleToHeight(l, m.At(x, z));
}

static void TestRects() {
    const SampleRect a{2, 3, 5, 6}, b{4, 1, 9, 4};
    const SampleRect u = UnionRect(a, b);
    Expect(u.x0 == 2 && u.z0 == 1 && u.x1 == 9 && u.z1 == 6, "union");
    Expect(UnionRect(SampleRect{}, a).x1 == 5, "union with empty");
    Expect(SampleRect{}.Empty() && SampleRect{}.Width() == 0, "default rect is empty");
    Expect(a.Width() == 4 && a.Height() == 4, "width / height");

    Heightmap m;
    CreateFlat(33, 0, m);
    const SampleRect c = ClipRect({-5, 30, 3, 40}, m);
    Expect(c.x0 == 0 && c.z0 == 30 && c.x1 == 3 && c.z1 == 32, "clip to the map");
    Expect(ClipRect({40, 0, 50, 5}, m).Empty(), "rect outside the map");
}

static void TestWeight() {
    Expect(Near(BrushWeight(0.0f, 10.0f, 0.5f), 1.0f), "centre = 1");
    Expect(Near(BrushWeight(5.0f, 10.0f, 0.5f), 1.0f), "inside the hard core = 1");
    Expect(Near(BrushWeight(7.5f, 10.0f, 0.5f), 0.5f), "middle of the fade = 0.5");
    Expect(BrushWeight(10.0f, 10.0f, 0.5f) == 0.0f, "edge = 0");
    Expect(BrushWeight(11.0f, 10.0f, 0.5f) == 0.0f, "outside = 0");
    Expect(Near(BrushWeight(9.9f, 10.0f, 0.0f), 1.0f), "falloff 0 = hard edge");
    Expect(Near(BrushWeight(5.0f, 10.0f, 1.0f), 0.5f), "falloff 1 fades from the centre");
    Expect(BrushWeight(1.0f, 0.0f, 0.5f) == 0.0f, "zero radius");
    float prev = 2.0f;
    bool monotonic = true;
    for (int i = 0; i <= 100; ++i) {
        const float w = BrushWeight(i * 0.1f, 10.0f, 0.7f);
        monotonic = monotonic && w <= prev + 1e-6f;
        prev = w;
    }
    Expect(monotonic, "weight never increases with distance");
}

static void TestBrushRect() {
    const Layout l = TestLayout();
    const Heightmap m = FlatAt(l, 10.0f);
    const SampleRect r = BrushRect(l, m, 32.0f, 32.0f, 4.0f);
    Expect(r.x0 == 28 && r.x1 == 36 && r.z0 == 28 && r.z1 == 36, "rect around the centre");
    const SampleRect edge = BrushRect(l, m, 1.0f, 63.0f, 4.0f);
    Expect(edge.x0 == 0 && edge.z1 == 64, "clipped at the map edge");
    Expect(BrushRect(l, m, 500.0f, 500.0f, 4.0f).Empty(), "brush off the map");
    Expect(BrushRect(l, m, 32.0f, 32.0f, 0.0f).Empty(), "zero radius");
}

static void TestRaiseLower() {
    const Layout l = TestLayout();
    Heightmap m = FlatAt(l, 10.0f);
    Brush b;
    b.mode = BrushMode::Raise;
    b.radius = 8.0f;
    b.strength = 6.0f; // m/s
    b.falloff = 0.5f;
    const SampleRect changed = ApplyBrush(l, m, b, 32.0f, 32.0f, 0.5f);
    Expect(!changed.Empty(), "raise changes samples");
    Expect(Near(HeightAt(l, m, 32, 32), 13.0f, 0.01f), "centre rises strength * dt");
    Expect(Near(HeightAt(l, m, 28, 32), 13.0f, 0.01f), "hard core rises fully");
    Expect(HeightAt(l, m, 38, 32) > 10.0f && HeightAt(l, m, 38, 32) < 13.0f, "fade rises partly");
    Expect(Near(HeightAt(l, m, 40, 32), 10.0f, 0.01f), "edge untouched");
    Expect(Near(HeightAt(l, m, 10, 10), 10.0f, 0.01f), "far sample untouched");
    Expect(changed.x0 >= 24 && changed.x1 <= 40, "changed rect stays inside the brush");

    b.mode = BrushMode::Lower;
    ApplyBrush(l, m, b, 32.0f, 32.0f, 0.5f);
    Expect(Near(HeightAt(l, m, 32, 32), 10.0f, 0.01f), "lower undoes an equal raise at the centre");

    // Clamped to the height range.
    b.mode = BrushMode::Raise;
    b.strength = 1000.0f;
    ApplyBrush(l, m, b, 32.0f, 32.0f, 1.0f);
    Expect(m.At(32, 32) == kMaxSample, "raise clamps at the top");
    b.mode = BrushMode::Lower;
    ApplyBrush(l, m, b, 32.0f, 32.0f, 1.0f);
    ApplyBrush(l, m, b, 32.0f, 32.0f, 1.0f);
    Expect(m.At(32, 32) == 0, "lower clamps at the bottom");

    Heightmap untouched = FlatAt(l, 10.0f);
    Expect(ApplyBrush(l, untouched, b, 32.0f, 32.0f, 0.0f).Empty(), "zero dt does nothing");
    b.strength = 0.0f;
    Expect(ApplyBrush(l, untouched, b, 32.0f, 32.0f, 1.0f).Empty(), "zero strength does nothing");
    b.strength = 5.0f;
    Expect(ApplyBrush(l, untouched, b, 500.0f, 500.0f, 1.0f).Empty(), "off the map does nothing");
}

static void TestSmooth() {
    const Layout l = TestLayout();
    Heightmap m = FlatAt(l, 10.0f);
    m.Set(32, 32, HeightToSample(l, 19.0f)); // one spike
    Brush b;
    b.mode = BrushMode::Smooth;
    b.radius = 4.0f;
    b.strength = 100.0f; // full blend in one step
    b.falloff = 0.0f;
    ApplyBrush(l, m, b, 32.0f, 32.0f, 1.0f);
    // Spike becomes the 3x3 average: (8 * 10 + 19) / 9 = 11.
    Expect(Near(HeightAt(l, m, 32, 32), 11.0f, 0.01f), "spike averaged with its neighbours");
    Expect(Near(HeightAt(l, m, 33, 32), 11.0f, 0.01f), "neighbours read the pre-step spike (snapshot)");
    Expect(Near(HeightAt(l, m, 34, 32), 10.0f, 0.01f), "two samples away unchanged");

    // A plane is a fixed point of smoothing (away from the clamp).
    Heightmap plane;
    CreateFlat(65, 0, plane);
    for (int z = 0; z < 65; ++z)
        for (int x = 0; x < 65; ++x)
            plane.Set(x, z, static_cast<std::uint16_t>(10000 + 100 * x + 50 * z));
    const Heightmap before = plane;
    ApplyBrush(l, plane, b, 32.0f, 32.0f, 1.0f);
    Expect(plane.samples == before.samples, "smoothing leaves a plane unchanged");
}

static void TestFlatten() {
    const Layout l = TestLayout();
    Heightmap m = FlatAt(l, 10.0f);
    m.Set(32, 32, HeightToSample(l, 30.0f));
    m.Set(33, 32, HeightToSample(l, 2.0f));
    Brush b;
    b.mode = BrushMode::Flatten;
    b.radius = 4.0f;
    b.strength = 100.0f;
    b.falloff = 0.0f;
    b.targetHeight = 20.0f;
    ApplyBrush(l, m, b, 32.0f, 32.0f, 1.0f);
    Expect(Near(HeightAt(l, m, 32, 32), 20.0f, 0.01f), "high sample pulled down to the target");
    Expect(Near(HeightAt(l, m, 33, 32), 20.0f, 0.01f), "low sample pulled up to the target");
    Expect(Near(HeightAt(l, m, 31, 31), 20.0f, 0.01f), "flat ground raised to the target");
    Expect(Near(HeightAt(l, m, 40, 32), 10.0f, 0.01f), "outside the brush unchanged");

    b.strength = 1.0f;
    m = FlatAt(l, 10.0f);
    ApplyBrush(l, m, b, 32.0f, 32.0f, 0.25f);
    Expect(Near(HeightAt(l, m, 32, 32), 12.5f, 0.01f), "partial blend: strength * dt of the way");
}

static void TestCopyPaste() {
    const Layout l = TestLayout();
    Heightmap m = FlatAt(l, 10.0f);
    const Heightmap original = m;
    Brush b;
    b.radius = 6.0f;
    const SampleRect changed = ApplyBrush(l, m, b, 20.0f, 40.0f, 0.5f);
    const SampleRect rect = UnionRect(changed, SampleRect{18, 38, 22, 42});
    const auto before = CopyRect(original, rect);
    const auto after = CopyRect(m, rect);
    Expect(before.size() == static_cast<std::size_t>(rect.Width()) * rect.Height(), "copy size");
    Expect(PasteRect(m, rect, before) && m.samples == original.samples, "paste before = undo");
    Expect(PasteRect(m, rect, after) && m.samples != original.samples, "paste after = redo");
    Expect(!PasteRect(m, rect, std::vector<std::uint16_t>(3)), "size mismatch refused");
    Expect(!PasteRect(m, SampleRect{60, 60, 70, 70}, std::vector<std::uint16_t>(121)), "rect off the map refused");
    Expect(CopyRect(m, SampleRect{}).empty(), "empty rect copies nothing");
}

static void TestSanitize() {
    Brush b;
    b.radius = std::nanf("");
    b.strength = -3.0f;
    b.falloff = 5.0f;
    const Brush s = SanitizeBrush(b);
    Expect(s.radius == Brush{}.radius && s.strength == 0.0f && s.falloff == 1.0f, "brush sanitised");
}

int main() {
    TestRects();
    TestWeight();
    TestBrushRect();
    TestRaiseLower();
    TestSmooth();
    TestFlatten();
    TestCopyPaste();
    TestSanitize();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("terrain_brush_test: all checks passed\n");
    return 0;
}
