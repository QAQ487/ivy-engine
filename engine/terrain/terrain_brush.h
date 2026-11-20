// terrain_brush.h
// Terrain sculpting brushes (ADR 0011, step 4). Pure logic in terrain-local space (no
// D3D / ImGui), so every brush is unit-testable on Linux; the editor tool
// (engine/editor/terrain_editor.cpp) turns mouse strokes into ApplyBrush calls and
// stores the touched rectangle for undo.

#pragma once

#include "engine/terrain/terrain_heightmap.h"

#include <cstdint>
#include <vector>

namespace terrain {

enum class BrushMode {
    Raise,   // add height (strength = metres per second at the centre)
    Lower,   // remove height
    Smooth,  // blend towards the 3x3 neighbour average (strength = blends per second)
    Flatten, // blend towards targetHeight (strength = blends per second)
};

inline constexpr float kMinBrushRadius = 0.1f;
inline constexpr float kMaxBrushRadius = 1000.0f;
inline constexpr float kMaxBrushStrength = 1000.0f;

struct Brush {
    BrushMode mode = BrushMode::Raise;
    float radius = 10.0f;      // metres
    float strength = 5.0f;     // see BrushMode
    float falloff = 0.5f;      // 0 = hard edge, 1 = fades from the centre
    float targetHeight = 0.0f; // terrain-local metres (Flatten)
};

// Clamps radius / strength / falloff (NaN -> defaults).
Brush SanitizeBrush(const Brush& brush);

// Inclusive sample rectangle. Empty when x1 < x0 or z1 < z0.
struct SampleRect {
    int x0 = 0;
    int z0 = 0;
    int x1 = -1;
    int z1 = -1;
    bool Empty() const { return x1 < x0 || z1 < z0; }
    int Width() const { return Empty() ? 0 : x1 - x0 + 1; }
    int Height() const { return Empty() ? 0 : z1 - z0 + 1; }
};

SampleRect UnionRect(const SampleRect& a, const SampleRect& b);
// The part of `rect` inside the map (empty when it misses the map).
SampleRect ClipRect(const SampleRect& rect, const Heightmap& map);

// 0..1 weight of a sample `distance` metres from the brush centre: 1 inside
// radius * (1 - falloff), cosine fade to 0 at the radius.
float BrushWeight(float distance, float radius, float falloff);

// Samples a brush at terrain-local (x, z) can touch (clipped to the map).
SampleRect BrushRect(const Layout& layout, const Heightmap& map, float x, float z, float radius);

// One brush step of `dt` seconds centred at terrain-local (x, z). Returns the samples
// that changed value (empty when none did). Heights are clamped to the map's range.
SampleRect ApplyBrush(const Layout& layout, Heightmap& map, const Brush& brush, float x, float z, float dt);

// Row-major copy of the samples in `rect` (clipped); empty for an empty rect.
std::vector<std::uint16_t> CopyRect(const Heightmap& map, const SampleRect& rect);
// Writes `data` (as returned by CopyRect for the same rect) back. False and no change
// when the rect does not lie inside the map or the size does not match.
bool PasteRect(Heightmap& map, const SampleRect& rect, const std::vector<std::uint16_t>& data);

} // namespace terrain
