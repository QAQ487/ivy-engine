// ibl.cpp
// Rendering: ibl implementation.

#include "ibl.h"

#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/scene/render_settings.h"
#include "engine/ecs/string_utils.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/core/content_path.h"

#include <fstream>

static ID3D11Device* s_device = nullptr;
static ID3D11DeviceContext* s_context = nullptr;
static ID3D11SamplerState* s_sampler = nullptr;
static ID3D11ShaderResourceView* s_irradiance = nullptr;
static ID3D11ShaderResourceView* s_prefilter = nullptr;
static ID3D11ShaderResourceView* s_brdfLut = nullptr;
static int s_brdfLutTexId = -1;
static bool s_ready = false;

static bool FileExistsW(const wchar_t* path) {
    if (!path || !path[0])
        return false;
    std::ifstream file(content_path::ReadableWideCurrent(path), std::ios::binary);
    return static_cast<bool>(file);
}

bool Ibl_Initialize() {
    s_device = GetDevice();
    s_context = GetDeviceContext();
    if (!s_device || !s_context)
        return false;

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(s_device->CreateSamplerState(&samplerDesc, &s_sampler)))
        return false;

    Ibl_ReloadFromSettings();
    return true;
}

void Ibl_Finalize() {
    s_ready = false;
    s_irradiance = nullptr;
    s_prefilter = nullptr;
    s_brdfLut = nullptr;
    s_brdfLutTexId = -1;
    SAFE_RELEASE(s_sampler);
    s_device = nullptr;
    s_context = nullptr;
}

void Ibl_ReloadFromSettings() {
    s_ready = false;
    s_irradiance = nullptr;
    s_prefilter = nullptr;
    s_brdfLut = nullptr;
    s_brdfLutTexId = -1;

    if (!s_device || !s_context)
        return;

    const RenderSettingsData& settings = RenderSettings_GetConst();
    if (!settings.iblEnabled)
        return;

    const std::wstring irradiancePath = ToWString(settings.iblIrradiancePath);
    const std::wstring prefilterPath = ToWString(settings.iblPrefilterPath);
    const std::wstring lutPath = ToWString(settings.iblBrdfLutPath);
    if (irradiancePath.empty() || prefilterPath.empty() || lutPath.empty()) {
        hal::dout << "Ibl_ReloadFromSettings failed: empty irradiance/prefilter/LUT path." << std::endl;
        return;
    }
    if (!FileExistsW(irradiancePath.c_str())) {
        hal::dout << "Ibl_ReloadFromSettings failed: missing " << settings.iblIrradiancePath << std::endl;
        return;
    }
    if (!FileExistsW(prefilterPath.c_str())) {
        hal::dout << "Ibl_ReloadFromSettings failed: missing " << settings.iblPrefilterPath << std::endl;
        return;
    }
    if (!FileExistsW(lutPath.c_str())) {
        hal::dout << "Ibl_ReloadFromSettings failed: missing " << settings.iblBrdfLutPath << std::endl;
        return;
    }

    s_irradiance = CubemapLoad(irradiancePath.c_str());
    if (!s_irradiance)
        hal::dout << "Ibl_ReloadFromSettings failed: irradiance " << settings.iblIrradiancePath << std::endl;
    s_prefilter = CubemapLoad(prefilterPath.c_str());
    if (!s_prefilter)
        hal::dout << "Ibl_ReloadFromSettings failed: prefilter " << settings.iblPrefilterPath << std::endl;
    s_brdfLutTexId = TextureLoad(lutPath.c_str(), false);
    s_brdfLut = TextureGetSRV(s_brdfLutTexId);
    if (!s_brdfLut)
        hal::dout << "Ibl_ReloadFromSettings failed: BRDF LUT " << settings.iblBrdfLutPath << std::endl;
    s_ready = s_irradiance && s_prefilter && s_brdfLut;
    if (!s_ready) {
        s_irradiance = nullptr;
        s_prefilter = nullptr;
        s_brdfLut = nullptr;
        s_brdfLutTexId = -1;
    }
}

void Ibl_Bind(Shader3DVariant variant) {
    s_brdfLut = TextureGetSRV(s_brdfLutTexId);
    if (!s_context)
        return;

    ID3D11ShaderResourceView* nullSrvs[3] = {};
    if (variant != Shader3DVariant::PBR || !s_ready) {
        s_context->PSSetShaderResources(7, 3, nullSrvs);
        return;
    }

    s_context->PSSetShaderResources(7, 1, &s_irradiance);
    s_context->PSSetShaderResources(8, 1, &s_prefilter);
    s_context->PSSetShaderResources(9, 1, &s_brdfLut);
    if (s_sampler)
        s_context->PSSetSamplers(2, 1, &s_sampler);
}

bool Ibl_IsReady() {
    return s_ready && RenderSettings_GetConst().iblEnabled;
}

float Ibl_Intensity() {
    return RenderSettings_GetConst().iblIntensity;
}
