#include "editor_project_browser.h"

#include "project_asset_index.h"
#include "imgui/imgui.h"
#include "imgui/icons_font_awesome.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/collider_component.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/scene/scene_loader.h"
#include "engine/core/engine_gui.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace MapEditor {

namespace {

static std::unique_ptr<GameObject> CreateModelGameObject(
    const std::string& assetPath, const std::string& displayName)
{
    const std::string leafName = displayName.empty()
        ? fs::path(assetPath).stem().string()
        : displayName;
    auto node = std::make_unique<GameObject>(leafName);
    node->AddComponent<engine::Transform>();
    auto* vc = node->AddComponent<VisualComponent>();
    vc->modelPath = assetPath;
    node->AddComponent<ColliderComponent>();
    return node;
}

static const char* EntryKindIcon(ProjectAssetIndex::Kind kind) {
    switch (kind) {
    case ProjectAssetIndex::Kind::Folder:  return ICON_FA_FOLDER;
    case ProjectAssetIndex::Kind::Model:   return ICON_FA_CUBE;
    case ProjectAssetIndex::Kind::Prefab:  return ICON_FA_BOXES_STACKED;
    case ProjectAssetIndex::Kind::Scene:   return ICON_FA_MAP;
    case ProjectAssetIndex::Kind::Texture: return ICON_FA_IMAGE;
    case ProjectAssetIndex::Kind::Effect:  return ICON_FA_FILE;
    case ProjectAssetIndex::Kind::UI:      return ICON_FA_BOXES_STACKED;
    case ProjectAssetIndex::Kind::Sound:   return ICON_FA_FILE_AUDIO;
    case ProjectAssetIndex::Kind::Script:  return ICON_FA_FILE_CODE;
    default:                               return ICON_FA_FILE;
    }
}

static bool EmitsAssetPath(ProjectAssetIndex::Kind kind) {
    return kind == ProjectAssetIndex::Kind::Model ||
        kind == ProjectAssetIndex::Kind::Texture ||
        kind == ProjectAssetIndex::Kind::Effect ||
        kind == ProjectAssetIndex::Kind::Sound ||
        kind == ProjectAssetIndex::Kind::Script;
}

static std::string FolderTreeLabel(
    const std::string& name,
    const std::string& stableId,
    ImGuiTreeNodeFlags flags,
    bool leafFolder)
{
    ImGuiID nodeId = ImGui::GetID(stableId.c_str());
    const int defaultOpen = (flags & ImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0;
    const bool isOpen = !leafFolder &&
        ImGui::GetStateStorage()->GetInt(nodeId, defaultOpen) != 0;
    const char* icon = (isOpen && !leafFolder) ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
    return std::string(icon) + " " + name + stableId;
}

static bool PassesFilter(
    const ImGuiTextFilter& filter,
    const ProjectAssetIndex::ProjectEntry& entry)
{
    return filter.PassFilter(entry.name.c_str()) ||
        filter.PassFilter(entry.relativePath.c_str()) ||
        filter.PassFilter(entry.assetPath.c_str());
}

static void DrawFolderTreeNode(
    const ProjectAssetIndex::ProjectFolderNode& node,
    std::string& selectedFolder)
{
    if (!node.relativePath.empty()) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedFolder == node.relativePath)
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool hasChildren = !node.children.empty();
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const std::string stableId = std::string("###") + node.relativePath;
        const std::string label = FolderTreeLabel(
            node.name, stableId, flags, !hasChildren);
        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (ImGui::IsItemClicked())
            selectedFolder = node.relativePath;

        if (hasChildren && open) {
            for (const auto& child : node.children)
                DrawFolderTreeNode(child, selectedFolder);
            ImGui::TreePop();
        }
        return;
    }

    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selectedFolder.empty())
        rootFlags |= ImGuiTreeNodeFlags_Selected;

    const std::string rootStableId = "###rom_root";
    const std::string rootLabel = FolderTreeLabel(
        node.name, rootStableId, rootFlags, false);
    const bool rootOpen = ImGui::TreeNodeEx(rootLabel.c_str(), rootFlags);
    if (ImGui::IsItemClicked())
        selectedFolder.clear();

    if (rootOpen) {
        for (const auto& child : node.children)
            DrawFolderTreeNode(child, selectedFolder);
        ImGui::TreePop();
    }
}

