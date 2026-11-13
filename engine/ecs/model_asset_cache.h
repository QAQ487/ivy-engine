/**===========================================================================
 File   : model_asset_cache.h
 Brief  : Shared FBX model cache and model-asset directory (rom/model).
 ===========================================================================**/
#ifndef MODEL_ASSET_CACHE_H
#define MODEL_ASSET_CACHE_H

#include "../rendering/model/model.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

class ModelAssetCache {
public:
    struct ObjectDef {
        std::string  displayName;  // path relative to rom/model, no extension
        std::string  assetPath;    // full project-relative path (rom/model/...)
        std::string  lowerName;    // lowercase assetPath for cache key + lookup
        std::wstring modelPath;    // wide path for LoadFBX
    };

    struct ModelFolderNode {
        std::string name;
        std::string relativePath;  // from rom/model, "" = root
        std::vector<ModelFolderNode> children;
    };

    static ModelAssetCache& Instance();
    static const char* AssetRootPath() { return "rom/model"; }
    static const char* PrimitiveAssetRootPath() { return "rom/model/primitives"; }

    // Legacy colored boxes / yellow cone / wall — keep files for old scenes,
    // but hide them from the default mesh picker as duplicate primitive choices.
    static bool IsHiddenPrimitiveAlias(const std::string& assetPath);

    void Initialize();
    void Finalize();
    bool IsFinalized() const { return m_finalized; }

    void RefreshAssetDirectory();
    bool BeginContentReload();
    bool PrepareContentModel(const std::string& path);
    void EndContentReload(bool commit);
    const std::vector<ObjectDef>& GetObjectDefinitions() const;
    const ModelFolderNode& GetFolderTree() const { return m_folderTree; }
    std::vector<const ObjectDef*> GetObjectDefinitionsInFolder(
        const std::string& folderRelativePath) const;

    Model3D* GetModel(const std::string& modelNameOrPath);
    // True when a non-null model occupies the cache for this path/name.
    bool IsModelLoaded(const std::string& modelNameOrPath) const;

private:
    ModelAssetCache() = default;

    void RebuildFolderTree();
    static std::string FolderPathForDef(const ObjectDef& def);

    bool m_finalized = false;
    bool m_contentReload = false;
    std::map<std::string, std::unique_ptr<Model3D>> m_previousModels;
    ModelFolderNode m_folderTree;
    std::vector<ObjectDef> m_objectDefs;
    std::map<std::string, std::unique_ptr<Model3D>> m_modelCache;
};

#endif // MODEL_ASSET_CACHE_H
