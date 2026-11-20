// terrain_splat.cpp
// See terrain_splat.h (ADR 0011).

#include "engine/terrain/terrain_splat.h"

#include <algorithm>
#include <cmath>

namespace terrain {

namespace {

int ResolutionFromSplatBytes(std::size_t bytes) {
    if (bytes == 0 || bytes % 4 != 0)
        return 0;
    const std::size_t count = bytes / 4;
    for (int r = kMinResolution; r <= kMaxResolution; r = (r - 1) * 2 + 1)
        if (static_cast<std::size_t>(r) * static_cast<std::size_t>(r) == count)
            return r;
    return 0;
}

// Grid geometry of a splat map over the terrain.
float CellX(const Layout& l, const SplatMap& m) { return l.sizeX / static_cast<float>(m.resolution - 1); }
float CellZ(const Layout& l, const SplatMap& m) { return l.sizeZ / static_cast<float>(m.resolution - 1); }

} // namespace

bool SplatMap::IsValid() const {
    return IsValidResolution(resolution) &&
           rgba.size() == static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution) * 4;
}

std::uint8_t SplatMap::Weight(int x, int z, int layer) const {
    if (!IsValid() || layer < 0 || layer >= kMaxLayers)
        return 0;
    x = std::clamp(x, 0, resolution - 1);
    z = std::clamp(z, 0, resolution - 1);
    return rgba[(static_cast<std::size_t>(z) * resolution + x) * 4 + layer];
}

bool CreateSplat(int resolution, int layer, SplatMap& out) {
    if (!IsValidResolution(resolution) || layer < 0 || layer >= kMaxLayers)
        return false;
    out.resolution = resolution;
    out.rgba.assign(static_cast<std::size_t>(resolution) * resolution * 4, 0);
    for (std::size_t i = static_cast<std::size_t>(layer); i < out.rgba.size(); i += 4)
        out.rgba[i] = 255;
    return true;
}

bool DecodeSplat(const std::uint8_t* data, std::size_t size, SplatMap& out, std::string* error) {
    const int r = data ? ResolutionFromSplatBytes(size) : 0;
    if (r == 0) {
        if (error)
            *error = "splat map is " + std::to_string(size) +
                     " bytes; a .splat file must be N*N RGBA8 texels with N = 2^n+1 between " +
                     std::to_string(kMinResolution) + " and " + std::to_string(kMaxResolution);
        return false;
    }
    out.resolution = r;
    out.rgba.assign(data, data + size);
    return true;
}

std::vector<std::uint8_t> EncodeSplat(const SplatMap& map) {
    return map.IsValid() ? map.rgba : std::vector<std::uint8_t>{};
}

SampleRect ClipSplatRect(const SampleRect& r, const SplatMap& map) {
    if (r.Empty() || !map.IsValid())
        return {};
    const int last = map.resolution - 1;
    const SampleRect c{std::max(r.x0, 0), std::max(r.z0, 0), std::min(r.x1, last), std::min(r.z1, last)};
    return c.Empty() ? SampleRect{} : c;
}

SampleRect SplatBrushRect(const Layout& layout, const SplatMap& map, float x, float z, float radius) {
    if (!map.IsValid() || !std::isfinite(x) || !std::isfinite(z) || !(radius > 0.0f))
        return {};
    const float cx = CellX(layout, map), cz = CellZ(layout, map);
    if (!(cx > 0.0f) || !(cz > 0.0f))
        return {};
    auto lo = [](float v) { return static_cast<int>(std::max(std::floor(v), -1e8f)); };
    auto hi = [](float v) { return static_cast<int>(std::min(std::ceil(v), 1e8f)); };
    return ClipSplatRect({lo((x - radius) / cx), lo((z - radius) / cz), hi((x + radius) / cx), hi((z + radius) / cz)},
                         map);
}

