// terrain_heightmap_test.cpp
// Linux-runnable unit tests for engine/terrain/terrain_heightmap.{h,cpp} (ADR 0011).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/terrain_heightmap_test tests/terrain_heightmap_test.cpp engine/terrain/terrain_heightmap.cpp
//   /tmp/terrain_heightmap_test

#include "engine/terrain/terrain_heightmap.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <vector>

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

// Height field h(x, z) = x + 2z sampled exactly into a 33x33 map (sizes 32 x 32,
// heightScale 65535 so one sample unit = one metre).
static Heightmap MakeRamp(Layout& layout) {
    layout = Layout{};
    layout.sizeX = 32.0f;
    layout.sizeZ = 32.0f;
    layout.heightScale = 65535.0f;
    layout.heightOffset = 0.0f;
    Heightmap map;
    CreateFlat(33, 0, map);
    for (int z = 0; z < 33; ++z)
        for (int x = 0; x < 33; ++x)
            map.Set(x, z, static_cast<std::uint16_t>(x + 2 * z));
    return map;
}

static void TestResolution() {
    Expect(IsValidResolution(33), "33 is valid");
    Expect(IsValidResolution(513), "513 is valid");
    Expect(IsValidResolution(4097), "4097 is valid");
    Expect(!IsValidResolution(17), "17 is below the minimum");
    Expect(!IsValidResolution(8193), "8193 is above the maximum");
    Expect(!IsValidResolution(512), "512 is not 2^n+1");
    Expect(!IsValidResolution(100), "100 is not 2^n+1");
    Expect(!IsValidResolution(-1), "negative is invalid");

    Expect(ResolutionFromR16ByteCount(33 * 33 * 2) == 33, "33x33 r16 size");
    Expect(ResolutionFromR16ByteCount(4097u * 4097u * 2u) == 4097, "4097x4097 r16 size");
    Expect(ResolutionFromR16ByteCount(32 * 32 * 2) == 0, "32x32 is not a valid r16 size");
    Expect(ResolutionFromR16ByteCount(33 * 33 * 2 + 1) == 0, "odd byte count");
    Expect(ResolutionFromR16ByteCount(0) == 0, "empty");
}

static void TestHeightmapAccess() {
    Heightmap map;
    Expect(!CreateFlat(100, 5, map), "CreateFlat rejects an invalid resolution");
    Expect(!map.IsValid(), "rejected map stays invalid");
    Expect(map.At(0, 0) == 0, "invalid map reads 0");

    Expect(CreateFlat(65, 1234, map), "CreateFlat 65");
    Expect(map.IsValid() && map.samples.size() == 65u * 65u, "flat map size");
    Expect(map.At(10, 20) == 1234, "flat value");

    map.Set(3, 4, 999);
    Expect(map.At(3, 4) == 999, "Set / At round trip");
    Expect(map.samples[4 * 65 + 3] == 999, "row-major: index = z * resolution + x");
    map.Set(-1, 0, 1);
    map.Set(65, 0, 1);
    Expect(map.At(0, 0) == 1234 && map.At(64, 0) == 1234, "out-of-range Set is ignored");
    map.Set(64, 64, 7);
    Expect(map.At(100, 100) == 7, "At clamps high coordinates");
    Expect(map.At(-5, 4) == map.At(0, 4), "At clamps low coordinates");
}

