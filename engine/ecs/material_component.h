/*****************************************************
 * File: engine/ecs/material_component.h
 * Project: d3d11_engine
 * Author: Terry
 * -----------------------------------------------------------------------------
 * Per-object material params for standard lit meshes.
 * Registered with ComponentFactory as "Material".
 *
 * Prefab JSON example:
 *   "Material": {
 *     "color": [1.0, 1.0, 1.0],
 *     "roughness": 0.5,
 *     "metallic": 0.0,
 *     "normalMap": "rom/texture/Normal Map/Wavy 0001.jpg",
 *     "normalStrength": 1.0,
 *     "shadeSteps": 3.0,
 *     "outlineWidth": 0.003,
 *     "shadowTint": [0.6, 0.65, 0.85]
 *   }
 *****************************************************/
#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <string>

struct ID3D11ShaderResourceView;

class MaterialComponent : public Component {
public:
    DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    std::string normalMapPath;
    float normalStrength = 1.0f;

    // Toon / outline
    float shadeSteps = 3.0f;
    float shadeSmoothness = 0.05f;
    float specularThreshold = 0.85f;
    float outlineWidth = 0.003f;
    DirectX::XMFLOAT4 outlineColor = {0.05f, 0.05f, 0.08f, 1.0f};
    DirectX::XMFLOAT3 shadowTint = {0.6f, 0.65f, 0.85f};
    float rimStrength = 0.25f;
    std::string rampTexturePath;

    const char* GetTypeName() const override { return "Material"; }

    void OnDetach() override;
    void OnReset() override;

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    ID3D11ShaderResourceView* GetNormalMapSRV();
    ID3D11ShaderResourceView* GetRampTextureSRV();

private:
    void ReleaseNormalMap();
    void ReleaseRampTexture();

    ID3D11ShaderResourceView* m_normalMapSRV = nullptr;
    std::string m_normalMapLoadedPath;

    ID3D11ShaderResourceView* m_rampSRV = nullptr;
    std::string m_rampLoadedPath;
};
