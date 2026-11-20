// terrain_heightmap.h
// Terrain core (ADR 0011): the heightmap, its .r16 file format, surface queries,
// chunk bounds and chunk LOD. Pure logic (no D3D / DirectXMath / Windows), so it is
// unit-testable on Linux; the renderer, collider and editor tools build on it.
//
// Conventions (fixed by ADR 0011):
//   - A heightmap is square, `resolution` = 2^n + 1 samples per side (33 .. 4097).
//   - Samples are row-major: index = z * resolution + x. Column x runs along +X,
//     row z along +Z.
//   - Terrain-local space: sample (0, 0) is the origin, the terrain covers
//     [0, sizeX] x [0, sizeZ] on XZ, Y is up. The owning GameObject's position moves
//     it; its rotation and scale are not applied.
//   - Height of a sample = heightOffset + (sample / 65535) * heightScale.
//   - Each grid cell is split into two triangles along the diagonal from (x, z) to
//     (x + 1, z + 1). SampleHeight, the chunk index buffers and the collider all use
//     this split, so what you see is what you stand on.
//   - .r16 file: resolution^2 little-endian uint16, no header (the raw format World
//     Machine, Gaea and Unity export). The resolution is inferred from the size.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace terrain {

inline constexpr int kMinResolution = 33;
inline constexpr int kMaxResolution = 4097;
inline constexpr int kMinChunkQuads = 8;
inline constexpr int kMaxChunkQuads = 256;
inline constexpr int kDefaultChunkQuads = 64;
inline constexpr std::uint16_t kMaxSample = 65535;

// Layout limits applied by SanitizeLayout.
inline constexpr float kMinSize = 1.0f;
inline constexpr float kMaxSize = 100000.0f;
inline constexpr float kMaxHeightScale = 10000.0f;
inline constexpr float kMaxHeightOffset = 100000.0f;

// LOD: level L draws every 2^L-th sample. Level L is used up to a distance of
// chunkWorldSize * kLodDistanceFactor * 2^L (times the bias).
inline constexpr float kLodDistanceFactor = 1.5f;
inline constexpr float kLodHysteresis = 0.1f;
inline constexpr float kMinLodBias = 0.1f;
inline constexpr float kMaxLodBias = 10.0f;

struct Float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// 2^n + 1 within [kMinResolution, kMaxResolution].
bool IsValidResolution(int resolution);
// Resolution of a .r16 file of this many bytes, or 0 when no valid resolution fits.
int ResolutionFromR16ByteCount(std::size_t byteCount);

struct Heightmap {
    int resolution = 0;
    std::vector<std::uint16_t> samples;

    bool IsValid() const;
    // Coordinates are clamped to the map, so edge neighbours read the edge sample.
    std::uint16_t At(int x, int z) const;
    // Out-of-range coordinates are ignored.
    void Set(int x, int z, std::uint16_t value);
};

// False (out untouched) when the resolution is invalid.
bool CreateFlat(int resolution, std::uint16_t value, Heightmap& out);

// .r16 bytes -> heightmap. On failure returns false, leaves `out` untouched and
// writes a reason to `error` (when non-null).
bool DecodeR16(const std::uint8_t* data, std::size_t size, Heightmap& out, std::string* error);
// Heightmap -> .r16 bytes (empty for an invalid heightmap).
std::vector<std::uint8_t> EncodeR16(const Heightmap& map);

struct Layout {
    float sizeX = 256.0f;
    float sizeZ = 256.0f;
    float heightScale = 100.0f;
    float heightOffset = 0.0f;
};

// Replaces NaN / infinite values with the defaults and clamps to the limits above
// (sizes >= kMinSize, heightScale >= 0). Use on every Layout read from JSON.
Layout SanitizeLayout(const Layout& layout);

float SampleToHeight(const Layout& layout, std::uint16_t sample);
// Nearest sample for a height, clamped to [0, kMaxSample]. A heightScale of 0 maps
// every height to 0.
std::uint16_t HeightToSample(const Layout& layout, float height);

