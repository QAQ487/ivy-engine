// project_asset_index.cpp
// Indexes project assets for the editor browser.

#include "project_asset_index.h"

#include "engine/core/content_path.h"
#include "engine/core/utf8_path.h"
#include "engine/ecs/model_asset_cache.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <locale>

namespace fs = std::filesystem;

namespace {
    const fs::path kProjectRoot = L"rom";
    const fs::path kScriptRoot = fs::path("game") / "scripts";
    const char* kScriptsFolder = "Scripts";

    std::string ToLowerString(const std::string& value) {
        std::string result = value;
        const std::locale loc = std::locale::classic();
        std::transform(result.begin(), result.end(), result.begin(),
            [loc](unsigned char c) {
                return static_cast<char>(std::tolower(c, loc));
            });
        return result;
    }

    bool StartsWithPath(const std::string& path, const char* prefix) {
        const std::string normalized = ToLowerString(path);
        const std::string prefixLower = ToLowerString(prefix);
        return normalized.size() >= prefixLower.size() &&
            normalized.compare(0, prefixLower.size(), prefixLower) == 0 &&
            (normalized.size() == prefixLower.size() ||
                normalized[prefixLower.size()] == '/');
    }

    bool ShouldSkipIndexedPath(const std::string& relativePath) {
        if (StartsWithPath(relativePath, "shader"))
            return true;

        if (StartsWithPath(relativePath, "scene")) {
            const std::string leaf = ToLowerString(PathToUtf8(PathFromUtf8(relativePath).filename()));
            if (leaf.size() >= 13 && leaf.compare(0, 13, "default_scene") == 0)
                return true;
        }
        return false;
    }

    ProjectAssetIndex::ProjectFolderNode* FindOrCreateFolderChild(
        ProjectAssetIndex::ProjectFolderNode& parent,
        const std::string& segment,
        const std::string& relativePath)
    {
        for (auto& child : parent.children) {
            if (child.name == segment)
                return &child;
        }
        parent.children.push_back({ segment, relativePath, {} });
        return &parent.children.back();
    }

    void AddFolderPath(ProjectAssetIndex::ProjectFolderNode& root, const std::string& relativePath) {
        ProjectAssetIndex::ProjectFolderNode* current = &root;
        std::string pathSoFar;
        size_t start = 0;
        while (start < relativePath.size()) {
            const size_t slash = relativePath.find('/', start);
            const std::string segment = (slash == std::string::npos)
                ? relativePath.substr(start)
                : relativePath.substr(start, slash - start);
            if (!segment.empty()) {
                pathSoFar = pathSoFar.empty() ? segment : pathSoFar + "/" + segment;
                current = FindOrCreateFolderChild(*current, segment, pathSoFar);
            }
            if (slash == std::string::npos) break;
            start = slash + 1;
        }
    }

    void SortFolderTree(ProjectAssetIndex::ProjectFolderNode& node) {
        std::sort(node.children.begin(), node.children.end(),
            [](const ProjectAssetIndex::ProjectFolderNode& a,
               const ProjectAssetIndex::ProjectFolderNode& b) {
                const std::string aSort = ToLowerString(a.name);
                const std::string bSort = ToLowerString(b.name);
                return aSort != bSort ? aSort < bSort : a.name < b.name;
            });
        for (auto& child : node.children)
            SortFolderTree(child);
    }

    const ProjectAssetIndex::ProjectFolderNode* FindFolderNode(
        const ProjectAssetIndex::ProjectFolderNode& node,
        const std::string& relativePath)
    {
        if (node.relativePath == relativePath)
            return &node;
        for (const auto& child : node.children) {
            if (const auto* found = FindFolderNode(child, relativePath))
                return found;
        }
        return nullptr;
    }
}

ProjectAssetIndex& ProjectAssetIndex::Instance() {
    static ProjectAssetIndex s_index;
    return s_index;
}

