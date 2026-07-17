#include "scene_camera.h"
#include "engine/core/engine_gui.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include <string>

namespace {

GameObject* FindNodeByName(GameObject* node, const std::string& name) {
    if (!node) return nullptr;
    if (node->GetName() == name) return node;
    for (const auto& child : node->GetChildren()) {
        if (GameObject* found = FindNodeByName(child.get(), name))
            return found;
    }
    return nullptr;
}

GameObject* FindNodeByNameInRoots(const std::vector<std::unique_ptr<GameObject>>& roots,
                                  const std::string& name) {
    if (name.empty()) return nullptr;
    for (const auto& root : roots) {
        if (GameObject* found = FindNodeByName(root.get(), name))
            return found;
    }
    return nullptr;
}

} // namespace

void SceneCameraSyncFromActiveNode(const std::vector<std::unique_ptr<GameObject>>& roots) {
    if (EngineGUI_GetCameraMode() != CameraMode::SceneCamera)
        return;

    GameObject* node = FindNodeByNameInRoots(roots, EngineGUI_GetActiveCameraNodeName());
    if (!node || !node->IsActive())
        return;

    auto* cam = node->GetComponent<CameraComponent>();
    if (!cam || !cam->enabled)
        return;

    auto* transform = node->GetComponent<engine::Transform>();
    if (!transform)
        return;

    cam->ApplyToGlobalCamera(transform);
}