// Distance between neighbouring samples (0 for an invalid heightmap).
float CellSizeX(const Layout& layout, const Heightmap& map);
float CellSizeZ(const Layout& layout, const Heightmap& map);

// Terrain-local position of a sample (indices clamped).
Float3 SamplePosition(const Layout& layout, const Heightmap& map, int x, int z);

// Height of the triangulated surface at terrain-local (x, z). Returns false (and
// leaves outHeight untouched) outside [0, sizeX] x [0, sizeZ] or for an invalid map.
bool SampleHeight(const Layout& layout, const Heightmap& map, float x, float z, float& outHeight);
// Unit normal of the triangle under (x, z); false where SampleHeight is false.
bool SampleSurfaceNormal(const Layout& layout, const Heightmap& map, float x, float z, Float3& outNormal);
// Smooth unit normal at a sample from its neighbours (one-sided at the edges); the
// vertex normal the renderer uses. {0, 1, 0} for an invalid map.
Float3 SampleVertexNormal(const Layout& layout, const Heightmap& map, int x, int z);

// ---------------------------------------------------------------------------
// Chunks: the map is drawn and culled in square chunks of chunkQuads x chunkQuads
// cells. Neighbouring chunks share their border samples.

// Power of two in [kMinChunkQuads, kMaxChunkQuads] that divides resolution - 1.
bool IsValidChunkQuads(int chunkQuads, int resolution);
// The requested size if valid, else the nearest valid size not above it (or the
// smallest valid one). 0 when the resolution is invalid.
int ResolveChunkQuads(int requested, int resolution);

struct ChunkGrid {
    int chunkQuads = 0;
    int chunksPerSide = 0;
    int ChunkCount() const { return chunksPerSide * chunksPerSide; }
};

// False when the resolution or chunk size is invalid.
bool MakeChunkGrid(int resolution, int chunkQuads, ChunkGrid& out);

struct ChunkBounds {
    std::uint16_t minSample = 0;
    std::uint16_t maxSample = 0;
};

// Per-chunk sample range, row-major (index = chunkZ * chunksPerSide + chunkX),
// border samples included. Resizes `out`; clears it when map and grid disagree.
void ComputeChunkBounds(const Heightmap& map, const ChunkGrid& grid, std::vector<ChunkBounds>& out);
// Recomputes only the chunks touching the inclusive sample rectangle
// [x0, x1] x [z0, z1] (after an edit). Falls back to ComputeChunkBounds when
// `inOut` does not match the grid.
void UpdateChunkBounds(const Heightmap& map, const ChunkGrid& grid,
                       int x0, int z0, int x1, int z1, std::vector<ChunkBounds>& inOut);

// Coarsest level: stride 2^level leaves at least 2 cells per chunk side.
int MaxLodLevel(int chunkQuads);

// Distance from a point to an axis-aligned box (0 inside).
float DistanceToAabb(const Float3& point, const Float3& boxMin, const Float3& boxMax);

// LOD level in [0, maxLevel] for a chunk `distance` away. lodBias > 1 switches to
// coarser levels sooner (clamped to [kMinLodBias, kMaxLodBias]). prevLevel (-1 =
// none) adds hysteresis: moving to a finer level needs kLodHysteresis more margin.
int SelectChunkLod(float distance, float chunkWorldSize, float lodBias, int maxLevel, int prevLevel);

// Triangle-list indices for one chunk's vertex grid of (chunkQuads + 1)^2 vertices
// (row-major, index = z * (chunkQuads + 1) + x), drawing every `stride`-th vertex.
// Clockwise seen from above (front faces in the engine's left-handed, cull-back
// setup), split along the (x, z)-(x + 1, z + 1) diagonal. False when stride is not
// a power of two dividing chunkQuads into at least 2 cells.
bool BuildChunkIndices(int chunkQuads, int stride, std::vector<std::uint32_t>& out);

} // namespace terrain
