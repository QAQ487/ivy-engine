// model_asset_cache.cpp
// ECS: model asset cache implementation.

#include "model_asset_cache.h"
#include "string_utils.h"
#include "engine/core/content_path.h"
#include "engine/core/utf8_path.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <locale>
#include <sstream>
#include <Windows.h>
#include <stdexcept>

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
        return ToLowerString(PathToUtf8(PathFromUtf8(path).stem()));
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
        if (ToLowerString(PathToUtf8(path.extension())) != ".fbx") continue;

        fs::path relativePath = content_path::LexicalChildPath(path, kModelAssetRoot);
        if (relativePath.empty())
            continue;
        const fs::path projectPath = kModelAssetRoot / relativePath;

        ObjectDef def;
        def.assetPath = PathToUtf8(projectPath);
        def.modelPath = projectPath.wstring();
        def.lowerName = NormalizeAssetKey(def.assetPath);
        relativePath.replace_extension();
        def.displayName = PathToUtf8(relativePath);

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

        const fs::path rel = content_path::LexicalChildPath(entry.path(), kModelAssetRoot);
        if (rel.empty()) continue;

        const std::string relStr = PathToUtf8(rel);
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

bool ModelAssetCache::IsHiddenPrimitiveAlias(const std::string& assetPath) {
    const std::string key = NormalizeAssetKey(assetPath);
    const bool underObjectFolder =
        key.rfind("object/", 0) == 0 ||
        key.find("/object/") != std::string::npos;
    if (!underObjectFolder)
        return false;

    const std::string stem = StemKey(key);
    return stem == "box_blue" || stem == "box_brown" || stem == "box_green" ||
        stem == "box_red" || stem == "box_sand" ||
        stem == "cone_yellow" || stem == "wall";
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

bool ModelAssetCache::IsModelLoaded(const std::string& modelNameOrPath) const {
    if (m_finalized || modelNameOrPath.empty()) return false;
    const ModelAssetCache::ObjectDef* def = FindObjectDef(m_objectDefs, modelNameOrPath);
    const std::string cacheKey = def
        ? def->lowerName
        : NormalizeAssetKey(modelNameOrPath);
    auto it = m_modelCache.find(cacheKey);
    return it != m_modelCache.end() && it->second != nullptr;
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
        // Cache the miss so callers (per-frame GetModel) do not re-resolve forever.
        m_modelCache.emplace(cacheKey, nullptr);
        return nullptr;
    }

    auto model = std::make_unique<Model3D>();
    if (!model->LoadFBX(wpath.c_str())) {
        OutputDebugStringA(("[ModelAssetCache] Failed to load: " + modelName + "\n").c_str());
        // One-shot failure: retrying LoadFBX every frame dominates frame time when
        // assets are missing from an isolated fixture / broken path.
        m_modelCache.emplace(cacheKey, nullptr);
        return nullptr;
    }

    Model3D* raw = model.get();
    m_modelCache.emplace(cacheKey, std::move(model));
    return raw;
}


bool ModelAssetCache::BeginContentReload() {
    if (m_contentReload) return true;
    m_previousModels.clear();
    // Stage only models referenced by the incoming scene. Old scene resources
    // remain in m_modelCache until commit; stale unused entries must not block reload.
    m_contentReload = true;
    return true;
}
void ModelAssetCache::EndContentReload(bool commit) {
    if (!m_contentReload) return;
    if (commit) m_modelCache.swap(m_previousModels);
    m_previousModels.clear();
    m_contentReload = false;
}

bool ModelAssetCache::PrepareContentModel(const std::string& modelName) {
    if (!m_contentReload || modelName.empty()) return true;
    const auto* def = FindObjectDef(m_objectDefs, modelName);
    const auto key = def ? def->lowerName : NormalizeAssetKey(modelName);
    if (m_previousModels.count(key)) return true;
    auto model = std::make_unique<Model3D>();
    if (!model->LoadFBXQuiet(ResolveModelWidePath(modelName, def).c_str())) return false;
    m_previousModels.emplace(key, std::move(model));
    return true;
}
