// content_removal_policy_test.cpp
// Linux-runnable unit tests for editor content-removal light policy.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/content_removal_policy_test tests/content_removal_policy_test.cpp
//   /tmp/content_removal_policy_test

#include "engine/editor/content_removal_policy.h"

#include <cstdio>
#include <string>
#include <vector>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void TestNormalize() {
    using content_reload::NormalizeRomLookupKey;
    Expect(NormalizeRomLookupKey("rom/texture/A.PNG") == "rom/texture/a.png",
           "lowercase");
    Expect(NormalizeRomLookupKey("rom\\texture\\a.png") == "rom/texture/a.png",
           "backslash to slash");
    Expect(NormalizeRomLookupKey("ROM/Texture/A.PNG") == "rom/texture/a.png",
           "ROM prefix case");
    Expect(NormalizeRomLookupKey("texture/a.png") == "rom/texture/a.png",
           "missing rom/ prefix prepended");
}

static void TestSceneReference() {
    using content_reload::SceneJsonReferencesRomPath;
    const char* scene =
        R"({"nodes":[{"components":{"visual":{"model":"rom/model/Box.fbx"},)"
        R"("sprite":{"texture":"rom/texture/Hero.PNG"}}}]})";

    Expect(SceneJsonReferencesRomPath(scene, "rom/model/Box.fbx"),
           "exact model path");
    Expect(SceneJsonReferencesRomPath(scene, "rom\\model\\box.fbx"),
           "slash + case normalize");
    Expect(SceneJsonReferencesRomPath(scene, "ROM/texture/hero.png"),
           "texture case");
    Expect(SceneJsonReferencesRomPath(scene, "texture/hero.png"),
           "rom/ prefix optional on probe");
    Expect(!SceneJsonReferencesRomPath(scene, "rom/texture/unused.png"),
           "unreferenced");
    Expect(!SceneJsonReferencesRomPath("", "rom/model/Box.fbx"),
           "empty scene");
    Expect(!SceneJsonReferencesRomPath(scene, ""),
           "empty path");
}

static void TestDecideUnreferenced() {
    using content_reload::DecideRemovedContentAction;
    using content_reload::RemovedContentAction;
    const char* scene = R"({"nodes":[{"model":"rom/model/Used.fbx"}]})";

    Expect(DecideRemovedContentAction("rom/texture/orphan.png", scene, false) ==
               RemovedContentAction::KeepWatching,
           "unreferenced removed -> no failure");
    Expect(DecideRemovedContentAction("rom/model/Used.fbx", scene, false) ==
               RemovedContentAction::ReportFailureRetain,
           "referenced by scene -> failure");
    Expect(DecideRemovedContentAction("rom/audio/cached.wav", scene, true) ==
               RemovedContentAction::ReportFailureRetain,
           "loaded in cache but not in scene -> failure");
    Expect(DecideRemovedContentAction("rom/model/Used.fbx", scene, true) ==
               RemovedContentAction::ReportFailureRetain,
           "referenced and loaded -> failure");
}

static void TestPathNormalizationInDecide() {
    using content_reload::DecideRemovedContentAction;
    using content_reload::RemovedContentAction;
    const char* scene =
        R"({"settings":{"render":{"skyboxCubemap":"rom/skybox/MySkybox.dds"}}})";

    Expect(DecideRemovedContentAction("ROM\\skybox\\myskybox.dds", scene, false) ==
               RemovedContentAction::ReportFailureRetain,
           "case + slash still counts as referenced");
    Expect(DecideRemovedContentAction("skybox/myskybox.dds", scene, false) ==
               RemovedContentAction::ReportFailureRetain,
           "rom/ prefix normalization still matches scene");
}

static void TestMultipleRemovalsMixed() {
    using content_reload::DecideRemovedContentAction;
    using content_reload::RemovedContentAction;

    const char* scene =
        R"({"nodes":[{"audio":"rom/audio/click.wav"},{"prefab":"rom/prefabs/Enemy.json"}]})";

    struct Case {
        const char* path;
        bool loaded;
        RemovedContentAction want;
        const char* label;
    };
    const Case cases[] = {
        {"rom/texture/temp_scratch.png", false, RemovedContentAction::KeepWatching,
         "mixed: orphan texture"},
        {"rom/audio/click.wav", false, RemovedContentAction::ReportFailureRetain,
         "mixed: scene audio"},
        {"rom/model/ghost.fbx", true, RemovedContentAction::ReportFailureRetain,
         "mixed: loaded model not in scene"},
        {"rom/prefabs/Enemy.json", false, RemovedContentAction::ReportFailureRetain,
         "mixed: scene prefab"},
        {"rom/prefabs/Unused.json", false, RemovedContentAction::KeepWatching,
         "mixed: unused prefab"},
    };

    int failureCount = 0;
    int keepCount = 0;
    for (const Case& c : cases) {
        const auto got = DecideRemovedContentAction(c.path, scene, c.loaded);
        Expect(got == c.want, c.label);
        if (got == RemovedContentAction::ReportFailureRetain)
            ++failureCount;
        else
            ++keepCount;
    }
    Expect(failureCount == 3 && keepCount == 2, "mixed batch tallies");
}

int main() {
    TestNormalize();
    TestSceneReference();
    TestDecideUnreferenced();
    TestPathNormalizationInDecide();
    TestMultipleRemovalsMixed();
    if (g_failed) {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    std::printf("CONTENT_REMOVAL_POLICY_TEST PASS\n");
    return 0;
}
