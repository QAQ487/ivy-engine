#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <string>

class GameObject;

namespace engine { class Transform; }

class DirectionalLightComponent : public Component {
public:
    bool enabled = true;
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    DirectX::XMFLOAT3 specColor = { 1.0f, 1.0f, 1.0f };
    float specPower = 32.0f;
    bool castShadows = true;

    const char* GetTypeName() const override { return "DirectionalLight"; }

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    static DirectX::XMFLOAT3 ComputeDirectionWS(const engine::Transform* transform);
};
