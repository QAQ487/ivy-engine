// editor_theme_test.cpp
// Linux-runnable unit tests for editor theme preset/scheme parsing and
// ImGuiCol_ coverage. Builds headless with Dear ImGui sources (no Win32/DX11).
//
// Prefer: ./tools/run_editor_theme_check.sh

#include "engine/editor/editor_theme.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void ExpectEqStr(const char* got, const char* want, const char* msg) {
    if (std::strcmp(got, want) != 0) {
        std::fprintf(stderr, "FAIL: %s (got '%s', want '%s')\n",
            msg, got ? got : "(null)", want ? want : "(null)");
        ++g_failed;
    }
}

static void TestParsePreset() {
    using editor_theme::StylePreset;
    Expect(editor_theme::ParseStylePreset("Trellis") == StylePreset::Trellis,
           "Trellis");
    Expect(editor_theme::ParseStylePreset("trellis") == StylePreset::Trellis,
           "trellis ci");
    Expect(editor_theme::ParseStylePreset("Hollow Soft") == StylePreset::HollowSoft,
           "Hollow Soft");
    Expect(editor_theme::ParseStylePreset("HollowSoft") == StylePreset::HollowSoft,
           "HollowSoft");
    Expect(editor_theme::ParseStylePreset("hollow_soft") == StylePreset::HollowSoft,
           "hollow_soft");
    Expect(editor_theme::ParseStylePreset("CanopyPro") == StylePreset::Trellis,
           "unknown → Trellis");
    Expect(editor_theme::ParseStylePreset("") == StylePreset::Trellis,
           "empty → Trellis");
    Expect(editor_theme::ParseStylePreset(nullptr) == StylePreset::Trellis,
           "null → Trellis");
}

static void TestParseScheme() {
    using editor_theme::ColorScheme;
    Expect(editor_theme::ParseColorScheme("Ivy") == ColorScheme::Ivy, "Ivy");
    Expect(editor_theme::ParseColorScheme("ivy") == ColorScheme::Ivy, "ivy ci");
    Expect(editor_theme::ParseColorScheme("Classic Dark") == ColorScheme::ClassicDark,
           "Classic Dark");
    Expect(editor_theme::ParseColorScheme("classic_dark") == ColorScheme::ClassicDark,
           "classic_dark");
    Expect(editor_theme::ParseColorScheme("Neutral Gray") == ColorScheme::NeutralGray,
           "Neutral Gray");
    Expect(editor_theme::ParseColorScheme("Neutral") == ColorScheme::NeutralGray,
           "Neutral alias");
    Expect(editor_theme::ParseColorScheme("Light") == ColorScheme::Light, "Light");
    Expect(editor_theme::ParseColorScheme("Neon") == ColorScheme::Ivy,
           "unknown → Ivy");
    Expect(editor_theme::ParseColorScheme("") == ColorScheme::Ivy, "empty → Ivy");
    Expect(editor_theme::ParseColorScheme(nullptr) == ColorScheme::Ivy, "null → Ivy");
}

static void TestClamp() {
    Expect(editor_theme::ClampStylePreset(-1) == editor_theme::StylePreset::Trellis,
           "clamp preset low");
    Expect(editor_theme::ClampStylePreset(99) == editor_theme::StylePreset::Trellis,
           "clamp preset high");
    Expect(editor_theme::ClampStylePreset(1) == editor_theme::StylePreset::HollowSoft,
           "clamp preset mid");
    Expect(editor_theme::ClampColorScheme(-1) == editor_theme::ColorScheme::Ivy,
           "clamp scheme low");
    Expect(editor_theme::ClampColorScheme(99) == editor_theme::ColorScheme::Ivy,
           "clamp scheme high");
    Expect(editor_theme::ClampColorScheme(2) == editor_theme::ColorScheme::NeutralGray,
           "clamp scheme mid");
}

static void TestNames() {
    ExpectEqStr(editor_theme::StylePresetName(editor_theme::StylePreset::Trellis),
                "Trellis", "preset name Trellis");
    ExpectEqStr(editor_theme::StylePresetName(editor_theme::StylePreset::HollowSoft),
                "Hollow Soft", "preset name Hollow Soft");
    ExpectEqStr(editor_theme::ColorSchemeName(editor_theme::ColorScheme::Ivy),
                "Ivy", "scheme name Ivy");
    ExpectEqStr(editor_theme::ColorSchemeName(editor_theme::ColorScheme::ClassicDark),
                "Classic Dark", "scheme name Classic Dark");
    ExpectEqStr(editor_theme::ColorSchemeName(editor_theme::ColorScheme::NeutralGray),
                "Neutral Gray", "scheme name Neutral Gray");
    ExpectEqStr(editor_theme::ColorSchemeName(editor_theme::ColorScheme::Light),
                "Light", "scheme name Light");
}

static void TestImGuiColCoverage() {
    // Unique sentinel unlikely to match any painted color.
    const ImVec4 sentinel(0.111f, 0.222f, 0.333f, 0.444f);

    for (int pi = 0; pi < static_cast<int>(editor_theme::StylePreset::Count); ++pi) {
        for (int si = 0; si < static_cast<int>(editor_theme::ColorScheme::Count); ++si) {
            ImGuiStyle style;
            for (int i = 0; i < ImGuiCol_COUNT; ++i)
                style.Colors[i] = sentinel;

            editor_theme::ApplyColors(
                style,
                static_cast<editor_theme::StylePreset>(pi),
                static_cast<editor_theme::ColorScheme>(si));

            char msg[128];
            std::snprintf(msg, sizeof(msg),
                "ImGuiCol coverage preset=%d scheme=%d", pi, si);
            Expect(editor_theme::ColorsFullyAssigned(style, sentinel), msg);

            // Core readable surfaces must stay opaque enough to see.
            Expect(style.Colors[ImGuiCol_Text].w > 0.5f, "Text alpha");
            Expect(style.Colors[ImGuiCol_WindowBg].w > 0.5f, "WindowBg alpha");
            Expect(style.Colors[ImGuiCol_Button].w > 0.0f, "Button alpha");
        }
    }

    ImGuiStyle ivyStyle;
    editor_theme::ApplyColors(
        ivyStyle, editor_theme::StylePreset::Trellis, editor_theme::ColorScheme::Ivy);
    const auto& sem = editor_theme::GetSemanticColors();
    Expect(sem.accent.w > 0.5f, "semantic accent");
    Expect(sem.danger.w > 0.5f, "semantic danger");
    Expect(sem.success.w > 0.5f, "semantic success");
    Expect(sem.text.w > 0.5f, "semantic text");
    // Ivy canopy ≈ #0F1A16
    Expect(sem.windowBg.x < 0.1f && sem.windowBg.y < 0.15f, "Ivy canopy dark");
}

int main() {
    TestParsePreset();
    TestParseScheme();
    TestClamp();
    TestNames();
    TestImGuiColCoverage();

    if (g_failed != 0) {
        std::fprintf(stderr, "editor_theme_test: %d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("editor_theme_test: PASS\n");
    return 0;
}