ProjectAssetIndex::Kind ProjectAssetIndex::ClassifyFile(const std::string& relativePath) {
    const fs::path path = PathFromUtf8(relativePath);
    const std::string ext = ToLowerString(PathToUtf8(path.extension()));
    if (ext == ".physmat")
        return Kind::PhysicsMaterial;
    if (ext == ".efk" || ext == ".efkefc")
        return Kind::Effect;
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb")
        return Kind::Model;
    if (ext == ".json") {
        if (StartsWithPath(relativePath, "prefabs"))
            return Kind::Prefab;
        if (StartsWithPath(relativePath, "scene"))
            return Kind::Scene;
        if (StartsWithPath(relativePath, "ui"))
            return Kind::UI;
        return Kind::Other;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga")
        return Kind::Texture;
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
        return Kind::Sound;
    if (ext == ".r16")
        return Kind::Heightmap;
    if (ext == ".cpp" && StartsWithPath(relativePath, kScriptsFolder))
        return Kind::Script;
    return Kind::Other;
}

void ProjectAssetIndex::RebuildFolderTree() {
    m_folderTree = { "rom", "", {} };

    std::error_code ec;
    if (!fs::exists(kProjectRoot, ec))
        return;

    for (fs::recursive_directory_iterator it(kProjectRoot, ec), end; it != end; ++it) {
        if (ec) break;
        const fs::directory_entry& entry = *it;
        if (!entry.is_directory(ec)) continue;

        // LexicalChildPath does not follow directory links, so the browser
        // path stays under the project rom/.
        const fs::path rel = content_path::LexicalChildPath(entry.path(), kProjectRoot);
        if (rel.empty()) continue;

        const std::string relStr = PathToUtf8(rel);
        if (StartsWithPath(relStr, "shader")) {
            it.disable_recursion_pending();
            continue;
        }
        if (ShouldSkipIndexedPath(relStr))
            continue;

        AddFolderPath(m_folderTree, relStr);
    }

    if (fs::exists(kScriptRoot, ec)) {
        AddFolderPath(m_folderTree, kScriptsFolder);
        for (fs::recursive_directory_iterator it(kScriptRoot, ec), end; it != end; ++it) {
            if (ec) break;
            const fs::directory_entry& entry = *it;
            if (!entry.is_directory(ec)) continue;

            const fs::path rel = content_path::LexicalChildPath(entry.path(), kScriptRoot);
            if (rel.empty()) continue;

            const std::string relStr = std::string(kScriptsFolder) + "/" + PathToUtf8(rel);
            AddFolderPath(m_folderTree, relStr);
        }
    }

    SortFolderTree(m_folderTree);
}

void ProjectAssetIndex::Refresh() {
    m_filesByFolder.clear();

    std::error_code ec;
    if (fs::exists(kProjectRoot, ec)) {
        for (fs::recursive_directory_iterator it(kProjectRoot, ec), end; it != end; ++it) {
            if (ec) break;
            const fs::directory_entry& entry = *it;

            const fs::path rel = content_path::LexicalChildPath(entry.path(), kProjectRoot);
            if (rel.empty()) continue;
            const std::string relStr = PathToUtf8(rel);

            if (entry.is_directory(ec) && StartsWithPath(relStr, "shader")) {
                it.disable_recursion_pending();
                continue;
            }
            if (!entry.is_regular_file(ec)) continue;
            if (ShouldSkipIndexedPath(relStr))
                continue;

            ProjectEntry fileEntry;
            fileEntry.name = PathToUtf8(entry.path().filename());
            fileEntry.relativePath = relStr;
            fileEntry.assetPath = PathToUtf8(kProjectRoot / rel);
            fileEntry.kind = ClassifyFile(relStr);

            std::string parentFolder;
            const size_t slash = relStr.find_last_of('/');
            if (slash != std::string::npos)
                parentFolder = relStr.substr(0, slash);

            m_filesByFolder[parentFolder].push_back(std::move(fileEntry));
        }
    }

    if (fs::exists(kScriptRoot, ec)) {
        for (fs::recursive_directory_iterator it(kScriptRoot, ec), end; it != end; ++it) {
            if (ec) break;
            const fs::directory_entry& entry = *it;
            if (!entry.is_regular_file(ec)) continue;
            if (ToLowerString(PathToUtf8(entry.path().extension())) != ".cpp") continue;

            const fs::path rel = content_path::LexicalChildPath(entry.path(), kScriptRoot);
            if (rel.empty()) continue;

            const std::string relStr = std::string(kScriptsFolder) + "/" + PathToUtf8(rel);
            ProjectEntry fileEntry;
            fileEntry.name = PathToUtf8(entry.path().filename());
            fileEntry.relativePath = relStr;
            fileEntry.assetPath = PathToUtf8(kScriptRoot / rel);
            fileEntry.kind = ClassifyFile(relStr);

            std::string parentFolder = kScriptsFolder;
            const size_t slash = relStr.find_last_of('/');
            if (slash != std::string::npos)
                parentFolder = relStr.substr(0, slash);

            m_filesByFolder[parentFolder].push_back(std::move(fileEntry));
        }
    }

    for (auto& pair : m_filesByFolder) {
        std::sort(pair.second.begin(), pair.second.end(),
            [](const ProjectEntry& a, const ProjectEntry& b) {
                const std::string aSort = ToLowerString(a.name);
                const std::string bSort = ToLowerString(b.name);
                return aSort != bSort ? aSort < bSort : a.name < b.name;
            });
    }

    RebuildFolderTree();
    ModelAssetCache::Instance().RefreshAssetDirectory();
}

std::vector<ProjectAssetIndex::ProjectEntry> ProjectAssetIndex::GetFolderContents(
    const std::string& folderRelativePath) const
{
    std::vector<ProjectEntry> contents;

    const ProjectFolderNode* folderNode = FindFolderNode(m_folderTree, folderRelativePath);
    if (folderNode) {
        for (const ProjectFolderNode& child : folderNode->children) {
            ProjectEntry folderEntry;
            folderEntry.kind = Kind::Folder;
            folderEntry.name = child.name;
            folderEntry.relativePath = child.relativePath;
            folderEntry.assetPath = std::string(AssetRootPath()) + "/" + child.relativePath;
            contents.push_back(std::move(folderEntry));
        }
    }

    const auto filesIt = m_filesByFolder.find(folderRelativePath);
    if (filesIt != m_filesByFolder.end()) {
        contents.insert(contents.end(), filesIt->second.begin(), filesIt->second.end());
    }

    std::sort(contents.begin(), contents.end(),
        [](const ProjectEntry& a, const ProjectEntry& b) {
            if (a.kind == Kind::Folder && b.kind != Kind::Folder) return true;
            if (a.kind != Kind::Folder && b.kind == Kind::Folder) return false;
            const std::string aSort = ToLowerString(a.name);
            const std::string bSort = ToLowerString(b.name);
            return aSort != bSort ? aSort < bSort : a.name < b.name;
        });

    return contents;
}

std::vector<ProjectAssetIndex::ProjectEntry> ProjectAssetIndex::GetFilesOfKind(Kind kind) const
{
    std::vector<ProjectEntry> result;
    for (const auto& pair : m_filesByFolder) {
        for (const auto& entry : pair.second) {
            if (entry.kind == kind)
                result.push_back(entry);
        }
    }
    return result;
}
