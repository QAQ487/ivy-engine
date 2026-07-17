#include "model_asset_cache.h"
#include "string_utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <locale>
#include <sstream>
#include <Windows.h>

namespace fs = std::filesystem;

namespace {
    const fs::path kModelAssetRoot = L"rom/model";

    std::string ToLowerString(const std::string& value) {
        std::string result = value;
        const std::locale loc = std::locale::classic();
        std::transform(result.begin(), result.end(), result.begin(),
            [loc](unsigned char c) { return static_cast<char>(std::tolower(c, loc)); });
        return result;
    }

    std::string NormalizeAssetKey(const std::string& path) {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return ToLowerString(normalized);
    }

    std::string StemKey(const std::string& path) {
        return ToLowerString(fs::path(path).stem().string());
    }

    const ModelAssetCache::ObjectDef* FindObjectDef(
        const std::vector<ModelAssetCache::ObjectDef>& defs,
        const std::string& modelName)
    {
        const std::string key = NormalizeAssetKey(modelName);
        for (const ModelAssetCache::ObjectDef& def : defs) {
            if (def.lowerName == key)
                return &def;
        }

        if (key.find('/') == std::string::npos) {
            const std::string stemKey = StemKey(modelName);
            const ModelAssetCache::ObjectDef* stemMatch = nullptr;
            const ModelAssetCache::ObjectDef* objectStemMatch = nullptr;
            for (const ModelAssetCache::ObjectDef& def : defs) {
                if (StemKey(def.displayName) != stemKey)
                    continue;
                stemMatch = &def;
                if (def.displayName.rfind("object/", 0) == 0)
                    objectStemMatch = &def;
            }
            if (objectStemMatch)
                return objectStemMatch;
            return stemMatch;
        }

        return nullptr;
    }

    std::wstring ResolveModelWidePath(
        const std::string& modelName,
        const ModelAssetCache::ObjectDef* def)
    {
        if (def)
            return def->modelPath;

        std::wstring wpath = ToWString(modelName);
        if (!wpath.empty() && fs::exists(wpath))
            return wpath;

        if (modelName.find('/') == std::string::npos) {
            const fs::path legacyPath =
                kModelAssetRoot / "object" / (modelName + ".fbx");
            if (fs::exists(legacyPath))
                return legacyPath.wstring();
        }

        return wpath;
    }

