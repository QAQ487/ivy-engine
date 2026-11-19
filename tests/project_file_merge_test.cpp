// project_file_merge_test.cpp
// Linux-runnable unit tests for engine/editor/project_file_merge.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/project_file_merge_test tests/project_file_merge_test.cpp
//   /tmp/project_file_merge_test

#include "engine/editor/project_file_merge.h"

#include <cstdio>
#include <string>

using nlohmann::json;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

// Same shape as ProjectManager::WriteProjectFile's patch (build.* is not managed).
static json EditorPatch() {
    json j;
    j["name"] = "MyGame";
    j["version"] = 1;
    j["lastOpenedScene"] = "scene_main.json";
    j["defaultScene"] = "scene_main.json";
    j["editor"] = {{"uiZoomPercent", 125}, {"assetBrowserListView", false}, {"assetBrowserTileSize", 100},
                   {"uiStylePreset", "Trellis"}, {"uiColorScheme", "Ivy"}};
    j["physics"] = {{"gravity", 12.0}};
    j["audio"] = {{"masterVolume", 0.5}, {"bgmVolume", 1.0}, {"sfxVolume", 1.0}, {"voiceVolume", 1.0},
                  {"uiClickSound", ""}};
    return j;
}

int main() {
    using project_file::MergeIntoExisting;

    const std::string existing = R"({
      "name": "Old", "version": 1,
      "save": { "folderName": "MyGameSaves" },
      "input": { "actions": { "Jump": { "type": "button", "bindings": ["Key.SPACE"] } } },
      "futureKey": [1, 2, 3],
      "physics": { "gravity": 9.81 },
      "audio": { "masterVolume": 1.0, "duckingDb": -6 },
      "editor": { "uiZoomPercent": 100, "newEditorOption": true },
      "build": { "packAllAssets": true, "alwaysInclude": ["rom/a/"], "compressAudio": true }
    })";

    // Unmanaged fields survive; managed fields take the editor's values.
    {
        const json out = MergeIntoExisting(existing, EditorPatch());
        Expect(out["save"]["folderName"] == "MyGameSaves", "save.folderName kept");
        Expect(out["input"]["actions"]["Jump"]["bindings"][0] == "Key.SPACE", "input section kept");
        Expect(out["futureKey"] == json({1, 2, 3}), "unknown top-level key kept");
        Expect(out["name"] == "MyGame", "name overwritten");
        Expect(out["physics"]["gravity"] == 12.0, "gravity overwritten");
        Expect(out["audio"]["masterVolume"] == 0.5, "managed audio key overwritten");
        Expect(out["audio"]["duckingDb"] == -6, "unknown audio key kept");
        Expect(out["editor"]["uiZoomPercent"] == 125, "managed editor key overwritten");
        Expect(out["editor"]["newEditorOption"] == true, "unknown editor key kept");
        // build is not managed: left exactly as in the file.
        Expect(out["build"]["packAllAssets"] == true && out["build"]["alwaysInclude"] == json({"rom/a/"}) &&
               out["build"]["compressAudio"] == true, "build untouched");
        Expect(out.dump().find("null") == std::string::npos, "no null written");
    }

    // Null in a patch removes a key, and a section emptied that way is dropped.
    {
        json patch = EditorPatch();
        patch["build"] = {{"packAllAssets", nullptr}};
        const json out = MergeIntoExisting(R"({"build":{"packAllAssets":true}})", patch);
        Expect(!out.contains("build"), "emptied section dropped");
        const json kept = MergeIntoExisting(R"({"build":{"packAllAssets":true,"x":1}})", patch);
        Expect(kept["build"] == json({{"x", 1}}), "other keys of the section kept");
    }

    // No file / broken file / not an object: just the managed fields, no nulls, no empty build.
    for (const char* text : {"", "{ broken", "[1,2]", "42"}) {
        const json out = MergeIntoExisting(text, EditorPatch());
        Expect(out.is_object() && out["name"] == "MyGame", "fresh file from patch");
        Expect(!out.contains("build"), "fresh file has no build");
        Expect(out.dump().find("null") == std::string::npos, "fresh file has no null");
    }

    // A managed section that was not an object is replaced by the editor's object.
    {
        const json out = MergeIntoExisting(R"({"audio":"loud","save":{"folderName":"X"}})", EditorPatch());
        Expect(out["audio"].is_object() && out["audio"]["masterVolume"] == 0.5, "non-object section replaced");
        Expect(out["save"]["folderName"] == "X", "save kept next to replaced section");
    }

    // UTF-8 BOM at the start of the file.
    {
        const std::string withBom = "\xEF\xBB\xBF" R"({"save":{"folderName":"磁力場"}})";
        const json out = MergeIntoExisting(withBom, EditorPatch());
        Expect(out.contains("save") && out["save"]["folderName"] == "磁力場", "BOM file parsed, UTF-8 kept");
    }

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("project_file_merge_test: all checks passed\n");
    return 0;
}
