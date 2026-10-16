/*****************************************************
 * File: engine/ecs/material_component.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 *****************************************************/
#include "material_component.h"

#include "engine/rendering/model/model_util.h"
#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <algorithm>

namespace {

DirectX::XMFLOAT3 ReadFloat3(const nlohmann::json& j, const DirectX::XMFLOAT3& fallback) {
    if (!j.is_array() || j.size() < 3)
        return fallback;
    return DirectX::XMFLOAT3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

DirectX::XMFLOAT4 ReadFloat4(const nlohmann::json& j, const DirectX::XMFLOAT4& fallback) {
    if (!j.is_array() || j.size() < 3)
        return fallback;
    const float a = (j.size() >= 4) ? j[3].get<float>() : fallback.w;
    return DirectX::XMFLOAT4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), a);
}

nlohmann::json ToJson(const DirectX::XMFLOAT3& v) {
    return nlohmann::json::array({ v.x, v.y, v.z });
}

nlohmann::json ToJson(const DirectX::XMFLOAT4& v) {
    return nlohmann::json::array({ v.x, v.y, v.z, v.w });
}

float Clamp01(float v) {
    return (std::max)(0.0f, (std::min)(1.0f, v));
}

} // namespace

void MaterialComponent::ReleaseNormalMap() {
    if (m_normalMapSRV) {
        m_normalMapSRV->Release();
        m_normalMapSRV = nullptr;
    }
    m_normalMapLoadedPath.clear();
}

void MaterialComponent::ReleaseRampTexture() {
    if (m_rampSRV) {
        m_rampSRV->Release();
        m_rampSRV = nullptr;
    }
    m_rampLoadedPath.clear();
}

ID3D11ShaderResourceView* MaterialComponent::GetNormalMapSRV() {
    if (normalMapPath.empty())
        return nullptr;

    if (m_normalMapSRV && m_normalMapLoadedPath == normalMapPath)
        return m_normalMapSRV;

    ReleaseNormalMap();

    const std::wstring wide = Utf8ToWide(normalMapPath.c_str());
    ID3D11ShaderResourceView* srv = nullptr;
    if (LoadTextureWIC_SRV(wide.c_str(), &srv) && srv) {
        m_normalMapSRV = srv;
        m_normalMapLoadedPath = normalMapPath;
    }
    return m_normalMapSRV;
}

ID3D11ShaderResourceView* MaterialComponent::GetRampTextureSRV() {
    if (rampTexturePath.empty())
        return nullptr;

    if (m_rampSRV && m_rampLoadedPath == rampTexturePath)
        return m_rampSRV;

    ReleaseRampTexture();

    const std::wstring wide = Utf8ToWide(rampTexturePath.c_str());
    ID3D11ShaderResourceView* srv = nullptr;
    if (LoadTextureWIC_SRV(wide.c_str(), &srv) && srv) {
        m_rampSRV = srv;
        m_rampLoadedPath = rampTexturePath;
    }
    return m_rampSRV;
}

void MaterialComponent::OnDetach() {
    ReleaseNormalMap();
    ReleaseRampTexture();
}

void MaterialComponent::OnReset() {
    ReleaseNormalMap();
    ReleaseRampTexture();
}

std::string MaterialComponent::Serialize() const {
    nlohmann::json j;
    j["color"] = ToJson(color);
    j["roughness"] = roughness;
    j["metallic"] = metallic;
    if (!normalMapPath.empty())
        j["normalMap"] = normalMapPath;
    j["normalStrength"] = normalStrength;
    j["shadeSteps"] = shadeSteps;
    j["shadeSmoothness"] = shadeSmoothness;
    j["specularThreshold"] = specularThreshold;
    j["outlineWidth"] = outlineWidth;
    j["outlineColor"] = ToJson(outlineColor);
    j["shadowTint"] = ToJson(shadowTint);
    j["rimStrength"] = rimStrength;
    if (!rampTexturePath.empty())
        j["rampTexture"] = rampTexturePath;
    return j.dump();
}

void MaterialComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("color"))
            color = ReadFloat3(j["color"], color);
        roughness = Clamp01(j.value("roughness", roughness));
        metallic = Clamp01(j.value("metallic", metallic));
        normalMapPath = j.value("normalMap", "");
        normalStrength = j.value("normalStrength", normalStrength);
        shadeSteps = j.value("shadeSteps", shadeSteps);
        shadeSmoothness = j.value("shadeSmoothness", shadeSmoothness);
        specularThreshold = Clamp01(j.value("specularThreshold", specularThreshold));
        outlineWidth = j.value("outlineWidth", outlineWidth);
        if (j.contains("outlineColor"))
            outlineColor = ReadFloat4(j["outlineColor"], outlineColor);
        if (j.contains("shadowTint"))
            shadowTint = ReadFloat3(j["shadowTint"], shadowTint);
        rimStrength = j.value("rimStrength", rimStrength);
        rampTexturePath = j.value("rampTexture", "");
        ReleaseNormalMap();
        ReleaseRampTexture();
    } catch (...) {
        OutputDebugStringA("[MaterialComponent] Deserialize: JSON parse error.\n");
    }
}
