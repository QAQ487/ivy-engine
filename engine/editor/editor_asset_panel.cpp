// editor_asset_panel.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"

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
