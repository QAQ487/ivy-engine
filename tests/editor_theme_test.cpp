// editor_theme_test.cpp
// Linux-runnable unit tests for editor theme preset/scheme parsing and
// ImGuiCol_ coverage. Builds headless with Dear ImGui sources (no Win32/DX11).
//
// Prefer: ./tools/run_editor_theme_check.sh

#include "engine/editor/editor_icon_button.h"
#include "engine/editor/editor_theme.h"

#include "imgui/imgui.h"

#include <cmath>
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
    Expect(static_cast<int>(ColorScheme::Count) == 5, "scheme count == 5");
    Expect(editor_theme::ParseColorScheme("Ivy") == ColorScheme::Ivy, "Ivy");
    Expect(editor_theme::ParseColorScheme("ivy") == ColorScheme::Ivy, "ivy ci");
    Expect(editor_theme::ParseColorScheme("Ivy Dark") == ColorScheme::IvyDark, "Ivy Dark");
    Expect(editor_theme::ParseColorScheme("IvyDark") == ColorScheme::IvyDark, "IvyDark");
    Expect(editor_theme::ParseColorScheme("ivy_dark") == ColorScheme::IvyDark, "ivy_dark");
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
    Expect(editor_theme::ClampColorScheme(
               static_cast<int>(editor_theme::ColorScheme::IvyDark)) ==
               editor_theme::ColorScheme::IvyDark,
           "clamp Ivy Dark");
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
    ExpectEqStr(editor_theme::ColorSchemeName(editor_theme::ColorScheme::IvyDark),
                "Ivy Dark", "scheme name Ivy Dark");
}

