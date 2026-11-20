// terrain_splat.h
// Terrain material painting (ADR 0011, step 5): a splat map holds, per sample, the
// weights of up to 4 layers (R, G, B, A = layers 0..3, summing to 255). Pure logic, so
// it is unit-testable on Linux; the renderer blends the layer textures by these weights
// and the editor's Paint tool calls PaintSplat.
//
// .splat file: resolution^2 RGBA8 texels, row-major (index = z * resolution + x), no
// header; resolution = 2^n + 1 (33 .. 4097), inferred from the size. Texel (x, z) sits
// at terrain-local (x / (res - 1) * sizeX, z / (res - 1) * sizeZ), like heightmap
// samples, but its resolution does not have to match the heightmap's.

#pragma once

#include "engine/terrain/terrain_brush.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace terrain {

inline constexpr int kMaxLayers = 4;

struct SplatMap {
    int resolution = 0;
    std::vector<std::uint8_t> rgba; // 4 bytes per texel

    bool IsValid() const;
    std::uint8_t Weight(int x, int z, int layer) const; // clamped coordinates; 0 when invalid
};

// All weight on `layer` (0..3).
bool CreateSplat(int resolution, int layer, SplatMap& out);

bool DecodeSplat(const std::uint8_t* data, std::size_t size, SplatMap& out, std::string* error);
std::vector<std::uint8_t> EncodeSplat(const SplatMap& map);

// Paint step of `dt` seconds with `brush` (radius, strength = fraction of full weight per
// second at the centre, falloff; brush.mode is ignored) on `layer` at terrain-local
// (x, z). Moves weight from the other layers to `layer` so each texel still sums to 255.
// Returns the texels that changed.
SampleRect PaintSplat(const Layout& layout, SplatMap& map, int layer, const Brush& brush,
                      float x, float z, float dt);

// Texel rectangle a brush can touch, and copy / paste of 4-byte texels for undo.
SampleRect SplatBrushRect(const Layout& layout, const SplatMap& map, float x, float z, float radius);
SampleRect ClipSplatRect(const SampleRect& rect, const SplatMap& map);
std::vector<std::uint8_t> CopySplatRect(const SplatMap& map, const SampleRect& rect);
bool PasteSplatRect(SplatMap& map, const SampleRect& rect, const std::vector<std::uint8_t>& data);

} // namespace terrain
