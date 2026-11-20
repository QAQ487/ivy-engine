// terrain_draw_plan.h
// Terrain chunk mesh and per-pass draw planning (ADR 0011). Pure logic (no D3D), so it
// is unit-testable on Linux; engine/terrain/terrain_renderer.cpp turns the plan into
// one instanced draw per LOD level.
//
// Chunk mesh: every chunk draws the same vertex buffer, (chunkQuads + 1)^2 surface
// vertices followed by the same number of skirt vertices (skirt index = surface index
// + (chunkQuads + 1)^2). The vertex shader reads the height of a vertex from the
// heightmap texture at chunk origin + vertex grid position, and pushes skirt vertices
// down by the chunk's skirt depth. Skirts hang from the four chunk edges and hide the
// cracks where neighbouring chunks use different LOD levels.

#pragma once

#include "engine/terrain/terrain_heightmap.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace terrain {

// Surface + skirt vertices of the shared chunk vertex buffer.
int ChunkVertexCount(int chunkQuads);

// BuildChunkIndices (surface) followed by skirt triangles along the four chunk edges at
// the same stride. Skirt quads are emitted with both windings, so they show from
// either side and need no special cull state. False for the same inputs as
// BuildChunkIndices.
bool BuildChunkMeshIndices(int chunkQuads, int stride, std::vector<std::uint32_t>& out);

// Per-instance data of one visible chunk (matches the TERRAINCHUNK input of
// VertexShaderTerrain.hlsl: float4 per instance).
struct ChunkInstance {
    float originX = 0.0f;    // sample index of the chunk's (0, 0) corner
    float originZ = 0.0f;
    float skirtDepth = 0.0f; // metres the skirt hangs below the surface
    float pad = 0.0f;
};
static_assert(sizeof(ChunkInstance) == 16, "ChunkInstance must stay a float4");

// How far a chunk's skirt hangs: its height range (the largest possible LOD crack along
// its edges) but at least one cell, so flat chunks still close sub-pixel gaps.
float ChunkSkirtDepth(const Layout& layout, const Heightmap& map, const ChunkBounds& bounds);

// World-space box of one chunk (skirt included), terrain placed at `origin`.
void ChunkWorldBox(const Layout& layout, const Heightmap& map, const ChunkGrid& grid,
                   const ChunkBounds& bounds, int chunkX, int chunkZ, const Float3& origin,
                   Float3& outMin, Float3& outMax);

struct PassOptions {
    Float3 camera;          // world position LOD distances are measured from
    float lodBias = 1.0f;   // > 1 = coarser sooner (already combined with any scene bias)
    bool lodEnabled = true; // false: every chunk at level 0
    int levelOffset = 0;    // added after selection (shadow passes draw coarser)
    // Main camera pass: remember each chunk's level for hysteresis. Other passes read
    // the remembered levels but do not change them.
    bool storeLevels = false;
};

// Chunks that pass `visible` (world box test; null = all visible), grouped by LOD
// level: outPerLevel[level] holds that level's instances in chunk order. lastLevels is
// the per-chunk hysteresis memory (-1 = none); it is resized to the chunk count when it
// does not match. outPerLevel is resized to MaxLodLevel(grid.chunkQuads) + 1. Returns
// the number of visible chunks.
int PlanChunkDraws(const Layout& layout, const Heightmap& map, const ChunkGrid& grid,
                   const std::vector<ChunkBounds>& bounds, const Float3& origin,
                   const PassOptions& options,
                   const std::function<bool(const Float3& boxMin, const Float3& boxMax)>& visible,
                   std::vector<int>& lastLevels,
                   std::vector<std::vector<ChunkInstance>>& outPerLevel);

} // namespace terrain
