// terrain_draw_plan.cpp
// See terrain_draw_plan.h (ADR 0011).

#include "engine/terrain/terrain_draw_plan.h"

#include <algorithm>
#include <cmath>

namespace terrain {

int ChunkVertexCount(int chunkQuads) {
    if (chunkQuads <= 0)
        return 0;
    return 2 * (chunkQuads + 1) * (chunkQuads + 1);
}

bool BuildChunkMeshIndices(int chunkQuads, int stride, std::vector<std::uint32_t>& out) {
    if (!BuildChunkIndices(chunkQuads, stride, out))
        return false;
    const std::uint32_t row = static_cast<std::uint32_t>(chunkQuads + 1);
    const std::uint32_t skirt = row * row; // skirt vertex = surface vertex + skirt
    auto addSkirt = [&](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t sa = a + skirt;
        const std::uint32_t sb = b + skirt;
        out.insert(out.end(), {a, b, sb, a, sb, sa}); // one winding
        out.insert(out.end(), {a, sb, b, a, sa, sb}); // and the other
    };
    const std::uint32_t n = static_cast<std::uint32_t>(chunkQuads);
    const std::uint32_t s = static_cast<std::uint32_t>(stride);
    for (std::uint32_t i = 0; i < n; i += s) {
        addSkirt(i, i + s);                         // z = 0 edge
        addSkirt(n * row + i, n * row + i + s);     // z = n edge
        addSkirt(i * row, (i + s) * row);           // x = 0 edge
        addSkirt(i * row + n, (i + s) * row + n);   // x = n edge
    }
    return true;
}

float ChunkSkirtDepth(const Layout& layout, const Heightmap& map, const ChunkBounds& bounds) {
    const float range = SampleToHeight(layout, bounds.maxSample) - SampleToHeight(layout, bounds.minSample);
    const float cell = std::min(CellSizeX(layout, map), CellSizeZ(layout, map));
    return std::max(range, cell);
}

void ChunkWorldBox(const Layout& layout, const Heightmap& map, const ChunkGrid& grid,
                   const ChunkBounds& bounds, int chunkX, int chunkZ, const Float3& origin,
                   Float3& outMin, Float3& outMax) {
    const float cellX = CellSizeX(layout, map);
    const float cellZ = CellSizeZ(layout, map);
    const float span = static_cast<float>(grid.chunkQuads);
    const float lo = SampleToHeight(layout, bounds.minSample) - ChunkSkirtDepth(layout, map, bounds);
    const float hi = SampleToHeight(layout, bounds.maxSample);
    outMin = {origin.x + static_cast<float>(chunkX) * span * cellX, origin.y + lo,
              origin.z + static_cast<float>(chunkZ) * span * cellZ};
    outMax = {origin.x + static_cast<float>(chunkX + 1) * span * cellX, origin.y + hi,
              origin.z + static_cast<float>(chunkZ + 1) * span * cellZ};
}

int PlanChunkDraws(const Layout& layout, const Heightmap& map, const ChunkGrid& grid,
                   const std::vector<ChunkBounds>& bounds, const Float3& origin,
                   const PassOptions& options,
                   const std::function<bool(const Float3& boxMin, const Float3& boxMax)>& visible,
                   std::vector<int>& lastLevels,
                   std::vector<std::vector<ChunkInstance>>& outPerLevel) {
    const int maxLevel = grid.chunkQuads > 0 ? MaxLodLevel(grid.chunkQuads) : 0;
    outPerLevel.resize(static_cast<std::size_t>(maxLevel) + 1);
    for (auto& level : outPerLevel)
        level.clear();
    const std::size_t count = static_cast<std::size_t>(std::max(grid.ChunkCount(), 0));
    if (!map.IsValid() || grid.chunkQuads <= 0 || bounds.size() != count ||
        grid.chunkQuads * grid.chunksPerSide != map.resolution - 1)
        return 0;
    if (lastLevels.size() != count)
        lastLevels.assign(count, -1);

    const float chunkWorldSize = static_cast<float>(grid.chunkQuads) *
                                 std::max(CellSizeX(layout, map), CellSizeZ(layout, map));
    const int offset = std::max(options.levelOffset, 0);
    int drawn = 0;
    for (int cz = 0; cz < grid.chunksPerSide; ++cz) {
        for (int cx = 0; cx < grid.chunksPerSide; ++cx) {
            const std::size_t i = static_cast<std::size_t>(cz) * grid.chunksPerSide + cx;
            Float3 mn, mx;
            ChunkWorldBox(layout, map, grid, bounds[i], cx, cz, origin, mn, mx);
            if (visible && !visible(mn, mx))
                continue;

            int level = 0;
            if (options.lodEnabled) {
                level = SelectChunkLod(DistanceToAabb(options.camera, mn, mx), chunkWorldSize,
                                       options.lodBias, maxLevel, lastLevels[i]);
                if (options.storeLevels)
                    lastLevels[i] = level;
                level = std::min(level + offset, maxLevel);
            }

            ChunkInstance inst;
            inst.originX = static_cast<float>(cx * grid.chunkQuads);
            inst.originZ = static_cast<float>(cz * grid.chunkQuads);
            inst.skirtDepth = ChunkSkirtDepth(layout, map, bounds[i]);
            outPerLevel[static_cast<std::size_t>(level)].push_back(inst);
            ++drawn;
        }
    }
    return drawn;
}

} // namespace terrain
