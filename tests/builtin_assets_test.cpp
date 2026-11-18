// builtin_assets_test.cpp
// Linux-runnable unit tests for engine/core/builtin_assets.h.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/builtin_assets_test tests/builtin_assets_test.cpp
//   /tmp/builtin_assets_test

#include "engine/core/builtin_assets.h"

#include <cstdio>

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

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::puts("builtin_assets_test: ok");
    return 0;
}
