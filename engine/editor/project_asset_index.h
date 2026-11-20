// project_asset_index.h
// Indexes project assets for the editor browser.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class ProjectAssetIndex {
public:
    enum class Kind {
        Folder,
        Model,
        Prefab,
        Scene,
        Texture,
        Effect,
        UI,
        Sound,
        Script,
        PhysicsMaterial,
        Heightmap,  // terrain .r16 (ADR 0011)
        Other
    };

    struct ProjectEntry {
        Kind kind = Kind::Other;
        std::string name;
        std::string relativePath;
        std::string assetPath;
    };

    struct ProjectFolderNode {
        std::string name;
        std::string relativePath;
        std::vector<ProjectFolderNode> children;
    };

    static ProjectAssetIndex& Instance();

    void Refresh();
    const ProjectFolderNode& GetFolderTree() const { return m_folderTree; }
    std::vector<ProjectEntry> GetFolderContents(const std::string& folderRelativePath) const;
    std::vector<ProjectEntry> GetFilesOfKind(Kind kind) const;

    static const char* AssetRootPath() { return "rom"; }

private:
    ProjectAssetIndex() = default;

    void RebuildFolderTree();
    static Kind ClassifyFile(const std::string& relativePath);

    ProjectFolderNode m_folderTree;
    std::unordered_map<std::string, std::vector<ProjectEntry>> m_filesByFolder;
};
