/*****************************************************
 * File: engine/editor_prefab_panel.cpp
 * Project: d3d11_engine
 * Author: Terry
 * Created: 2026-03-12
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   engine::GameObject → ::GameObject (alias, no logic change).
 *****************************************************/
#include "editor_prefab_panel.h"

#include "imgui/imgui.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/ecs/game_object.h"
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

namespace MapEditor {

GameObject* PrefabPanel(
    const std::string& prefabFolder,
    std::vector<std::unique_ptr<GameObject>>& nodes)
{
    static std::vector<std::string> s_paths;
    static std::string              s_lastFolder;

    auto Rescan = [&]() {
        s_lastFolder = prefabFolder;
        s_paths.clear();
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(prefabFolder, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                s_paths.push_back(entry.path().string());
        }
    };

    if (s_lastFolder != prefabFolder) Rescan();

    if (ImGui::Button("Refresh##PrefabPanel", ImVec2(130, 0))) Rescan();
    ImGui::Separator();

    if (s_paths.empty()) {
        ImGui::TextDisabled("No prefabs found in: %s", prefabFolder.c_str());
        return nullptr;
    }

    GameObject* spawned = nullptr;

    ImGui::BeginChild("PrefabPanelGrid");
    float visX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    for (int i = 0; i < static_cast<int>(s_paths.size()); i++) {
        const std::string& path = s_paths[i];
        std::string stem = fs::path(path).stem().string();
        ImGui::PushID(i);

        if (ImGui::Button(stem.c_str(), ImVec2(120, 80))) {
            auto node = PrefabSerializer::LoadFromFile(path);
            if (node) {
                spawned = node.get();
                nodes.push_back(std::move(node));
            }
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("ASSET_PREFAB",
                path.c_str(), path.size() + 1);
            ImGui::Text("Prefab: %s", stem.c_str());
            ImGui::EndDragDropSource();
        }

        float last_x2 = ImGui::GetItemRectMax().x;
        float next_x2 = last_x2 + ImGui::GetStyle().ItemSpacing.x + 120;
        if (i + 1 < static_cast<int>(s_paths.size()) && next_x2 < visX2)
            ImGui::SameLine();

        ImGui::PopID();
    }
    ImGui::EndChild();

    return spawned;
}

} // namespace MapEditor
