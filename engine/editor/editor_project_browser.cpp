// editor_project_browser.cpp
// Editor project/asset browser panel.

#include "editor_project_browser.h"

#include "asset_browser_text.h"
#include "project_asset_index.h"
#include "project_asset_ops.h"
#include "project_manager.h"
#include "imgui/imgui.h"
#include "imgui/icons_font_awesome.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/collider_component.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/scene/scene_loader.h"
#include "engine/core/engine_gui.h"
#include "engine/core/utf8_path.h"
#include "engine/rendering/environment/skybox_asset_path.h"

#include "libraries/nlohmann/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace MapEditor {

namespace {

enum class BrowserModalOp {
    None,
    RenameAsset,
    DeleteAsset,
    RenameFolder,
    DeleteFolder,
    NewScene,
    NewFolder
};

struct BrowserModalState {
    BrowserModalOp op = BrowserModalOp::None;
    std::string relPath;
    std::string assetPath;
    std::string displayName;
    std::string originalExt;
    ProjectAssetIndex::Kind kind = ProjectAssetIndex::Kind::Other;
    char nameBuf[256]{};
    std::string error;
    std::vector<std::string> referencingScenes;
    bool acknowledgeRefs = false;
};

static BrowserModalState s_modal;

static std::unique_ptr<GameObject> CreateModelGameObject(
    const std::string& assetPath, const std::string& displayName)
{
    const std::string leafName = displayName.empty()
        ? PathToUtf8(PathFromUtf8(assetPath).stem())
        : displayName;
    auto node = std::make_unique<GameObject>(leafName);
    node->AddComponent<engine::Transform>();
    auto* vc = node->AddComponent<VisualComponent>();
    vc->modelPath = assetPath;
    node->AddComponent<ColliderComponent>();
    EngineGUI_FitNewCollider(node.get());
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
    case ProjectAssetIndex::Kind::Sound:            return ICON_FA_FILE_AUDIO;
    case ProjectAssetIndex::Kind::Script:           return ICON_FA_FILE_CODE;
    case ProjectAssetIndex::Kind::PhysicsMaterial:  return ICON_FA_FILE;
    default:                                        return ICON_FA_FILE;
    }
}

static bool EmitsAssetPath(ProjectAssetIndex::Kind kind) {
    return kind == ProjectAssetIndex::Kind::Model ||
        kind == ProjectAssetIndex::Kind::Texture ||
        kind == ProjectAssetIndex::Kind::Effect ||
        kind == ProjectAssetIndex::Kind::Sound ||
        kind == ProjectAssetIndex::Kind::Script ||
        kind == ProjectAssetIndex::Kind::PhysicsMaterial;
}

static bool KindHasOpenUse(ProjectAssetIndex::Kind kind) {
    return kind == ProjectAssetIndex::Kind::Model ||
        kind == ProjectAssetIndex::Kind::Prefab ||
        kind == ProjectAssetIndex::Kind::Scene;
}

static bool KindNeedsReferenceScan(ProjectAssetIndex::Kind kind) {
    return kind == ProjectAssetIndex::Kind::Model ||
        kind == ProjectAssetIndex::Kind::Prefab ||
        kind == ProjectAssetIndex::Kind::Script ||
        kind == ProjectAssetIndex::Kind::Texture ||
        kind == ProjectAssetIndex::Kind::Sound;
}

static std::string ParentRelativePath(const std::string& relPath) {
    const size_t slash = relPath.find_last_of('/');
    if (slash == std::string::npos)
        return {};
    return relPath.substr(0, slash);
}

static std::string JoinRelative(const std::string& parent, const std::string& leaf) {
    if (parent.empty())
        return leaf;
    return parent + "/" + leaf;
}

static void FillNameBuffer(const std::string& text) {
    strncpy_s(s_modal.nameBuf, sizeof(s_modal.nameBuf), text.c_str(), _TRUNCATE);
}

static void ResetModal() {
    s_modal = BrowserModalState{};
}

