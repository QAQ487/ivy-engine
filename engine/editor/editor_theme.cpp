// editor_theme.cpp
// Style presets (Trellis / Hollow Soft) and color schemes for the editor ImGui UI.
// Preset geometry follows the ivy_themes reference; CanopyPro is intentionally omitted.

#include "engine/editor/editor_theme.h"

#include "engine/core/content_path.h"
#include "engine/core/utf8_path.h"
#include "imgui/icons_font_awesome.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace editor_theme {
namespace {

namespace fs = std::filesystem;

StylePreset g_preset = StylePreset::Trellis;
ColorScheme g_scheme = ColorScheme::Ivy;
SemanticColors g_semantics{};
Fonts g_fonts{};
bool g_labelFontPushed = false;

inline ImVec4 A(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); }

inline ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t);
}

inline ImVec4 Rgb(int r, int g, int b, float a = 1.0f)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

bool EqualsCi(const char* a, const char* b)
{
    if (!a || !b)
        return false;
    while (*a && *b) {
        const unsigned char ca = static_cast<unsigned char>(*a++);
        const unsigned char cb = static_cast<unsigned char>(*b++);
        if (std::tolower(ca) != std::tolower(cb))
            return false;
    }
    return *a == *b;
}

// Palette tokens shared by recipe painters.
struct Palette {
    ImVec4 canopy{};
    ImVec4 hollow{};
    ImVec4 lumen{};
    ImVec4 moss{};
    ImVec4 fern{};      // accent
    ImVec4 trellis{};   // lines
    ImVec4 line{};
    ImVec4 line2{};
    ImVec4 danger{};
};

Palette MakeIvyPalette()
{
    Palette p;
    p.canopy  = Rgb(15, 26, 22);     // #0F1A16
    p.hollow  = Rgb(26, 44, 38);     // #1A2C26
    p.lumen   = Rgb(230, 239, 232);  // #E6EFE8
    p.moss    = Rgb(158, 176, 168);  // #9EB0A8
    p.fern    = Rgb(159, 208, 176);  // #9FD0B0
    p.trellis = Rgb(126, 154, 143);  // #7E9A8F
    // color-mix(in srgb, trellis 34%, canopy)
    p.line = ImVec4(
        (0.34f * 126 + 0.66f * 15) / 255.0f,
        (0.34f * 154 + 0.66f * 26) / 255.0f,
        (0.34f * 143 + 0.66f * 22) / 255.0f,
        1.0f);
    p.line2 = ImVec4(
        (0.40f * 126 + 0.60f * 15) / 255.0f,
        (0.40f * 154 + 0.60f * 26) / 255.0f,
        (0.40f * 143 + 0.60f * 22) / 255.0f,
        1.0f);
    p.danger = Rgb(201, 123, 114); // #C97B72
    return p;
}

// Darker Ivy: same accent/text tokens, deepened surfaces/frames/borders.
Palette MakeIvyDarkPalette()
{
    Palette p;
    p.canopy  = Rgb(10, 18, 15);     // #0A120F  (window / title / menu ground)
    p.hollow  = Rgb(18, 28, 24);     // #121C18  (panels; step above canopy)
    p.lumen   = Rgb(230, 239, 232);  // #E6EFE8  (unchanged — contrast on darkest bg)
    p.moss    = Rgb(138, 156, 148);  // #8A9C94  (muted; still legible on canopy)
    p.fern    = Rgb(159, 208, 176);  // #9FD0B0
    p.trellis = Rgb(110, 136, 126);  // #6E887E  (slightly dimmer lines)
    // trellis mixed into dark canopy (same 34% / 40% recipe as Ivy)
    p.line = ImVec4(
        (0.34f * 110 + 0.66f * 10) / 255.0f,
        (0.34f * 136 + 0.66f * 18) / 255.0f,
        (0.34f * 126 + 0.66f * 15) / 255.0f,
        1.0f);
    p.line2 = ImVec4(
        (0.40f * 110 + 0.60f * 10) / 255.0f,
        (0.40f * 136 + 0.60f * 18) / 255.0f,
        (0.40f * 126 + 0.60f * 15) / 255.0f,
        1.0f);
    p.danger = Rgb(201, 123, 114); // #C97B72
    return p;
}

