// terrain_settings_test.cpp
// Linux-runnable unit tests for engine/terrain/terrain_settings.{h,cpp} (ADR 0011).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/terrain_settings_test tests/terrain_settings_test.cpp engine/terrain/terrain_settings.cpp engine/terrain/terrain_heightmap.cpp
//   /tmp/terrain_settings_test

#include "engine/terrain/terrain_settings.h"

#include <cmath>
#include <cstdio>

using namespace terrain;
using nlohmann::json;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void TestDefaults() {
    const Settings d = SettingsFromJson(json::object());
    Expect(d.heightmap.empty() && d.texture.empty(), "empty paths by default");
    Expect(d.layout.sizeX == 256.0f && d.layout.sizeZ == 256.0f, "default size");
    Expect(d.layout.heightScale == 100.0f && d.layout.heightOffset == 0.0f, "default height range");
    Expect(d.chunkQuads == kDefaultChunkQuads, "default chunk size");
    Expect(d.lodBias == 1.0f && d.textureTileSize == 8.0f, "default bias / tiling");
    Expect(d.roughness == 0.9f && d.metallic == 0.0f, "default material");
    Expect(d.castShadows && d.enabled && d.collision, "default flags");

    const Settings notObject = SettingsFromJson(json::array());
    Expect(notObject.layout.sizeX == 256.0f, "non-object JSON gives defaults");
}

static void TestRoundTrip() {
    Settings s;
    s.heightmap = "rom/terrain/island.r16";
    s.layout.sizeX = 512.0f;
    s.layout.sizeZ = 300.0f;
    s.layout.heightScale = 80.0f;
    s.layout.heightOffset = -20.0f;
    s.chunkQuads = 32;
    s.lodBias = 2.0f;
    s.texture = "rom/texture/grass.png";
    s.textureTileSize = 4.0f;
    s.colorR = 0.5f;
    s.colorG = 0.6f;
    s.colorB = 0.7f;
    s.roughness = 0.4f;
    s.metallic = 0.1f;
    s.castShadows = false;
    s.enabled = false;
    s.collision = false;

    const json j = SettingsToJson(s);
    Expect(j["color"]["g"].get<float>() == 0.6f, "color written as an object");
    const Settings b = SettingsFromJson(json::parse(j.dump()));
    Expect(b.heightmap == s.heightmap && b.texture == s.texture, "paths survive");
    Expect(b.layout.sizeX == 512.0f && b.layout.sizeZ == 300.0f, "size survives");
    Expect(b.layout.heightScale == 80.0f && b.layout.heightOffset == -20.0f, "height range survives");
    Expect(b.chunkQuads == 32 && b.lodBias == 2.0f && b.textureTileSize == 4.0f, "numbers survive");
    Expect(b.colorR == 0.5f && b.colorG == 0.6f && b.colorB == 0.7f, "color survives");
    Expect(b.roughness == 0.4f && b.metallic == 0.1f, "material survives");
    Expect(!b.castShadows && !b.enabled && !b.collision, "flags survive");
}

static void TestSanitising() {
    const json j = {
        {"heightmap", 42},          // wrong type -> ""
        {"sizeX", -10},             // below minimum
        {"heightScale", "tall"},    // wrong type -> default
        {"chunkQuads", 100000},     // clamped
        {"lodBias", 0.0},           // clamped up
        {"textureTileSize", 0},     // clamped up
        {"color", {{"r", 5}, {"g", -1}}},
        {"roughness", 3},
        {"metallic", -2},
        {"castShadows", "yes"},     // wrong type -> default true
        {"enabled", 0},             // wrong type -> default true
    };
    const Settings s = SettingsFromJson(j);
    Expect(s.heightmap.empty(), "non-string heightmap ignored");
    Expect(s.layout.sizeX == kMinSize, "size clamped");
    Expect(s.layout.heightScale == 100.0f, "non-number height scale ignored");
    Expect(s.chunkQuads == kMaxChunkQuads, "chunk size clamped");
    Expect(s.lodBias == kMinLodBias, "bias clamped");
    Expect(s.textureTileSize == kMinTextureTileSize, "tile size clamped");
    Expect(s.colorR == 1.0f && s.colorG == 0.0f && s.colorB == 1.0f, "color clamped, missing channel kept");
    Expect(s.roughness == 1.0f && s.metallic == 0.0f, "material clamped");
    Expect(s.castShadows && s.enabled, "non-bool flags keep defaults");

    Settings bad;
    bad.lodBias = std::nanf("");
    bad.textureTileSize = std::nanf("");
    bad.chunkQuads = -5;
    const Settings c = SanitizeSettings(bad);
    Expect(c.lodBias == 1.0f && c.textureTileSize == 8.0f, "NaN -> defaults");
    Expect(c.chunkQuads == kMinChunkQuads, "negative chunk size clamped");
}

static void TestLayers() {
    Settings legacy;
    legacy.texture = "rom/texture/grass.png";
    legacy.textureTileSize = 3.0f;
    const auto eff = EffectiveLayers(legacy);
    Expect(eff.size() == 1 && eff[0].texture == legacy.texture && eff[0].tileSize == 3.0f,
           "no layers -> legacy texture is layer 0");
    Expect(!SettingsToJson(legacy).contains("layers") && !SettingsToJson(legacy).contains("splatmap"),
           "legacy scenes do not gain new keys");

    const json j = {{"layers", json::array({{{"texture", "a.png"}, {"tileSize", 4}},
                                            {{"texture", "b.png"}},
                                            42,
                                            {{"texture", "c.png"}, {"tileSize", -1}},
                                            {{"texture", "d.png"}},
                                            {{"texture", "e.png"}}})},
                    {"splatmap", "rom/terrain/x.splat"}};
    const Settings s = SettingsFromJson(j);
    Expect(s.layers.size() == 4, "at most 4 layers, non-objects skipped");
    Expect(s.layers[0].tileSize == 4.0f && s.layers[1].tileSize == 8.0f, "tile size and default");
    Expect(s.layers[2].tileSize == kMinTextureTileSize, "tile size clamped");
    Expect(s.layers[3].texture == "d.png", "order kept");
    Expect(s.splatmap == "rom/terrain/x.splat", "splat path");
    const Settings b = SettingsFromJson(json::parse(SettingsToJson(s).dump()));
    Expect(b.layers.size() == 4 && b.layers[1].texture == "b.png" && b.splatmap == s.splatmap, "round trip");
    Expect(EffectiveLayers(b).size() == 4, "explicit layers used");
}

int main() {
    TestLayers();
    TestDefaults();
    TestRoundTrip();
    TestSanitising();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("terrain_settings_test: all checks passed\n");
    return 0;
}
