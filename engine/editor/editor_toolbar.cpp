// editor_toolbar.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_internal.h"

#include "engine/core/engine_gui.h"
#include "engine/core/engine_gui_internal.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/editor/editor_icon_button.h"
#include "engine/editor/editor_theme.h"
#include "engine/editor/editor_undo.h"
#include "engine/editor/project_manager.h"
#include "engine/editor/transform_gizmo.h"
#include "engine/scene/scene_loader.h"
#include "engine/scripting/script_factory.h"
#include "debug_camera.h"
#include "grid.h"
#include "imgui/imgui.h"
#include "imgui/icons_font_awesome.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

ReloadLightState GetReloadLightState(bool active, bool paused, bool pending, const ReloadLight& light) {
    if (!active || paused) return ReloadLightState::Paused;
    if (pending) return ReloadLightState::Pending;
    return light.failed ? ReloadLightState::Failed : ReloadLightState::Watching;
}


// Square icon hit-target sized from the (toolbar-local) frame height.
float ToolbarIconSize()
{
    return IM_TRUNC(ImGui::GetFrameHeight() + 0.5f);
}

bool ToolbarIconButton(const char* icon, const char* tooltip)
{
    return editor_icon_button::IconButtonTooltip(icon, tooltip, ImVec2(ToolbarIconSize(), 0.0f));
}

bool ToolbarIconToggle(const char* icon, bool active, const char* tooltip)
{
    return editor_icon_button::IconToggleTooltip(
        icon, active, tooltip, ImVec2(ToolbarIconSize(), 0.0f));
}

void DrawReloadLight(const char* id, const char* name, ReloadLightState state,
    const ReloadLight& light, const std::string& detail) {
    // Size from the current (toolbar-local) frame height so lights track font zoom.
    const float size = IM_TRUNC(ImGui::GetFrameHeight() * 0.65f + 0.5f);
    const float glowR = IM_TRUNC(size * 0.40f + 0.5f);
    const float coreR = IM_TRUNC(size * 0.22f + 0.5f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool bright = state == ReloadLightState::Watching && ImGui::GetTime() < light.brightUntil;
    ImU32 color;
    const char* status;
    switch (state) {
    case ReloadLightState::Paused: color = IM_COL32(110, 115, 125, 255); status = "Paused / inactive"; break;
    case ReloadLightState::Pending: color = IM_COL32(245, 190, 55, 255); status = "Waiting / processing"; break;
    case ReloadLightState::Failed: color = IM_COL32(235, 75, 75, 255); status = "Reload failed"; break;
    default: color = bright ? IM_COL32(135, 255, 165, 255) : IM_COL32(60, 165, 95, 255);
        status = bright ? "Reload succeeded" : "Watching"; break;
    }
    const ImVec2 center(IM_TRUNC(origin.x + size * 0.5f), IM_TRUNC(origin.y + size * 0.5f));
    if (bright) ImGui::GetWindowDrawList()->AddCircleFilled(center, glowR, IM_COL32(90, 255, 140, 65));
    ImGui::GetWindowDrawList()->AddCircleFilled(center, coreR, color);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %s", name, status);
        if (state == ReloadLightState::Paused) ImGui::TextUnformatted("Hot-load requires an open project.");
        ImGui::TextUnformatted(detail.c_str());
        ImGui::EndTooltip();
    }
}


