// texture_cook_policy_test.cpp
// Linux-runnable unit tests for engine/core/texture_cook_policy.h.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/texture_cook_policy_test tests/texture_cook_policy_test.cpp
//   /tmp/texture_cook_policy_test

#include "engine/core/texture_cook_policy.h"

#include <cstdio>
#include <string>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

int main() {
    using namespace texture_cook;

    // DdsSiblingPath: narrow and wide, case-insensitive, rejects non-images.
    Expect(DdsSiblingPath(std::string("rom/tex/a.png")) == "rom/tex/a.dds", "png -> dds");
    Expect(DdsSiblingPath(std::string("rom/tex/a.JPG")) == "rom/tex/a.dds", "JPG -> dds");
    Expect(DdsSiblingPath(std::string("a.jpeg")) == "a.dds", "jpeg -> dds");
    Expect(DdsSiblingPath(std::string("a.bmp")) == "a.dds", "bmp -> dds");
    Expect(DdsSiblingPath(std::wstring(L"rom\\tex\\a.png")) == L"rom\\tex\\a.dds", "wide backslash path");
    Expect(DdsSiblingPath(std::string("rom/tex/a.dds")).empty(), "dds has no sibling");
    Expect(DdsSiblingPath(std::string("rom/tex/a.tga")).empty(), "tga is not cooked");
    Expect(DdsSiblingPath(std::string("rom/tex/noext")).empty(), "no extension");
    Expect(DdsSiblingPath(std::string("rom/a.b/noext")).empty(), "dot only in directory");
    Expect(DdsSiblingPath(std::string("rom/tex/a.b.png")) == "rom/tex/a.b.dds", "keeps inner dots");
    Expect(DdsSiblingPath(std::string()).empty(), "empty path");

    // ShouldCookRomTexture: game textures yes, engine-owned paths no.
    Expect(ShouldCookRomTexture("texture/hero.png"), "game texture is cooked");
    Expect(ShouldCookRomTexture("model/hero/diffuse.JPG"), "model texture is cooked");
    Expect(ShouldCookRomTexture("hero.png"), "file directly under rom is cooked");
    Expect(!ShouldCookRomTexture("texture/hero.dds"), "dds is not cooked again");
    Expect(!ShouldCookRomTexture("texture/hero.tga"), "tga is not cooked");
    Expect(!ShouldCookRomTexture("branding/ivy_logo.png"), "branding stays png");
    Expect(!ShouldCookRomTexture("font/atlas.png"), "font stays png");
    Expect(!ShouldCookRomTexture("skybox/source/sky.png"), "skybox source stays png");
    Expect(!ShouldCookRomTexture("skybox/cache/sky.png"), "skybox cache stays png");
    Expect(!ShouldCookRomTexture("shader/x.png"), "shader dir stays png");
    Expect(!ShouldCookRomTexture("editor/icon.png"), "editor state stays png");
    Expect(!ShouldCookRomTexture("texture/fade.png"), "host fade texture stays png");
    Expect(!ShouldCookRomTexture("Texture/Fade.PNG"), "host fade texture, any case");
    Expect(!ShouldCookRomTexture("texture/consolab_ascii_512.png"), "debug text atlas stays png");
    Expect(ShouldCookRomTexture("texture/fonts_hero.png"), "dir match is exact, not prefix");
    Expect(!ShouldCookRomTexture(""), "empty path");

    // BC size rule.
    Expect(IsBlockCompressibleSize(256, 256), "256x256 ok");
    Expect(IsBlockCompressibleSize(4, 8), "4x8 ok");
    Expect(!IsBlockCompressibleSize(30, 32), "30 wide not ok");
    Expect(!IsBlockCompressibleSize(32, 33), "33 high not ok");
    Expect(!IsBlockCompressibleSize(2, 2), "below one block not ok");
    Expect(!IsBlockCompressibleSize(0, 0), "zero not ok");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("texture_cook_policy_test: ok");
    return 0;
}
