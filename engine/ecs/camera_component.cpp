// camera_component.cpp
// ECS camera component (view/projection).

#include "camera_component.h"
#include "engine/ecs/transform.h"
#include "camera.h"
#include "libraries/nlohmann/json.hpp"
#include <Windows.h>
#include "engine/debug_draw/debug_ofstream.h"

using namespace DirectX;

namespace {

XMFLOAT3 TransformAxisWS(const engine::Transform* transform, XMVECTOR localAxis, XMFLOAT3 fallback) {
    if (!transform) return fallback;
    const XMMATRIX world = transform->GetWorldMatrix();
    XMVECTOR worldAxis = XMVector3TransformNormal(localAxis, world);
    if (XMVector3Equal(worldAxis, XMVectorZero()))
        return fallback;
    worldAxis = XMVector3Normalize(worldAxis);
    XMFLOAT3 out {};
    XMStoreFloat3(&out, worldAxis);
    return out;
}

} // namespace

DirectX::XMFLOAT3 CameraComponent::ComputeEyeWS(const engine::Transform* transform) {
    if (!transform) return { 0.0f, 0.0f, 0.0f };
    const XMMATRIX world = transform->GetWorldMatrix();
    XMFLOAT3 pos {};
    XMStoreFloat3(&pos, world.r[3]);
    return pos;
}

DirectX::XMFLOAT3 CameraComponent::ComputeForwardWS(const engine::Transform* transform) {
    return TransformAxisWS(transform, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), { 0.0f, 0.0f, 1.0f });
}

DirectX::XMFLOAT3 CameraComponent::ComputeUpWS(const engine::Transform* transform) {
    return TransformAxisWS(transform, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), { 0.0f, 1.0f, 0.0f });
}

DirectX::XMFLOAT3 CameraComponent::ComputeRightWS(const engine::Transform* transform) {
    return TransformAxisWS(transform, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), { 1.0f, 0.0f, 0.0f });
}

DirectX::XMFLOAT3 CameraComponent::ComputeViewUpWS(const DirectX::XMFLOAT3& forwardWS) {
    XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&forwardWS));
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Cross(worldUp, f);
    if (XMVectorGetX(XMVector3LengthSq(right)) < 1e-6f) {
        // Looking nearly straight up/down — use world +Z as bank reference.
        worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        right = XMVector3Cross(worldUp, f);
    }
    right = XMVector3Normalize(right);
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(f, right));
    XMFLOAT3 out {};
    XMStoreFloat3(&out, up);
    return out;
}

void CameraComponent::ApplyToGlobalCamera(const engine::Transform* transform) const {
    const XMFLOAT3 eye = ComputeEyeWS(transform);
    const XMFLOAT3 forward = ComputeForwardWS(transform);
    const XMFLOAT3 viewUp = ComputeViewUpWS(forward);

    Camera& cam = GetCamera();
    cam.SetPosition(eye);
    cam.SetTarget({ eye.x + forward.x, eye.y + forward.y, eye.z + forward.z });
    cam.SetUp(viewUp);
    cam.SetFov(fov);
    cam.orthographic = orthographic;
    cam.orthographicHeight = orthographicHeight;
    cam.SetClip(nearClip, farClip);
}

std::string CameraComponent::Serialize() const {
    nlohmann::json j;
    j["enabled"] = enabled;
    j["debugFly"] = debugFly;
    j["fov"] = fov;
    j["orthographic"] = orthographic;
    j["orthographicHeight"] = orthographicHeight;
    j["nearClip"] = nearClip;
    j["farClip"] = farClip;
    return j.dump();
}

void CameraComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        enabled = j.value("enabled", true);
        debugFly = j.value("debugFly", false);
        fov = j.value("fov", 45.0f);
        orthographic = j.value("orthographic", false);
        orthographicHeight = j.value("orthographicHeight", 10.0f);
        if (!(orthographicHeight > 0.001f)) orthographicHeight = 10.0f;
        nearClip = j.value("nearClip", 0.5f);
        farClip = j.value("farClip", 1000.0f);
    } catch (...) {
        hal::DebugWriteA("[CameraComponent] Deserialize: JSON parse error.\n");
    }
}