void DrawEditorToolbar(bool playVisuals, bool sceneLoadBusy, float screenW, float menuBarHeight)
{
    // --- Toolbar ---
    const float toolbarH = EditorToolbarHeight();
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(screenW, toolbarH));
    PushToolbarChromeStyle();
    ImGui::Begin("##Toolbar", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    {
        const float iconSz = ToolbarIconSize();
        const float lightSize = IM_TRUNC(iconSz * 0.65f + 0.5f);
        const float lightGap = ImGui::GetStyle().ItemSpacing.x;
        const float lightsX = ImGui::GetWindowContentRegionMax().x - lightSize * 3 - lightGap * 2;
        const float lightsY = ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - lightSize) * 0.5f;
        const auto toolbarPos = ImGui::GetWindowPos();
        const auto contentMin = ImGui::GetWindowContentRegionMin();
        ImGui::PushClipRect(ImVec2(contentMin.x + toolbarPos.x, contentMin.y + toolbarPos.y),
            ImVec2(toolbarPos.x + lightsX - lightGap, toolbarPos.y + ImGui::GetWindowContentRegionMax().y), true);
        const auto& theme = editor_theme::GetSemanticColors();
        if (playVisuals) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme.danger);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.dangerHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.dangerActive);
            if (ToolbarIconButton(ICON_FA_STOP, "Stop (F2 / F3)")) {
                ReturnToStart();
            }
            ImGui::PopStyleColor(3);
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, theme.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.accentHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.accentActive);
            if (ToolbarIconButton(ICON_FA_PLAY, "Play in Viewport (F2)")) {
                StartPlay(PlayMode::Viewport);
            }
            ImGui::SameLine(0.0f, 2.0f);
            const float caretW = IM_TRUNC(iconSz * 0.60f + 0.5f);
            const bool openPlayMenu = editor_icon_button::IconButtonTooltip(
                ICON_FA_CARET_DOWN "##PlayMenuBtn", "Play Mode", ImVec2(caretW, iconSz));
            if (openPlayMenu)
                ImGui::OpenPopup("PlayModeDropdownMenu");

            if (ImGui::BeginPopup("PlayModeDropdownMenu")) {
                if (ImGui::MenuItem("Play in Viewport", "F2")) {
                    StartPlay(PlayMode::Viewport);
                }
                if (ImGui::MenuItem("Play Fullscreen", "F3")) {
                    StartPlay(PlayMode::Fullscreen);
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        {
            const bool canSave = !playVisuals && !sceneLoadBusy;
            if (!canSave)
                ImGui::BeginDisabled();
            if (ToolbarIconButton(ICON_FA_FLOPPY_DISK, "Save Scene (Ctrl+S)"))
                SaveScene();
            if (!canSave)
                ImGui::EndDisabled();
        }

        ImGui::SameLine();
        {
            const bool canEditUndo = !playVisuals && !sceneLoadBusy;
            if (!canEditUndo || !EditorUndoStack::Instance().CanUndo())
                ImGui::BeginDisabled();
            const bool undoPressed = ToolbarIconButton(ICON_FA_ROTATE_LEFT, "Undo (Ctrl+Z)");
            if (!canEditUndo || !EditorUndoStack::Instance().CanUndo())
                ImGui::EndDisabled();
            if (undoPressed) {
                if (EditorUndo_Undo()) {
                    EngineGUI_MarkSceneDirty();
                    SyncSceneRuntime();
                }
            }
            ImGui::SameLine();
            if (!canEditUndo || !EditorUndoStack::Instance().CanRedo())
                ImGui::BeginDisabled();
            const bool redoPressed = ToolbarIconButton(ICON_FA_ROTATE_RIGHT, "Redo (Ctrl+Y)");
            if (!canEditUndo || !EditorUndoStack::Instance().CanRedo())
                ImGui::EndDisabled();
            if (redoPressed) {
                if (EditorUndo_Redo()) {
                    EngineGUI_MarkSceneDirty();
                    SyncSceneRuntime();
                }
            }
        }

        ImGui::SameLine();
        if (ToolbarIconButton(ICON_FA_ARROWS_ROTATE, "Reload Scripts"))
            SceneLoader_ScanScriptDllsForChanges(true);

        if (!playVisuals) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, theme.danger);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.dangerHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.dangerActive);
            if (ToolbarIconButton(ICON_FA_BACKWARD_STEP, "Return to Start")) ReturnToStart();
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            const bool highlightUiOverlay = g_showUiOverlay;
            if (highlightUiOverlay)
                ImGui::PushStyleColor(ImGuiCol_Button, theme.toggleOn);
            const bool overlayToggled = ToolbarIconButton(
                highlightUiOverlay ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                highlightUiOverlay ? "Hide UI Overlay" : "Show UI Overlay");
            if (overlayToggled)
                g_showUiOverlay = !g_showUiOverlay;
            if (highlightUiOverlay)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            const bool highlightGrid = Grid_IsVisible();
            if (highlightGrid)
                ImGui::PushStyleColor(ImGuiCol_Button, theme.toggleOn);
            const bool gridToggled = ToolbarIconButton(
                highlightGrid ? ICON_FA_BORDER_ALL : ICON_FA_BORDER_NONE,
                highlightGrid ? "Hide Grid" : "Show Grid");
            if (gridToggled)
                Grid_SetVisible(!highlightGrid);
            if (highlightGrid)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ToolbarIconToggle(ICON_FA_CUBE, g_editMode == EditorEditMode::Scene3D,
                    "Scene Edit Mode"))
                EngineGUI_SetEditMode(EditorEditMode::Scene3D);
            ImGui::SameLine();
            if (ToolbarIconToggle(ICON_FA_LAYER_GROUP, g_editMode == EditorEditMode::UI,
                    "UI Edit Mode"))
                EngineGUI_SetEditMode(EditorEditMode::UI);

            ImGui::SameLine();
            bool view2D = DebugCamera_Is2DView();
            ImGui::BeginDisabled(playVisuals);
            if (ImGui::Checkbox("2D View", &view2D))
                DebugCamera_Set2DView(view2D);
            ImGui::EndDisabled();
            ImGui::SameLine();
            TransformGizmo::DrawToolbarControls();

            ImGui::SameLine();
            // Camera selector
            {
                std::vector<std::string> camNames;
                std::function<void(GameObject*)> collectCams = [&](GameObject* node) {
                    if (!node || !node->IsActive()) return;
                    if (auto* camera = node->GetComponent<CameraComponent>();
                        camera && camera->enabled && node->GetComponent<engine::Transform>())
                        camNames.push_back(node->GetName());
                    for (const auto& child : node->GetChildren())
                        collectCams(child.get());
                };
                for (const auto& node : g_sceneNodes)
                    collectCams(node.get());

                if (g_activeCameraNode.empty() && camNames.size() == 1)
                    g_activeCameraNode = camNames[0];

                const bool validSelection = g_cameraMode == CameraMode::SceneCamera
                    && std::count(camNames.begin(),camNames.end(),g_activeCameraNode)==1;
                const std::string currentLabel = validSelection ? g_activeCameraNode
                    : camNames.empty() ? "No enabled game camera" : "Select game camera";

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Camera:");
                ImGui::SameLine();
                const float comboPad =
                    ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f;
                float comboW = ImGui::CalcTextSize(currentLabel.c_str()).x + comboPad;
                comboW = ImMax(comboW,
                    ImGui::CalcTextSize("No enabled game camera").x + comboPad);
                comboW = ImMax(comboW,
                    ImGui::CalcTextSize("Select game camera").x + comboPad);
                ImGui::SetNextItemWidth(IM_TRUNC(comboW + 0.5f));
                if (ImGui::BeginCombo("##Camera", currentLabel.c_str())) {
                    for (const auto& name : camNames) {
                        bool sel = (g_cameraMode == CameraMode::SceneCamera && g_activeCameraNode == name);
                        if (ImGui::Selectable(name.c_str(), sel)) {
                            g_cameraMode = CameraMode::SceneCamera;
                            g_activeCameraNode = name;
                            EngineGUI_MarkSceneDirty();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        ImGui::PopClipRect();
        ImGui::SetCursorPos(ImVec2(IM_TRUNC(lightsX + 0.5f), IM_TRUNC(lightsY + 0.5f)));
        const bool projectOpen = ProjectManager::Instance().IsOpen();
        DrawReloadLight("##SceneReloadLight", "Scene JSON",
            GetReloadLightState(projectOpen && !SceneLoader_GetCurrentScenePath().empty(), false,
                sceneLoadBusy || g_sceneReloadConflict.has_value() || g_sceneFileMonitor.HasPendingChange(), g_sceneReloadLight),
            g_sceneReloadLight, g_sceneReloadStatus);
        ImGui::SameLine(0.0f, lightGap);
        DrawReloadLight("##ContentReloadLight", "Content",
            GetReloadLightState(projectOpen, false, g_contentMonitor.Pending() || !g_contentChanges.empty() || g_contentSceneReload,
                g_contentReloadLight), g_contentReloadLight, g_contentReloadStatus);
        ImGui::SameLine(0.0f, lightGap);
        DrawReloadLight("##ScriptReloadLight", "Script DLL",
            GetReloadLightState(projectOpen, false, ScriptFactory::Instance().PendingReloadCount() != 0, g_scriptReloadLight),
            g_scriptReloadLight, g_scriptReloadStatus);
    }
    ImGui::End();
    PopToolbarChromeStyle();
}
