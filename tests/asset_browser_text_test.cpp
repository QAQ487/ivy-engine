// asset_browser_text_test.cpp
// Linux-runnable unit tests for Asset Browser name wrap / ellipsis helpers.
// No Windows / ImGui headers.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/asset_browser_text_test tests/asset_browser_text_test.cpp

#include "engine/editor/asset_browser_text.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void ExpectEq(const std::string& got, const char* want, const char* msg) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s (got '%s', want '%s')\n",
            msg, got.c_str(), want);
        ++g_failed;
    }
}

// Monospace-ish measure: 10px per Unicode codepoint (not per byte).
static float MeasureFixed(std::string_view text, void* /*user*/) {
    float w = 0.0f;
    const char* p = text.data();
    const char* end = p + text.size();
    while (p < end) {
        const std::size_t n = asset_browser_text::Utf8CodepointByteLength(p, end - p);
        if (n == 0)
            break;
        w += 10.0f;
        p += n;
    }
    return w;
}

static void TestUtf8Boundaries() {
    Expect(asset_browser_text::Utf8CodepointByteLength("", 0) == 0, "empty len");
    Expect(asset_browser_text::Utf8CodepointByteLength("a", 1) == 1, "ascii");
    // U+00E9 Latin small e acute → C3 A9
    Expect(asset_browser_text::Utf8CodepointByteLength("\xC3\xA9", 2) == 2, "2-byte");
    // U+4E2D CJK → E4 B8 AD
    Expect(asset_browser_text::Utf8CodepointByteLength("\xE4\xB8\xAD", 3) == 3, "3-byte");
    // U+1F600 😀 → F0 9F 98 80
    Expect(asset_browser_text::Utf8CodepointByteLength("\xF0\x9F\x98\x80", 4) == 4, "4-byte");
    // Truncated 3-byte sequence: do not claim a full codepoint past available bytes.
    Expect(asset_browser_text::Utf8CodepointByteLength("\xE4\xB8", 2) == 0,
        "truncated multibyte returns 0");
}

static void TestEllipsizeBasic() {
    ExpectEq(asset_browser_text::Ellipsize("", 100.0f, MeasureFixed, nullptr),
        "", "empty stays empty");
    ExpectEq(asset_browser_text::Ellipsize("abc", 100.0f, MeasureFixed, nullptr),
        "abc", "fits unchanged");
    ExpectEq(asset_browser_text::Ellipsize("abcdefghij", 50.0f, MeasureFixed, nullptr),
        "ab...", "truncate with ellipsis (5 slots: 2 + ...)");
    ExpectEq(asset_browser_text::Ellipsize("abcdefghij", 30.0f, MeasureFixed, nullptr),
        "...", "only ellipsis fits");
    ExpectEq(asset_browser_text::Ellipsize("abcdefghij", 20.0f, MeasureFixed, nullptr),
        "", "too narrow for ellipsis");
}

