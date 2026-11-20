// terrain_heightmap.cpp
// See terrain_heightmap.h (ADR 0011).

#include "engine/terrain/terrain_heightmap.h"

#include <algorithm>
#include <cmath>

namespace terrain {

namespace {

bool IsPowerOfTwo(int v) {
    return v > 0 && (v & (v - 1)) == 0;
}

float FiniteOr(float v, float fallback) {
    return std::isfinite(v) ? v : fallback;
}

int ClampInt(int v, int lo, int hi) {
    return std::min(std::max(v, lo), hi);
}

Float3 Normalize(float x, float y, float z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    if (!(len > 0.0f) || !std::isfinite(len))
        return {0.0f, 1.0f, 0.0f};
    return {x / len, y / len, z / len};
}

// Cell under terrain-local (x, z) and the position inside it. False outside.
struct CellHit {
    int cx = 0;
    int cz = 0;
    float fx = 0.0f; // 0..1 across the cell along X
    float fz = 0.0f; // 0..1 across the cell along Z
};

bool LocateCell(const Layout& layout, const Heightmap& map, float x, float z, CellHit& out) {
    if (!map.IsValid() || !std::isfinite(x) || !std::isfinite(z))
        return false;
    if (x < 0.0f || z < 0.0f || x > layout.sizeX || z > layout.sizeZ)
        return false;
    const int cells = map.resolution - 1;
    const float gx = x / layout.sizeX * static_cast<float>(cells);
    const float gz = z / layout.sizeZ * static_cast<float>(cells);
    out.cx = ClampInt(static_cast<int>(std::floor(gx)), 0, cells - 1);
    out.cz = ClampInt(static_cast<int>(std::floor(gz)), 0, cells - 1);
    out.fx = std::min(std::max(gx - static_cast<float>(out.cx), 0.0f), 1.0f);
    out.fz = std::min(std::max(gz - static_cast<float>(out.cz), 0.0f), 1.0f);
    return true;
}

ChunkBounds BoundsOfChunk(const Heightmap& map, const ChunkGrid& grid, int chunkX, int chunkZ) {
    const int x0 = chunkX * grid.chunkQuads;
    const int z0 = chunkZ * grid.chunkQuads;
    ChunkBounds b;
    b.minSample = kMaxSample;
    b.maxSample = 0;
    for (int z = z0; z <= z0 + grid.chunkQuads; ++z) {
        const std::uint16_t* row = &map.samples[static_cast<std::size_t>(z) * map.resolution];
        for (int x = x0; x <= x0 + grid.chunkQuads; ++x) {
            b.minSample = std::min(b.minSample, row[x]);
            b.maxSample = std::max(b.maxSample, row[x]);
        }
    }
    return b;
}

bool GridMatchesMap(const Heightmap& map, const ChunkGrid& grid) {
    return map.IsValid() && grid.chunkQuads > 0 &&
           grid.chunkQuads * grid.chunksPerSide == map.resolution - 1;
}

} // namespace

bool IsValidResolution(int resolution) {
    return resolution >= kMinResolution && resolution <= kMaxResolution &&
           IsPowerOfTwo(resolution - 1);
}

int ResolutionFromR16ByteCount(std::size_t byteCount) {
    if (byteCount == 0 || byteCount % 2 != 0)
        return 0;
    const std::size_t count = byteCount / 2;
    for (int r = kMinResolution; r <= kMaxResolution; r = (r - 1) * 2 + 1) {
        if (static_cast<std::size_t>(r) * static_cast<std::size_t>(r) == count)
            return r;
    }
    return 0;
}

bool Heightmap::IsValid() const {
    return IsValidResolution(resolution) &&
           samples.size() == static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
}

std::uint16_t Heightmap::At(int x, int z) const {
    if (!IsValid())
        return 0;
    x = ClampInt(x, 0, resolution - 1);
    z = ClampInt(z, 0, resolution - 1);
    return samples[static_cast<std::size_t>(z) * resolution + x];
}

void Heightmap::Set(int x, int z, std::uint16_t value) {
    if (!IsValid() || x < 0 || z < 0 || x >= resolution || z >= resolution)
        return;
    samples[static_cast<std::size_t>(z) * resolution + x] = value;
}

bool CreateFlat(int resolution, std::uint16_t value, Heightmap& out) {
    if (!IsValidResolution(resolution))
        return false;
    out.resolution = resolution;
    out.samples.assign(static_cast<std::size_t>(resolution) * resolution, value);
    return true;
}

bool DecodeR16(const std::uint8_t* data, std::size_t size, Heightmap& out, std::string* error) {
    if (!data || size == 0) {
        if (error)
            *error = "heightmap file is empty";
        return false;
    }
    const int resolution = ResolutionFromR16ByteCount(size);
    if (resolution == 0) {
        if (error)
            *error = "heightmap file is " + std::to_string(size) +
                     " bytes; a .r16 heightmap must be N*N 16-bit samples with N = 2^n+1 between " +
                     std::to_string(kMinResolution) + " and " + std::to_string(kMaxResolution);
        return false;
    }
    Heightmap map;
    map.resolution = resolution;
    map.samples.resize(static_cast<std::size_t>(resolution) * resolution);
    for (std::size_t i = 0; i < map.samples.size(); ++i) {
        map.samples[i] = static_cast<std::uint16_t>(data[2 * i] | (data[2 * i + 1] << 8));
    }
    out = std::move(map);
    return true;
}

std::vector<std::uint8_t> EncodeR16(const Heightmap& map) {
    std::vector<std::uint8_t> bytes;
    if (!map.IsValid())
        return bytes;
    bytes.resize(map.samples.size() * 2);
    for (std::size_t i = 0; i < map.samples.size(); ++i) {
        bytes[2 * i] = static_cast<std::uint8_t>(map.samples[i] & 0xFF);
        bytes[2 * i + 1] = static_cast<std::uint8_t>(map.samples[i] >> 8);
    }
    return bytes;
}

Layout SanitizeLayout(const Layout& in) {
    const Layout defaults;
    Layout out;
    out.sizeX = std::min(std::max(FiniteOr(in.sizeX, defaults.sizeX), kMinSize), kMaxSize);
    out.sizeZ = std::min(std::max(FiniteOr(in.sizeZ, defaults.sizeZ), kMinSize), kMaxSize);
    out.heightScale = std::min(std::max(FiniteOr(in.heightScale, defaults.heightScale), 0.0f), kMaxHeightScale);
    out.heightOffset = std::min(std::max(FiniteOr(in.heightOffset, defaults.heightOffset), -kMaxHeightOffset),
                                kMaxHeightOffset);
    return out;
}

float SampleToHeight(const Layout& layout, std::uint16_t sample) {
    return layout.heightOffset + (static_cast<float>(sample) / static_cast<float>(kMaxSample)) * layout.heightScale;
}

std::uint16_t HeightToSample(const Layout& layout, float height) {
    if (!(layout.heightScale > 0.0f) || !std::isfinite(height))
        return 0;
    const float t = (height - layout.heightOffset) / layout.heightScale * static_cast<float>(kMaxSample);
    const float clamped = std::min(std::max(t, 0.0f), static_cast<float>(kMaxSample));
    return static_cast<std::uint16_t>(std::lround(clamped));
}

float CellSizeX(const Layout& layout, const Heightmap& map) {
    return map.IsValid() ? layout.sizeX / static_cast<float>(map.resolution - 1) : 0.0f;
}

float CellSizeZ(const Layout& layout, const Heightmap& map) {
    return map.IsValid() ? layout.sizeZ / static_cast<float>(map.resolution - 1) : 0.0f;
}

Float3 SamplePosition(const Layout& layout, const Heightmap& map, int x, int z) {
    if (!map.IsValid())
        return {};
    x = ClampInt(x, 0, map.resolution - 1);
    z = ClampInt(z, 0, map.resolution - 1);
    return {static_cast<float>(x) * CellSizeX(layout, map),
            SampleToHeight(layout, map.At(x, z)),
            static_cast<float>(z) * CellSizeZ(layout, map)};
}

bool SampleHeight(const Layout& layout, const Heightmap& map, float x, float z, float& outHeight) {
    CellHit c;
    if (!LocateCell(layout, map, x, z, c))
        return false;
    const float a = SampleToHeight(layout, map.At(c.cx, c.cz));
    const float b = SampleToHeight(layout, map.At(c.cx + 1, c.cz));
    const float cc = SampleToHeight(layout, map.At(c.cx, c.cz + 1));
    const float d = SampleToHeight(layout, map.At(c.cx + 1, c.cz + 1));
    if (c.fz >= c.fx) // triangle (a, c, d)
        outHeight = a + c.fz * (cc - a) + c.fx * (d - cc);
    else              // triangle (a, d, b)
        outHeight = a + c.fx * (b - a) + c.fz * (d - b);
    return true;
}

bool SampleSurfaceNormal(const Layout& layout, const Heightmap& map, float x, float z, Float3& outNormal) {
    CellHit c;
    if (!LocateCell(layout, map, x, z, c))
        return false;
    const float a = SampleToHeight(layout, map.At(c.cx, c.cz));
    const float b = SampleToHeight(layout, map.At(c.cx + 1, c.cz));
    const float cc = SampleToHeight(layout, map.At(c.cx, c.cz + 1));
    const float d = SampleToHeight(layout, map.At(c.cx + 1, c.cz + 1));
    const float cellX = CellSizeX(layout, map);
    const float cellZ = CellSizeZ(layout, map);
    float dhdx, dhdz;
    if (c.fz >= c.fx) {
        dhdx = (d - cc) / cellX;
        dhdz = (cc - a) / cellZ;
    } else {
        dhdx = (b - a) / cellX;
        dhdz = (d - b) / cellZ;
    }
    outNormal = Normalize(-dhdx, 1.0f, -dhdz);
    return true;
}

Float3 SampleVertexNormal(const Layout& layout, const Heightmap& map, int x, int z) {
    if (!map.IsValid())
        return {0.0f, 1.0f, 0.0f};
    const int last = map.resolution - 1;
    x = ClampInt(x, 0, last);
    z = ClampInt(z, 0, last);
    const int xl = std::max(x - 1, 0), xr = std::min(x + 1, last);
    const int zl = std::max(z - 1, 0), zr = std::min(z + 1, last);
    const float dhdx = (SampleToHeight(layout, map.At(xr, z)) - SampleToHeight(layout, map.At(xl, z))) /
                       (static_cast<float>(xr - xl) * CellSizeX(layout, map));
    const float dhdz = (SampleToHeight(layout, map.At(x, zr)) - SampleToHeight(layout, map.At(x, zl))) /
                       (static_cast<float>(zr - zl) * CellSizeZ(layout, map));
    return Normalize(-dhdx, 1.0f, -dhdz);
}

bool IsValidChunkQuads(int chunkQuads, int resolution) {
    return IsValidResolution(resolution) && IsPowerOfTwo(chunkQuads) &&
           chunkQuads >= kMinChunkQuads && chunkQuads <= kMaxChunkQuads &&
           (resolution - 1) % chunkQuads == 0;
}

int ResolveChunkQuads(int requested, int resolution) {
    if (!IsValidResolution(resolution))
        return 0;
    int q = ClampInt(requested, kMinChunkQuads, kMaxChunkQuads);
    while (!IsPowerOfTwo(q))
        q &= q - 1; // round down to a power of two
    while (q > kMinChunkQuads && (resolution - 1) % q != 0)
        q /= 2;
    return IsValidChunkQuads(q, resolution) ? q : 0;
}

bool MakeChunkGrid(int resolution, int chunkQuads, ChunkGrid& out) {
    if (!IsValidChunkQuads(chunkQuads, resolution))
        return false;
    out.chunkQuads = chunkQuads;
    out.chunksPerSide = (resolution - 1) / chunkQuads;
    return true;
}

void ComputeChunkBounds(const Heightmap& map, const ChunkGrid& grid, std::vector<ChunkBounds>& out) {
    out.clear();
    if (!GridMatchesMap(map, grid))
        return;
    out.resize(static_cast<std::size_t>(grid.ChunkCount()));
    for (int cz = 0; cz < grid.chunksPerSide; ++cz)
        for (int cx = 0; cx < grid.chunksPerSide; ++cx)
            out[static_cast<std::size_t>(cz) * grid.chunksPerSide + cx] = BoundsOfChunk(map, grid, cx, cz);
}

void UpdateChunkBounds(const Heightmap& map, const ChunkGrid& grid,
                       int x0, int z0, int x1, int z1, std::vector<ChunkBounds>& inOut) {
    if (!GridMatchesMap(map, grid) || inOut.size() != static_cast<std::size_t>(grid.ChunkCount())) {
        ComputeChunkBounds(map, grid, inOut);
        return;
    }
    if (x0 > x1) std::swap(x0, x1);
    if (z0 > z1) std::swap(z0, z1);
    const int last = map.resolution - 1;
    if (x1 < 0 || z1 < 0 || x0 > last || z0 > last)
        return;
    x0 = ClampInt(x0, 0, last); x1 = ClampInt(x1, 0, last);
    z0 = ClampInt(z0, 0, last); z1 = ClampInt(z1, 0, last);
    // A border sample belongs to the chunks on both sides: widen by one sample.
    const int n = grid.chunksPerSide;
    const int cx0 = ClampInt((x0 - 1) / grid.chunkQuads, 0, n - 1);
    const int cx1 = ClampInt(x1 / grid.chunkQuads, 0, n - 1);
    const int cz0 = ClampInt((z0 - 1) / grid.chunkQuads, 0, n - 1);
    const int cz1 = ClampInt(z1 / grid.chunkQuads, 0, n - 1);
    for (int cz = cz0; cz <= cz1; ++cz)
        for (int cx = cx0; cx <= cx1; ++cx)
            inOut[static_cast<std::size_t>(cz) * n + cx] = BoundsOfChunk(map, grid, cx, cz);
}

int MaxLodLevel(int chunkQuads) {
    int level = 0;
    while (chunkQuads >> (level + 1) >= 2)
        ++level;
    return level;
}

float DistanceToAabb(const Float3& p, const Float3& mn, const Float3& mx) {
    const float dx = std::max(std::max(mn.x - p.x, 0.0f), p.x - mx.x);
    const float dy = std::max(std::max(mn.y - p.y, 0.0f), p.y - mx.y);
    const float dz = std::max(std::max(mn.z - p.z, 0.0f), p.z - mx.z);
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    return std::isfinite(d) ? d : 0.0f;
}

int SelectChunkLod(float distance, float chunkWorldSize, float lodBias, int maxLevel, int prevLevel) {
    if (maxLevel <= 0 || !(chunkWorldSize > 0.0f) || !std::isfinite(chunkWorldSize))
        return 0;
    if (!std::isfinite(distance) || distance < 0.0f)
        distance = 0.0f;
    if (!std::isfinite(lodBias))
        lodBias = 1.0f;
    lodBias = std::min(std::max(lodBias, kMinLodBias), kMaxLodBias);

    const float base = chunkWorldSize * kLodDistanceFactor;
    auto levelFor = [&](float d) {
        int level = 0;
        float limit = base;
        while (level < maxLevel && d >= limit) {
            ++level;
            limit *= 2.0f;
        }
        return level;
    };

    const float d = distance * lodBias;
    int level = levelFor(d);
    if (prevLevel >= 0 && prevLevel <= maxLevel && level < prevLevel) {
        // Going finer: only once the chunk is clearly inside the finer range.
        level = std::min(prevLevel, levelFor(d * (1.0f + kLodHysteresis)));
    }
    return level;
}

bool BuildChunkIndices(int chunkQuads, int stride, std::vector<std::uint32_t>& out) {
    if (!IsPowerOfTwo(chunkQuads) || !IsPowerOfTwo(stride) || chunkQuads / stride < 2 ||
        chunkQuads % stride != 0)
        return false;
    const std::uint32_t row = static_cast<std::uint32_t>(chunkQuads + 1);
    const int cells = chunkQuads / stride;
    out.clear();
    out.reserve(static_cast<std::size_t>(cells) * cells * 6);
    for (int z = 0; z < chunkQuads; z += stride) {
        for (int x = 0; x < chunkQuads; x += stride) {
            const std::uint32_t a = static_cast<std::uint32_t>(z) * row + static_cast<std::uint32_t>(x);
            const std::uint32_t b = a + static_cast<std::uint32_t>(stride);
            const std::uint32_t c = a + static_cast<std::uint32_t>(stride) * row;
            const std::uint32_t d = c + static_cast<std::uint32_t>(stride);
            out.insert(out.end(), {a, c, d, a, d, b});
        }
    }
    return true;
}

} // namespace terrain
