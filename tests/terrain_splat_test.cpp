// terrain_splat_test.cpp
// Linux-runnable unit tests for engine/terrain/terrain_splat.{h,cpp} (ADR 0011).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/terrain_splat_test tests/terrain_splat_test.cpp engine/terrain/terrain_splat.cpp engine/terrain/terrain_brush.cpp engine/terrain/terrain_heightmap.cpp
//   /tmp/terrain_splat_test

#include "engine/terrain/terrain_splat.h"

#include <cstdio>

using namespace terrain;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static Layout TestLayout() {
    Layout l;
    l.sizeX = 64.0f;
    l.sizeZ = 64.0f;
    return l;
}

static bool AllSum255(const SplatMap& m) {
    for (std::size_t i = 0; i < m.rgba.size(); i += 4)
        if (m.rgba[i] + m.rgba[i + 1] + m.rgba[i + 2] + m.rgba[i + 3] != 255)
            return false;
    return true;
}

static void TestCreateAndFile() {
    SplatMap m;
    Expect(!CreateSplat(100, 0, m), "invalid resolution");
    Expect(!CreateSplat(65, 4, m), "invalid layer");
    Expect(CreateSplat(65, 2, m) && m.IsValid(), "create");
    Expect(m.Weight(10, 10, 2) == 255 && m.Weight(10, 10, 0) == 0, "all weight on the layer");
    Expect(m.Weight(-3, 99, 2) == 255, "clamped coordinates");
    Expect(m.Weight(1, 1, 7) == 0, "invalid layer reads 0");

    const auto bytes = EncodeSplat(m);
    Expect(bytes.size() == 65u * 65u * 4u, "encoded size");
    SplatMap back;
    std::string error;
    Expect(DecodeSplat(bytes.data(), bytes.size(), back, &error) && back.rgba == m.rgba && back.resolution == 65,
           "round trip");
    Expect(!DecodeSplat(bytes.data(), bytes.size() - 4, back, &error) && !error.empty(), "truncated file refused");
    Expect(!DecodeSplat(nullptr, 0, back, nullptr), "null data refused");
    Expect(EncodeSplat(SplatMap{}).empty(), "invalid map encodes to nothing");
}

static void TestPaint() {
    const Layout l = TestLayout();
    SplatMap m;
    CreateSplat(65, 0, m);
    Brush b;
    b.radius = 8.0f;
    b.strength = 2.0f; // full weight in half a second at the centre
    b.falloff = 0.5f;
    SampleRect changed = PaintSplat(l, m, 1, b, 32.0f, 32.0f, 0.25f);
    Expect(!changed.Empty(), "paint changes texels");
    Expect(m.Weight(32, 32, 1) >= 127 && m.Weight(32, 32, 1) <= 128, "half way after half the time");
    Expect(m.Weight(32, 32, 0) + m.Weight(32, 32, 1) == 255, "weight moved from layer 0");
    Expect(m.Weight(10, 10, 0) == 255, "far texel untouched");
    Expect(AllSum255(m), "every texel sums to 255");
    Expect(changed.x0 >= 24 && changed.x1 <= 40, "changed rect inside the brush");

    PaintSplat(l, m, 1, b, 32.0f, 32.0f, 1.0f);
    Expect(m.Weight(32, 32, 1) == 255, "saturates at full weight");

    // Painting a third layer takes weight proportionally from the other two.
    CreateSplat(65, 0, m);
    b.strength = 1.0f;
    PaintSplat(l, m, 1, b, 32.0f, 32.0f, 0.5f);
    PaintSplat(l, m, 3, b, 32.0f, 32.0f, 0.5f);
    const int w0 = m.Weight(32, 32, 0), w1 = m.Weight(32, 32, 1), w3 = m.Weight(32, 32, 3);
    Expect(w3 >= 127 && w3 <= 128, "third layer half way");
    Expect(w0 > 0 && w1 > 0 && std::abs(w0 - w1) <= 2, "others shrink proportionally");
    Expect(AllSum255(m), "sums stay 255");

    // A broken texel (all zero) is repaired.
    m.rgba[(32 * 65 + 32) * 4 + 0] = 0;
    m.rgba[(32 * 65 + 32) * 4 + 1] = 0;
    m.rgba[(32 * 65 + 32) * 4 + 3] = 0;
    PaintSplat(l, m, 2, b, 32.0f, 32.0f, 0.1f);
    Expect(AllSum255(m), "zero texel repaired");

    Expect(PaintSplat(l, m, 5, b, 32.0f, 32.0f, 0.1f).Empty(), "bad layer does nothing");
    Expect(PaintSplat(l, m, 1, b, 32.0f, 32.0f, 0.0f).Empty(), "zero dt does nothing");
    Expect(PaintSplat(l, m, 1, b, 900.0f, 900.0f, 0.1f).Empty(), "off the terrain does nothing");
}

static void TestDifferentResolution() {
    // A 33 x 33 splat over the same 64 m terrain: texel spacing 2 m.
    const Layout l = TestLayout();
    SplatMap m;
    CreateSplat(33, 0, m);
    const SampleRect r = SplatBrushRect(l, m, 32.0f, 32.0f, 4.0f);
    Expect(r.x0 == 14 && r.x1 == 18, "brush rect in splat texels");
    Brush b;
    b.radius = 4.0f;
    b.strength = 100.0f;
    b.falloff = 0.0f;
    PaintSplat(l, m, 2, b, 32.0f, 32.0f, 1.0f);
    Expect(m.Weight(16, 16, 2) == 255 && m.Weight(17, 16, 2) == 255, "painted around (16, 16)");
    Expect(m.Weight(19, 16, 2) == 0, "outside the brush untouched");
}

static void TestCopyPaste() {
    const Layout l = TestLayout();
    SplatMap m;
    CreateSplat(65, 0, m);
    const SplatMap original = m;
    Brush b;
    const SampleRect changed = PaintSplat(l, m, 1, b, 20.0f, 20.0f, 0.3f);
    const auto before = CopySplatRect(original, changed);
    const auto after = CopySplatRect(m, changed);
    Expect(before.size() == static_cast<std::size_t>(changed.Width()) * changed.Height() * 4, "copy size");
    Expect(PasteSplatRect(m, changed, before) && m.rgba == original.rgba, "undo");
    Expect(PasteSplatRect(m, changed, after) && m.rgba != original.rgba, "redo");
    Expect(!PasteSplatRect(m, changed, std::vector<std::uint8_t>(5)), "size mismatch refused");
    Expect(!PasteSplatRect(m, SampleRect{60, 60, 70, 70}, std::vector<std::uint8_t>(11 * 11 * 4)),
           "rect off the map refused");
}

int main() {
    TestCreateAndFile();
    TestPaint();
    TestDifferentResolution();
    TestCopyPaste();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("terrain_splat_test: all checks passed\n");
    return 0;
}