    ModelAssetCache::ModelFolderNode* FindOrCreateFolderChild(
        ModelAssetCache::ModelFolderNode& parent,
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

    void SortFolderTree(ModelAssetCache::ModelFolderNode& node) {
        std::sort(node.children.begin(), node.children.end(),
            [](const ModelAssetCache::ModelFolderNode& a,
               const ModelAssetCache::ModelFolderNode& b) {
                return ToLowerString(a.name) != ToLowerString(b.name)
                    ? ToLowerString(a.name) < ToLowerString(b.name)
                    : a.name < b.name;
            });
        for (auto& child : node.children)
            SortFolderTree(child);
    }
}

ModelAssetCache& ModelAssetCache::Instance() {
    // Intentionally leaked: cached Model3D objects own D3D/FBX resources and the
    // process shutdown order is not guaranteed. Finalize() releases them while
    // the renderer is still alive; avoiding a static destructor prevents a
    // second late std::map teardown during CRT shutdown.
    static ModelAssetCache* s_cache = new ModelAssetCache();
    return *s_cache;
}

void ModelAssetCache::Initialize() {
    m_finalized = false;
    RefreshAssetDirectory();
}

void ModelAssetCache::Finalize() {
    if (m_finalized) return;
    // Do not clear m_modelCache here. Model3D owns D3D/FBX resources and has
    // nested std::map members; destroying the cache during app shutdown has
    // caused CRT std::_Tree crashes when exit ordering overlaps with graphics
    // teardown. The singleton is intentionally leaked, so the OS reclaims these
    // resources when the process exits.
    m_objectDefs.clear();
    m_finalized = true;
}

void ModelAssetCache::RefreshAssetDirectory() {
    m_objectDefs.clear();

    std::error_code ec;
    if (!fs::exists(kModelAssetRoot, ec)) return;

    for (const auto& entry : fs::recursive_directory_iterator(kModelAssetRoot, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        fs::path path = entry.path();
        if (ToLowerString(path.extension().string()) != ".fbx") continue;

        ObjectDef def;
        def.assetPath = path.generic_string();
        def.modelPath = path.wstring();
        def.lowerName = NormalizeAssetKey(def.assetPath);

        fs::path relativePath = fs::relative(path, kModelAssetRoot, ec);
        if (ec) {
            ec.clear();
            relativePath = path.filename();
        }
        relativePath.replace_extension();
        def.displayName = relativePath.generic_string();

        m_objectDefs.push_back(std::move(def));
    }

    std::sort(m_objectDefs.begin(), m_objectDefs.end(),
        [](const ObjectDef& a, const ObjectDef& b) {
            const std::string aSort = ToLowerString(a.displayName);
            const std::string bSort = ToLowerString(b.displayName);
            return aSort != bSort ? aSort < bSort : a.displayName < b.displayName;
        });

    RebuildFolderTree();
}

void ModelAssetCache::RebuildFolderTree() {
    m_folderTree = { "model", "", {} };

    std::error_code ec;
    if (!fs::exists(kModelAssetRoot, ec)) return;

    for (const auto& entry : fs::recursive_directory_iterator(kModelAssetRoot, ec)) {
        if (ec) break;
        if (!entry.is_directory(ec)) continue;

        fs::path rel = fs::relative(entry.path(), kModelAssetRoot, ec);
        if (ec || rel.empty()) continue;

        const std::string relStr = rel.generic_string();
        ModelFolderNode* current = &m_folderTree;
        std::string pathSoFar;
        size_t start = 0;
        while (start < relStr.size()) {
            const size_t slash = relStr.find('/', start);
            const std::string segment = (slash == std::string::npos)
                ? relStr.substr(start)
                : relStr.substr(start, slash - start);
            if (!segment.empty()) {
                pathSoFar = pathSoFar.empty() ? segment : pathSoFar + "/" + segment;
                current = FindOrCreateFolderChild(*current, segment, pathSoFar);
            }
            if (slash == std::string::npos) break;
            start = slash + 1;
        }
    }

    SortFolderTree(m_folderTree);
}

std::string ModelAssetCache::FolderPathForDef(const ObjectDef& def) {
    const size_t slash = def.displayName.find_last_of('/');
    return slash == std::string::npos ? std::string() : def.displayName.substr(0, slash);
}

const std::vector<ModelAssetCache::ObjectDef>& ModelAssetCache::GetObjectDefinitions() const {
    return m_objectDefs;
}

std::vector<const ModelAssetCache::ObjectDef*> ModelAssetCache::GetObjectDefinitionsInFolder(
    const std::string& folderRelativePath) const
{
    std::vector<const ObjectDef*> result;
    for (const ObjectDef& def : m_objectDefs) {
        if (FolderPathForDef(def) == folderRelativePath)
            result.push_back(&def);
    }
    return result;
}

Model3D* ModelAssetCache::GetModel(const std::string& modelName) {
    if (m_finalized) return nullptr;

    const ModelAssetCache::ObjectDef* def = FindObjectDef(m_objectDefs, modelName);
    const std::string cacheKey = def
        ? def->lowerName
        : NormalizeAssetKey(modelName);

    auto it = m_modelCache.find(cacheKey);
    if (it != m_modelCache.end()) return it->second.get();

    const std::wstring wpath = ResolveModelWidePath(modelName, def);
    if (wpath.empty()) {
        OutputDebugStringA(("[ModelAssetCache] Unknown model: " + modelName + "\n").c_str());
        return nullptr;
    }

    auto model = std::make_unique<Model3D>();
    if (!model->LoadFBX(wpath.c_str())) {
        OutputDebugStringA(("[ModelAssetCache] Failed to load: " + modelName + "\n").c_str());
        return nullptr;
    }

    Model3D* raw = model.get();
    m_modelCache.emplace(cacheKey, std::move(model));
    return raw;
}
