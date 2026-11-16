// editor_icon_button.h
// Toolbar icon buttons that center Font Awesome ink boxes (not advance metrics).

#pragma once

#include "imgui/imgui.h"

namespace editor_icon_button {

// Pure geometry: draw position for AddText so glyph ink (x0..x1, y0..y1) *
// scale is centered in [btn_min, btn_max]. Result is truncated toward zero
// (same as ImGui IM_TRUNC) so subsequent AddText stays pixel-snapped.
inline void CenteredGlyphDrawPos(
    float btn_min_x, float btn_min_y, float btn_max_x, float btn_max_y,
    float glyph_x0, float glyph_y0, float glyph_x1, float glyph_y1,
    float scale, float* out_x, float* out_y)
{
    const float ink_w = (glyph_x1 - glyph_x0) * scale;
    const float ink_h = (glyph_y1 - glyph_y0) * scale;
    const float x = btn_min_x + ((btn_max_x - btn_min_x) - ink_w) * 0.5f - glyph_x0 * scale;
    const float y = btn_min_y + ((btn_max_y - btn_min_y) - ink_h) * 0.5f - glyph_y0 * scale;
    if (out_x)
        *out_x = static_cast<float>(static_cast<int>(x));
    if (out_y)
        *out_y = static_cast<float>(static_cast<int>(y));
}

// icon_label may include an ImGui "##id" suffix (drawn text stops before ##).
// size.x/y <= 0 → use GetFrameHeight() on that axis (square when both <= 0).
bool IconButton(const char* icon_label, const ImVec2& size = ImVec2(0, 0));
bool IconToggle(const char* icon_label, bool active, const ImVec2& size = ImVec2(0, 0));

// Convenience: IconButton / IconToggle + hover tooltip (DelayShort).
bool IconButtonTooltip(const char* icon_label, const char* tooltip,
                       const ImVec2& size = ImVec2(0, 0));
bool IconToggleTooltip(const char* icon_label, bool active, const char* tooltip,
                       const ImVec2& size = ImVec2(0, 0));

} // namespace editor_icon_button
