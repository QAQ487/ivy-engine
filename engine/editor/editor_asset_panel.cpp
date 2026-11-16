// editor_asset_panel.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_internal.h"

#include "engine/core/engine_gui.h"
#include "engine/ecs/game_object.h"
#include "engine/editor/editor_project_browser.h"
#include "engine/editor/editor_undo.h"
#include "engine/scene/scene_loader.h"
#include "imgui/imgui.h"

#include <memory>

void DrawEditorAssetPanel(bool playVisuals, bool sceneLoadBusy, float screenW, float screenH, float bottomHeight)
{
    // --- Asset Browser (bottom) ---
    ImGui::SetNextWindowPos(ImVec2(0, screenH - bottomHeight));
    ImGui::SetNextWindowSize(ImVec2(screenW, bottomHeight));
    ImGui::Begin("Asset Browser", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (sceneLoadBusy || playVisuals)
            ImGui::BeginDisabled(true);
        if (GameObject* spawned = MapEditor::ProjectBrowserPanel(g_sceneNodes)) {
            if (!sceneLoadBusy && !playVisuals) {
                g_selNode = spawned;
                g_selUiElement = nullptr;
                const int idx = [&]() {
                    for (int i = 0; i < (int)g_sceneNodes.size(); ++i)
                        if (g_sceneNodes[i].get() == spawned) return i;
                    return (int)g_sceneNodes.size() - 1;
                }();
                EditorUndo_PushCreate(spawned, nullptr, idx);
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        if (sceneLoadBusy || playVisuals)
            ImGui::EndDisabled();
    }
    ImGui::End();
}
