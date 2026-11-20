// inspector_widgets.h
// Compact, width-safe widgets for the Properties (inspector) panel: component
// headers with an accent bar, icon property rows (word label when wide, icon
// only when narrow, tooltip on hover), colored XYZ fields, icon toggle rows and
// asset path fields. Every widget fills the available width and never exceeds
// it. Depends only on Dear ImGui (Linux-tested: tools/run_inspector_widgets_check.sh).
#pragma once

#include "imgui/imgui.h"

#include <string>

class Component;

namespace insp {

// Accent per component category (darkened automatically on light schemes).
enum class Accent { Transform, Render, Light, Camera, Physics, Audio, Script, Effect, Neutral };
ImVec4 AccentColor(Accent accent);

enum class HeaderAction { None, Remove, Reset };

struct HeaderResult {
    bool open = false;          // draw the body
    bool changed = false;       // *enabled toggled this frame
    HeaderAction action = HeaderAction::None;
};

// Collapsible component header. `id` must be stable (use the JSON type name).
// enabled: nullptr hides the toggle. component: enables "Copy JSON" in the menu.
// canRemove / canReset add those menu items. extraMenu runs inside the menu
// (may be nullptr).
HeaderResult ComponentHeader(const char* id, const char* icon, const char* name, Accent accent,
                             bool* enabled, const Component* component,
                             bool canRemove = true, bool canReset = true,
                             void (*extraMenu)(void*) = nullptr, void* extraMenuUser = nullptr);

// Property row: label column (icon + word, or icon only on narrow panels, or
// just the word when icon == nullptr) and the next widget fills the rest.
// tip is shown on label hover. Give the widget the id "##v".
//     { insp::PropRow row(ICON_FA_WEIGHT_HANGING, "Mass"); ImGui::DragFloat("##v", &m); }
class PropRow {
public:
    PropRow(const char* icon, const char* word, const char* tip = nullptr);
    ~PropRow();
    PropRow(const PropRow&) = delete;
    PropRow& operator=(const PropRow&) = delete;
};

// Draw just the label column of a row and leave the cursor at the field start;
// returns the field width. Used by composite rows.
float RowLabel(const char* icon, const char* word, const char* tip);

// Three (or two) colored axis fields (X red, Y green, Z blue, W gray) in one
// row (axes picks the chip letters, e.g. "XZ"); stacks one axis per line when too narrow. Clicking an axis chip sets
// that axis to resetValue. wheelStep > 0 lets the mouse wheel nudge a field.
bool DragVecN(const char* icon, const char* word, const char* tip, float* v, int n,
              float speed, float vmin = 0.0f, float vmax = 0.0f,
              float resetValue = 0.0f, float wheelStep = 0.0f, const char* fmt = "%.3f",
              const char* axes = "XYZW");

// One row of equal icon toggle buttons (wraps to more rows when narrow).
// Each toggle shows its tooltip on hover. Returns true when any value changed.
struct Toggle {
    const char* icon;
    const char* tip;
    bool* value;
    const char* warn = nullptr;  // non-null: warning tint + extra tooltip line
};
bool ToggleRow(const Toggle* toggles, int count);

// Segmented single choice drawn as icon buttons filling the width; wraps.
bool ChoiceBar(const char* id, const char* const* icons, const char* const* tips, int count,
               int* selected);

// Asset path field inside a PropRow: full path editable, tooltip shows it, an
// X button clears it, and dropping an "ASSET_PATH" payload with one of exts
// assigns it. Returns true when path changed.
bool AssetField(const char* icon, const char* word, const char* tip, std::string& path,
                const char* const* exts, int extCount);

// Square icon-only button, glyph centered by its ink box; tooltip on hover.
// iconId may carry a "##id" suffix. size <= 0 = frame height.
bool IconButton(const char* iconId, const char* tip, float size = 0.0f);

// Small inline icon (after SameLine) with a hover tooltip, colored.
void HintIcon(const char* icon, const ImVec4& color, const char* tip);
inline void HintIconSameLine(const char* icon, const ImVec4& color, const char* tip)
{
    ImGui::SameLine();
    HintIcon(icon, color, tip);
}

// Full-width button with an icon and short word; tooltip on hover.
bool WideButton(const char* icon, const char* word, const char* tip = nullptr);

// Equal-width icon buttons in one row (wraps). Returns the index pressed or -1.
int IconButtonRow(const char* id, const char* const* icons, const char* const* tips, int count);

// Small sub-heading line inside a component body (icon + word, muted, rule).
// hint: optional icon at the right end with that tooltip (warning or info).
void SubHeading(const char* icon, const char* word, const char* hint = nullptr,
                bool hintIsWarning = true);

// Read-only value row (clipped text, full text in the tooltip).
void ReadOnlyRow(const char* icon, const char* word, const char* tip, const char* text,
                 const ImVec4* color = nullptr);

// Shorten text with "..." so it fits maxW pixels at the current font.
std::string Ellipsize(const std::string& text, float maxW);

ImVec4 WarningColor();
ImVec4 InfoColor();
ImVec4 ErrorColor();

} // namespace insp
