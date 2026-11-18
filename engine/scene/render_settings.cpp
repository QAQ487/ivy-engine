// render_settings.cpp
// Scene: render settings implementation.

#include "engine/scene/scene_loader.h"
#include "render_settings.h"
#include <stdexcept>
#include "scene_json_validation.h"

#include "engine/rendering/environment/skybox.h"
#include "engine/rendering/environment/ibl.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/rendering/culling/cull_settings_math.h"
#include "engine/rendering/culling/lod_state.h"
#include "engine/scene/render_setting_access.h"
#include <cmath>
#include <cstring>
#include "engine/rendering/culling/lod_select_math.h"

static RenderSettingsData g_renderSettings;

void RenderSettings_ResetToDefaults() {
    g_renderSettings = {};
}

RenderSettingsData& RenderSettings_Get() {
    return g_renderSettings;
}

const RenderSettingsData& RenderSettings_GetConst() {
    return g_renderSettings;
}

bool RenderSettings_SetBoolByName(const char* key, bool value) {
    return render_setting_access::SetBool(g_renderSettings, key, value);
}

bool RenderSettings_GetBoolByName(const char* key, bool* outValue) {
    bool v = false;
    if (!outValue || !render_setting_access::GetBool(g_renderSettings, key, v)) return false;
    *outValue = v;
    return true;
}

bool RenderSettings_SetNumberByName(const char* key, double value) {
    if (key && std::strcmp(key, "lodForceLevel") == 0) {
        if (!std::isfinite(value)) return false;
        Lod_SetForceLevel(static_cast<int>(std::lround(value)));
        return true;
    }
    return render_setting_access::SetNumber(g_renderSettings, key, value);
}

bool RenderSettings_GetNumberByName(const char* key, double* outValue) {
    if (!outValue) return false;
    if (key && std::strcmp(key, "lodForceLevel") == 0) {
        *outValue = Lod_GetForceLevel();
        return true;
    }
    double v = 0.0;
    if (!render_setting_access::GetNumber(g_renderSettings, key, v)) return false;
    *outValue = v;
    return true;
}

static DirectX::XMFLOAT4 ReadColor4(const nlohmann::json& j, const DirectX::XMFLOAT4& fallback) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return {
        j[0].get<float>(),
        j[1].get<float>(),
        j[2].get<float>(),
        j[3].get<float>()
    };
}

static DirectX::XMFLOAT3 ReadColor3(const nlohmann::json& j, const DirectX::XMFLOAT3& fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return {
        j[0].get<float>(),
        j[1].get<float>(),
        j[2].get<float>()
    };
}

ShadowQualityPreset RenderSettings_GetShadowPreset(ShadowQuality quality) {
    ShadowQualityPreset p {};
    switch (quality) {
    case ShadowQuality::Low:
        p.mapSize = 512;
        p.cascadeCount = 2;
        p.maxDistance = 40.0f;
        break;
    case ShadowQuality::High:
        p.mapSize = 2048;
        p.cascadeCount = 4;
        p.maxDistance = 90.0f;
        break;
    case ShadowQuality::Medium:
    default:
        p.mapSize = 1024;
        p.cascadeCount = 4;
        p.maxDistance = 70.0f;
        break;
    }
    return p;
}

ShadowQualityPreset RenderSettings_GetShadowPreset() {
    ShadowQualityPreset preset = RenderSettings_GetShadowPreset(g_renderSettings.shadowQuality);
    // Optional per-scene range override; map size and cascade count stay with the preset.
    preset.maxDistance = cull_settings::ResolveShadowDistance(
        preset.maxDistance, g_renderSettings.shadowDistance);
    return preset;
}

const char* RenderSettings_ShadowQualityLabel(ShadowQuality quality) {
    switch (quality) {
    case ShadowQuality::Low: return "Low";
    case ShadowQuality::High: return "High";
    case ShadowQuality::Medium:
    default: return "Medium";
    }
}

const char* RenderSettings_ShadowQualityId(ShadowQuality quality) {
    switch (quality) {
    case ShadowQuality::Low: return "low";
    case ShadowQuality::High: return "high";
    case ShadowQuality::Medium:
    default: return "medium";
    }
}

ShadowQuality RenderSettings_ShadowQualityFromId(const std::string& id) {
    if (id == "low" || id == "Low" || id == "0")
        return ShadowQuality::Low;
    if (id == "high" || id == "High" || id == "2")
        return ShadowQuality::High;
    return ShadowQuality::Medium;
}

