#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <string>

class PointLightComponent : public Component {
public:
    bool enabled = true;
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    DirectX::XMFLOAT3 specColor = { 1.0f, 1.0f, 1.0f };
    float specPower = 32.0f;
    float range = 15.0f;
    DirectX::XMFLOAT3 attenuation = { 1.0f, 0.09f, 0.032f };

    const char* GetTypeName() const override { return "PointLight"; }

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;
};
