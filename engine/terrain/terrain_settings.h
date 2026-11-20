// terrain_settings.h
// JSON fields of the "Terrain" scene component (ADR 0011). Pure (nlohmann only), so the
// defaults, clamping and round trip are unit-testable on Linux; the component in
// engine/ecs/terrain_component.h owns one of these plus the loaded heightmap.
//
//   "Terrain": {
//     "heightmap": "<.r16 path under rom/>",   // "" = flat placeholder
//     "sizeX": 256, "sizeZ": 256,              // metres along X / Z
//     "heightScale": 100, "heightOffset": 0,   // metres
//     "chunkQuads": 64,                        // cells per chunk side (power of two)
//     "lodBias": 1,                            // > 1 = coarser sooner
//     "texture": "<image path under rom/>",    // tiled albedo ("" = color only)
//     "textureTileSize": 8,                    // metres per texture repeat
//     "color": { "r": 1, "g": 1, "b": 1 },
//     "roughness": 0.9, "metallic": 0,
//     "castShadows": true, "enabled": true,
//     "collision": true,                       // static heightfield collider (step 3)
//     "layers": [ { "texture": "<image>", "tileSize": 6 }, ... ],  // up to 4 (step 5)
//     "splatmap": "<.splat path under rom/>"   // layer weights; "" = all layer 0
//   }

#pragma once

#include "engine/terrain/terrain_heightmap.h"
#include "libraries/nlohmann/json.hpp"

#include <string>
#include <vector>

namespace terrain {

// Resolution of the flat map drawn while "heightmap" is empty.
inline constexpr int kPlaceholderResolution = 129;
inline constexpr float kMinTextureTileSize = 0.01f;
inline constexpr float kMaxTextureTileSize = 10000.0f;

// One paintable material layer (step 5).
struct LayerSettings {
    std::string texture;
    float tileSize = 8.0f; // metres per texture repeat
};

struct Settings {
    std::string heightmap;
    Layout layout;
    int chunkQuads = kDefaultChunkQuads;
    float lodBias = 1.0f;
    std::string texture;
    float textureTileSize = 8.0f;
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
    float roughness = 0.9f;
    float metallic = 0.0f;
    bool castShadows = true;
    bool enabled = true;
    // Static heightfield collider on the owner (unless it has its own collider component).
    bool collision = true;
    // Paint layers (at most 4). Empty: one layer from `texture` / `textureTileSize`
    // (scenes from before step 5).
    std::vector<LayerSettings> layers;
    std::string splatmap;
};

// Layers the renderer uses: `layers`, or the single legacy texture when it is empty.
std::vector<LayerSettings> EffectiveLayers(const Settings& s);

// Clamps every number (NaN -> default); the layout goes through SanitizeLayout.
// chunkQuads is only range-clamped here; ResolveChunkQuads fits it to the map.
Settings SanitizeSettings(const Settings& in);

nlohmann::json SettingsToJson(const Settings& s);
// Missing keys and keys of the wrong type keep their defaults; the result is sanitised.
Settings SettingsFromJson(const nlohmann::json& j);

} // namespace terrain