Palette MakeNeutralGrayPalette()
{
    Palette p;
    p.canopy  = Rgb(18, 18, 20);
    p.hollow  = Rgb(32, 32, 36);
    p.lumen   = Rgb(230, 230, 232);
    p.moss    = Rgb(150, 150, 156);
    p.fern    = Rgb(160, 170, 190); // cool accent
    p.trellis = Rgb(110, 110, 118);
    p.line    = Mix(p.trellis, p.canopy, 0.66f);
    p.line2   = Mix(p.trellis, p.canopy, 0.55f);
    p.danger  = Rgb(200, 110, 110);
    return p;
}

Palette MakeLightPalette()
{
    Palette p;
    p.canopy  = Rgb(236, 238, 240); // page
    p.hollow  = Rgb(250, 251, 252); // panels
    p.lumen   = Rgb(28, 32, 36);    // text
    p.moss    = Rgb(100, 108, 116); // muted
    p.fern    = Rgb(40, 130, 90);   // accent
    p.trellis = Rgb(160, 168, 174);
    p.line    = Mix(p.trellis, p.canopy, 0.45f);
    p.line2   = Mix(p.trellis, p.canopy, 0.30f);
    p.danger  = Rgb(180, 70, 70);
    return p;
}

void FillSemanticsFromPalette(SemanticColors& s, const Palette& p, bool light)
{
    s.text = p.lumen;
    s.textMuted = p.moss;
    s.windowBg = p.canopy;

    s.accent = p.fern;
    s.accentHovered = Mix(p.fern, light ? Rgb(0, 0, 0) : p.lumen, light ? 0.12f : 0.18f);
    s.accentActive = Mix(p.fern, light ? Rgb(0, 0, 0) : p.canopy, light ? 0.22f : 0.25f);

    // Success CTA: fern-leaning green (matches former start-screen green on Ivy).
    s.success = Mix(p.fern, Rgb(40, 120, 70), light ? 0.15f : 0.35f);
    s.success.w = 1.0f;
    s.successHovered = Mix(s.success, p.lumen, light ? 0.08f : 0.15f);
    s.successActive = Mix(s.success, p.canopy, 0.25f);

    s.warning = Rgb(210, 170, 70);
    s.danger = p.danger;
    s.dangerHovered = Mix(p.danger, p.lumen, 0.18f);
    s.dangerActive = Mix(p.danger, p.canopy, 0.30f);

    s.toggleOn = Mix(p.fern, Rgb(40, 120, 130), 0.35f);
    s.toggleOn.w = 1.0f;
}

void FillSemanticsClassicDark(SemanticColors& s)
{
    // Preserve the previous hardcoded Play/Stop/start-screen hues.
    s.text = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
    s.textMuted = ImVec4(0.55f, 0.58f, 0.64f, 1.0f);
    s.windowBg = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);

    s.accent = ImVec4(0.20f, 0.40f, 0.70f, 1.0f);
    s.accentHovered = ImVec4(0.30f, 0.55f, 0.85f, 1.0f);
    s.accentActive = ImVec4(0.15f, 0.30f, 0.55f, 1.0f);

    s.success = ImVec4(0.18f, 0.55f, 0.32f, 1.0f);
    s.successHovered = ImVec4(0.22f, 0.65f, 0.38f, 1.0f);
    s.successActive = ImVec4(0.14f, 0.45f, 0.26f, 1.0f);

    s.warning = ImVec4(0.90f, 0.70f, 0.00f, 1.0f);

    s.danger = ImVec4(0.55f, 0.20f, 0.20f, 1.0f);
    s.dangerHovered = ImVec4(0.70f, 0.30f, 0.30f, 1.0f);
    s.dangerActive = ImVec4(0.40f, 0.15f, 0.15f, 1.0f);

    s.toggleOn = ImVec4(0.20f, 0.55f, 0.55f, 1.0f);
}

