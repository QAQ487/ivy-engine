// editor_layout.cpp
// Editor chrome metrics and fallback viewport geometry (free aspect).
// Live game viewport comes from the docking central node (see editor_docking).

#include "editor_gui_common_includes.h"
#include "engine/editor/editor_docking_math.h"

// Toolbar chrome uses compact local padding (see PushToolbarChromeStyle) so
// FontSizeBase 15.5/16 + theme FramePadding no longer clip in a fixed 36px row.
static constexpr float kToolbarWindowPadX = 6.0f;
static constexpr float kToolbarWindowPadY = 4.0f;
static constexpr float kToolbarFramePadX = 6.0f;
static constexpr float kToolbarFramePadY = 4.0f;
static constexpr float kToolbarItemSpacingX = 4.0f;

float Ui(float px) { return px * g_uiZoom; }

InspectorRow::InspectorRow(const char* label)
{
    ImGui::PushID(label);
    const float startX = ImGui::GetCursorPosX();
    const float colW = (std::max)(Ui(64.0f), ImGui::GetContentRegionAvail().x * 0.4f);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::AlignTextToFramePadding();
    ImGui::PushClipRect(p, ImVec2(p.x + colW - gap, p.y + ImGui::GetFrameHeight()), true);
    ImGui::TextUnformatted(label);
    ImGui::PopClipRect();
    if (ImGui::GetItemRectSize().x > colW - gap && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", label);

    ImGui::SameLine(startX + colW);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

InspectorRow::~InspectorRow()
{
    ImGui::PopID();
}

bool InspectorToggleGrid(const char* const* labels, int count, int* selected, int columns)
{
    if (!labels || !selected || count <= 0)
        return false;
    columns = (std::max)(1, (std::min)(columns, count));
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float w = (std::max)(1.0f,
        (ImGui::GetContentRegionAvail().x - spacing * static_cast<float>(columns - 1)) /
            static_cast<float>(columns));

    bool changed = false;
    for (int i = 0; i < count; ++i) {
        if (i % columns != 0)
            ImGui::SameLine();
        const bool active = (*selected == i);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(labels[i], ImVec2(w, 0.0f)) && !active) {
            *selected = i;
            changed = true;
        }
        if (active)
            ImGui::PopStyleColor();
    }
    return changed;
}

ImVec2 ToolbarWindowPadding()
{
    return ImVec2(Ui(kToolbarWindowPadX), Ui(kToolbarWindowPadY));
}

ImVec2 ToolbarFramePadding()
{
    return ImVec2(Ui(kToolbarFramePadX), Ui(kToolbarFramePadY));
}

void PushToolbarChromeStyle()
{
    const ImVec2 windowPad = ToolbarWindowPadding();
    const ImVec2 framePad = ToolbarFramePadding();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPad);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePad);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
        ImVec2(Ui(kToolbarItemSpacingX), framePad.y));
}

void PopToolbarChromeStyle()
{
    ImGui::PopStyleVar(3);
}

float EditorMenuBarHeight() {
    return g_imguiInitialized ? ImGui::GetFrameHeight() : 22.0f;
}

// Must match PushToolbarChromeStyle padding: frame + window pad on both sides.
float EditorToolbarHeight() {
    if (!g_imguiInitialized)
        return 36.0f;
    const ImVec2 windowPad = ToolbarWindowPadding();
    const ImVec2 framePad = ToolbarFramePadding();
    const float frameH = ImGui::GetFontSize() + framePad.y * 2.0f;
    return IM_TRUNC(frameH + windowPad.y * 2.0f + 0.5f);
}

float EditorTopChrome() {
    return EditorMenuBarHeight() + EditorToolbarHeight();
}

EditorLayoutMetrics ComputeEditorLayout(float screenW, float screenH) {
    // Fallback geometry when the docking central node is not yet available.
    // Available hole between default panel sizes; game view is 16:9-fitted inside.
    EditorLayoutMetrics layout;
    layout.topChrome = EditorTopChrome();
    const EditorDefaultPanelSizes panels =
        ComputeDefaultPanelSizes(screenW, (std::max)(1.0f, screenH - layout.topChrome), g_uiZoom);
    layout.leftWidth = panels.leftWidth;
    layout.rightWidth = panels.rightWidth;
    layout.bottomHeight = panels.bottomHeight;

    EditorClientRect hole{};
    hole.x = layout.leftWidth;
    hole.y = layout.topChrome;
    hole.w = (std::max)(kEditorDockViewportMinWidth,
        screenW - layout.leftWidth - layout.rightWidth);
    hole.h = (std::max)(kEditorDockViewportMinHeight,
        screenH - layout.topChrome - layout.bottomHeight);
    layout.sidePanelH = hole.h;

    const EditorClientRect fitted = FitEditor16x9Viewport(hole);
    layout.viewportX = fitted.x;
    layout.viewportY = fitted.y;
    layout.viewportW = fitted.w;
    layout.viewportH = fitted.h;
    return layout;
}