SampleRect PaintSplat(const Layout& layout, SplatMap& map, int layer, const Brush& in,
                      float x, float z, float dt) {
    const Brush brush = SanitizeBrush(in);
    if (layer < 0 || layer >= kMaxLayers || !(dt > 0.0f) || !std::isfinite(dt) || brush.strength <= 0.0f)
        return {};
    const SampleRect rect = SplatBrushRect(layout, map, x, z, brush.radius);
    if (rect.Empty())
        return {};
    const float cx = CellX(layout, map), cz = CellZ(layout, map);
    SampleRect changed;
    for (int tz = rect.z0; tz <= rect.z1; ++tz) {
        for (int tx = rect.x0; tx <= rect.x1; ++tx) {
            const float dx = static_cast<float>(tx) * cx - x;
            const float dz = static_cast<float>(tz) * cz - z;
            const float w = BrushWeight(std::sqrt(dx * dx + dz * dz), brush.radius, brush.falloff);
            if (w <= 0.0f)
                continue;
            std::uint8_t* t = &map.rgba[(static_cast<std::size_t>(tz) * map.resolution + tx) * 4];
            float f[kMaxLayers];
            float sum = 0.0f;
            for (int i = 0; i < kMaxLayers; ++i) {
                f[i] = t[i];
                sum += f[i];
            }
            if (sum <= 0.0f) { // broken texel: start from layer 0
                f[0] = 255.0f;
                sum = 255.0f;
            }
            for (int i = 0; i < kMaxLayers; ++i)
                f[i] *= 255.0f / sum; // normalise
            const float target = std::min(255.0f, f[layer] + brush.strength * dt * w * 255.0f);
            const float others = 255.0f - f[layer];
            const float keep = others > 0.0f ? (255.0f - target) / others : 0.0f;
            for (int i = 0; i < kMaxLayers; ++i)
                f[i] = (i == layer) ? target : f[i] * keep;

            // Round, then give any rounding remainder to the painted layer (sum stays 255).
            std::uint8_t out[kMaxLayers];
            int total = 0;
            for (int i = 0; i < kMaxLayers; ++i) {
                out[i] = static_cast<std::uint8_t>(std::lround(std::clamp(f[i], 0.0f, 255.0f)));
                total += out[i];
            }
            const int fixed = std::clamp(static_cast<int>(out[layer]) + (255 - total), 0, 255);
            out[layer] = static_cast<std::uint8_t>(fixed);

            bool diff = false;
            for (int i = 0; i < kMaxLayers; ++i) {
                diff = diff || out[i] != t[i];
                t[i] = out[i];
            }
            if (diff)
                changed = UnionRect(changed, SampleRect{tx, tz, tx, tz});
        }
    }
    return changed;
}

std::vector<std::uint8_t> CopySplatRect(const SplatMap& map, const SampleRect& rect) {
    const SampleRect r = ClipSplatRect(rect, map);
    std::vector<std::uint8_t> out;
    if (r.Empty())
        return out;
    out.reserve(static_cast<std::size_t>(r.Width()) * r.Height() * 4);
    for (int z = r.z0; z <= r.z1; ++z) {
        const std::uint8_t* row = &map.rgba[(static_cast<std::size_t>(z) * map.resolution + r.x0) * 4];
        out.insert(out.end(), row, row + static_cast<std::size_t>(r.Width()) * 4);
    }
    return out;
}

bool PasteSplatRect(SplatMap& map, const SampleRect& rect, const std::vector<std::uint8_t>& data) {
    const SampleRect c = ClipSplatRect(rect, map);
    if (rect.Empty() || c.x0 != rect.x0 || c.z0 != rect.z0 || c.x1 != rect.x1 || c.z1 != rect.z1)
        return false;
    const std::size_t rowBytes = static_cast<std::size_t>(rect.Width()) * 4;
    if (data.size() != rowBytes * rect.Height())
        return false;
    for (int z = rect.z0; z <= rect.z1; ++z)
        std::copy(data.begin() + static_cast<std::ptrdiff_t>((z - rect.z0) * rowBytes),
                  data.begin() + static_cast<std::ptrdiff_t>((z - rect.z0 + 1) * rowBytes),
                  map.rgba.begin() + static_cast<std::ptrdiff_t>((static_cast<std::size_t>(z) * map.resolution + rect.x0) * 4));
    return true;
}

} // namespace terrain
