// prefab_runtime.cpp
// Instantiates and manages prefab instances at runtime.

#include "prefab_runtime.h"

#include "prefab_serializer.h"
#include "engine/scene/scene_runtime.h"
#include "../scripting/script_component.h"
#include "transform.h"
#include "../scene/scene_loader.h"
#include "scene_manager.h"
#include "engine/physics/core/physics_system.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>

using namespace DirectX;

namespace {

PrefabRuntimeBeforeDestroyFn g_beforeDestroyCallback = nullptr;
GameObject* g_spawnRoot = nullptr;
std::vector<GameObject*> g_pendingDestroy;
std::unordered_set<GameObject*> g_pendingDestroySet;

struct PendingSpawn {
    std::string prefabPath;
    XMFLOAT3 worldPos{};
    XMFLOAT3 worldRot{};
    GameObject* parent = nullptr;
    bool startActive = true;
};

std::mutex g_pendingMutex;
std::vector<PendingSpawn> g_pendingSpawn;

void CollectSubtree(GameObject* node, std::vector<GameObject*>& out) {
    if (!node) return;
    out.push_back(node);
    for (const auto& child : node->GetChildren())
        CollectSubtree(child.get(), out);
}

std::unique_ptr<GameObject> ExtractFromSceneGraph(GameObject* obj) {
    if (!obj || obj == g_spawnRoot) return nullptr;

    auto& roots = SceneLoader_GetSceneNodes();
    for (auto it = roots.begin(); it != roots.end(); ++it) {
        if (it->get() == obj) {
            std::unique_ptr<GameObject> owned = std::move(*it);
            roots.erase(it);
            owned->SetParent(nullptr);
            return owned;
        }
    }

    GameObject* parent = obj->GetParent();
    if (!parent) return nullptr;

    auto& siblings = parent->GetChildren();
    auto it = std::find_if(siblings.begin(), siblings.end(),
        [obj](const std::unique_ptr<GameObject>& c) { return c.get() == obj; });
    if (it == siblings.end()) return nullptr;

    std::unique_ptr<GameObject> owned = std::move(*it);
    siblings.erase(it);
    owned->SetParent(nullptr);
    return owned;
}

void DestroySubtreeOwned(std::unique_ptr<GameObject> owned) {
    if (!owned) return;
    owned.reset();
}

bool IsInSceneGraph(GameObject* obj) {
    if (!obj) return false;

    std::function<bool(GameObject*)> visit;
    visit = [&](GameObject* node) {
        if (!node) return false;
        if (node == obj) return true;
        for (const auto& child : node->GetChildren()) {
            if (child && visit(child.get()))
                return true;
        }
        return false;
    };

    for (const auto& root : SceneLoader_GetSceneNodes()) {
        if (root && visit(root.get()))
            return true;
    }
    return false;
}

} // namespace

void PrefabRuntime_SetBeforeDestroyCallback(PrefabRuntimeBeforeDestroyFn callback) {
    g_beforeDestroyCallback = callback;
}

bool PrefabRuntime_IsSpawnRoot(const GameObject* obj) {
    return obj && obj->GetName() == PREFAB_RUNTIME_SPAWN_ROOT_NAME;
}

bool PrefabRuntime_IsRuntimeSpawned(const GameObject* obj) {
    if (!obj || PrefabRuntime_IsSpawnRoot(obj)) return false;
    for (const GameObject* p = obj->GetParent(); p; p = p->GetParent()) {
        if (PrefabRuntime_IsSpawnRoot(p)) return true;
    }
    return false;
}

GameObject* PrefabRuntime_EnsureSpawnRoot() {
    if (g_spawnRoot && !IsInSceneGraph(g_spawnRoot))
        g_spawnRoot = nullptr;
    if (g_spawnRoot && !PrefabRuntime_IsSpawnRoot(g_spawnRoot))
        g_spawnRoot = nullptr;
    if (g_spawnRoot) return g_spawnRoot;

    auto& roots = SceneLoader_GetSceneNodes();
    for (const auto& root : roots) {
        if (root && root->GetName() == PREFAB_RUNTIME_SPAWN_ROOT_NAME) {
            g_spawnRoot = root.get();
            return g_spawnRoot;
        }
    }

    auto spawnRoot = std::make_unique<GameObject>(PREFAB_RUNTIME_SPAWN_ROOT_NAME);
    spawnRoot->SetActive(true);
    g_spawnRoot = spawnRoot.get();
    roots.push_back(std::move(spawnRoot));
    return g_spawnRoot;
}

