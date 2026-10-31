// project_asset_ops.h
// Project asset create/rename/delete operations.

#pragma once

#include <string>
#include <vector>

class ProjectAssetOps {
public:
    static bool RenameAsset(const std::string& oldRel, const std::string& newRel, std::string& error);
    static bool DeleteAssetToRecycleBin(const std::string& relPath, std::string& error);
    static bool CreateFolder(const std::string& parentRel, const std::string& name, std::string& error);
    static bool CreateMinimalScene(const std::string& parentRel, const std::string& name, std::string& error);
    static bool IsCurrentSceneOpen(const std::string& relPath);
    static bool FolderContainsCurrentScene(const std::string& relPath);
    static std::vector<std::string> FindSceneReferences(const std::string& relPath);
    static bool ShowInExplorer(const std::string& relPath, bool selectItem, std::string& error);

    static std::string AssetPathFromRelative(const std::string& relPath);
    static std::string AbsolutePathFromRelative(const std::string& relPath);
    static bool IsScriptsPath(const std::string& relPath);
    static bool IsValidLeafName(const std::string& name, std::string& error);
};
