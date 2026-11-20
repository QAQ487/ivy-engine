// terrain_draw_plan_test.cpp
// Linux-runnable unit tests for engine/terrain/terrain_draw_plan.{h,cpp} (ADR 0011).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/terrain_draw_plan_test tests/terrain_draw_plan_test.cpp engine/terrain/terrain_draw_plan.cpp engine/terrain/terrain_heightmap.cpp
//   /tmp/terrain_draw_plan_test

#include "engine/terrain/terrain_draw_plan.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <utility>
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

static void TestMeshIndices() {
    Expect(ChunkVertexCount(8) == 2 * 81, "vertex count = surface + skirt copy");
    Expect(ChunkVertexCount(0) == 0, "no vertices for an empty chunk");

    std::vector<std::uint32_t> idx;
    Expect(!BuildChunkMeshIndices(8, 3, idx), "bad stride rejected");

    const int n = 8;
    const std::uint32_t row = n + 1;
    const std::uint32_t skirtBase = row * row;
    for (int stride : {1, 2, 4}) {
        Expect(BuildChunkMeshIndices(n, stride, idx), "mesh indices build");
        const std::size_t cells = static_cast<std::size_t>(n / stride);
        const std::size_t surface = cells * cells * 6;
        const std::size_t skirt = 4 * cells * 12; // 4 edges, 2 quads (both windings) per segment
        Expect(idx.size() == surface + skirt, "surface + skirt index count");

        // Every skirt triangle uses only edge vertices (surface or their skirt copies),
        // and each edge segment's quad covers the segment.
        bool edgeOnly = true;
        std::uint32_t maxIndex = 0;
        for (std::size_t i = surface; i < idx.size(); ++i) {
            maxIndex = std::max(maxIndex, idx[i]);
            const std::uint32_t v = idx[i] >= skirtBase ? idx[i] - skirtBase : idx[i];
            const std::uint32_t x = v % row, z = v / row;
            if (!(x == 0 || z == 0 || x == static_cast<std::uint32_t>(n) || z == static_cast<std::uint32_t>(n)))
                edgeOnly = false;
            if (x % static_cast<std::uint32_t>(stride) != 0 || z % static_cast<std::uint32_t>(stride) != 0)
                edgeOnly = false;
        }
        Expect(edgeOnly, "skirts only use edge vertices on the LOD stride");
        Expect(maxIndex < static_cast<std::uint32_t>(ChunkVertexCount(n)), "skirt indices inside the vertex buffer");

        // Each skirt quad appears with both windings: every triangle (a,b,c) has a
        // reversed partner covering the same three vertices.
        std::map<std::set<std::uint32_t>, int> windings;
        for (std::size_t i = surface; i < idx.size(); i += 3) {
            const std::set<std::uint32_t> tri{idx[i], idx[i + 1], idx[i + 2]};
            windings[tri] += 1;
        }
        bool paired = true;
        for (const auto& kv : windings)
            paired = paired && kv.second == 2;
        Expect(paired, "every skirt triangle is drawn with both windings");
    }
}

static Heightmap MakeMap(Layout& layout) {
    layout = Layout{};
    layout.sizeX = 64.0f;
    layout.sizeZ = 64.0f;
    layout.heightScale = 65535.0f; // one sample unit = one metre
    Heightmap map;
    CreateFlat(65, 10, map);
    map.Set(40, 40, 30); // inside chunk (1,1) of a 2x2 grid of 32-quad chunks
    return map;
}

static void TestBoxesAndSkirts() {
    Layout layout;
    const Heightmap map = MakeMap(layout);
    ChunkGrid grid;
    MakeChunkGrid(65, 32, grid);
    std::vector<ChunkBounds> bounds;
    ComputeChunkBounds(map, grid, bounds);

    Expect(Near(ChunkSkirtDepth(layout, map, bounds[0]), 1.0f), "flat chunk skirt = one cell");
    Expect(Near(ChunkSkirtDepth(layout, map, bounds[3]), 20.0f), "bumpy chunk skirt = height range");

    Float3 mn, mx;
    ChunkWorldBox(layout, map, grid, bounds[3], 1, 1, {100.0f, 5.0f, -50.0f}, mn, mx);
    Expect(Near(mn.x, 132.0f) && Near(mx.x, 164.0f), "box X follows chunk and origin");
    Expect(Near(mn.z, -18.0f) && Near(mx.z, 14.0f), "box Z follows chunk and origin");
    Expect(Near(mx.y, 35.0f), "box top = highest sample + origin");
    Expect(Near(mn.y, 5.0f + 10.0f - 20.0f), "box bottom includes the skirt");
}