static void TestR16RoundTrip() {
    Heightmap map;
    CreateFlat(33, 0, map);
    for (std::size_t i = 0; i < map.samples.size(); ++i)
        map.samples[i] = static_cast<std::uint16_t>(i * 977u);
    map.samples[0] = 0x1234;
    const std::vector<std::uint8_t> bytes = EncodeR16(map);
    Expect(bytes.size() == 33u * 33u * 2u, "encoded size");
    Expect(bytes[0] == 0x34 && bytes[1] == 0x12, "little-endian");

    Heightmap back;
    std::string error;
    Expect(DecodeR16(bytes.data(), bytes.size(), back, &error), "decode");
    Expect(back.resolution == 33 && back.samples == map.samples, "round trip keeps every sample");

    Heightmap untouched;
    CreateFlat(33, 42, untouched);
    error.clear();
    Expect(!DecodeR16(bytes.data(), bytes.size() - 2, untouched, &error), "truncated file fails");
    Expect(!error.empty(), "truncated file reports a reason");
    Expect(untouched.At(0, 0) == 42 && untouched.resolution == 33, "failed decode leaves output untouched");
    Expect(!DecodeR16(nullptr, 0, untouched, nullptr), "null data fails without an error string");

    Heightmap invalid;
    Expect(EncodeR16(invalid).empty(), "invalid map encodes to nothing");
}

static void TestLayout() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Layout bad;
    bad.sizeX = nan;
    bad.sizeZ = -5.0f;
    bad.heightScale = inf;
    bad.heightOffset = nan;
    const Layout s = SanitizeLayout(bad);
    Expect(s.sizeX == Layout{}.sizeX, "NaN size -> default");
    Expect(s.sizeZ == kMinSize, "negative size -> minimum");
    Expect(s.heightScale == Layout{}.heightScale, "infinite height scale -> default");
    Expect(s.heightOffset == 0.0f, "NaN offset -> default");

    Layout big;
    big.sizeX = 1e9f;
    big.heightScale = -3.0f;
    big.heightOffset = -1e9f;
    const Layout b = SanitizeLayout(big);
    Expect(b.sizeX == kMaxSize, "size clamped to maximum");
    Expect(b.heightScale == 0.0f, "negative height scale -> 0");
    Expect(b.heightOffset == -kMaxHeightOffset, "offset clamped");

    Layout l;
    l.heightScale = 200.0f;
    l.heightOffset = -50.0f;
    Expect(Near(SampleToHeight(l, 0), -50.0f), "sample 0 = offset");
    Expect(Near(SampleToHeight(l, kMaxSample), 150.0f), "max sample = offset + scale");
    Expect(HeightToSample(l, -50.0f) == 0, "offset -> sample 0");
    Expect(HeightToSample(l, 150.0f) == kMaxSample, "top -> max sample");
    Expect(HeightToSample(l, -1000.0f) == 0, "below range clamps");
    Expect(HeightToSample(l, 1000.0f) == kMaxSample, "above range clamps");
    const std::uint16_t mid = HeightToSample(l, 50.0f);
    Expect(Near(SampleToHeight(l, mid), 50.0f, 0.01f), "height round trip within one step");
    Layout flat;
    flat.heightScale = 0.0f;
    Expect(HeightToSample(flat, 10.0f) == 0, "zero height scale maps to 0");
}

