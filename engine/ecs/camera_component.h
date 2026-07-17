#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <string>

namespace engine { class Transform; }

class CameraComponent : public Component {
public:
    bool enabled = true;
    float fov = 45.0f;
    float nearClip = 0.5f;
    float farClip = 1000.0f;

    const char* GetTypeName() const override { return "Camera"; }

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    static DirectX::XMFLOAT3 ComputeEyeWS(const engine::Transform* transform);
    static DirectX::XMFLOAT3 ComputeForwardWS(const engine::Transform* transform);
    static DirectX::XMFLOAT3 ComputeUpWS(const engine::Transform* transform);
    static DirectX::XMFLOAT3 ComputeRightWS(const engine::Transform* transform);
    // Stable view up from forward + world Y (avoids upside-down past 90° pitch).
    static DirectX::XMFLOAT3 ComputeViewUpWS(const DirectX::XMFLOAT3& forwardWS);

    void ApplyToGlobalCamera(const engine::Transform* transform) const;
};