void RenderSettings_ApplyShadowQuality() {
    if(SceneLoader_Is2D()) return;
    if (!GetDevice() || !GetDeviceContext())
        return;

    const ShadowQualityPreset preset = RenderSettings_GetShadowPreset();
    const DirectX::XMFLOAT3 dir = Shadow::GetLightDirection(0);
    const bool backface = Shadow::IsBackfaceDepthEnabled();

    if (!Shadow::Reconfigure(preset.mapSize, preset.cascadeCount))
        return;

    Shadow::SetLightDirection(0, dir);
    Shadow::SetBackfaceDepth(backface);
}

RenderSettingsData RenderSettings_ParseJson(const nlohmann::json& renderJson, RenderSettingsData settings) {
    if (!renderJson.is_object()) throw std::runtime_error("render settings must be an object");
    auto types = RenderSettings_ToJson();
    // The legacy format accepts either a quality ID or an integer.
    types.erase("shadowQuality");
    SceneJson_ValidateTypes(renderJson, types, "settings.render");
    if (renderJson.contains("shadowQuality") && !renderJson["shadowQuality"].is_string()
        && !renderJson["shadowQuality"].is_number_integer())
        throw std::runtime_error("Invalid type: settings.render.shadowQuality");

    settings.skyboxEnabled = renderJson.value("skyboxEnabled", settings.skyboxEnabled);
    settings.skyboxCubemapPath = renderJson.value(
        "skyboxCubemap", settings.skyboxCubemapPath);
    settings.skyboxVerticalFov = renderJson.value(
        "skyboxVerticalFov", settings.skyboxVerticalFov);
    if (!(settings.skyboxVerticalFov > 0.0f))
        settings.skyboxVerticalFov = 0.0f;
    else if (settings.skyboxVerticalFov > 180.0f)
        settings.skyboxVerticalFov = 180.0f;
    settings.skyboxTint = ReadColor4(
        renderJson.value("skyboxTint", nlohmann::json{}), settings.skyboxTint);
    settings.skyboxExposure = renderJson.value(
        "skyboxExposure", settings.skyboxExposure);
    settings.skyboxRotationYDeg = renderJson.value(
        "skyboxRotationYDeg", settings.skyboxRotationYDeg);
    settings.iblEnabled = renderJson.value("iblEnabled", settings.iblEnabled);
    settings.iblIntensity = renderJson.value("iblIntensity", settings.iblIntensity);
    settings.iblIrradiancePath = renderJson.value(
        "iblIrradiance", settings.iblIrradiancePath);
    settings.iblPrefilterPath = renderJson.value(
        "iblPrefilter", settings.iblPrefilterPath);
    settings.iblBrdfLutPath = renderJson.value(
        "iblBrdfLut", settings.iblBrdfLutPath);
    settings.clearColor = ReadColor4(
        renderJson.value("clearColor", nlohmann::json{}), settings.clearColor);
    settings.defaultShader = renderJson.value(
        "defaultShader", settings.defaultShader);

    if (renderJson.contains("shadowQuality")) {
        if (renderJson["shadowQuality"].is_string())
            settings.shadowQuality = RenderSettings_ShadowQualityFromId(
                renderJson["shadowQuality"].get<std::string>());
        else if (renderJson["shadowQuality"].is_number_integer()) {
            const int q = renderJson["shadowQuality"].get<int>();
            if (q <= 0) settings.shadowQuality = ShadowQuality::Low;
            else if (q >= 2) settings.shadowQuality = ShadowQuality::High;
            else settings.shadowQuality = ShadowQuality::Medium;
        }
    }

    settings.fogEnabled = renderJson.value("fogEnabled", settings.fogEnabled);
    if (renderJson.contains("fogColor"))
        settings.fogColor = ReadColor3(renderJson["fogColor"], settings.fogColor);
    settings.fogDensity = renderJson.value("fogDensity", settings.fogDensity);
    settings.fogStart = renderJson.value("fogStart", settings.fogStart);
    settings.fogHeight = renderJson.value("fogHeight", settings.fogHeight);
    settings.fogHeightFalloff = renderJson.value(
        "fogHeightFalloff", settings.fogHeightFalloff);
    settings.frustumCullingEnabled = renderJson.value(
        "frustumCullingEnabled", settings.frustumCullingEnabled);
    settings.shadowCascadeCullingEnabled = renderJson.value(
        "shadowCascadeCullingEnabled", settings.shadowCascadeCullingEnabled);
    settings.spriteCullingEnabled = renderJson.value(
        "spriteCullingEnabled", settings.spriteCullingEnabled);
    settings.lodEnabled = renderJson.value("lodEnabled", settings.lodEnabled);
    settings.billboardCullingEnabled = renderJson.value(
        "billboardCullingEnabled", settings.billboardCullingEnabled);
    settings.effectCullingEnabled = renderJson.value(
        "effectCullingEnabled", settings.effectCullingEnabled);
    settings.effectCullRadius = cull_settings::ClampEffectCullRadius(
        renderJson.value("effectCullRadius", settings.effectCullRadius));
    settings.lightSelectionEnabled = renderJson.value(
        "lightSelectionEnabled", settings.lightSelectionEnabled);
    settings.animCullingEnabled = renderJson.value(
        "animCullingEnabled", settings.animCullingEnabled);
    settings.lodBias = lod_select::ClampBias(
        renderJson.value("lodBias", settings.lodBias));
    settings.lodDebugLabels = renderJson.value("lodDebugLabels", settings.lodDebugLabels);
    settings.shadowLodOffset = lod_select::ClampShadowLodOffset(
        renderJson.value("shadowLodOffset", settings.shadowLodOffset));
    settings.cullDistanceScale = cull_settings::ClampCullDistanceScale(
        renderJson.value("cullDistanceScale", settings.cullDistanceScale));
    settings.shadowDistance = cull_settings::ClampShadowDistanceOverride(
        renderJson.value("shadowDistance", settings.shadowDistance));

    return settings;
}