static void TestSampling() {
    Layout layout;
    const Heightmap map = MakeRamp(layout);
    Expect(Near(CellSizeX(layout, map), 1.0f) && Near(CellSizeZ(layout, map), 1.0f), "cell size");

    const Float3 p = SamplePosition(layout, map, 3, 5);
    Expect(Near(p.x, 3.0f) && Near(p.y, 13.0f) && Near(p.z, 5.0f), "sample position");

    float h = -1.0f;
    Expect(SampleHeight(layout, map, 3.0f, 5.0f, h) && Near(h, 13.0f), "height at a sample");
    // A plane is reproduced exactly on both triangles of a cell.
    Expect(SampleHeight(layout, map, 3.25f, 5.75f, h) && Near(h, 3.25f + 11.5f), "plane, upper triangle");
    Expect(SampleHeight(layout, map, 3.75f, 5.25f, h) && Near(h, 3.75f + 10.5f), "plane, lower triangle");
    Expect(SampleHeight(layout, map, 32.0f, 32.0f, h) && Near(h, 96.0f), "far corner is inside");
    Expect(SampleHeight(layout, map, 0.0f, 0.0f, h) && Near(h, 0.0f), "origin is inside");

    h = 123.0f;
    Expect(!SampleHeight(layout, map, -0.01f, 1.0f, h) && h == 123.0f, "outside -X");
    Expect(!SampleHeight(layout, map, 1.0f, 32.01f, h), "outside +Z");
    Expect(!SampleHeight(layout, map, std::nanf(""), 1.0f, h), "NaN position");

    Float3 n;
    Expect(SampleSurfaceNormal(layout, map, 10.3f, 10.7f, n), "surface normal inside");
    const float len = std::sqrt(1.0f + 1.0f + 4.0f);
    Expect(Near(n.x, -1.0f / len) && Near(n.y, 1.0f / len) && Near(n.z, -2.0f / len), "plane normal, upper triangle");
    Expect(SampleSurfaceNormal(layout, map, 10.7f, 10.3f, n) && Near(n.z, -2.0f / len), "plane normal, lower triangle");

    const Float3 vn = SampleVertexNormal(layout, map, 10, 10);
    Expect(Near(vn.x, -1.0f / len) && Near(vn.z, -2.0f / len), "vertex normal of a plane");
    const Float3 edge = SampleVertexNormal(layout, map, 0, 32);
    Expect(Near(edge.x, -1.0f / len) && Near(edge.z, -2.0f / len), "one-sided vertex normal at the edge");
}

static void TestTriangleSplit() {
    // Only the (1,1) corner is raised: the split along (0,0)-(1,1) puts the ridge on
    // that diagonal, so the cell centre is half the corner height on both triangles.
    Layout layout;
    layout.sizeX = 32.0f;
    layout.sizeZ = 32.0f;
    layout.heightScale = 65535.0f;
    Heightmap map;
    CreateFlat(33, 0, map);
    map.Set(1, 1, 10);
    float h = 0.0f;
    Expect(SampleHeight(layout, map, 0.5f, 0.5f, h) && Near(h, 5.0f), "diagonal (0,0)-(1,1) carries the ridge");
    Expect(SampleHeight(layout, map, 0.9f, 0.1f, h) && Near(h, 1.0f), "lower triangle (a, d, b)");
    Expect(SampleHeight(layout, map, 0.1f, 0.9f, h) && Near(h, 1.0f), "upper triangle (a, c, d)");

    // Raising (1,0) instead touches only the lower triangle.
    CreateFlat(33, 0, map);
    map.Set(1, 0, 10);
    Expect(SampleHeight(layout, map, 0.2f, 0.8f, h) && Near(h, 0.0f), "upper triangle ignores corner b");
    Expect(SampleHeight(layout, map, 0.8f, 0.2f, h) && Near(h, 6.0f), "lower triangle uses corner b");
}

