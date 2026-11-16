// editor_layout.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"

// Toolbar chrome uses compact local padding (see PushToolbarChromeStyle) so
// FontSizeBase 15.5/16 + theme FramePadding no longer clip in a fixed 36px row.
static constexpr float kToolbarWindowPadX = 6.0f;
static constexpr float kToolbarWindowPadY = 4.0f;
static constexpr float kToolbarFramePadX = 6.0f;
static constexpr float kToolbarFramePadY = 4.0f;
static constexpr float kToolbarItemSpacingX = 4.0f;
static constexpr float kLeftPanelWidth = 300.0f;
static constexpr float kRightPanelWidth = 350.0f;
static constexpr float kBottomPanelMinHeight = 340.0f;
static constexpr float kBottomPanelScreenFraction = 0.34f;
static constexpr float kEditorViewportMinHeight = 120.0f;
// Preserve preview aspect from original 1920x1080 layout (250px bottom, 58px top chrome).
static constexpr float kEditorViewportAspect =
    (1920.0f - kLeftPanelWidth - kRightPanelWidth) /
    (1080.0f - 58.0f - 250.0f);

float Ui(float px) { return px * g_uiZoom; }

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
    EditorLayoutMetrics layout;
    layout.topChrome = EditorTopChrome();
    layout.leftWidth = Ui(kLeftPanelWidth);
    layout.rightWidth = Ui(kRightPanelWidth);

    layout.bottomHeight = (std::max)(Ui(kBottomPanelMinHeight), screenH * kBottomPanelScreenFraction);
    const float maxBottomHeight = screenH - layout.topChrome - Ui(kEditorViewportMinHeight);
    layout.bottomHeight = (std::min)(layout.bottomHeight, maxBottomHeight);

    const float previewAvailX = layout.leftWidth;
    const float previewAvailY = layout.topChrome;
    const float previewAvailW = (std::max)(1.0f, screenW - layout.leftWidth - layout.rightWidth);
    const float previewAvailH = (std::max)(
        1.0f, screenH - layout.topChrome - layout.bottomHeight);
    layout.sidePanelH = previewAvailH;

    layout.viewportW = previewAvailW;
    layout.viewportH = layout.viewportW / kEditorViewportAspect;
    if (layout.viewportH > previewAvailH) {
        layout.viewportH = previewAvailH;
        layout.viewportW = layout.viewportH * kEditorViewportAspect;
    }
    layout.viewportX = previewAvailX + (previewAvailW - layout.viewportW) * 0.5f;
    layout.viewportY = previewAvailY + (previewAvailH - layout.viewportH) * 0.5f;

    // Side panels extend to viewport edges so letterbox gutters are filled.
    layout.leftWidth = layout.viewportX;
    layout.rightWidth = screenW - (layout.viewportX + layout.viewportW);
    return layout;
}