void RenderSettings_ApplyFromJson(const nlohmann::json& renderJson) {
    if (!renderJson.is_object()) return;
    g_renderSettings = RenderSettings_ParseJson(renderJson, g_renderSettings);
    RenderSettings_ReloadSkybox();
    RenderSettings_ApplyShadowQuality();
}

nlohmann::json RenderSettings_ToJson() {
    nlohmann::json render;
    render["skyboxEnabled"] = g_renderSettings.skyboxEnabled;
    render["skyboxCubemap"] = g_renderSettings.skyboxCubemapPath;
    render["skyboxVerticalFov"] = g_renderSettings.skyboxVerticalFov;
    render["skyboxTint"] = {
        g_renderSettings.skyboxTint.x,
        g_renderSettings.skyboxTint.y,
        g_renderSettings.skyboxTint.z,
        g_renderSettings.skyboxTint.w
    };
    render["skyboxExposure"] = g_renderSettings.skyboxExposure;
    render["skyboxRotationYDeg"] = g_renderSettings.skyboxRotationYDeg;
    render["iblEnabled"] = g_renderSettings.iblEnabled;
    render["iblIntensity"] = g_renderSettings.iblIntensity;
    render["iblIrradiance"] = g_renderSettings.iblIrradiancePath;
    render["iblPrefilter"] = g_renderSettings.iblPrefilterPath;
    render["iblBrdfLut"] = g_renderSettings.iblBrdfLutPath;
    render["clearColor"] = {
        g_renderSettings.clearColor.x,
        g_renderSettings.clearColor.y,
        g_renderSettings.clearColor.z,
        g_renderSettings.clearColor.w
    };
    render["defaultShader"] = g_renderSettings.defaultShader;
    render["shadowQuality"] = RenderSettings_ShadowQualityId(g_renderSettings.shadowQuality);
    render["fogEnabled"] = g_renderSettings.fogEnabled;
    render["fogColor"] = {
        g_renderSettings.fogColor.x,
        g_renderSettings.fogColor.y,
        g_renderSettings.fogColor.z
    };
    render["fogDensity"] = g_renderSettings.fogDensity;
    render["fogStart"] = g_renderSettings.fogStart;
    render["fogHeight"] = g_renderSettings.fogHeight;
    render["fogHeightFalloff"] = g_renderSettings.fogHeightFalloff;
    render["frustumCullingEnabled"] = g_renderSettings.frustumCullingEnabled;
    render["shadowCascadeCullingEnabled"] = g_renderSettings.shadowCascadeCullingEnabled;
    render["spriteCullingEnabled"] = g_renderSettings.spriteCullingEnabled;
    render["lodEnabled"] = g_renderSettings.lodEnabled;
    render["billboardCullingEnabled"] = g_renderSettings.billboardCullingEnabled;
    render["effectCullingEnabled"] = g_renderSettings.effectCullingEnabled;
    render["effectCullRadius"] = g_renderSettings.effectCullRadius;
    render["lightSelectionEnabled"] = g_renderSettings.lightSelectionEnabled;
    render["animCullingEnabled"] = g_renderSettings.animCullingEnabled;
    render["lodBias"] = g_renderSettings.lodBias;
    render["shadowLodOffset"] = g_renderSettings.shadowLodOffset;
    render["lodDebugLabels"] = g_renderSettings.lodDebugLabels;
    render["cullDistanceScale"] = g_renderSettings.cullDistanceScale;
    render["shadowDistance"] = g_renderSettings.shadowDistance;
    return render;
}

void RenderSettings_ReloadSkybox() {
    if(SceneLoader_Is2D()) return;
    Skybox_ReloadFromSettings();
    Ibl_ReloadFromSettings();
}

Shader3DVariant RenderSettings_GetDefaultShader() {
    return Shader3DVariantFromId(g_renderSettings.defaultShader);
}