static void TestChunks() {
    Expect(IsValidChunkQuads(64, 513), "64 on 513");
    Expect(!IsValidChunkQuads(64, 33), "64 does not fit 33");
    Expect(!IsValidChunkQuads(48, 513), "48 is not a power of two");
    Expect(!IsValidChunkQuads(4, 513), "4 is below the minimum");
    Expect(!IsValidChunkQuads(512, 1025), "512 is above the maximum");

    Expect(ResolveChunkQuads(64, 513) == 64, "valid request kept");
    Expect(ResolveChunkQuads(64, 33) == 32, "too big for the map -> map size");
    Expect(ResolveChunkQuads(100, 513) == 64, "non power of two rounds down");
    Expect(ResolveChunkQuads(1, 513) == kMinChunkQuads, "too small -> minimum");
    Expect(ResolveChunkQuads(100000, 4097) == kMaxChunkQuads, "too big -> maximum");
    Expect(ResolveChunkQuads(64, 100) == 0, "invalid resolution");

    ChunkGrid grid;
    Expect(!MakeChunkGrid(513, 48, grid), "MakeChunkGrid rejects a bad size");
    Expect(MakeChunkGrid(129, 32, grid) && grid.chunksPerSide == 4 && grid.ChunkCount() == 16, "4x4 grid");

    Heightmap map;
    CreateFlat(129, 100, map);
    map.Set(32, 32, 500); // corner shared by chunks (0,0), (1,0), (0,1), (1,1)
    map.Set(100, 10, 7);  // inside chunk (3,0)
    std::vector<ChunkBounds> bounds;
    ComputeChunkBounds(map, grid, bounds);
    Expect(bounds.size() == 16u, "one bounds entry per chunk");
    Expect(bounds[0].maxSample == 500 && bounds[1].maxSample == 500 &&
           bounds[4].maxSample == 500 && bounds[5].maxSample == 500, "shared corner counts for all four chunks");
    Expect(bounds[2].maxSample == 100 && bounds[2].minSample == 100, "untouched chunk");
    Expect(bounds[3].minSample == 7 && bounds[3].maxSample == 100, "min of chunk (3,0)");

    // Edit a border sample and update only that rectangle.
    map.Set(64, 64, 9000);
    std::vector<ChunkBounds> partial = bounds;
    UpdateChunkBounds(map, grid, 64, 64, 64, 64, partial);
    std::vector<ChunkBounds> full;
    ComputeChunkBounds(map, grid, full);
    bool same = partial.size() == full.size();
    for (std::size_t i = 0; same && i < full.size(); ++i)
        same = partial[i].minSample == full[i].minSample && partial[i].maxSample == full[i].maxSample;
    Expect(same, "partial update of a border sample matches a full recompute");
    Expect(partial[5].maxSample == 9000 && partial[10].maxSample == 9000, "both sides of the border updated");

    // Reversed / partly outside rectangles and stale vectors.
    map.Set(0, 0, 1);
    UpdateChunkBounds(map, grid, 5, 5, -10, -10, partial);
    Expect(partial[0].minSample == 1, "reversed rectangle clipped to the map");
    std::vector<ChunkBounds> stale(3);
    UpdateChunkBounds(map, grid, 0, 0, 0, 0, stale);
    Expect(stale.size() == 16u, "stale vector falls back to a full compute");

    ChunkGrid wrong;
    MakeChunkGrid(65, 32, wrong);
    ComputeChunkBounds(map, wrong, bounds);
    Expect(bounds.empty(), "grid for another resolution yields no bounds");
}

static void TestLod() {
    Expect(MaxLodLevel(64) == 5, "64 quads -> levels 0..5 (stride 32 leaves 2 cells)");
    Expect(MaxLodLevel(8) == 2, "8 quads -> levels 0..2");

    Expect(Near(DistanceToAabb({5, 5, 5}, {0, 0, 0}, {10, 10, 10}), 0.0f), "inside box");
    Expect(Near(DistanceToAabb({13, 14, 5}, {0, 0, 0}, {10, 10, 10}), 5.0f), "outside box");

    const float chunk = 10.0f; // level 0 up to 15, level 1 up to 30, level 2 up to 60 ...
    Expect(SelectChunkLod(0.0f, chunk, 1.0f, 5, -1) == 0, "close -> 0");
    Expect(SelectChunkLod(14.9f, chunk, 1.0f, 5, -1) == 0, "just inside level 0");
    Expect(SelectChunkLod(15.0f, chunk, 1.0f, 5, -1) == 1, "level 1 boundary");
    Expect(SelectChunkLod(45.0f, chunk, 1.0f, 5, -1) == 2, "level 2");
    Expect(SelectChunkLod(1e6f, chunk, 1.0f, 5, -1) == 5, "clamped to max level");
    Expect(SelectChunkLod(8.0f, chunk, 2.0f, 5, -1) == 1, "bias 2 switches sooner");
    Expect(SelectChunkLod(20.0f, chunk, 0.5f, 5, -1) == 0, "bias 0.5 switches later");
    Expect(SelectChunkLod(20.0f, chunk, std::nanf(""), 5, -1) == 1, "NaN bias -> 1");
    Expect(SelectChunkLod(-5.0f, chunk, 1.0f, 5, -1) == 0, "negative distance -> 0");
    Expect(SelectChunkLod(100.0f, 0.0f, 1.0f, 5, -1) == 0, "zero chunk size -> 0");

    // Hysteresis: at 14.0 a chunk coming from level 1 stays at 1 (14 * 1.1 >= 15) ...
    Expect(SelectChunkLod(14.0f, chunk, 1.0f, 5, 1) == 1, "hysteresis keeps the coarser level");
    // ... and goes finer once well inside.
    Expect(SelectChunkLod(13.0f, chunk, 1.0f, 5, 1) == 0, "finer once inside the margin");
    // Going coarser has no delay.
    Expect(SelectChunkLod(15.0f, chunk, 1.0f, 5, 0) == 1, "coarser switches at the boundary");
    Expect(SelectChunkLod(40.0f, chunk, 1.0f, 5, 99) == 2, "out-of-range previous level ignored");
}