static void TestEllipsizeUtf8Safe() {
    // "caféX" — é is 2 bytes; width = 5 codepoints * 10 = 50.
    const char* cafe = "caf\xC3\xA9X";
    ExpectEq(asset_browser_text::Ellipsize(cafe, 50.0f, MeasureFixed, nullptr),
        cafe, "utf8 fits");
    // Max width 40 → 4 slots (ellipsis uses 3). Prefix budget is 1 codepoint → "c...".
    ExpectEq(asset_browser_text::Ellipsize(cafe, 40.0f, MeasureFixed, nullptr),
        "c...", "utf8 ellipsis keeps whole codepoints");

    // "caféXYZ" = 7 codepoints (70px). Width 60 → prefix budget 30 → "caf...".
    // Taking "café" (4) + "..." would be 70px and must not happen by splitting wrongly;
    // the fitted prefix stops before é because "caf" is the longest ASCII-safe fit in budget.
    const char* cafeLong = "caf\xC3\xA9XYZ";
    ExpectEq(asset_browser_text::Ellipsize(cafeLong, 60.0f, MeasureFixed, nullptr),
        "caf...", "utf8 does not split é to fit one more letter");
    const std::string cafeClip =
        asset_browser_text::Ellipsize(cafeLong, 60.0f, MeasureFixed, nullptr);
    Expect(cafeClip.find("\xC3...") == std::string::npos, "no orphan C3 before ellipsis");

    // Width 50 with long CJK: "中文名很长" = 5 chars.
    const char* cjk = "\xE4\xB8\xAD\xE6\x96\x87\xE5\x90\x8D\xE5\xBE\x88\xE9\x95\xBF";
    Expect(asset_browser_text::Ellipsize(cjk, 50.0f, MeasureFixed, nullptr) == cjk,
        "cjk fits at 50");
    const std::string clipped = asset_browser_text::Ellipsize(cjk, 40.0f, MeasureFixed, nullptr);
    Expect(clipped == "\xE4\xB8\xAD...", "cjk ellipsis keeps whole first glyph");
    Expect(clipped.find("\xE6...") == std::string::npos, "no split mid-glyph");
}

static void TestWrapTwoLines() {
    // "hello world" at width 60 → line1 "hello" (or "hello "), line2 "world".
    const std::string wrapped = asset_browser_text::WrapEllipsize(
        "hello world", 60.0f, 2, MeasureFixed, nullptr);
    ExpectEq(wrapped, "hello\nworld", "wrap on space into 2 lines");

    // Fits on one line.
    ExpectEq(asset_browser_text::WrapEllipsize("hi", 100.0f, 2, MeasureFixed, nullptr),
        "hi", "short single line");

    // Empty.
    ExpectEq(asset_browser_text::WrapEllipsize("", 100.0f, 2, MeasureFixed, nullptr),
        "", "empty wrap");
}

static void TestWrapSingleLongWord() {
    // "scene_cylinder" = 14 chars. Width 80 → 8 codepoints/line.
    // Line1: scene_cy (8), line2: linder (6) fits → no ellipsis.
    ExpectEq(
        asset_browser_text::WrapEllipsize("scene_cylinder", 80.0f, 2, MeasureFixed, nullptr),
        "scene_cy\nlinder",
        "break long word across 2 lines");

    // Width 50 → 5 per line. Line1 "scene", line2 needs "..." for rest "_cylinder" (9).
    // Remaining "_cylinder" at 50 → ellipsize to "_c..." (2 + ...).
    ExpectEq(
        asset_browser_text::WrapEllipsize("scene_cylinder", 50.0f, 2, MeasureFixed, nullptr),
        "scene\n_c...",
        "long word second line ellipsized");
}

static void TestWrapVeryLongAndMaxLines() {
    const std::string longName(40, 'a');
    const std::string out = asset_browser_text::WrapEllipsize(
        longName, 40.0f, 2, MeasureFixed, nullptr);
    // Line1: 4 a's, line2: ellipsized remaining → "a..." (1+3) under width 40.
    ExpectEq(out, "aaaa\na...", "very long name 2-line ellipsis");

    // maxLines 1 → ellipsize only.
    ExpectEq(
        asset_browser_text::WrapEllipsize("abcdefghij", 50.0f, 1, MeasureFixed, nullptr),
        "ab...",
        "maxLines 1 is ellipsize");

    ExpectEq(
        asset_browser_text::WrapEllipsize("abc", 100.0f, 0, MeasureFixed, nullptr),
        "",
        "maxLines 0 yields empty");
}

int main() {
    TestUtf8Boundaries();
    TestEllipsizeBasic();
    TestEllipsizeUtf8Safe();
    TestWrapTwoLines();
    TestWrapSingleLongWord();
    TestWrapVeryLongAndMaxLines();

    if (g_failed != 0) {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    std::printf("asset_browser_text_test: OK\n");
    return 0;
}
