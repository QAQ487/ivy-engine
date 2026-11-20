// terrain_brush.cpp
// See terrain_brush.h (ADR 0011).

#include "engine/terrain/terrain_brush.h"

#include <algorithm>
#include <cmath>

namespace terrain {

namespace {

float ClampFinite(float v, float lo, float hi, float fallback) {
    if (!std::isfinite(v))
        return fallback;
    return std::min(std::max(v, lo), hi);
}

std::uint16_t ToSample(float value) {
    const float c = std::min(std::max(value, 0.0f), static_cast<float>(kMaxSample));
    return static_cast<std::uint16_t>(std::lround(c));
}

} // namespace

Brush SanitizeBrush(const Brush& in) {
    const Brush d;
    Brush b = in;
    b.radius = ClampFinite(in.radius, kMinBrushRadius, kMaxBrushRadius, d.radius);
    b.strength = ClampFinite(in.strength, 0.0f, kMaxBrushStrength, d.strength);
    b.falloff = ClampFinite(in.falloff, 0.0f, 1.0f, d.falloff);
    b.targetHeight = ClampFinite(in.targetHeight, -1e6f, 1e6f, 0.0f);
    return b;
}

SampleRect UnionRect(const SampleRect& a, const SampleRect& b) {
    if (a.Empty()) return b;
    if (b.Empty()) return a;
    return {std::min(a.x0, b.x0), std::min(a.z0, b.z0), std::max(a.x1, b.x1), std::max(a.z1, b.z1)};
}

SampleRect ClipRect(const SampleRect& r, const Heightmap& map) {
    if (r.Empty() || !map.IsValid())
        return {};
    const int last = map.resolution - 1;
    SampleRect c{std::max(r.x0, 0), std::max(r.z0, 0), std::min(r.x1, last), std::min(r.z1, last)};
    return c.Empty() ? SampleRect{} : c;
}

float BrushWeight(float distance, float radius, float falloff) {
    if (!(radius > 0.0f) || !std::isfinite(distance) || distance >= radius)
        return 0.0f;
    falloff = std::min(std::max(std::isfinite(falloff) ? falloff : 0.5f, 0.0f), 1.0f);
    const float t = std::max(distance, 0.0f) / radius;
    const float inner = 1.0f - falloff;
    if (t <= inner || falloff <= 0.0f)
        return 1.0f;
    const float u = (t - inner) / falloff; // 0..1 across the fade
    return 0.5f * (1.0f + std::cos(3.14159265f * u));
}

SampleRect BrushRect(const Layout& layout, const Heightmap& map, float x, float z, float radius) {
    if (!map.IsValid() || !std::isfinite(x) || !std::isfinite(z) || !(radius > 0.0f))
        return {};
    const float cellX = CellSizeX(layout, map);
    const float cellZ = CellSizeZ(layout, map);
    if (!(cellX > 0.0f) || !(cellZ > 0.0f))
        return {};
    auto lo = [](float v) { return static_cast<int>(std::max(std::floor(v), -1e8f)); };
    auto hi = [](float v) { return static_cast<int>(std::min(std::ceil(v), 1e8f)); };
    const SampleRect r{lo((x - radius) / cellX), lo((z - radius) / cellZ),
                       hi((x + radius) / cellX), hi((z + radius) / cellZ)};
    return ClipRect(r, map);
}

SampleRect ApplyBrush(const Layout& layout, Heightmap& map, const Brush& in, float x, float z, float dt) {
    const Brush brush = SanitizeBrush(in);
    if (!(dt > 0.0f) || !std::isfinite(dt) || brush.strength <= 0.0f)
        return {};
    const SampleRect rect = BrushRect(layout, map, x, z, brush.radius);
    if (rect.Empty())
        return {};

    const float cellX = CellSizeX(layout, map);
    const float cellZ = CellSizeZ(layout, map);
    const float samplesPerMetre = layout.heightScale > 0.0f
        ? static_cast<float>(kMaxSample) / layout.heightScale : 0.0f;

    // Smooth reads a snapshot (rect plus one sample border) so the result does not
    // depend on the order samples are visited.
    std::vector<std::uint16_t> snapshot;
    SampleRect snapRect;
    if (brush.mode == BrushMode::Smooth) {
        snapRect = ClipRect({rect.x0 - 1, rect.z0 - 1, rect.x1 + 1, rect.z1 + 1}, map);
        snapshot = CopyRect(map, snapRect);
    }
    auto snap = [&](int sx, int sz) {
        sx = std::min(std::max(sx, snapRect.x0), snapRect.x1);
        sz = std::min(std::max(sz, snapRect.z0), snapRect.z1);
        return static_cast<float>(snapshot[static_cast<std::size_t>(sz - snapRect.z0) * snapRect.Width() +
                                           (sx - snapRect.x0)]);
    };
    const float targetSample = static_cast<float>(HeightToSample(layout, brush.targetHeight));

    SampleRect changed;
    for (int sz = rect.z0; sz <= rect.z1; ++sz) {
        for (int sx = rect.x0; sx <= rect.x1; ++sx) {
            const float dx = static_cast<float>(sx) * cellX - x;
            const float dz = static_cast<float>(sz) * cellZ - z;
            const float w = BrushWeight(std::sqrt(dx * dx + dz * dz), brush.radius, brush.falloff);
            if (w <= 0.0f)
                continue;
            const float cur = static_cast<float>(map.At(sx, sz));
            float next = cur;
            switch (brush.mode) {
            case BrushMode::Raise:
                next = cur + brush.strength * dt * w * samplesPerMetre;
                break;
            case BrushMode::Lower:
                next = cur - brush.strength * dt * w * samplesPerMetre;
                break;
            case BrushMode::Smooth: {
                float sum = 0.0f;
                for (int oz = -1; oz <= 1; ++oz)
                    for (int ox = -1; ox <= 1; ++ox)
                        sum += snap(sx + ox, sz + oz);
                const float k = std::min(brush.strength * dt * w, 1.0f);
                next = cur + (sum / 9.0f - cur) * k;
                break;
            }
            case BrushMode::Flatten: {
                const float k = std::min(brush.strength * dt * w, 1.0f);
                next = cur + (targetSample - cur) * k;
                break;
            }
            }
            const std::uint16_t value = ToSample(next);
            if (value != map.At(sx, sz)) {
                map.Set(sx, sz, value);
                changed = UnionRect(changed, SampleRect{sx, sz, sx, sz});
            }
        }
    }
    return changed;
}

std::vector<std::uint16_t> CopyRect(const Heightmap& map, const SampleRect& rect) {
    const SampleRect r = ClipRect(rect, map);
    std::vector<std::uint16_t> out;
    if (r.Empty())
        return out;
    out.reserve(static_cast<std::size_t>(r.Width()) * r.Height());
    for (int z = r.z0; z <= r.z1; ++z) {
        const std::uint16_t* row = &map.samples[static_cast<std::size_t>(z) * map.resolution];
        out.insert(out.end(), row + r.x0, row + r.x1 + 1);
    }
    return out;
}

bool PasteRect(Heightmap& map, const SampleRect& rect, const std::vector<std::uint16_t>& data) {
    if (rect.Empty() || !map.IsValid())
        return false;
    const SampleRect clipped = ClipRect(rect, map);
    if (clipped.x0 != rect.x0 || clipped.z0 != rect.z0 || clipped.x1 != rect.x1 || clipped.z1 != rect.z1)
        return false;
    if (data.size() != static_cast<std::size_t>(rect.Width()) * rect.Height())
        return false;
    for (int z = rect.z0; z <= rect.z1; ++z)
        std::copy(data.begin() + static_cast<std::ptrdiff_t>(z - rect.z0) * rect.Width(),
                  data.begin() + static_cast<std::ptrdiff_t>(z - rect.z0 + 1) * rect.Width(),
                  map.samples.begin() + static_cast<std::ptrdiff_t>(z) * map.resolution + rect.x0);
    return true;
}

} // namespace terrain