static void TestSchemePersistenceRoundTrip() {
    for (int i = 0; i < static_cast<int>(editor_theme::ColorScheme::Count); ++i) {
        const auto scheme = static_cast<editor_theme::ColorScheme>(i);
        const char* name = editor_theme::ColorSchemeName(scheme);
        char msg[96];
        std::snprintf(msg, sizeof(msg), "round-trip scheme %d (%s)", i, name);
        Expect(editor_theme::ParseColorScheme(name) == scheme, msg);
    }
    // Invalid persisted strings still fall back to Ivy (default).
    Expect(editor_theme::ParseColorScheme("not-a-scheme") == editor_theme::ColorScheme::Ivy,
           "invalid persist → Ivy");
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

static float SrgbToLinear(float u) {
    return (u <= 0.04045f) ? (u / 12.92f)
                           : std::pow((u + 0.055f) / 1.055f, 2.4f);
}

static float RelativeLuminance(const ImVec4& c) {
    return 0.2126f * SrgbToLinear(c.x) +
           0.7152f * SrgbToLinear(c.y) +
           0.0722f * SrgbToLinear(c.z);
}

static float ContrastRatio(const ImVec4& a, const ImVec4& b) {
    const float l1 = RelativeLuminance(a);
    const float l2 = RelativeLuminance(b);
    const float hi = (l1 > l2) ? l1 : l2;
    const float lo = (l1 > l2) ? l2 : l1;
    return (hi + 0.05f) / (lo + 0.05f);
}

static float MeanRgb(const ImVec4& c) {
    return (c.x + c.y + c.z) / 3.0f;
}

static void TestIvyDarkDarkerThanIvy() {
    ImGuiStyle ivy;
    ImGuiStyle ivyDark;
    editor_theme::ApplyColors(
        ivy, editor_theme::StylePreset::Trellis, editor_theme::ColorScheme::Ivy);
    editor_theme::ApplyColors(
        ivyDark, editor_theme::StylePreset::Trellis, editor_theme::ColorScheme::IvyDark);

    Expect(MeanRgb(ivyDark.Colors[ImGuiCol_WindowBg]) <
               MeanRgb(ivy.Colors[ImGuiCol_WindowBg]),
           "Ivy Dark WindowBg darker than Ivy");
    Expect(MeanRgb(ivyDark.Colors[ImGuiCol_TitleBg]) <
               MeanRgb(ivy.Colors[ImGuiCol_TitleBg]),
           "Ivy Dark TitleBg darker than Ivy");
    Expect(MeanRgb(ivyDark.Colors[ImGuiCol_MenuBarBg]) <
               MeanRgb(ivy.Colors[ImGuiCol_MenuBarBg]),
           "Ivy Dark MenuBarBg darker than Ivy");
    Expect(MeanRgb(ivyDark.Colors[ImGuiCol_FrameBg]) <
               MeanRgb(ivy.Colors[ImGuiCol_FrameBg]),
           "Ivy Dark FrameBg darker than Ivy");

    // Lumen-on-canopy contrast should stay WCAG AA-ish for normal text (>= 4.5).
    const float textContrast = ContrastRatio(
        ivyDark.Colors[ImGuiCol_Text], ivyDark.Colors[ImGuiCol_WindowBg]);
    Expect(textContrast >= 4.5f, "Ivy Dark text contrast >= 4.5");

    const float mutedContrast = ContrastRatio(
        ivyDark.Colors[ImGuiCol_TextDisabled], ivyDark.Colors[ImGuiCol_WindowBg]);
    Expect(mutedContrast >= 2.5f, "Ivy Dark muted text still legible");

    // Hover/active distinguishable from base button.
    Expect(MeanRgb(ivyDark.Colors[ImGuiCol_ButtonHovered]) !=
               MeanRgb(ivyDark.Colors[ImGuiCol_Button]),
           "Ivy Dark ButtonHovered != Button");
    Expect(MeanRgb(ivyDark.Colors[ImGuiCol_ButtonActive]) !=
               MeanRgb(ivyDark.Colors[ImGuiCol_Button]),
           "Ivy Dark ButtonActive != Button");

    // Same fern accent family as Ivy.
    const auto& darkSem = editor_theme::GetSemanticColors();
    Expect(darkSem.accent.x > 0.5f && darkSem.accent.y > 0.7f,
           "Ivy Dark keeps fern accent");
}

static void TestCenteredGlyphDrawPos() {
    float x = 0.0f, y = 0.0f;

    // Symmetric glyph in a 20x20 button → draw at origin that places ink dead center.
    // Ink 4..12 in both axes (w=h=8). Center at 10 → draw = 10 - 8 = 2 after offset.
    editor_icon_button::CenteredGlyphDrawPos(
        0, 0, 20, 20, 4, 4, 12, 12, 1.0f, &x, &y);
    Expect(x == 2.0f && y == 2.0f, "symmetric center");

    // Asymmetric FA-like bearings: more empty below (Y0=-2, Y1=10 → h=12) in 24px button.
    editor_icon_button::CenteredGlyphDrawPos(
        0, 0, 24, 24, 1, -2, 15, 10, 1.0f, &x, &y);
    // ink_h=12; top pad=(24-12)/2=6; draw_y = 6 - (-2) = 8
    Expect(y == 8.0f, "asymmetric vertical center");
    // ink_w=14; left pad=(24-14)/2=5; draw_x = 5 - 1 = 4
    Expect(x == 4.0f, "asymmetric horizontal center");

    // Narrow caret button 12x24 with glyph 2..10 x 8..16.
    editor_icon_button::CenteredGlyphDrawPos(
        0, 0, 12, 24, 2, 8, 10, 16, 1.0f, &x, &y);
    // ink 8x8; pad_x=(12-8)/2=2 → x=2-2=0; pad_y=(24-8)/2=8 → y=8-8=0
    Expect(x == 0.0f && y == 0.0f, "narrow caret button center");

    // Truncation toward zero (positive coords).
    editor_icon_button::CenteredGlyphDrawPos(
        0.4f, 0.4f, 20.4f, 20.4f, 0, 0, 8, 8, 1.0f, &x, &y);
    // ink 8; pad=(20-8)/2=6; draw=0.4+6=6.4 → trunc 6
    Expect(x == 6.0f && y == 6.0f, "truncates toward zero");

    // Scale factor 2: glyph box in baked space, scaled to display.
    editor_icon_button::CenteredGlyphDrawPos(
        0, 0, 40, 40, 2, 2, 6, 6, 2.0f, &x, &y);
    // ink 8x8; pad=(40-8)/2=16; draw=16-4=12
    Expect(x == 12.0f && y == 12.0f, "scaled glyph center");
}

static void TestToolbarChromeFitsFont() {
    // Mirrors engine_gui EditorToolbarHeight compact chrome (padY=4 at 100%).
    // Old fixed Ui(36) could not fit Trellis/HollowSoft theme WindowPadding+FramePadding.
    constexpr float kPadY = 4.0f;
    constexpr float kOldToolbar = 36.0f;
    const float zooms[] = { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    const float fontBases[] = { 15.5f, 16.0f }; // Trellis / Hollow Soft

    // At 100%, theme padding alone already exceeds the old fixed 36px row.
    Expect(15.5f + 4.0f * 2.0f + 8.0f * 2.0f > kOldToolbar, "Trellis theme chrome > 36");
    Expect(16.0f + 6.0f * 2.0f + 12.0f * 2.0f > kOldToolbar, "HollowSoft theme chrome > 36");

    for (float fontBase : fontBases) {
        for (float zoom : zooms) {
            const float fontSize = fontBase * zoom;
            const float padY = kPadY * zoom;
            const float frameH = fontSize + padY * 2.0f;
            const float toolbarH = frameH + padY * 2.0f;
            // Square icon button: content box == fontSize after FramePadding.
            const float iconContent = frameH - padY * 2.0f;

            char msg[128];
            std::snprintf(msg, sizeof(msg),
                "chrome fits fontBase=%.1f zoom=%.2f (h=%.1f)", fontBase, zoom, toolbarH);
            Expect(toolbarH + 0.01f >= frameH, msg);
            Expect(iconContent + 0.01f >= fontSize, msg);
        }
    }
}

int main() {
    TestParsePreset();
    TestParseScheme();
    TestClamp();
    TestNames();
    TestSchemePersistenceRoundTrip();
    TestImGuiColCoverage();
    TestIvyDarkDarkerThanIvy();
    TestCenteredGlyphDrawPos();
    TestToolbarChromeFitsFont();

    if (g_failed != 0) {
        std::fprintf(stderr, "editor_theme_test: %d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("editor_theme_test: PASS\n");
    return 0;
}