static void TestChunkIndices() {
    std::vector<std::uint32_t> idx;
    Expect(!BuildChunkIndices(8, 3, idx), "stride must be a power of two");
    Expect(!BuildChunkIndices(8, 8, idx), "stride must leave 2 cells");
    Expect(!BuildChunkIndices(12, 2, idx), "chunkQuads must be a power of two");

    Expect(BuildChunkIndices(8, 1, idx) && idx.size() == 8u * 8u * 6u, "full detail index count");
    const std::uint32_t row = 9;
    std::uint32_t maxIndex = 0;
    bool allClockwise = true;
    for (std::size_t t = 0; t < idx.size(); t += 3) {
        // Vertex positions on the XZ grid (y = 0); D3D front faces are clockwise in
        // the left-handed view from above, i.e. cross(e1, e2).y > 0.
        float px[3], pz[3];
        for (int k = 0; k < 3; ++k) {
            maxIndex = std::max(maxIndex, idx[t + k]);
            px[k] = static_cast<float>(idx[t + k] % row);
            pz[k] = static_cast<float>(idx[t + k] / row);
        }
        const float e1x = px[1] - px[0], e1z = pz[1] - pz[0];
        const float e2x = px[2] - px[0], e2z = pz[2] - pz[0];
        const float crossY = e1z * e2x - e1x * e2z;
        if (!(crossY > 0.0f))
            allClockwise = false;
    }
    Expect(maxIndex == row * row - 1, "indices cover the whole vertex grid");
    Expect(allClockwise, "every triangle faces up (clockwise from above)");
    // First cell: a=0, b=1, c=9, d=10 -> (a, c, d), (a, d, b): diagonal 0-10.
    Expect(idx[0] == 0 && idx[1] == 9 && idx[2] == 10 && idx[3] == 0 && idx[4] == 10 && idx[5] == 1,
           "split along the (x,z)-(x+1,z+1) diagonal, matching SampleHeight");

    Expect(BuildChunkIndices(8, 4, idx) && idx.size() == 2u * 2u * 6u, "stride 4 index count");
    std::set<std::uint32_t> used(idx.begin(), idx.end());
    bool onStride = true;
    for (std::uint32_t v : used)
        onStride = onStride && (v % row) % 4 == 0 && (v / row) % 4 == 0;
    Expect(onStride, "coarse level only uses every 4th vertex");
    Expect(used.count(row * row - 1) == 1, "coarse level still reaches the far corner");
}

int main() {
    TestResolution();
    TestHeightmapAccess();
    TestR16RoundTrip();
    TestLayout();
    TestSampling();
    TestTriangleSplit();
    TestChunks();
    TestLod();
    TestChunkIndices();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("terrain_heightmap_test: all checks passed\n");
    return 0;
}
