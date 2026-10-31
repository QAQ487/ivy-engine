// spot_light_component.h
// ECS spot light component.

#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <string>

class SpotLightComponent : public Component {
public:
    bool enabled = true;
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    DirectX::XMFLOAT3 specColor = { 1.0f, 1.0f, 1.0f };
    float specPower = 32.0f;
    float range = 10.0f;
    DirectX::XMFLOAT3 attenuation = { 1.0f, 0.0f, 1.0f };
    float innerAngleDeg = 20.0f;
    float outerAngleDeg = 30.0f;
    bool castShadows = false;

    const char* GetTypeName() const override { return "SpotLight"; }

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;
};
