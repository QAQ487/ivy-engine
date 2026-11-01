// Skybox Asset Browser path helpers (eligible extension + rom-relative resolution).
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/skybox_asset_path_test tests/skybox_asset_path_test.cpp

#include "engine/rendering/environment/skybox_asset_path.h"

#include <cstdio>
#include <string>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void TestEligibleExtension() {
    Expect(skybox_asset::IsEligibleCubemapPath("rom/skybox/panorama_test.png"), "png eligible");
    Expect(skybox_asset::IsEligibleCubemapPath("rom/skybox/MySkybox.dds"), "dds eligible");
    Expect(skybox_asset::IsEligibleCubemapPath("rom/skybox/Foo.PNG"), "PNG case insensitive");
    Expect(skybox_asset::IsEligibleCubemapPath("rom\\skybox\\Bar.DDS"), "DDS backslash path");
    Expect(!skybox_asset::IsEligibleCubemapPath("rom/texture/a.jpg"), "jpg not eligible");
    Expect(!skybox_asset::IsEligibleCubemapPath("rom/texture/a.tga"), "tga not eligible");
    Expect(!skybox_asset::IsEligibleCubemapPath("rom/model/Box.fbx"), "fbx not eligible");
    Expect(!skybox_asset::IsEligibleCubemapPath(""), "empty not eligible");
    Expect(!skybox_asset::IsEligibleCubemapPath(nullptr), "null not eligible");
    Expect(!skybox_asset::IsEligibleCubemapPath("rom/skybox/noext"), "missing extension");
}

static void TestRomPathFromAssetPath() {
    Expect(skybox_asset::RomPathFromAssetPath("rom/skybox/panorama_test.png") ==
        "rom/skybox/panorama_test.png", "png rom path");
    Expect(skybox_asset::RomPathFromAssetPath("rom/skybox/MySkybox.dds") ==
        "rom/skybox/MySkybox.dds", "dds rom path");
    Expect(skybox_asset::RomPathFromAssetPath("rom\\skybox\\panorama_test.png") ==
        "rom/skybox/panorama_test.png", "backslash normalized");

    Expect(skybox_asset::RomPathFromAssetPath("rom/texture/a.jpg").empty(),
        "wrong extension rejected");
    Expect(skybox_asset::RomPathFromAssetPath("skybox/panorama_test.png").empty(),
        "missing rom/ prefix rejected");
    Expect(skybox_asset::RomPathFromAssetPath("rom/../secret.png").empty(),
        "traversal rejected");
    Expect(skybox_asset::RomPathFromAssetPath("rom/skybox/../secret.png").empty(),
        "mid traversal rejected");
    Expect(skybox_asset::RomPathFromAssetPath("/abs/rom/skybox/a.png").empty(),
        "absolute unix rejected");
    Expect(skybox_asset::RomPathFromAssetPath("C:/Windows/a.png").empty(),
        "drive path rejected");
    Expect(skybox_asset::RomPathFromAssetPath("").empty(), "empty rejected");
    Expect(skybox_asset::RomPathFromAssetPath(nullptr).empty(), "null rejected");
    Expect(skybox_asset::RomPathFromAssetPath("rom").empty(), "rom alone rejected");
    Expect(skybox_asset::RomPathFromAssetPath("game/scripts/a.png").empty(),
        "out-of-rom project path rejected");
}

int main() {
    TestEligibleExtension();
    TestRomPathFromAssetPath();
    if (g_failed) {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    std::printf("SKYBOX_ASSET_PATH_TEST PASS\n");
    return 0;
}
