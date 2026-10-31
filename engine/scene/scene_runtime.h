#pragma once
#include "engine/ecs/game_object.h"
#include <memory>
#include <vector>

class SceneBase;

// Registers editor scene GameObjects with the active scene for physics/collision.
class SceneRuntime {
public:
    static SceneRuntime& Instance();

    void Clear(SceneBase* scene, bool destroyColliders = false);
    void SyncFromEditorRoots(const std::vector<std::unique_ptr<GameObject>>& roots,
                             SceneBase* scene);
    // Rebuild colliders on editor nodes for debug drawing without physics registration.
    void ApplyCollidersFromEditorRoots(
        const std::vector<std::unique_ptr<GameObject>>& roots);

    void RegisterSubtree(GameObject* root, SceneBase* scene);
    void UnregisterSubtree(GameObject* root, SceneBase* scene, bool destroyColliders = true);

private:
    void RegisterTree(GameObject* node, SceneBase* scene);
    void UnregisterTree(GameObject* node, SceneBase* scene, bool destroyColliders);
    void ApplyCollidersTree(GameObject* node);

    std::vector<GameObject*> m_registered;
};