void ApplyTrellisGeometry(ImGuiStyle& style)
{
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 1.0f;
    style.TabBarOverlineSize = 2.0f;

    style.WindowPadding = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.CellPadding = ImVec2(6.0f, 3.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    style.WindowTitleAlign = ImVec2(0.00f, 0.50f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(0.0f, 4.0f);
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.FontSizeBase = 15.5f;
}

void ApplyHollowSoftGeometry(ImGuiStyle& style)
{
    style.WindowRounding = 6.0f;
    style.ChildRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 0.0f;
    style.TabBarOverlineSize = 0.0f;

    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;
    style.WindowTitleAlign = ImVec2(0.00f, 0.50f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize = 0.0f;
    style.SeparatorTextPadding = ImVec2(2.0f, 6.0f);

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.FontSizeBase = 16.0f;
}

void PaintSharedFromPalette(ImGuiStyle& s, const Palette& p)
{
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = p.lumen;
    c[ImGuiCol_TextDisabled] = p.moss;
    c[ImGuiCol_TextLink] = p.fern;
    c[ImGuiCol_TextSelectedBg] = A(p.fern, 0.32f);
    c[ImGuiCol_WindowBg] = p.hollow;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = Mix(p.hollow, p.canopy, 0.25f);
    c[ImGuiCol_MenuBarBg] = p.canopy;
    c[ImGuiCol_TitleBg] = p.canopy;
    c[ImGuiCol_TitleBgActive] = p.canopy;
    c[ImGuiCol_TitleBgCollapsed] = p.canopy;
    c[ImGuiCol_ScrollbarBg] = A(p.canopy, 0.55f);
    c[ImGuiCol_ScrollbarGrab] = p.line2;
    c[ImGuiCol_ScrollbarGrabHovered] = p.trellis;
    c[ImGuiCol_ScrollbarGrabActive] = p.fern;
    c[ImGuiCol_InputTextCursor] = p.fern;
    c[ImGuiCol_NavCursor] = p.fern;
    c[ImGuiCol_NavWindowingHighlight] = A(p.fern, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = A(p.canopy, 0.65f);
    c[ImGuiCol_ModalWindowDimBg] = A(p.canopy, 0.70f);
    c[ImGuiCol_PlotLines] = p.trellis;
    c[ImGuiCol_PlotLinesHovered] = p.fern;
    c[ImGuiCol_PlotHistogram] = Mix(p.trellis, p.fern, 0.4f);
    c[ImGuiCol_PlotHistogramHovered] = p.fern;
    c[ImGuiCol_TableHeaderBg] = Mix(p.hollow, p.canopy, 0.4f);
    c[ImGuiCol_TableBorderStrong] = p.line2;
    c[ImGuiCol_TableBorderLight] = p.line;
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = A(p.lumen, 0.02f);
    c[ImGuiCol_DragDropTarget] = p.fern;
    c[ImGuiCol_DragDropTargetBg] = A(p.fern, 0.12f);
    c[ImGuiCol_UnsavedMarker] = p.fern;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    // Docking overlays (ImGui docking branch); match accent / empty central node.
    c[ImGuiCol_DockingPreview] = A(p.fern, 0.70f);
    c[ImGuiCol_DockingEmptyBg] = p.canopy;
}

void PaintTrellisColors(ImGuiStyle& style, const Palette& p)
{
    PaintSharedFromPalette(style, p);
    ImVec4* c = style.Colors;

    c[ImGuiCol_Border] = p.line;
    c[ImGuiCol_FrameBg] = p.canopy;
    c[ImGuiCol_FrameBgHovered] = Mix(p.canopy, p.hollow, 0.55f);
    c[ImGuiCol_FrameBgActive] = Mix(p.canopy, p.fern, 0.18f);
    c[ImGuiCol_CheckMark] = p.fern;
    c[ImGuiCol_SliderGrab] = p.fern;
    c[ImGuiCol_SliderGrabActive] = Mix(p.fern, p.lumen, 0.25f);
    c[ImGuiCol_Button] = Mix(p.hollow, p.canopy, 0.25f);
    c[ImGuiCol_ButtonHovered] = Mix(p.hollow, p.fern, 0.16f);
    c[ImGuiCol_ButtonActive] = Mix(p.hollow, p.fern, 0.28f);
    c[ImGuiCol_Header] = A(p.fern, 0.14f);
    c[ImGuiCol_HeaderHovered] = A(p.fern, 0.22f);
    c[ImGuiCol_HeaderActive] = A(p.fern, 0.28f);
    c[ImGuiCol_Separator] = p.line;
    c[ImGuiCol_SeparatorHovered] = p.fern;
    c[ImGuiCol_SeparatorActive] = p.fern;
    c[ImGuiCol_ResizeGrip] = A(p.trellis, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = A(p.fern, 0.45f);
    c[ImGuiCol_ResizeGripActive] = A(p.fern, 0.70f);
    c[ImGuiCol_Tab] = p.canopy;
    c[ImGuiCol_TabHovered] = Mix(p.hollow, p.fern, 0.14f);
    c[ImGuiCol_TabSelected] = p.hollow;
    c[ImGuiCol_TabSelectedOverline] = p.fern;
    c[ImGuiCol_TabDimmed] = p.canopy;
    c[ImGuiCol_TabDimmedSelected] = p.hollow;
    c[ImGuiCol_TabDimmedSelectedOverline] = A(p.fern, 0.55f);
    c[ImGuiCol_TreeLines] = p.line;
}

void PaintHollowSoftColors(ImGuiStyle& style, const Palette& p)
{
    PaintSharedFromPalette(style, p);
    ImVec4* c = style.Colors;

    c[ImGuiCol_WindowBg] = p.hollow;
    c[ImGuiCol_PopupBg] = Mix(p.hollow, p.lumen, 0.03f);
    c[ImGuiCol_Border] = A(p.line, 0.0f);
    c[ImGuiCol_FrameBg] = Mix(p.canopy, p.hollow, 0.25f);
    c[ImGuiCol_FrameBgHovered] = Mix(p.canopy, p.fern, 0.16f);
    c[ImGuiCol_FrameBgActive] = Mix(p.canopy, p.fern, 0.26f);
    c[ImGuiCol_CheckMark] = p.fern;
    c[ImGuiCol_SliderGrab] = p.fern;
    c[ImGuiCol_SliderGrabActive] = Mix(p.fern, p.lumen, 0.30f);
    c[ImGuiCol_Button] = Mix(p.hollow, p.lumen, 0.05f);
    c[ImGuiCol_ButtonHovered] = Mix(p.hollow, p.fern, 0.28f);
    c[ImGuiCol_ButtonActive] = Mix(p.hollow, p.fern, 0.42f);
    c[ImGuiCol_Header] = Mix(p.hollow, p.fern, 0.30f);
    c[ImGuiCol_HeaderHovered] = Mix(p.hollow, p.fern, 0.42f);
    c[ImGuiCol_HeaderActive] = Mix(p.hollow, p.fern, 0.52f);
    c[ImGuiCol_Separator] = A(p.line2, 0.80f);
    c[ImGuiCol_SeparatorHovered] = p.fern;
    c[ImGuiCol_SeparatorActive] = p.fern;
    c[ImGuiCol_ResizeGrip] = A(p.fern, 0.15f);
    c[ImGuiCol_ResizeGripHovered] = A(p.fern, 0.40f);
    c[ImGuiCol_ResizeGripActive] = A(p.fern, 0.65f);
    c[ImGuiCol_Tab] = Mix(p.canopy, p.hollow, 0.4f);
    c[ImGuiCol_TabHovered] = Mix(p.hollow, p.fern, 0.30f);
    c[ImGuiCol_TabSelected] = Mix(p.hollow, p.fern, 0.18f);
    c[ImGuiCol_TabSelectedOverline] = A(p.fern, 0.0f);
    c[ImGuiCol_TabDimmed] = p.canopy;
    c[ImGuiCol_TabDimmedSelected] = p.hollow;
    c[ImGuiCol_TabDimmedSelectedOverline] = A(p.fern, 0.0f);
    c[ImGuiCol_TitleBg] = Mix(p.hollow, p.canopy, 0.35f);
    c[ImGuiCol_TitleBgActive] = Mix(p.hollow, p.canopy, 0.20f);
    c[ImGuiCol_MenuBarBg] = Mix(p.canopy, p.hollow, 0.35f);
    c[ImGuiCol_TreeLines] = A(p.trellis, 0.45f);
}

void EnsureAllColorsWritten(ImGuiStyle& style)
{
    // Repair non-finite colour slots only. Do NOT treat the unit-test coverage
    // sentinel as unset — painters must write every ImGuiCol_ (incl. docking)
    // or ColorsFullyAssigned will fail.
    ImGuiStyle filled{};
    ImGui::StyleColorsDark(&filled);
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const ImVec4& c = style.Colors[i];
        const bool nonFinite = !std::isfinite(c.x) || !std::isfinite(c.y) ||
                               !std::isfinite(c.z) || !std::isfinite(c.w);
        if (nonFinite)
            style.Colors[i] = filled.Colors[i];
    }
}

} // namespace

const char* StylePresetName(StylePreset preset)
{
    switch (preset) {
    case StylePreset::Trellis: return "Trellis";
    case StylePreset::HollowSoft: return "Hollow Soft";
    default: return kDefaultPresetName;
    }
}

const char* ColorSchemeName(ColorScheme scheme)
{
    switch (scheme) {
    case ColorScheme::Ivy: return "Ivy";
	case ColorScheme::IvyDark: return "Ivy Dark";
    case ColorScheme::ClassicDark: return "Classic Dark";
    case ColorScheme::NeutralGray: return "Neutral Gray";
    case ColorScheme::Light: return "Light";
    default: return kDefaultSchemeName;
    }
}

StylePreset ParseStylePreset(const char* name)
{
    if (!name || !*name)
        return StylePreset::Trellis;
    if (EqualsCi(name, "Trellis"))
        return StylePreset::Trellis;
    if (EqualsCi(name, "Hollow Soft") || EqualsCi(name, "HollowSoft") ||
        EqualsCi(name, "hollow_soft"))
        return StylePreset::HollowSoft;
    return StylePreset::Trellis;
}

ColorScheme ParseColorScheme(const char* name)
{
    if (!name || !*name)
        return ColorScheme::Ivy;
    if (EqualsCi(name, "Ivy"))
        return ColorScheme::Ivy;
    if (EqualsCi(name, "Ivy Dark") || EqualsCi(name, "IvyDark") ||
        EqualsCi(name, "ivy_dark"))
        return ColorScheme::IvyDark;
    if (EqualsCi(name, "Classic Dark") || EqualsCi(name, "ClassicDark") ||
        EqualsCi(name, "classic_dark"))
        return ColorScheme::ClassicDark;
    if (EqualsCi(name, "Neutral Gray") || EqualsCi(name, "NeutralGray") ||
        EqualsCi(name, "neutral_gray") || EqualsCi(name, "Neutral"))
        return ColorScheme::NeutralGray;
    if (EqualsCi(name, "Light"))
        return ColorScheme::Light;
    return ColorScheme::Ivy;
}

StylePreset ClampStylePreset(int value)
{
    if (value < 0 || value >= static_cast<int>(StylePreset::Count))
        return StylePreset::Trellis;
    return static_cast<StylePreset>(value);
}

ColorScheme ClampColorScheme(int value)
{
    if (value < 0 || value >= static_cast<int>(ColorScheme::Count))
        return ColorScheme::Ivy;
    return static_cast<ColorScheme>(value);
}

const SemanticColors& GetSemanticColors() { return g_semantics; }
const Fonts& GetFonts() { return g_fonts; }
StylePreset GetStylePreset() { return g_preset; }
ColorScheme GetColorScheme() { return g_scheme; }

void ApplyColors(ImGuiStyle& style, StylePreset preset, ColorScheme scheme)
{
    preset = ClampStylePreset(static_cast<int>(preset));
    scheme = ClampColorScheme(static_cast<int>(scheme));
    g_preset = preset;
    g_scheme = scheme;

    if (scheme == ColorScheme::ClassicDark) {
        ImGui::StyleColorsDark(&style);
        // Classic Dark keeps StyleColorsDark hues; still assign DragDropTargetBg
        // and reaffirm docking colors (StyleColorsDark sets them; keep explicit).
        style.Colors[ImGuiCol_DragDropTargetBg] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_DockingPreview] =
            style.Colors[ImGuiCol_HeaderActive] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
        style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        FillSemanticsClassicDark(g_semantics);
        EnsureAllColorsWritten(style);
        return;
    }

    Palette palette;
    bool light = false;
    switch (scheme) {
    case ColorScheme::NeutralGray:
        palette = MakeNeutralGrayPalette();
        break;
    case ColorScheme::Light:
        palette = MakeLightPalette();
        light = true;
        break;
    case ColorScheme::IvyDark:
        palette = MakeIvyDarkPalette();
        break;
    case ColorScheme::Ivy:
    default:
        palette = MakeIvyPalette();
        break;
    }

    if (preset == StylePreset::HollowSoft)
        PaintHollowSoftColors(style, palette);
    else
        PaintTrellisColors(style, palette);

    FillSemanticsFromPalette(g_semantics, palette, light);
    EnsureAllColorsWritten(style);
}

void Apply(ImGuiStyle& style, StylePreset preset, ColorScheme scheme)
{
    preset = ClampStylePreset(static_cast<int>(preset));
    scheme = ClampColorScheme(static_cast<int>(scheme));

    // Reset sizes to defaults, then apply preset geometry.
    const float keepFontScaleMain = style.FontScaleMain;
    const float keepFontScaleDpi = style.FontScaleDpi;
    style = ImGuiStyle();
    style.FontScaleMain = keepFontScaleMain;
    style.FontScaleDpi = keepFontScaleDpi;

    if (preset == StylePreset::HollowSoft)
        ApplyHollowSoftGeometry(style);
    else
        ApplyTrellisGeometry(style);

    ApplyColors(style, preset, scheme);
}

bool LoadFonts(ImGuiIO& io, Fonts* out, float bodySize, float monoSize)
{
    Fonts local{};
    local.bodySize = bodySize;
    local.monoSize = monoSize;

    auto resolveFont = [](const char* romRel) -> std::string {
        const fs::path p = content_path::ReadableFileCurrent(romRel);
        if (p.empty())
            return {};
        return PathToUtf8(p);
    };

    const std::string bodyPath = resolveFont("rom/font/IBMPlexSansJP-Regular.ttf");
    const std::string monoPath = resolveFont("rom/font/IBMPlexMono-Regular.ttf");
    const std::string faPath = resolveFont("rom/font/fa-solid-900.ttf");

    auto fallbackDefault = [&](const char* reason) -> bool {
        std::fprintf(stderr, "Warning: editor font load failed (%s); using AddFontDefault.\n",
                     reason ? reason : "unknown");
        local.body = io.Fonts->AddFontDefault();
        local.mono = local.body;
        io.FontDefault = local.body;
        g_fonts = local;
        if (out)
            *out = local;
        return false;
    };

    if (bodyPath.empty())
        return fallbackDefault("missing IBMPlexSansJP-Regular.ttf");
    if (monoPath.empty())
        return fallbackDefault("missing IBMPlexMono-Regular.ttf");

    // ImGui 1.92 dynamic atlas: nullptr glyph ranges → glyphs rasterize on demand (CJK).
    ImFontConfig bodyCfg;
    bodyCfg.OversampleH = 2;
    bodyCfg.OversampleV = 2;
    bodyCfg.PixelSnapH = false;
    std::snprintf(bodyCfg.Name, sizeof(bodyCfg.Name), "IBM Plex Sans JP");
    local.body = io.Fonts->AddFontFromFileTTF(bodyPath.c_str(), bodySize, &bodyCfg, nullptr);
    if (!local.body)
        return fallbackDefault("AddFontFromFileTTF body failed");

    if (!faPath.empty()) {
        ImFontConfig faCfg;
        faCfg.MergeMode = true;
        faCfg.PixelSnapH = true;
        // Natural advances: toolbar buttons center via glyph ink boxes
        // (editor_icon_button); menus/Asset Browser keep ImGui text layout.
        // No GlyphOffset — per-glyph ink centering replaces a global Y nudge.
        faCfg.GlyphMinAdvanceX = 0.0f;
        faCfg.OversampleH = 2;
        faCfg.OversampleV = 2;
        static const ImWchar faRanges[] = { (ImWchar)ICON_MIN_FA, (ImWchar)ICON_MAX_FA, 0 };
        std::snprintf(faCfg.Name, sizeof(faCfg.Name), "Font Awesome 6 Solid");
        // Rasterize at the same base size as the body font; ImGui 1.92
        // FontScaleMain re-bakes crisply under UI zoom / DPI.
        if (!io.Fonts->AddFontFromFileTTF(faPath.c_str(), bodySize, &faCfg, faRanges)) {
            std::fprintf(stderr,
                "Warning: failed to merge icon font rom/font/fa-solid-900.ttf\n");
        }
    }
    else {
        std::fprintf(stderr,
            "Warning: failed to load icon font rom/font/fa-solid-900.ttf\n");
    }

    ImFontConfig monoCfg;
    monoCfg.OversampleH = 2;
    monoCfg.OversampleV = 2;
    monoCfg.PixelSnapH = true;
    std::snprintf(monoCfg.Name, sizeof(monoCfg.Name), "IBM Plex Mono");
    local.mono = io.Fonts->AddFontFromFileTTF(monoPath.c_str(), monoSize, &monoCfg, nullptr);
    if (!local.mono) {
        std::fprintf(stderr,
            "Warning: failed to load IBMPlexMono-Regular.ttf; labels use body font.\n");
        local.mono = local.body;
    }

    io.FontDefault = local.body;
    g_fonts = local;
    if (out)
        *out = local;
    return true;
}

bool PushLabelFont()
{
    if (g_labelFontPushed)
        return false;
    if (g_preset != StylePreset::Trellis)
        return false;
    if (!g_fonts.mono || g_fonts.mono == g_fonts.body)
        return false;
    ImGui::PushFont(g_fonts.mono, g_fonts.monoSize);
    g_labelFontPushed = true;
    return true;
}

void PopLabelFont()
{
    if (!g_labelFontPushed)
        return;
    ImGui::PopFont();
    g_labelFontPushed = false;
}

bool ColorsFullyAssigned(const ImGuiStyle& style, const ImVec4& sentinel)
{
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const ImVec4& c = style.Colors[i];
        if (c.x == sentinel.x && c.y == sentinel.y &&
            c.z == sentinel.z && c.w == sentinel.w)
            return false;
    }
    return true;
}

} // namespace editor_theme