static void DrawBreadcrumb(std::string& selectedFolder) {
    if (ImGui::SmallButton("rom"))
        selectedFolder.clear();
    if (!selectedFolder.empty()) {
        std::string pathSoFar;
        size_t start = 0;
        while (start < selectedFolder.size()) {
            const size_t slash = selectedFolder.find('/', start);
            const std::string segment = (slash == std::string::npos)
                ? selectedFolder.substr(start)
                : selectedFolder.substr(start, slash - start);
            pathSoFar = pathSoFar.empty() ? segment : pathSoFar + "/" + segment;

            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
            ImGui::PushID(pathSoFar.c_str());
            if (ImGui::SmallButton(segment.c_str()))
                selectedFolder = pathSoFar;
            ImGui::PopID();

            if (slash == std::string::npos) break;
            start = slash + 1;
        }
    }
}

static bool TryLoadSceneAsset(const ProjectAssetIndex::ProjectEntry& entry) {
    return EngineGUI_LoadFromFile(entry.assetPath.c_str());
}

} // namespace

GameObject* ProjectBrowserPanel(std::vector<std::unique_ptr<GameObject>>& nodes) {
    static ImGuiTextFilter s_filter;
    static std::string s_selectedFolder;
    static bool s_initialized = false;

    ProjectAssetIndex& index = ProjectAssetIndex::Instance();
    if (!s_initialized) {
        index.Refresh();
        s_initialized = true;
    }

    if (ImGui::Button("Refresh##ProjectBrowser", ImVec2(90, 0)))
        index.Refresh();
    ImGui::SameLine();
    DrawBreadcrumb(s_selectedFolder);
    ImGui::SameLine();
    s_filter.Draw("Search##ProjectBrowser", 180.0f);
    ImGui::Separator();

    const float folderWidth = 200.0f;
    ImGui::BeginChild("ProjectFolderTree", ImVec2(folderWidth, 0), true);
    DrawFolderTreeNode(index.GetFolderTree(), s_selectedFolder);
    ImGui::EndChild();

    ImGui::SameLine();

    GameObject* spawned = nullptr;
    ImGui::BeginChild("ProjectFileGrid", ImVec2(0, 0), true);
    const std::vector<ProjectAssetIndex::ProjectEntry> contents =
        index.GetFolderContents(s_selectedFolder);

    if (contents.empty()) {
        ImGui::TextDisabled("Folder is empty");
    }
    else {
        std::vector<const ProjectAssetIndex::ProjectEntry*> visible;
        visible.reserve(contents.size());
        for (const ProjectAssetIndex::ProjectEntry& entry : contents) {
            if (PassesFilter(s_filter, entry))
                visible.push_back(&entry);
        }

        if (visible.empty()) {
            ImGui::TextDisabled("No matching assets");
        }
        else {
            float visX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            int drawn = 0;
            for (const ProjectAssetIndex::ProjectEntry* entry : visible) {
                ImGui::PushID(entry->assetPath.c_str());

                std::string label = std::string(EntryKindIcon(entry->kind)) + "\n" + entry->name;
                if (ImGui::Button(label.c_str(), ImVec2(100, 70))) {
                    if (entry->kind == ProjectAssetIndex::Kind::Folder) {
                        s_selectedFolder = entry->relativePath;
                    }
                    else if (entry->kind == ProjectAssetIndex::Kind::Model) {
                        auto node = CreateModelGameObject(entry->assetPath, entry->name);
                        spawned = node.get();
                        nodes.push_back(std::move(node));
                    }
                    else if (entry->kind == ProjectAssetIndex::Kind::Prefab) {
                        auto node = PrefabSerializer::LoadFromFile(entry->assetPath);
                        if (node) {
                            spawned = node.get();
                            nodes.push_back(std::move(node));
                        }
                    }
                    else if (entry->kind == ProjectAssetIndex::Kind::Scene) {
                        SceneLoader_CopySceneFileName(
                            fs::path(entry->relativePath).filename().string());
                    }
                }

                if (entry->kind == ProjectAssetIndex::Kind::Scene &&
                    ImGui::IsItemHovered() &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    TryLoadSceneAsset(*entry);
                }

                if (entry->kind == ProjectAssetIndex::Kind::Prefab &&
                    ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload(
                        "ASSET_PREFAB", entry->assetPath.c_str(), entry->assetPath.size() + 1);
                    ImGui::Text("Prefab: %s", entry->name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (EmitsAssetPath(entry->kind) &&
                    ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload(
                        "ASSET_PATH", entry->assetPath.c_str(), entry->assetPath.size() + 1);
                    ImGui::Text("%s", entry->name.c_str());
                    ImGui::EndDragDropSource();
                }

                ++drawn;
                float last_x2 = ImGui::GetItemRectMax().x;
                float next_x2 = last_x2 + ImGui::GetStyle().ItemSpacing.x + 100;
                if (drawn < static_cast<int>(visible.size()) && next_x2 < visX2)
                    ImGui::SameLine();
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
    return spawned;
}

} // namespace MapEditor
