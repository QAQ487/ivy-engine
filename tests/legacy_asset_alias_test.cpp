// legacy_asset_alias_test.cpp
// Linux-runnable unit tests for engine/core/legacy_asset_alias.h.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/legacy_asset_alias_test tests/legacy_asset_alias_test.cpp
//   /tmp/legacy_asset_alias_test

#include "engine/core/legacy_asset_alias.h"

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
    using namespace legacy_alias;

    Expect(IsLegacyFontPath("rom/texture/consolab_ascii_512.png"), "bitmap atlas");
    Expect(IsLegacyFontPath("ROM\\Texture\\Consolab_ASCII_512.PNG"), "backslash and case");
    Expect(IsLegacyFontPath("rom/font/LemonSlice.ttf"), "LemonSlice");
    Expect(IsLegacyFontPath("ROM/FONT/lemonslice.TTF"), "LemonSlice, any case");
    Expect(!IsLegacyFontPath("rom/texture/consolab_ascii_512.dds"), "other extension");
    Expect(!IsLegacyFontPath("rom/texture/my_font.png"), "other texture");
    Expect(!IsLegacyFontPath("builtin:font/IBMPlexMono-Regular.ttf"), "the default font is not legacy");
    Expect(!IsLegacyFontPath(""), "empty");

    Expect(FontPath("rom/texture/consolab_ascii_512.png", false) == "builtin:font/IBMPlexMono-Regular.ttf",
           "missing legacy atlas becomes the default font");
    Expect(FontPath("rom/font/LemonSlice.ttf", false) == "builtin:font/IBMPlexMono-Regular.ttf",
           "missing LemonSlice becomes the default font");
    Expect(FontPath("rom/font/LemonSlice.ttf", true) == "rom/font/LemonSlice.ttf", "a project's own LemonSlice wins");
    Expect(FontPath("rom/texture/consolab_ascii_512.png", true) == "rom/texture/consolab_ascii_512.png",
           "a project's own atlas file wins");
    Expect(FontPath("rom/font/Other.ttf", false) == "rom/font/Other.ttf", "other paths are untouched");
    Expect(FontPath("", false).empty(), "empty stays empty");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("legacy_asset_alias_test: ok");
    return 0;
}
