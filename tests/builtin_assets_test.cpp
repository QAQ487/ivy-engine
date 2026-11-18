// builtin_assets_test.cpp
// Linux-runnable unit tests for engine/core/builtin_assets.h.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/builtin_assets_test tests/builtin_assets_test.cpp
//   /tmp/builtin_assets_test

#include "engine/core/builtin_assets.h"

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
    const builtin_assets::Asset font = builtin_assets::DebugMonoFontTtf();
    Expect(font.data != nullptr, "debug font has data");
    Expect(font.size > 100000, "debug font is the full TTF, not a stub");
    // TrueType scaler type 0x00010000, or 'true' / 'OTTO' for other flavours.
    const bool ttf = font.size >= 4 &&
        ((font.data[0] == 0 && font.data[1] == 1 && font.data[2] == 0 && font.data[3] == 0) ||
         (font.data[0] == 't' && font.data[1] == 'r' && font.data[2] == 'u' && font.data[3] == 'e') ||
         (font.data[0] == 'O' && font.data[1] == 'T' && font.data[2] == 'T' && font.data[3] == 'O'));
    Expect(ttf, "debug font starts with a font scaler tag");

    // Built-in textures answer to their rom/ path.
    const unsigned char kPngSig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const char* const kPaths[] = {"rom/branding/ivy_logo.png", "rom/skybox/ibl/brdf_lut.png"};
    for (const char* path : kPaths) {
        builtin_assets::Asset a;
        Expect(builtin_assets::FindTexture(std::string(path), &a), path);
        bool sig = a.size > 8;
        for (int i = 0; sig && i < 8; ++i)
            sig = a.data[i] == kPngSig[i];
        Expect(sig, "built-in texture is a PNG");
    }
    builtin_assets::Asset w;
    Expect(builtin_assets::FindTexture(std::wstring(L"ROM\\Skybox\\IBL\\BRDF_LUT.PNG"), &w), "wide, backslash, any case");
    // Default UI font answers to its rom/ path, not the retired ones.
    builtin_assets::Asset f;
    Expect(builtin_assets::FindFont(std::string("rom/font/IBMPlexMono-Regular.ttf"), &f), "default UI font");
    Expect(f.data == font.data && f.size == font.size, "UI font is the debug font data");
    Expect(builtin_assets::FindFont(std::wstring(L"ROM\\Font\\ibmplexmono-regular.TTF"), nullptr), "font, wide, any case");
    Expect(!builtin_assets::FindFont(std::string("rom/font/LemonSlice.ttf"), nullptr), "LemonSlice is gone");
    Expect(!builtin_assets::FindTexture(std::string("rom/font/IBMPlexMono-Regular.ttf"), nullptr), "a font is not a texture");
    Expect(!builtin_assets::FindTexture(std::string("rom/skybox/ibl/irradiance.dds"), nullptr),
           "per-skybox IBL maps are content, not built in");
    Expect(!builtin_assets::FindTexture(std::string("rom/branding/other.png"), nullptr), "unknown path");
    Expect(!builtin_assets::FindTexture(std::string("branding/ivy_logo.png"), nullptr), "needs the rom/ prefix");
    Expect(!builtin_assets::FindTexture(std::wstring(L"rom/branding/ivy_logo\u00e9.png"), nullptr), "non-ASCII never matches");
    Expect(!builtin_assets::FindTexture(std::string(), nullptr), "empty path");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("builtin_assets_test: ok");
    return 0;
}
