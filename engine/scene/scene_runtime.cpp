#include "scene_runtime.h"
#include "engine/ecs/collider_component_access.h"
#include "scene_manager.h"

#include <algorithm>

SceneRuntime& SceneRuntime::Instance() {
    static SceneRuntime s_runtime;
    return s_runtime;
}

void SceneRuntime::Clear(SceneBase* scene, bool destroyColliders) {
    if (!scene) return;
    for (GameObject* obj : m_registered) {
        if (!obj) continue;
        if (destroyColliders) {
            if (auto* cc = FindColliderComponent(obj))
                cc->ClearFromOwner();
        }
        scene->RemoveObject(obj);
    }
    m_registered.clear();
}

void SceneRuntime::ApplyCollidersTree(GameObject* node) {
    if (!node) return;

    node->SyncLegacyTransformFromComponents();

    if (auto* cc = FindColliderComponent(node))
        cc->ApplyToOwner();

    for (const auto& child : node->GetChildren())
        ApplyCollidersTree(child.get());
}

void SceneRuntime::ApplyCollidersFromEditorRoots(
    const std::vector<std::unique_ptr<GameObject>>& roots)
{
    for (const auto& root : roots)
        if (root) ApplyCollidersTree(root.get());
}

void SceneRuntime::RegisterTree(GameObject* node, SceneBase* scene) {
    if (!node || !scene) return;

    node->SyncLegacyTransformFromComponents();

    if (auto* cc = FindColliderComponent(node)) {
        cc->ApplyToOwner();
        if (node->GetCollider()) {
            scene->AddObject(node);
            m_registered.push_back(node);
        }
    }

    for (const auto& child : node->GetChildren())
        RegisterTree(child.get(), scene);
}

void SceneRuntime::SyncFromEditorRoots(
    const std::vector<std::unique_ptr<GameObject>>& roots,
    SceneBase* scene)
{
    Clear(scene, true);
    for (const auto& root : roots)
        if (root) RegisterTree(root.get(), scene);
}

void SceneRuntime::UnregisterTree(GameObject* node, SceneBase* scene, bool destroyColliders) {
    if (!node || !scene) return;

    for (const auto& child : node->GetChildren())
        UnregisterTree(child.get(), scene, destroyColliders);

    auto it = std::find(m_registered.begin(), m_registered.end(), node);
    if (it != m_registered.end()) {
        if (destroyColliders) {
            if (auto* cc = FindColliderComponent(node))
                cc->ClearFromOwner();
        }
        scene->RemoveObject(node);
        m_registered.erase(it);
    }
}

void SceneRuntime::UnregisterSubtree(GameObject* root, SceneBase* scene, bool destroyColliders) {
    if (!root || !scene) return;
    UnregisterTree(root, scene, destroyColliders);
}

void SceneRuntime::RegisterSubtree(GameObject* root, SceneBase* scene) {
    if (!root || !scene) return;
    RegisterTree(root, scene);
}
