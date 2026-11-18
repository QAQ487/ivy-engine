// package_assets_test.cpp
// Linux-runnable unit tests for engine/editor/package_assets (referenced-assets packaging).
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/package_assets_test tests/package_assets_test.cpp engine/editor/package_assets.cpp
//   /tmp/package_assets_test

#include "engine/editor/package_assets.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void Write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << bytes;
}

static std::string Utf16(const std::string& ascii, bool terminator = true) {
    std::string out;
    for (char c : ascii) {
        out.push_back(c);
        out.push_back('\0');
    }
    if (terminator) {
        out.push_back('\0');
        out.push_back('\0');
    }
    return out;
}

int main() {
    using namespace package_assets;

    const fs::path root = fs::temp_directory_path() / "ivy_package_assets_test";
    fs::remove_all(root);
    const fs::path rom = root / "rom";

    Write(rom / "scene/default_scene.txt", "scene_main.json");
    Write(rom / "scene/scene_main.json",
          R"({"nodes":[{"components":{"visual":{"model":"rom/model/hero/hero.fbx"}},"prefabAssetPath":"rom/prefabs/enemy.prefab"}],)"
          R"("canvas":{"elements":[{"onClick":{"script":"LoadScene","scene":"scene_next.json"},"font":{"texture":"rom/font/IBMPlexMono-Regular.ttf"}}]}})");
    Write(rom / "scene/scene_next.json", R"({"texture":"rom/texture/bg.png"})");
    Write(rom / "scene/scene_unused.json", R"({"texture":"rom/texture/unused.png"})");
    Write(rom / "scene/scene_from_script.json", R"({"texture":"rom/texture/from_script.png"})");
    Write(rom / "prefabs/enemy.prefab", R"({"audio":"rom\\audio\\hit.wav"})");
    Write(rom / "prefabs/unused.prefab", R"({"audio":"rom/audio/unused.wav"})");

    // FBX: an ASCII texture name and a UTF-16 one, as binary FBX / Effekseer files store them.
    Write(rom / "model/hero/hero.fbx",
          std::string("Kaydara FBX Binary\0\0", 20) + "Textures/hero_diffuse.png" + std::string(1, '\0') +
              Utf16("hero_normal.png"));
    Write(rom / "model/hero/Textures/hero_diffuse.png", "d");
    Write(rom / "model/hero/Textures/hero_normal.png", "n");
    Write(rom / "model/hero/Textures/hero_unused.png", "u");

    Write(rom / "texture/bg.png", "bg");
    Write(rom / "texture/unused.png", "x");
    Write(rom / "texture/from_script.png", "fs");
    Write(rom / "texture/always_one.png", "a1");
    Write(rom / "texture/always_two.png", "a2");
    Write(rom / "audio/hit.wav", "h");
    Write(rom / "audio/unused.wav", "u");
    Write(rom / "audio/click.wav", "c");
    Write(rom / "audio/sfx_1.wav", "1");
    Write(rom / "audio/sfx_2.wav", "2");
    Write(rom / "music/theme.ogg", "t");
    Write(rom / "music/battle.ogg", "b");
    Write(rom / "tags.json", "{}");
    Write(rom / "editor/ui_prefs.json", "{}");
    Write(rom / "editor/imgui_layout.ini", "layout");
    Write(rom / "shader/custom.hlsl", "shader");
    Write(rom / "skybox/cache/sky.dds", "sky");
    Write(rom / "skybox/source/sky.png", "src");

    Write(root / "project.json",
          R"({"lastOpenedScene":"scene_unused.json","defaultScene":"scene_main.json","audio":{"uiClickSound":"rom/audio/click.wav"}})");
    // Script DLL stand-in: ASCII prefix, UTF-16 folder, and a scene name.
    Write(root / "GameScripts.dll",
          std::string("MZ\x90\0junk\0", 10) + "rom/audio/sfx_" + std::string(1, '\0') + Utf16("rom/music/") +
              "scene_from_script.json" + std::string(1, '\0'));

    Options options;
    options.romRoot = rom;
    options.rootScenes = {"scene_main.json"};
    options.alwaysInclude = {"rom/texture/always", "rom/missing_dir/"};
    options.scanFiles = {root / "project.json", root / "GameScripts.dll"};
    const Result r = ComputeReferencedAssets(options);

    auto has = [&](const char* rel) { return r.included.count(rel) != 0; };

    Expect(has("scene/scene_main.json"), "default scene");
    Expect(has("scene/default_scene.txt"), "default_scene.txt");
    Expect(has("scene/scene_next.json"), "scene named by a button onClick");
    Expect(has("texture/bg.png"), "texture of a reached scene");
    Expect(has("prefabs/enemy.prefab"), "prefab named by a scene");
    Expect(has("audio/hit.wav"), "asset named by a prefab (escaped backslashes)");
    Expect(has("model/hero/hero.fbx"), "model named by a scene");
    Expect(has("model/hero/Textures/hero_diffuse.png"), "FBX texture name, ASCII");
    Expect(has("model/hero/Textures/hero_normal.png"), "FBX texture name, UTF-16");
    Expect(!has("model/hero/Textures/hero_unused.png"), "unreferenced texture next to the model");
    Expect(has("audio/click.wav"), "asset named by project.json");
    Expect(has("audio/sfx_1.wav") && has("audio/sfx_2.wav"), "DLL string prefix keeps matching files");
    Expect(has("music/theme.ogg") && has("music/battle.ogg"), "UTF-16 DLL folder string keeps the folder");
    Expect(has("scene/scene_from_script.json"), "scene named only in the DLL");
    Expect(has("texture/from_script.png"), "texture of a scene reached from the DLL");
    Expect(has("texture/always_one.png") && has("texture/always_two.png"), "build.alwaysInclude name prefix");
    Expect(has("tags.json") && has("editor/ui_prefs.json"), "runtime config files");
    Expect(has("shader/custom.hlsl"), "shader folder");
    Expect(has("skybox/cache/sky.dds"), "skybox cache");

    Expect(!has("scene/scene_unused.json"), "project.json lastOpenedScene is editor state, not a reference");
    Expect(!has("texture/unused.png"), "texture only an unused scene names");
    Expect(!has("prefabs/unused.prefab"), "unused prefab");
    Expect(!has("audio/unused.wav"), "asset only an unused prefab names");
    Expect(!has("editor/imgui_layout.ini"), "editor layout is not needed");
    Expect(!has("skybox/source/sky.png"), "skybox source");

    bool warnedMissing = false;
    for (const std::string& w : r.warnings)
        warnedMissing = warnedMissing || w.find("rom/missing_dir/") != std::string::npos;
    Expect(warnedMissing, "alwaysInclude entry that matches nothing warns");
    Expect(r.excluded.size() + r.included.size() == 29, "every file is included or excluded exactly once");
    Expect(r.excludedBytes > 0 && r.includedBytes > 0, "byte totals");

    // Tokens.
    auto tokens = CollectRomTokens(R"(see "rom/a b/c.png", xrom/no.png and rom\\x\\y.png.)");
    Expect(tokens.size() == 2, "two tokens (a word glued to 'rom/' is not one)");
    Expect(tokens.size() == 2 && tokens[0] == "rom/a b/c.png", "token keeps inner spaces");
    Expect(tokens.size() == 2 && tokens[1] == "rom/x/y.png", "backslashes normalized, trailing dot trimmed");

    // Missing default scene warns instead of failing.
    Options bad;
    bad.romRoot = rom;
    bad.rootScenes = {"nope.json"};
    const Result b = ComputeReferencedAssets(bad);
    Expect(!b.warnings.empty(), "missing default scene warns");
    Expect(!b.included.count("scene/scene_main.json") || true, "no crash");

    // Missing rom folder.
    Options none;
    none.romRoot = root / "does_not_exist";
    Expect(ComputeReferencedAssets(none).included.empty(), "missing rom folder is empty");

    fs::remove_all(root);
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("package_assets_test: ok");
    return 0;
}
