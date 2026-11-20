// terrain_settings.cpp
// See terrain_settings.h (ADR 0011).

#include "engine/terrain/terrain_settings.h"

#include <algorithm>
#include <cmath>

namespace terrain {

namespace {

float Clamp(float v, float lo, float hi, float fallback) {
    if (!std::isfinite(v))
        return fallback;
    return std::min(std::max(v, lo), hi);
}

float ReadFloat(const nlohmann::json& j, const char* key, float fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number())
        return fallback;
    return it->get<float>();
}

int ReadInt(const nlohmann::json& j, const char* key, int fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number())
        return fallback;
    const double v = it->get<double>();
    if (!std::isfinite(v))
        return fallback;
    return static_cast<int>(std::max(std::min(v, 1e6), -1e6));
}

bool ReadBool(const nlohmann::json& j, const char* key, bool fallback) {
    auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}

std::string ReadString(const nlohmann::json& j, const char* key) {
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

} // namespace

Settings SanitizeSettings(const Settings& in) {
    const Settings d;
    Settings s = in;
    s.layout = SanitizeLayout(in.layout);
    s.chunkQuads = std::min(std::max(in.chunkQuads, kMinChunkQuads), kMaxChunkQuads);
    s.lodBias = Clamp(in.lodBias, kMinLodBias, kMaxLodBias, d.lodBias);
    s.textureTileSize = Clamp(in.textureTileSize, kMinTextureTileSize, kMaxTextureTileSize, d.textureTileSize);
    s.colorR = Clamp(in.colorR, 0.0f, 1.0f, d.colorR);
    s.colorG = Clamp(in.colorG, 0.0f, 1.0f, d.colorG);
    s.colorB = Clamp(in.colorB, 0.0f, 1.0f, d.colorB);
    s.roughness = Clamp(in.roughness, 0.0f, 1.0f, d.roughness);
    s.metallic = Clamp(in.metallic, 0.0f, 1.0f, d.metallic);
    return s;
}

nlohmann::json SettingsToJson(const Settings& s) {
    nlohmann::json j;
    j["heightmap"] = s.heightmap;
    j["sizeX"] = s.layout.sizeX;
    j["sizeZ"] = s.layout.sizeZ;
    j["heightScale"] = s.layout.heightScale;
    j["heightOffset"] = s.layout.heightOffset;
    j["chunkQuads"] = s.chunkQuads;
    j["lodBias"] = s.lodBias;
    j["texture"] = s.texture;
    j["textureTileSize"] = s.textureTileSize;
    j["color"] = {{"r", s.colorR}, {"g", s.colorG}, {"b", s.colorB}};
    j["roughness"] = s.roughness;
    j["metallic"] = s.metallic;
    j["castShadows"] = s.castShadows;
    j["enabled"] = s.enabled;
    j["collision"] = s.collision;
    return j;
}

Settings SettingsFromJson(const nlohmann::json& j) {
    Settings s;
    if (!j.is_object())
        return s;
    s.heightmap = ReadString(j, "heightmap");
    s.layout.sizeX = ReadFloat(j, "sizeX", s.layout.sizeX);
    s.layout.sizeZ = ReadFloat(j, "sizeZ", s.layout.sizeZ);
    s.layout.heightScale = ReadFloat(j, "heightScale", s.layout.heightScale);
    s.layout.heightOffset = ReadFloat(j, "heightOffset", s.layout.heightOffset);
    s.chunkQuads = ReadInt(j, "chunkQuads", s.chunkQuads);
    s.lodBias = ReadFloat(j, "lodBias", s.lodBias);
    s.texture = ReadString(j, "texture");
    s.textureTileSize = ReadFloat(j, "textureTileSize", s.textureTileSize);
    auto color = j.find("color");
    if (color != j.end() && color->is_object()) {
        s.colorR = ReadFloat(*color, "r", s.colorR);
        s.colorG = ReadFloat(*color, "g", s.colorG);
        s.colorB = ReadFloat(*color, "b", s.colorB);
    }
    s.roughness = ReadFloat(j, "roughness", s.roughness);
    s.metallic = ReadFloat(j, "metallic", s.metallic);
    s.castShadows = ReadBool(j, "castShadows", s.castShadows);
    s.enabled = ReadBool(j, "enabled", s.enabled);
    s.collision = ReadBool(j, "collision", s.collision);
    return SanitizeSettings(s);
}

} // namespace terrain
