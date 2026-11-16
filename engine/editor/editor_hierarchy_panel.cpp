// editor_hierarchy_panel.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"

void DrawSceneNodeHierarchy(GameObject* node, GameObject* rootOwner) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node->GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (g_selNode == node)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool nodeOpen = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(node), flags, "%s", node->GetName().c_str());

    if (!g_isPlaying && ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        g_selNode = node;
        g_selUiElement = nullptr;
    }

    if (!g_isPlaying && ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Empty Child"))
            SpawnChildGameObject(node);
        if (ImGui::MenuItem("Duplicate"))
            SpawnDuplicate(node);
        ImGui::Separator();
        if (ImGui::MenuItem("Delete"))
            DeleteGameObject(node);
        ImGui::EndPopup();
    }

    // Drag source
    if (!g_isPlaying && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        GameObject* ptr = node;
        ImGui::SetDragDropPayload("HIERARCHY_GO", &ptr, sizeof(ptr));
        ImGui::Text("Move: %s", node->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target
    if (!g_isPlaying && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("HIERARCHY_GO")) {
            GameObject* dragged = *static_cast<GameObject**>(pl->Data);
            if (dragged && dragged != node) {
                GameObject* oldParent = dragged->GetParent();
                int oldIndex = -1;
                if (oldParent) {
                    const auto& ch = oldParent->GetChildren();
                    for (int i = 0; i < (int)ch.size(); ++i)
                        if (ch[i].get() == dragged) { oldIndex = i; break; }
                } else {
                    for (int i = 0; i < (int)g_sceneNodes.size(); ++i)
                        if (g_sceneNodes[i].get() == dragged) { oldIndex = i; break; }
                }
                GameObject* dragParent = dragged->GetParent();
                if (dragParent) {
                    dragParent->ReparentChild(dragged, *node);
                }
                else {
                    auto it = std::find_if(g_sceneNodes.begin(), g_sceneNodes.end(),
                        [dragged](const std::unique_ptr<GameObject>& p) { return p.get() == dragged; });
                    if (it != g_sceneNodes.end()) {
                        std::unique_ptr<GameObject> owned = std::move(*it);
                        g_sceneNodes.erase(it);
                        owned->SetParent(node);
                        node->AddChild(std::move(owned));
                    }
                }
                const int newIndex = (int)node->GetChildren().size() - 1;
                EditorUndo_PushReparent(dragged, oldParent, oldIndex, node, newIndex);
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
                if (g_selNode == dragged) g_selNode = nullptr;
            }
        }
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {
            std::string path(static_cast<const char*>(pl->Data));
            auto prefabNode = PrefabSerializer::LoadFromFile(path);
            if (prefabNode) {
                GameObject* raw = prefabNode.get();
                node->AddChild(std::move(prefabNode));
                const int idx = (int)node->GetChildren().size() - 1;
                EditorUndo_PushCreate(raw, node, idx);
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (nodeOpen && !node->GetChildren().empty()) {
        for (auto& child : node->GetChildren())
            DrawSceneNodeHierarchy(child.get(), rootOwner);
        ImGui::TreePop();
    }
}


void DrawEditorHierarchyPanel(bool playVisuals, bool* open)
{
    if (open && !*open)
        return;

    // --- Scene Hierarchy (dockable) ---
    if (!ImGui::Begin(kEditorHierarchyWindowTitle, open)) {
        ImGui::End();
        return;
    }
    {
        if (playVisuals)
            ImGui::BeginDisabled(true);

        if (ImGui::CollapsingHeader("Game Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& rootNode : g_sceneNodes)
                if (rootNode) DrawSceneNodeHierarchy(rootNode.get(), rootNode.get());

            // Root drop zone
            ImGui::Selectable("##RootDropZone", false,
                ImGuiSelectableFlags_AllowOverlap, ImVec2(-1, 6));
            if (!playVisuals && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("HIERARCHY_GO")) {
                    GameObject* dragged = *static_cast<GameObject**>(pl->Data);
                    if (dragged && dragged->GetParent()) {
                        GameObject* oldParent = dragged->GetParent();
                        int oldIndex = -1;
                        auto& siblings = oldParent->GetChildren();
                        for (int i = 0; i < (int)siblings.size(); ++i)
                            if (siblings[i].get() == dragged) { oldIndex = i; break; }
                        auto it = std::find_if(siblings.begin(), siblings.end(),
                            [dragged](const std::unique_ptr<GameObject>& c) { return c.get() == dragged; });
                        if (it != siblings.end()) {
                            std::unique_ptr<GameObject> owned = std::move(*it);
                            siblings.erase(it);
                            owned->SetParent(nullptr);
                            g_sceneNodes.push_back(std::move(owned));
                            const int newIndex = (int)g_sceneNodes.size() - 1;
                            EditorUndo_PushReparent(dragged, oldParent, oldIndex, nullptr, newIndex);
                            EngineGUI_MarkSceneDirty();
                            SyncSceneRuntime();
                        }
                    }
                }
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {
                    std::string path(static_cast<const char*>(pl->Data));
                    auto prefabNode = PrefabSerializer::LoadFromFile(path);
                    if (prefabNode) {
                        GameObject* raw = prefabNode.get();
                        g_sceneNodes.push_back(std::move(prefabNode));
                        EditorUndo_PushCreate(raw, nullptr, (int)g_sceneNodes.size() - 1);
                        EngineGUI_MarkSceneDirty();
                        SyncSceneRuntime();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            g_sceneNodes.erase(
                std::remove_if(g_sceneNodes.begin(), g_sceneNodes.end(),
                    [](const std::unique_ptr<GameObject>& n) { return !n; }),
                g_sceneNodes.end());
        }

        if (ImGui::CollapsingHeader("Canvas Elements", ImGuiTreeNodeFlags_DefaultOpen)) {
            EngineGUIUI::DrawCanvasElementHierarchy(g_selUiElement, g_selNode);
        }

        if (playVisuals)
            ImGui::EndDisabled();
    }
    ImGui::End();
}
