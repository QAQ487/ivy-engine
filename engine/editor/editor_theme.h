// editor_theme.h
// Editor Dear ImGui style presets and color schemes.
// Two independent axes: style preset (geometry / label treatment) and color scheme.

#pragma once

#include "imgui/imgui.h"

#include <cstddef>

namespace editor_theme {

enum class StylePreset : int {
    Trellis = 0,     // default: hairline, square corners, mono labels
    HollowSoft = 1,
    Count
};

enum class ColorScheme : int {
    Ivy = 0,           // default: portfolio canopy / fern palette
	IvyDark = 1,       // darker Ivy surfaces; same fern accent / lumen text
    ClassicDark = 2,   // ImGui StyleColorsDark (previous editor look)
    NeutralGray = 3,
    Light = 4,
    Count
};

// Semantic colors for hardcoded PushStyleColor call sites (Play/Stop, start screen, etc.).
struct SemanticColors {
    ImVec4 text{};
    ImVec4 textMuted{};
    ImVec4 accent{};          // primary interactive / Play
    ImVec4 accentHovered{};
    ImVec4 accentActive{};
    ImVec4 success{};         // affirmative CTA (Open Project)
    ImVec4 successHovered{};
    ImVec4 successActive{};
    ImVec4 warning{};
    ImVec4 danger{};          // Stop / Return to Start
    ImVec4 dangerHovered{};
    ImVec4 dangerActive{};
    ImVec4 toggleOn{};        // active toolbar toggle highlight
    ImVec4 windowBg{};
};

struct Fonts {
    ImFont* body = nullptr;   // IBM Plex Sans JP (+ merged Font Awesome)
    ImFont* mono = nullptr;   // IBM Plex Mono (Trellis labels / headers)
    float bodySize = 16.0f;
    float monoSize = 13.0f;
};

inline constexpr const char* kDefaultPresetName = "Trellis";
inline constexpr const char* kDefaultSchemeName = "Ivy";

const char* StylePresetName(StylePreset preset);
const char* ColorSchemeName(ColorScheme scheme);

// Case-insensitive. Unknown / null / empty → defaults (Trellis / Ivy).
StylePreset ParseStylePreset(const char* name);
ColorScheme ParseColorScheme(const char* name);

StylePreset ClampStylePreset(int value);
ColorScheme ClampColorScheme(int value);

const SemanticColors& GetSemanticColors();
const Fonts& GetFonts();

StylePreset GetStylePreset();
ColorScheme GetColorScheme();

// Apply geometry (preset) + ImGuiCol_ table + semantic colors (scheme).
// Preserves FontScaleMain / FontScaleDpi. Caller re-captures base style for zoom.
void Apply(ImGuiStyle& style, StylePreset preset, ColorScheme scheme);

// Paint only Colors[] + semantics (no geometry reset). Used by unit tests after
// filling Colors[] with a sentinel to prove every ImGuiCol_ entry is written.
void ApplyColors(ImGuiStyle& style, StylePreset preset, ColorScheme scheme);

// Resolve rom/font/* via content_path (project rom/ then resources/).
// On failure: AddFontDefault, log warning, return false.
bool LoadFonts(ImGuiIO& io, Fonts* out,
               float bodySize = 16.0f, float monoSize = 13.0f);

// Push mono font when Trellis and mono loaded; otherwise no-op. Pair with PopLabelFont.
bool PushLabelFont();
void PopLabelFont();

// Unit-test helper: after Apply, every Colors[i] differs from the given sentinel.
bool ColorsFullyAssigned(const ImGuiStyle& style, const ImVec4& sentinel);

} // namespace editor_theme
