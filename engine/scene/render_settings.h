// render_settings.h
// Declares ShadowQuality.

#pragma once

#include "libraries/nlohmann/json.hpp"
#include <DirectXMath.h>
#include <string>
#include "engine/rendering/d3d11/shader.h"

enum class ShadowQuality {
    Low = 0,
    Medium = 1, // current default (4 x 1024, distance 70)
    High = 2
};

struct ShadowQualityPreset {
    unsigned int mapSize = 1024;
    int cascadeCount = 4;
    float maxDistance = 70.0f;
};

struct RenderSettingsData {
    bool skyboxEnabled = true;
    std::string skyboxCubemapPath = "rom/skybox/MySkybox.dds";
    DirectX::XMFLOAT4 skyboxTint = { 1.0f, 1.0f, 1.0f, 1.0f };
    float skyboxExposure = 1.0f;
    float skyboxRotationYDeg = 0.0f;
    bool iblEnabled = true;
    float iblIntensity = 1.0f;
    std::string iblIrradiancePath = "rom/skybox/ibl/irradiance.dds";
    std::string iblPrefilterPath = "rom/skybox/ibl/prefilter.dds";
    std::string iblBrdfLutPath = "rom/skybox/ibl/brdf_lut.png";
    DirectX::XMFLOAT4 clearColor = { 0.69f, 0.77f, 0.87f, 1.0f };
    std::string defaultShader = "standard";
    ShadowQuality shadowQuality = ShadowQuality::Medium;

    // Height fog (in-shader, opaque lit meshes)
    bool fogEnabled = false;
    DirectX::XMFLOAT3 fogColor = { 0.69f, 0.77f, 0.87f };
    float fogDensity = 0.02f;
    float fogStart = 10.0f;
    float fogHeight = 5.0f;
    float fogHeightFalloff = 0.15f;
};

void RenderSettings_ResetToDefaults();
RenderSettingsData& RenderSettings_Get();
const RenderSettingsData& RenderSettings_GetConst();

RenderSettingsData RenderSettings_ParseJson(const nlohmann::json& renderJson, RenderSettingsData settings = {});
void RenderSettings_ApplyFromJson(const nlohmann::json& renderJson);
nlohmann::json RenderSettings_ToJson();

void RenderSettings_ReloadSkybox();
Shader3DVariant RenderSettings_GetDefaultShader();

ShadowQualityPreset RenderSettings_GetShadowPreset();
ShadowQualityPreset RenderSettings_GetShadowPreset(ShadowQuality quality);
const char* RenderSettings_ShadowQualityLabel(ShadowQuality quality);
ShadowQuality RenderSettings_ShadowQualityFromId(const std::string& id);
const char* RenderSettings_ShadowQualityId(ShadowQuality quality);

// Rebuild shadow maps to match current quality (safe to call after D3D init).
void RenderSettings_ApplyShadowQuality();
