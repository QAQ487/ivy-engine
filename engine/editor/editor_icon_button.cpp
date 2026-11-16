// editor_icon_button.cpp
// Custom icon buttons: ButtonBehavior frame + ink-box-centered glyph draw.

#include "engine/editor/editor_icon_button.h"

#include "imgui/imgui_internal.h"

namespace editor_icon_button {
namespace {

ImVec2 ResolveSize(const ImVec2& size_arg)
{
    const float frame = ImGui::GetFrameHeight();
    ImVec2 size = size_arg;
    if (size.x <= 0.0f)
        size.x = frame;
    if (size.y <= 0.0f)
        size.y = (size_arg.x <= 0.0f) ? frame : size.x;
    // Pixel-snap the hit box so ink centering lands on whole pixels.
    size.x = static_cast<float>(static_cast<int>(size.x + 0.5f));
    size.y = static_cast<float>(static_cast<int>(size.y + 0.5f));
    return size;
}

bool IconButtonEx(const char* icon_label, const ImVec2& size_arg, bool active)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(icon_label);
    const ImVec2 size = ResolveSize(size_arg);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImGuiCol col_idx = active ? ImGuiCol_ButtonActive
        : (held && hovered) ? ImGuiCol_ButtonActive
        : hovered ? ImGuiCol_ButtonHovered
        : ImGuiCol_Button;
    // Active toggles keep the active fill even when hovered.
    if (active && hovered && !held)
        col_idx = ImGuiCol_ButtonActive;

    const ImU32 col = ImGui::GetColorU32(col_idx);
    ImGui::RenderNavCursor(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    const char* text_end = ImGui::FindRenderedTextEnd(icon_label);
    if (text_end > icon_label) {
        unsigned int codepoint = 0;
        const int bytes = ImTextCharFromUtf8(&codepoint, icon_label, text_end);
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        ImFontBaked* baked = font ? font->GetFontBaked(font_size) : nullptr;
        const ImFontGlyph* glyph = (baked && bytes > 0 && codepoint != 0)
            ? baked->FindGlyph((ImWchar)codepoint)
            : nullptr;

        const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
        if (glyph && glyph->Visible) {
            const float scale = (baked->Size > 0.0f) ? (font_size / baked->Size) : 1.0f;
            float draw_x = 0.0f;
            float draw_y = 0.0f;
            CenteredGlyphDrawPos(
                bb.Min.x, bb.Min.y, bb.Max.x, bb.Max.y,
                glyph->X0, glyph->Y0, glyph->X1, glyph->Y1,
                scale, &draw_x, &draw_y);
            window->DrawList->AddText(font, font_size, ImVec2(draw_x, draw_y),
                text_col, icon_label, text_end);
        }
        else {
            // Fallback: ImGui's usual label centering (advance metrics).
            const ImVec2 label_size = ImGui::CalcTextSize(icon_label, text_end, false);
            ImGui::RenderTextClipped(
                bb.Min + style.FramePadding,
                bb.Max - style.FramePadding,
                icon_label, text_end, &label_size, style.ButtonTextAlign, &bb);
        }
    }

    return pressed;
}

} // namespace

bool IconButton(const char* icon_label, const ImVec2& size)
{
    return IconButtonEx(icon_label, size, false);
}

bool IconToggle(const char* icon_label, bool active, const ImVec2& size)
{
    return IconButtonEx(icon_label, size, active);
}

bool IconButtonTooltip(const char* icon_label, const char* tooltip, const ImVec2& size)
{
    const bool pressed = IconButton(icon_label, size);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

bool IconToggleTooltip(const char* icon_label, bool active, const char* tooltip,
                       const ImVec2& size)
{
    const bool pressed = IconToggle(icon_label, active, size);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

} // namespace editor_icon_button