GameObject* PrefabRuntime_InstantiateImmediate(const char* prefabPath,
                                               const XMFLOAT3& worldPos,
                                               const XMFLOAT3& worldRot,
                                               GameObject* parent,
                                               bool startActive)
{
    if (!prefabPath || !prefabPath[0]) return nullptr;

    std::unique_ptr<GameObject> instance = PrefabSerializer::LoadFromFile(prefabPath);
    if (!instance) return nullptr;

    instance->SetPrefabAssetPath(prefabPath);
    instance->SetPosition(worldPos);
    instance->SetRotation(worldRot);
    instance->SyncComponentsFromLegacyTransform();
    if (engine::Transform* t = instance->GetComponent<engine::Transform>()) {
        t->localPosition = worldPos;
        t->localRotation = worldRot;
    }
    instance->SetActive(startActive);

    GameObject* parentNode = parent ? parent : PrefabRuntime_EnsureSpawnRoot();
    if (!parentNode) return nullptr;

    GameObject* raw = instance.get();
    parentNode->AddChild(std::move(instance));

    if (SceneBase* scene = SceneManager::Instance().GetCurrentScene())
        SceneRuntime::Instance().RegisterSubtree(raw, scene);

    return raw;
}

GameObject* PrefabRuntime_Instantiate(const char* prefabPath,
                                      const XMFLOAT3& worldPos,
                                      const XMFLOAT3& worldRot,
                                      GameObject* parent,
                                      bool startActive)
{
    // Defer during GameObject::Update so workers / mid-iteration never
    // mutate the scene graph. Spawned objects appear at FlushSpawned.
    if (GameObject::IsInsideUpdate()) {
        if (!prefabPath || !prefabPath[0]) return nullptr;
        PendingSpawn req;
        req.prefabPath = prefabPath;
        req.worldPos = worldPos;
        req.worldRot = worldRot;
        req.parent = parent;
        req.startActive = startActive;
        {
            std::lock_guard<std::mutex> lock(g_pendingMutex);
            g_pendingSpawn.push_back(std::move(req));
        }
        return nullptr;
    }

    return PrefabRuntime_InstantiateImmediate(
        prefabPath, worldPos, worldRot, parent, startActive);
}

void PrefabRuntime_FlushSpawned() {
    std::vector<PendingSpawn> toSpawn;
    {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        toSpawn.swap(g_pendingSpawn);
    }
    for (const PendingSpawn& req : toSpawn) {
        PrefabRuntime_InstantiateImmediate(
            req.prefabPath.c_str(),
            req.worldPos,
            req.worldRot,
            req.parent,
            req.startActive);
    }
}

void PrefabRuntime_Destroy(GameObject* obj) {
    if (!obj || PrefabRuntime_IsSpawnRoot(obj)) return;

    std::lock_guard<std::mutex> lock(g_pendingMutex);
    if (g_pendingDestroySet.count(obj)) return;

    obj->SetActive(false);

    if (g_beforeDestroyCallback)
        g_beforeDestroyCallback(obj);

    g_pendingDestroy.push_back(obj);
    g_pendingDestroySet.insert(obj);
}

void PrefabRuntime_DestroyAllSpawnedChildren() {
    std::vector<GameObject*> children;
    for (const auto& root : SceneLoader_GetSceneNodes()) {
        if (!root || !PrefabRuntime_IsSpawnRoot(root.get())) continue;
        for (const auto& child : root->GetChildren()) {
            if (child)
                children.push_back(child.get());
        }
        break;
    }

    if (children.empty()) return;

    for (GameObject* obj : children)
        PrefabRuntime_Destroy(obj);
    PrefabRuntime_FlushDestroyed();
}

void PrefabRuntime_ResetSessionState() {
    g_beforeDestroyCallback = nullptr;
    g_spawnRoot = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        g_pendingDestroy.clear();
        g_pendingDestroySet.clear();
        g_pendingSpawn.clear();
    }
}

void PrefabRuntime_FlushDestroyed() {
    std::vector<GameObject*> toFlush;
    {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        if (g_pendingDestroy.empty()) return;
        toFlush.swap(g_pendingDestroy);
        g_pendingDestroySet.clear();
    }

    SceneBase* scene = SceneManager::Instance().GetCurrentScene();

    for (GameObject* obj : toFlush) {
        if (!obj) continue;

        std::vector<GameObject*> subtree;
        CollectSubtree(obj, subtree);

        for (GameObject* node : subtree) {
            PhysicsSystem_NotifyObjectDestroyed(node);
            if (ScriptComponent* sc = node->GetComponent<ScriptComponent>())
                sc->Teardown();
        }

        if (scene)
            SceneRuntime::Instance().UnregisterSubtree(obj, scene, true);

        std::unique_ptr<GameObject> owned = ExtractFromSceneGraph(obj);
        DestroySubtreeOwned(std::move(owned));
    }

    if (g_spawnRoot) {
        bool rootStillExists = false;
        for (const auto& root : SceneLoader_GetSceneNodes()) {
            if (root.get() == g_spawnRoot) {
                rootStillExists = true;
                break;
            }
        }
        if (!rootStillExists)
            g_spawnRoot = nullptr;
    }
}