static void AdjustSelectionAfterRename(
    std::string& selectedFolder,
    const std::string& oldRel,
    const std::string& newRel)
{
    if (selectedFolder == oldRel) {
        selectedFolder = newRel;
        return;
    }
    const std::string prefix = oldRel + "/";
    if (selectedFolder.size() > prefix.size() &&
        selectedFolder.compare(0, prefix.size(), prefix) == 0) {
        selectedFolder = newRel + selectedFolder.substr(oldRel.size());
    }
}

static void AdjustSelectionAfterDelete(
    std::string& selectedFolder,
    const std::string& deletedRel)
{
    if (selectedFolder == deletedRel ||
        (selectedFolder.size() > deletedRel.size() &&
            selectedFolder.compare(0, deletedRel.size(), deletedRel) == 0 &&
            selectedFolder[deletedRel.size()] == '/')) {
        selectedFolder = ParentRelativePath(deletedRel);
    }
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

static bool TryLoadSceneAsset(const ProjectAssetIndex::ProjectEntry& entry) {
    return EngineGUI_LoadFromFile(entry.assetPath.c_str());
}

static GameObject* TryOpenOrUseAsset(
    const ProjectAssetIndex::ProjectEntry& entry,
    std::vector<std::unique_ptr<GameObject>>& nodes)
{
    if (entry.kind == ProjectAssetIndex::Kind::Model) {
        auto node = CreateModelGameObject(entry.assetPath, entry.name);
        GameObject* spawned = node.get();
        nodes.push_back(std::move(node));
        return spawned;
    }
    if (entry.kind == ProjectAssetIndex::Kind::Prefab) {
        auto node = PrefabSerializer::LoadFromFile(entry.assetPath);
        if (!node)
            return nullptr;
        GameObject* spawned = node.get();
        nodes.push_back(std::move(node));
        return spawned;
    }
    if (entry.kind == ProjectAssetIndex::Kind::Scene) {
        TryLoadSceneAsset(entry);
        return nullptr;
    }
    return nullptr;
}

static void OpenRenameModal(const std::string& relPath, const std::string& name, bool isFolder) {
    ResetModal();
    s_modal.op = isFolder ? BrowserModalOp::RenameFolder : BrowserModalOp::RenameAsset;
    s_modal.relPath = relPath;
    s_modal.displayName = name;
    if (isFolder) {
        s_modal.originalExt.clear();
        FillNameBuffer(name);
    }
    else {
        const fs::path path = PathFromUtf8(name);
        s_modal.originalExt = PathToUtf8(path.extension());
        FillNameBuffer(PathToUtf8(path.stem()));
    }
}

static void OpenDeleteModal(
    const std::string& relPath,
    const std::string& name,
    ProjectAssetIndex::Kind kind,
    bool isFolder)
{
    ResetModal();
    s_modal.op = isFolder ? BrowserModalOp::DeleteFolder : BrowserModalOp::DeleteAsset;
    s_modal.relPath = relPath;
    s_modal.displayName = name;
    s_modal.kind = kind;
    if (!isFolder && KindNeedsReferenceScan(kind))
        s_modal.referencingScenes = ProjectAssetOps::FindSceneReferences(relPath);
}

static void OpenNewFolderModal(const std::string& parentRel) {
    ResetModal();
    s_modal.op = BrowserModalOp::NewFolder;
    s_modal.relPath = parentRel;
    FillNameBuffer("");
}

static void OpenNewSceneModal(const std::string& parentRel) {
    ResetModal();
    s_modal.op = BrowserModalOp::NewScene;
    s_modal.relPath = parentRel;
    FillNameBuffer("");
}

static void DrawFolderContextMenu(const std::string& relativePath, const std::string& name) {
    if (!ImGui::BeginPopupContextItem("folder_ctx"))
        return;

    const bool isRoot = relativePath.empty();
    const bool isScripts = ProjectAssetOps::IsScriptsPath(relativePath);

    if (!isScripts) {
        if (ImGui::MenuItem("New Scene..."))
            OpenNewSceneModal(relativePath);
    }
    if (ImGui::MenuItem("New Folder..."))
        OpenNewFolderModal(relativePath);
    if (ImGui::MenuItem("Show in Explorer")) {
        std::string error;
        ProjectAssetOps::ShowInExplorer(relativePath, false, error);
    }
    if (ImGui::MenuItem("Rename...", nullptr, false, !isRoot))
        OpenRenameModal(relativePath, name, true);
    if (ImGui::MenuItem("Delete...", nullptr, false, !isRoot))
        OpenDeleteModal(relativePath, name, ProjectAssetIndex::Kind::Folder, true);

    ImGui::EndPopup();
}

static void DrawAssetContextMenu(
    const ProjectAssetIndex::ProjectEntry& entry,
    std::vector<std::unique_ptr<GameObject>>& nodes,
    GameObject*& spawned)
{
    if (!ImGui::BeginPopupContextItem("asset_ctx"))
        return;

    if (KindHasOpenUse(entry.kind)) {
        if (ImGui::MenuItem("Open / Use")) {
            GameObject* node = TryOpenOrUseAsset(entry, nodes);
            if (node)
                spawned = node;
        }
    }
    else if (entry.kind == ProjectAssetIndex::Kind::Script) {
        ImGui::MenuItem("Open / Use", nullptr, false, false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", entry.assetPath.c_str());
    }

    // Same .png/.dds filter and rom-path resolution as Rendering Settings drag-drop.
    if (skybox_asset::IsEligibleCubemapPath(entry.assetPath)) {
        if (ImGui::BeginMenu("Set as")) {
            if (ImGui::MenuItem("Skybox"))
                EngineGUI_ApplySkyboxCubemapPath(entry.assetPath);
            ImGui::EndMenu();
        }
    }

    if (ImGui::MenuItem("Show in Explorer")) {
        std::string error;
        ProjectAssetOps::ShowInExplorer(entry.relativePath, true, error);
    }
    if (ImGui::MenuItem("Rename..."))
        OpenRenameModal(entry.relativePath, entry.name, false);
    if (ImGui::MenuItem("Delete..."))
        OpenDeleteModal(entry.relativePath, entry.name, entry.kind, false);
    if (ImGui::MenuItem("Copy Path"))
        ImGui::SetClipboardText(entry.assetPath.c_str());

    ImGui::EndPopup();
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
        DrawFolderContextMenu(node.relativePath, node.name);

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
    DrawFolderContextMenu(node.relativePath, node.name);

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

static bool DrawCenteredModal(const char* id, bool* open) {
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    return ImGui::BeginPopupModal(id, open, ImGuiWindowFlags_AlwaysAutoResize);
}

static void CloseModalIfDismissed(BrowserModalOp op, bool stillOpen) {
    if (s_modal.op == op && !stillOpen)
        ResetModal();
}

static float MeasureImGuiText(std::string_view text, void* /*user*/) {
    return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
}

static void DrawMultilineCentered(
    ImDrawList* drawList,
    ImVec2 topLeft,
    float width,
    const std::string& text,
    ImU32 col)
{
    if (!drawList || text.empty())
        return;
    float y = topLeft.y;
    const float lineH = ImGui::GetTextLineHeight();
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string_view line = (end == std::string::npos)
            ? std::string_view(text).substr(start)
            : std::string_view(text).substr(start, end - start);
        const ImVec2 size = ImGui::CalcTextSize(line.data(), line.data() + line.size());
        const float x = topLeft.x + (width - size.x) * 0.5f;
        drawList->AddText(ImVec2(x, y), col, line.data(), line.data() + line.size());
        y += lineH;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
}

struct BrowserSelection {
    std::string name;
    std::string assetPath;
    std::string relativePath;
    bool isFolder = false;
};

static void SetBrowserSelection(
    BrowserSelection& selection,
    const ProjectAssetIndex::ProjectEntry& entry)
{
    selection.name = entry.name;
    selection.assetPath = entry.assetPath;
    selection.relativePath = entry.relativePath;
    selection.isFolder = (entry.kind == ProjectAssetIndex::Kind::Folder);
}

static void HandleAssetItemActivation(
    const ProjectAssetIndex::ProjectEntry& entry,
    std::string& selectedFolder,
    std::vector<std::unique_ptr<GameObject>>& nodes,
    GameObject*& spawned,
    bool activated,
    bool doubleClicked)
{
    if (activated) {
        if (entry.kind == ProjectAssetIndex::Kind::Folder) {
            selectedFolder = entry.relativePath;
        }
        else if (entry.kind == ProjectAssetIndex::Kind::Model ||
            entry.kind == ProjectAssetIndex::Kind::Prefab) {
            GameObject* node = TryOpenOrUseAsset(entry, nodes);
            if (node)
                spawned = node;
        }
        else if (entry.kind == ProjectAssetIndex::Kind::Scene) {
            SceneLoader_CopySceneFileName(
                PathToUtf8(PathFromUtf8(entry.relativePath).filename()));
        }
    }
    if (doubleClicked && entry.kind == ProjectAssetIndex::Kind::Scene)
        TryLoadSceneAsset(entry);
}

static void HandleAssetItemDragDrop(const ProjectAssetIndex::ProjectEntry& entry) {
    if (entry.kind == ProjectAssetIndex::Kind::Prefab &&
        ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(
            "ASSET_PREFAB", entry.assetPath.c_str(), entry.assetPath.size() + 1);
        ImGui::Text("Prefab: %s", entry.name.c_str());
        ImGui::EndDragDropSource();
    }
    if (EmitsAssetPath(entry.kind) &&
        ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(
            "ASSET_PATH", entry.assetPath.c_str(), entry.assetPath.size() + 1);
        ImGui::Text("%s", entry.name.c_str());
        ImGui::EndDragDropSource();
    }
}

static void DrawAssetItemContext(
    const ProjectAssetIndex::ProjectEntry& entry,
    std::vector<std::unique_ptr<GameObject>>& nodes,
    GameObject*& spawned)
{
    if (entry.kind == ProjectAssetIndex::Kind::Folder)
        DrawFolderContextMenu(entry.relativePath, entry.name);
    else
        DrawAssetContextMenu(entry, nodes, spawned);
}

static void DrawAssetHoverTooltip(const ProjectAssetIndex::ProjectEntry& entry) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        return;
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(entry.name.c_str());
    if (!entry.assetPath.empty())
        ImGui::TextDisabled("%s", entry.assetPath.c_str());
    else if (!entry.relativePath.empty())
        ImGui::TextDisabled("rom/%s", entry.relativePath.c_str());
    ImGui::EndTooltip();
}

static void DrawBrowserModals(std::string& selectedFolder) {
    bool renameAssetOpen = (s_modal.op == BrowserModalOp::RenameAsset);
    if (renameAssetOpen)
        ImGui::OpenPopup("Rename Asset");
    if (DrawCenteredModal("Rename Asset", renameAssetOpen ? &renameAssetOpen : nullptr)) {
        ImGui::Text("Rename Asset");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##RenameAssetName", s_modal.nameBuf, IM_ARRAYSIZE(s_modal.nameBuf));
        if (!s_modal.originalExt.empty())
            ImGui::TextDisabled("Extension %s is kept.", s_modal.originalExt.c_str());
        if (!s_modal.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_modal.error.c_str());
        ImGui::Separator();
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            const std::string newRel = JoinRelative(
                ParentRelativePath(s_modal.relPath),
                std::string(s_modal.nameBuf) + s_modal.originalExt);
            if (ProjectAssetOps::RenameAsset(s_modal.relPath, newRel, s_modal.error)) {
                ProjectAssetIndex::Instance().Refresh();
                ResetModal();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ResetModal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    CloseModalIfDismissed(BrowserModalOp::RenameAsset, renameAssetOpen);

    bool deleteAssetOpen = (s_modal.op == BrowserModalOp::DeleteAsset);
    if (deleteAssetOpen)
        ImGui::OpenPopup("Delete Asset");
    if (DrawCenteredModal("Delete Asset", deleteAssetOpen ? &deleteAssetOpen : nullptr)) {
        const bool openScene = ProjectAssetOps::IsCurrentSceneOpen(s_modal.relPath);
        ImGui::Text("Delete Asset");
        ImGui::Text("Delete '%s'?", s_modal.displayName.c_str());
        if (openScene) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "This scene is currently open");
        }
        else if (!s_modal.referencingScenes.empty()) {
            ImGui::TextWrapped("This asset is referenced by:");
            for (const std::string& scene : s_modal.referencingScenes)
                ImGui::BulletText("%s", scene.c_str());
            ImGui::Checkbox("I understand this asset is referenced", &s_modal.acknowledgeRefs);
        }
        if (!s_modal.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_modal.error.c_str());
        ImGui::Separator();
        const bool canDelete = !openScene &&
            (s_modal.referencingScenes.empty() || s_modal.acknowledgeRefs);
        if (!canDelete)
            ImGui::BeginDisabled();
        if (ImGui::Button("Delete", ImVec2(120, 0)) && canDelete) {
            if (ProjectAssetOps::DeleteAssetToRecycleBin(s_modal.relPath, s_modal.error)) {
                AdjustSelectionAfterDelete(selectedFolder, s_modal.relPath);
                ProjectAssetIndex::Instance().Refresh();
                ResetModal();
                ImGui::CloseCurrentPopup();
            }
        }
        if (!canDelete)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ResetModal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    CloseModalIfDismissed(BrowserModalOp::DeleteAsset, deleteAssetOpen);

    bool renameFolderOpen = (s_modal.op == BrowserModalOp::RenameFolder);
    if (renameFolderOpen)
        ImGui::OpenPopup("Rename Folder");
    if (DrawCenteredModal("Rename Folder", renameFolderOpen ? &renameFolderOpen : nullptr)) {
        ImGui::Text("Rename Folder");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##RenameFolderName", s_modal.nameBuf, IM_ARRAYSIZE(s_modal.nameBuf));
        if (!s_modal.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_modal.error.c_str());
        ImGui::Separator();
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            const std::string newRel = JoinRelative(
                ParentRelativePath(s_modal.relPath), s_modal.nameBuf);
            if (ProjectAssetOps::RenameAsset(s_modal.relPath, newRel, s_modal.error)) {
                AdjustSelectionAfterRename(selectedFolder, s_modal.relPath, newRel);
                ProjectAssetIndex::Instance().Refresh();
                ResetModal();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ResetModal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    CloseModalIfDismissed(BrowserModalOp::RenameFolder, renameFolderOpen);

    bool deleteFolderOpen = (s_modal.op == BrowserModalOp::DeleteFolder);
    if (deleteFolderOpen)
        ImGui::OpenPopup("Delete Folder");
    if (DrawCenteredModal("Delete Folder", deleteFolderOpen ? &deleteFolderOpen : nullptr)) {
        const bool containsOpen = ProjectAssetOps::FolderContainsCurrentScene(s_modal.relPath);
        ImGui::Text("Delete Folder");
        ImGui::Text("Delete '%s' and its contents?", s_modal.displayName.c_str());
        if (containsOpen) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                "This folder contains the currently open scene");
        }
        if (!s_modal.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_modal.error.c_str());
        ImGui::Separator();
        if (containsOpen)
            ImGui::BeginDisabled();
        if (ImGui::Button("Delete", ImVec2(120, 0)) && !containsOpen) {
            if (ProjectAssetOps::DeleteAssetToRecycleBin(s_modal.relPath, s_modal.error)) {
                AdjustSelectionAfterDelete(selectedFolder, s_modal.relPath);
                ProjectAssetIndex::Instance().Refresh();
                ResetModal();
                ImGui::CloseCurrentPopup();
            }
        }
        if (containsOpen)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ResetModal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    CloseModalIfDismissed(BrowserModalOp::DeleteFolder, deleteFolderOpen);

    bool newSceneOpen = (s_modal.op == BrowserModalOp::NewScene);
    if (newSceneOpen)
        ImGui::OpenPopup("New Scene");
    if (DrawCenteredModal("New Scene", newSceneOpen ? &newSceneOpen : nullptr)) {
        ImGui::Text("New Scene Name");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##NewSceneName", s_modal.nameBuf, IM_ARRAYSIZE(s_modal.nameBuf));
        if (!s_modal.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_modal.error.c_str());
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (ProjectAssetOps::CreateMinimalScene(s_modal.relPath, s_modal.nameBuf, s_modal.error)) {
                ProjectAssetIndex::Instance().Refresh();
                ResetModal();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ResetModal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    CloseModalIfDismissed(BrowserModalOp::NewScene, newSceneOpen);

    bool newFolderOpen = (s_modal.op == BrowserModalOp::NewFolder);
    if (newFolderOpen)
        ImGui::OpenPopup("New Folder");
    if (DrawCenteredModal("New Folder", newFolderOpen ? &newFolderOpen : nullptr)) {
        ImGui::Text("New Folder Name");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##NewFolderName", s_modal.nameBuf, IM_ARRAYSIZE(s_modal.nameBuf));
        if (!s_modal.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_modal.error.c_str());
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (ProjectAssetOps::CreateFolder(s_modal.relPath, s_modal.nameBuf, s_modal.error)) {
                ProjectAssetIndex::Instance().Refresh();
                ResetModal();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ResetModal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    CloseModalIfDismissed(BrowserModalOp::NewFolder, newFolderOpen);
}

} // namespace

GameObject* ProjectBrowserPanel(std::vector<std::unique_ptr<GameObject>>& nodes) {
    static ImGuiTextFilter s_filter;
    static std::string s_selectedFolder;
    static bool s_initialized = false;
    static bool s_listView = false;
    static int s_tileSize = 100;
    static std::string s_prefsProjectRoot;
    static BrowserSelection s_selection;

    ProjectAssetIndex& index = ProjectAssetIndex::Instance();
    if (!s_initialized) {
        index.Refresh();
        s_initialized = true;
    }

    // Sync view prefs when the open project changes (project.json, then ui_prefs overlay).
    ProjectManager& project = ProjectManager::Instance();
    const std::string& root = project.IsOpen() ? project.GetRootPath() : std::string{};
    if (s_prefsProjectRoot != root) {
        s_prefsProjectRoot = root;
        if (project.IsOpen()) {
            s_listView = project.GetAssetBrowserListView();
            s_tileSize = ProjectManager::ClampAssetBrowserTileSize(
                project.GetAssetBrowserTileSize());
            // ui_prefs.json is written immediately on toggle/slider; prefer it when present.
            try {
                const fs::path prefsPath =
                    fs::path(project.GetRootPath()) / "rom" / "editor" / "ui_prefs.json";
                std::ifstream prefsFile(prefsPath);
                if (prefsFile.is_open()) {
                    nlohmann::json j;
                    prefsFile >> j;
                    if (j.contains("assetBrowserListView"))
                        s_listView = j.value("assetBrowserListView", s_listView);
                    if (j.contains("assetBrowserTileSize")) {
                        s_tileSize = ProjectManager::ClampAssetBrowserTileSize(
                            j.value("assetBrowserTileSize", s_tileSize));
                    }
                    project.SetAssetBrowserListView(s_listView);
                    project.SetAssetBrowserTileSize(s_tileSize);
                }
            }
            catch (...) {
            }
        }
        else {
            s_listView = false;
            s_tileSize = 100;
        }
        s_selection = {};
    }

    auto persistBrowserPrefs = [&]() {
        if (!project.IsOpen())
            return;
        project.SetAssetBrowserListView(s_listView);
        project.SetAssetBrowserTileSize(s_tileSize);
        // Immediate write alongside zoom/FPS prefs (merge into ui_prefs.json).
        try {
            const fs::path dest =
                fs::path(project.GetRootPath()) / "rom" / "editor" / "ui_prefs.json";
            std::error_code ec;
            fs::create_directories(dest.parent_path(), ec);
            nlohmann::json j = nlohmann::json::object();
            {
                std::ifstream existing(dest);
                if (existing.is_open()) {
                    try {
                        existing >> j;
                    }
                    catch (...) {
                        j = nlohmann::json::object();
                    }
                }
            }
            if (!j.is_object())
                j = nlohmann::json::object();
            j["assetBrowserListView"] = s_listView;
            j["assetBrowserTileSize"] = s_tileSize;
            std::ofstream file(dest, std::ios::trunc);
            if (file.is_open())
                file << j.dump(2);
        }
        catch (...) {
        }
    };

    if (ImGui::Button("Refresh##ProjectBrowser", ImVec2(90, 0))) {
        EngineGUI_RequestContentRefresh();
        index.Refresh();
    }
    ImGui::SameLine();
    DrawBreadcrumb(s_selectedFolder);
    ImGui::SameLine();
    s_filter.Draw("Search##ProjectBrowser", 180.0f);

    ImGui::SameLine();
    if (ImGui::Button(s_listView ? (ICON_FA_TABLE_CELLS " Grid##AssetView")
                                 : (ICON_FA_LIST " List##AssetView"))) {
        s_listView = !s_listView;
        persistBrowserPrefs();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", s_listView ? "Switch to grid view" : "Switch to list view");

    if (!s_listView) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("Tile##AssetTileSize", &s_tileSize, 64, 160)) {
            s_tileSize = ProjectManager::ClampAssetBrowserTileSize(s_tileSize);
            if (project.IsOpen()) {
                project.SetAssetBrowserListView(s_listView);
                project.SetAssetBrowserTileSize(s_tileSize);
            }
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            persistBrowserPrefs();
    }

    ImGui::Separator();

    const float folderWidth = 200.0f;
    const float statusHeight = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
    ImGui::BeginChild("ProjectFolderTree", ImVec2(folderWidth, -statusHeight), true);
    DrawFolderTreeNode(index.GetFolderTree(), s_selectedFolder);
    ImGui::EndChild();

    ImGui::SameLine();

    GameObject* spawned = nullptr;
    ImGui::BeginChild("ProjectFileGrid", ImVec2(0, -statusHeight), true);
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
        else if (s_listView) {
            const float availW = ImGui::GetContentRegionAvail().x;
            const float iconW = ImGui::CalcTextSize(ICON_FA_FILE).x + ImGui::GetStyle().ItemSpacing.x;
            for (const ProjectAssetIndex::ProjectEntry* entry : visible) {
                ImGui::PushID(entry->assetPath.empty()
                    ? entry->relativePath.c_str()
                    : entry->assetPath.c_str());

                const bool selected = (s_selection.assetPath == entry->assetPath &&
                    s_selection.relativePath == entry->relativePath);
                const float nameBudget = (std::max)(0.0f, availW - iconW - 8.0f);
                const std::string clippedName = asset_browser_text::Ellipsize(
                    entry->name, nameBudget, MeasureImGuiText, nullptr);
                const std::string label = std::string(EntryKindIcon(entry->kind)) + " " +
                    clippedName + "##row";

                const bool activated = ImGui::Selectable(
                    label.c_str(),
                    selected,
                    ImGuiSelectableFlags_AllowDoubleClick);
                const bool doubleClicked = ImGui::IsItemHovered() &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                if (ImGui::IsItemClicked())
                    SetBrowserSelection(s_selection, *entry);

                HandleAssetItemActivation(
                    *entry, s_selectedFolder, nodes, spawned, activated, doubleClicked);
                DrawAssetItemContext(*entry, nodes, spawned);
                HandleAssetItemDragDrop(*entry);
                DrawAssetHoverTooltip(*entry);

                ImGui::PopID();
            }
        }
        else {
            const float tileW = static_cast<float>(s_tileSize);
            const float lineH = ImGui::GetTextLineHeight();
            const float textPad = 4.0f;
            const float textAreaH = lineH * 2.0f + textPad;
            const float iconAreaH = tileW * 0.45f;
            const float tileH = iconAreaH + textAreaH + 6.0f;
            const float nameWidth = tileW - 8.0f;

            const float visX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            int drawn = 0;
            for (const ProjectAssetIndex::ProjectEntry* entry : visible) {
                ImGui::PushID(entry->assetPath.empty()
                    ? entry->relativePath.c_str()
                    : entry->assetPath.c_str());

                const bool selected = (s_selection.assetPath == entry->assetPath &&
                    s_selection.relativePath == entry->relativePath);
                const ImVec2 tileSize(tileW, tileH);
                ImGui::InvisibleButton("##tile", tileSize);
                const bool hovered = ImGui::IsItemHovered();
                const bool activated = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                const bool doubleClicked = hovered &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                if (activated)
                    SetBrowserSelection(s_selection, *entry);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 rmin = ImGui::GetItemRectMin();
                const ImVec2 rmax = ImGui::GetItemRectMax();
                const ImU32 bg = selected
                    ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                    : (hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                               : ImGui::GetColorU32(ImGuiCol_FrameBg));
                dl->AddRectFilled(rmin, rmax, bg, ImGui::GetStyle().FrameRounding);
                if (selected || hovered)
                    dl->AddRect(rmin, rmax, ImGui::GetColorU32(ImGuiCol_Border),
                        ImGui::GetStyle().FrameRounding);

                const char* icon = EntryKindIcon(entry->kind);
                const ImVec2 iconSize = ImGui::CalcTextSize(icon);
                const ImVec2 iconPos(
                    rmin.x + (tileW - iconSize.x) * 0.5f,
                    rmin.y + (iconAreaH - iconSize.y) * 0.5f + 2.0f);
                dl->AddText(iconPos, ImGui::GetColorU32(ImGuiCol_Text), icon);

                const std::string wrapped = asset_browser_text::WrapEllipsize(
                    entry->name, nameWidth, 2, MeasureImGuiText, nullptr);
                DrawMultilineCentered(
                    dl,
                    ImVec2(rmin.x + 4.0f, rmin.y + iconAreaH),
                    nameWidth,
                    wrapped,
                    ImGui::GetColorU32(ImGuiCol_Text));

                HandleAssetItemActivation(
                    *entry, s_selectedFolder, nodes, spawned, activated, doubleClicked);
                DrawAssetItemContext(*entry, nodes, spawned);
                HandleAssetItemDragDrop(*entry);
                DrawAssetHoverTooltip(*entry);

                ++drawn;
                const float last_x2 = ImGui::GetItemRectMax().x;
                const float next_x2 = last_x2 + ImGui::GetStyle().ItemSpacing.x + tileW;
                if (drawn < static_cast<int>(visible.size()) && next_x2 < visX2)
                    ImGui::SameLine();
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    // Status line: selected asset full name + rom/-relative path.
    ImGui::BeginChild("ProjectBrowserStatus", ImVec2(0, 0), false);
    if (s_selection.name.empty()) {
        ImGui::TextDisabled("No asset selected");
    }
    else {
        const char* path = !s_selection.assetPath.empty()
            ? s_selection.assetPath.c_str()
            : s_selection.relativePath.c_str();
        ImGui::TextUnformatted(s_selection.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("— %s", path);
    }
    ImGui::EndChild();

    DrawBrowserModals(s_selectedFolder);
    return spawned;
}

} // namespace MapEditor
