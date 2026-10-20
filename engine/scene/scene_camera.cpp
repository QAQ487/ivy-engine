#include "camera_director.h"
#include "camera.h"
#include "engine/scene/scene_loader.h"
#include "debug_camera.h"
#include <cmath>
#include <functional>
#include <algorithm>
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

std::string SceneCameraResolveDefault(const std::vector<std::unique_ptr<GameObject>>& roots, const std::string& preferred) {
    std::vector<std::string> names;
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node || !node->IsActive()) return;
        auto* cam = node->GetComponent<CameraComponent>();
        if (cam && cam->enabled && node->GetComponent<engine::Transform>()) names.push_back(node->GetName());
        for (const auto& child : node->GetChildren()) visit(child.get());
    };
    for (const auto& root : roots) visit(root.get());
    if (!preferred.empty() && std::count(names.begin(),names.end(),preferred)==1) return preferred;
    return names.size()==1 ? names.front() : std::string{};
}

void SceneCameraBeginPlay() {
    CameraDirector_BeginPlay(SceneLoader_GetSceneNodes());
    if(EngineGUI_GetCameraMode()!=CameraMode::SceneCamera) return;
    auto& roots=SceneLoader_GetSceneNodes();
    const auto name=SceneCameraResolveDefault(roots,EngineGUI_GetActiveCameraNodeName());
    auto* node=FindNodeByNameInRoots(roots,name);
    if(!node) return;
    auto* camera=node->GetComponent<CameraComponent>();
    auto* transform=node->GetComponent<engine::Transform>();
    if(!camera || !camera->debugFly || !transform) return;
    EngineGUI_SetActiveCameraNodeName(name);
    DebugCamera_SetEnabled(true);
    camera->ApplyToGlobalCamera(transform);
    // Use a useful orbit radius while preserving the authored eye and direction.
    const auto forward=CameraComponent::ComputeForwardWS(transform);
    const auto eye=CameraComponent::ComputeEyeWS(transform);
    GetCamera().SetTarget({eye.x+forward.x*10.0f,eye.y+forward.y*10.0f,eye.z+forward.z*10.0f});
    DebugCamera_SyncOrbitFromCurrentView();
    DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
}
