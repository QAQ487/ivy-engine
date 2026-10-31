// audio_listener_sync.cpp
// Audio: audio listener sync implementation.

#include "audio_listener_sync.h"

#include "audio_manager.h"
#include "engine/core/engine_gui.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "camera.h"
#include "debug_camera.h"
#include "third_person_camera.h"

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
    const std::string& name)
{
    if (name.empty()) return nullptr;
    for (const auto& root : roots) {
        if (GameObject* found = FindNodeByName(root.get(), name))
            return found;
    }
    return nullptr;
}

GameObject* FindFirstListener(GameObject* node) {
    if (!node) return nullptr;
    if (node->GetComponent<AudioListenerComponent>())
        return node;
    for (const auto& child : node->GetChildren()) {
        if (GameObject* found = FindFirstListener(child.get()))
            return found;
    }
    return nullptr;
}

GameObject* FindFirstListenerInRoots(const std::vector<std::unique_ptr<GameObject>>& roots) {
    for (const auto& root : roots) {
        if (GameObject* found = FindFirstListener(root.get()))
            return found;
    }
    return nullptr;
}

void ApplyPoseFromCamera() {
    const Camera& cam = GetCamera();
    AudioManager::Instance().SetListener(nullptr);
    AudioManager::Instance().SetListener(cam.position, cam.GetForward(), cam.up);
}

void ApplyPoseFromNode(GameObject* node) {
    if (!node) {
        ApplyPoseFromCamera();
        return;
    }
    const engine::Transform* t = node->GetTransform();
    const CameraComponent* cam = node->GetComponent<CameraComponent>();
    DirectX::XMFLOAT3 pos = t ? CameraComponent::ComputeEyeWS(t) : node->GetPosition();
    DirectX::XMFLOAT3 fwd = cam && t
        ? CameraComponent::ComputeForwardWS(t)
        : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 up = CameraComponent::ComputeViewUpWS(fwd);
    AudioManager::Instance().SetListener(nullptr);
    AudioManager::Instance().SetListener(pos, fwd, up);
}

} // namespace

void AudioListenerSync_EnsureOnCamera(GameObject* cameraNode) {
    if (!cameraNode) return;
    if (!cameraNode->GetComponent<CameraComponent>()) return;
    if (!cameraNode->GetComponent<AudioListenerComponent>())
        cameraNode->AddComponent<AudioListenerComponent>();
}

void AudioListenerSync_Tick(const std::vector<std::unique_ptr<GameObject>>& roots) {
    if (DebugCamera_IsEnabled()) {
        ApplyPoseFromCamera();
        return;
    }

    GameObject* active = nullptr;
    const CameraMode mode = EngineGUI_GetCameraMode();
    if (mode == CameraMode::SceneCamera) {
        active = FindNodeByNameInRoots(roots, EngineGUI_GetActiveCameraNodeName());
    }
    else if (mode == CameraMode::ThirdPerson) {
        active = ThirdPersonCamera_GetFollowCamNode();
    }

    if (EngineGUI_IsPlaying() && active)
        AudioListenerSync_EnsureOnCamera(active);

    if (active && active->GetComponent<AudioListenerComponent>()) {
        ApplyPoseFromNode(active);
        return;
    }

    if (GameObject* fallback = FindFirstListenerInRoots(roots)) {
        ApplyPoseFromNode(fallback);
        return;
    }

    ApplyPoseFromCamera();
}