static void TestPlan() {
    Layout layout;
    const Heightmap map = MakeMap(layout);
    ChunkGrid grid;
    MakeChunkGrid(65, 32, grid);
    std::vector<ChunkBounds> bounds;
    ComputeChunkBounds(map, grid, bounds);
    const Float3 origin{0.0f, 0.0f, 0.0f};

    std::vector<int> last;
    std::vector<std::vector<ChunkInstance>> perLevel;
    PassOptions opt;
    opt.camera = {16.0f, 50.0f, 16.0f}; // above chunk (0,0)
    opt.storeLevels = true;

    int drawn = PlanChunkDraws(layout, map, grid, bounds, origin, opt, nullptr, last, perLevel);
    Expect(drawn == 4, "all chunks drawn without a visibility test");
    Expect(perLevel.size() == static_cast<std::size_t>(MaxLodLevel(32) + 1), "one bucket per LOD level");
    Expect(last.size() == 4u, "hysteresis memory sized to the chunk count");
    std::size_t total = 0;
    for (const auto& l : perLevel)
        total += l.size();
    Expect(total == 4u, "every drawn chunk lands in exactly one bucket");
    Expect(!perLevel[0].empty() && Near(perLevel[0][0].originX, 0.0f) && Near(perLevel[0][0].originZ, 0.0f),
           "chunk under the camera at full detail, origin in samples");

    // Far camera: everything coarse.
    opt.camera = {5000.0f, 0.0f, 5000.0f};
    PlanChunkDraws(layout, map, grid, bounds, origin, opt, nullptr, last, perLevel);
    Expect(perLevel.back().size() == 4u, "far chunks at the coarsest level");
    Expect(last[0] == MaxLodLevel(32), "main pass stores the level");

    // Shadow-style pass: offset applied, memory untouched.
    opt.camera = {16.0f, 50.0f, 16.0f};
    opt.storeLevels = false;
    opt.levelOffset = 1;
    std::vector<int> before = last;
    PlanChunkDraws(layout, map, grid, bounds, origin, opt, nullptr, last, perLevel);
    Expect(last == before, "non-storing pass keeps the hysteresis memory");

    // LOD off: everything at level 0 even far away, offset ignored.
    opt.lodEnabled = false;
    opt.camera = {5000.0f, 0.0f, 5000.0f};
    PlanChunkDraws(layout, map, grid, bounds, origin, opt, nullptr, last, perLevel);
    Expect(perLevel[0].size() == 4u, "LOD disabled -> level 0");

    // Visibility callback: only boxes reaching past x = 40 survive (chunks x = 1).
    opt.lodEnabled = true;
    opt.levelOffset = 0;
    drawn = PlanChunkDraws(layout, map, grid, bounds, origin, opt,
                           [](const Float3&, const Float3& mx) { return mx.x > 40.0f; }, last, perLevel);
    Expect(drawn == 2, "visibility callback culls chunks");
    bool onlyRight = true;
    for (const auto& l : perLevel)
        for (const auto& inst : l)
            onlyRight = onlyRight && Near(inst.originX, 32.0f);
    Expect(onlyRight, "culled chunks are not planned");
    bool skirtOk = true;
    for (const auto& l : perLevel)
        for (const auto& inst : l)
            if (Near(inst.originZ, 32.0f))
                skirtOk = skirtOk && Near(inst.skirtDepth, 20.0f);
    Expect(skirtOk, "instance carries the chunk's skirt depth");

    // Mismatched inputs plan nothing.
    std::vector<ChunkBounds> wrong(3);
    Expect(PlanChunkDraws(layout, map, grid, wrong, origin, opt, nullptr, last, perLevel) == 0,
           "bounds for another grid -> nothing drawn");
    Heightmap invalid;
    Expect(PlanChunkDraws(layout, invalid, grid, bounds, origin, opt, nullptr, last, perLevel) == 0,
           "invalid map -> nothing drawn");
}

int main() {
    TestMeshIndices();
    TestBoxesAndSkirts();
    TestPlan();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("terrain_draw_plan_test: all checks passed\n");
    return 0;
}
