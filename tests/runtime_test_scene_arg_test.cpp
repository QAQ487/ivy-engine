// runtime_test_scene_arg_test.cpp
// Linux-runnable pure validation for --scene path rules.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/runtime_test_scene_arg_test tests/runtime_test_scene_arg_test.cpp
//   /tmp/runtime_test_scene_arg_test
#include "engine/testing/runtime_test_scene_arg.h"
#include <cstdio>
#include <string>

static int g_fails = 0;
static void expect(bool cond, const char* label) {
    if (!cond) {
        std::fprintf(stderr, "FAIL %s\n", label);
        ++g_fails;
    }
}

int main() {
    using ivy::testing::ValidateRuntimeTestSceneArg;
    std::string normalized, error;

    expect(ValidateRuntimeTestSceneArg("rom/scene/player.json", &normalized, &error), "valid rom json");
    expect(normalized == "rom/scene/player.json", "normalized unchanged");
    expect(error.empty(), "no error on success");

    expect(ValidateRuntimeTestSceneArg("rom\\scene\\player.JSON", &normalized, &error), "backslash + case");
    expect(normalized == "rom/scene/player.JSON", "backslash normalized");

    expect(!ValidateRuntimeTestSceneArg("../rom/scene/x.json", &normalized, &error), "reject parent segment");
    expect(!ValidateRuntimeTestSceneArg("rom/../scene/x.json", &normalized, &error), "reject embedded ..");
    expect(!ValidateRuntimeTestSceneArg("/rom/scene/x.json", &normalized, &error), "reject absolute unix");
    expect(!ValidateRuntimeTestSceneArg("C:/rom/scene/x.json", &normalized, &error), "reject absolute windows");
    expect(!ValidateRuntimeTestSceneArg("scene/player.json", &normalized, &error), "reject non-rom");
    expect(!ValidateRuntimeTestSceneArg("rom/scene/player.txt", &normalized, &error), "reject non-json");
    expect(!ValidateRuntimeTestSceneArg("rom/scene/", &normalized, &error), "reject trailing slash");
    expect(!ValidateRuntimeTestSceneArg("", &normalized, &error), "reject empty");
    expect(!ValidateRuntimeTestSceneArg("rom/scene/./player.json", &normalized, &error), "reject dot segment");

    if (g_fails) {
        std::fprintf(stderr, "%d scene-arg checks failed\n", g_fails);
        return 1;
    }
    std::printf("PASS runtime_test_scene_arg validation\n");
    return 0;
}
