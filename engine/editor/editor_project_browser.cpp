#include "editor_project_browser.h"

#include "project_asset_index.h"
#include "project_asset_ops.h"
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
        ? fs::path(assetPath).stem().string()
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
        fs::path path(name);
        s_modal.originalExt = path.extension().string();
        FillNameBuffer(path.stem().string());
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
                    else if (entry->kind == ProjectAssetIndex::Kind::Model ||
                        entry->kind == ProjectAssetIndex::Kind::Prefab) {
                        GameObject* node = TryOpenOrUseAsset(*entry, nodes);
                        if (node)
                            spawned = node;
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

                if (entry->kind == ProjectAssetIndex::Kind::Folder)
                    DrawFolderContextMenu(entry->relativePath, entry->name);
                else
                    DrawAssetContextMenu(*entry, nodes, spawned);

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

    DrawBrowserModals(s_selectedFolder);
    return spawned;
}

} // namespace MapEditor
