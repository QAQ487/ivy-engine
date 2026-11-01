// skybox.cpp
// Implements CB_Skybox.

#include "skybox.h"

#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/scene/render_settings.h"
#include "engine/ecs/string_utils.h"
#include "engine/core/content_path.h"

#include <fstream>
#include <vector>

using namespace DirectX;

struct CB_Skybox {
    XMFLOAT4X4 viewRot;
    XMFLOAT4X4 proj;
    XMFLOAT4X4 rotationY;
};

struct CB_SkyboxParams {
    XMFLOAT4 tint;
    float exposure;
    XMFLOAT3 padding;
};

struct SkyboxVertex {
    XMFLOAT3 position;
};

static ID3D11Device* s_device = nullptr;
static ID3D11DeviceContext* s_context = nullptr;
static ID3D11VertexShader* s_vs = nullptr;
static ID3D11PixelShader* s_ps = nullptr;
static ID3D11InputLayout* s_inputLayout = nullptr;
static ID3D11Buffer* s_vertexBuffer = nullptr;
static ID3D11Buffer* s_cbSkybox = nullptr;
static ID3D11Buffer* s_cbParams = nullptr;
static ID3D11SamplerState* s_sampler = nullptr;
static ID3D11RasterizerState* s_rasterCullFront = nullptr;
static ID3D11DepthStencilState* s_depthReadOnly = nullptr;
static ID3D11ShaderResourceView* s_cubemapSrv = nullptr;
static std::wstring s_loadedCubemapPath;

static bool LoadCompiledShader(const char* path, ID3D11Device* device,
    ID3D11VertexShader** outVs, ID3D11PixelShader** outPs)
{
    std::ifstream file(content_path::ReadableFileCurrent(path), std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    std::vector<char> data(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(data.data(), static_cast<std::streamsize>(data.size()));

    if (outVs) {
        if (FAILED(device->CreateVertexShader(data.data(), data.size(), nullptr, outVs)))
            return false;
    }
    if (outPs) {
        if (FAILED(device->CreatePixelShader(data.data(), data.size(), nullptr, outPs)))
            return false;
    }
    return true;
}

static XMMATRIX StripTranslation(XMMATRIX view) {
    view.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    return view;
}

bool Skybox_Initialize() {
    s_device = GetDevice();
    s_context = GetDeviceContext();
    if (!s_device || !s_context) return false;

    std::ifstream vsFile(content_path::ReadableFileCurrent("rom/shader/SkyboxVS.cso"), std::ios::binary);
    if (!vsFile) {
        MessageBoxA(nullptr,
            "Cannot open rom/shader/SkyboxVS.cso\nPlease build the project to generate .cso files.",
            "Skybox VS Load Error", MB_OK | MB_ICONERROR);
        return false;
    }
    vsFile.seekg(0, std::ios::end);
    std::vector<char> vsData(static_cast<size_t>(vsFile.tellg()));
    vsFile.seekg(0, std::ios::beg);
    vsFile.read(vsData.data(), static_cast<std::streamsize>(vsData.size()));

    if (FAILED(s_device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &s_vs)))
        return false;

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    if (FAILED(s_device->CreateInputLayout(layout, 1, vsData.data(), vsData.size(), &s_inputLayout)))
        return false;

    if (!LoadCompiledShader("rom/shader/SkyboxPS.cso", s_device, nullptr, &s_ps))
        return false;

    static const SkyboxVertex vertices[] = {
        { {-1.0f, -1.0f, -1.0f} }, { { 1.0f, -1.0f, -1.0f} }, { { 1.0f,  1.0f, -1.0f} },
        { {-1.0f, -1.0f, -1.0f} }, { { 1.0f,  1.0f, -1.0f} }, { {-1.0f,  1.0f, -1.0f} },
        { {-1.0f, -1.0f,  1.0f} }, { { 1.0f,  1.0f,  1.0f} }, { { 1.0f, -1.0f,  1.0f} },
        { {-1.0f, -1.0f,  1.0f} }, { {-1.0f,  1.0f,  1.0f} }, { { 1.0f,  1.0f,  1.0f} },
        { {-1.0f, -1.0f,  1.0f} }, { { 1.0f, -1.0f,  1.0f} }, { { 1.0f, -1.0f, -1.0f} },
        { {-1.0f, -1.0f,  1.0f} }, { { 1.0f, -1.0f, -1.0f} }, { {-1.0f, -1.0f, -1.0f} },
        { {-1.0f,  1.0f, -1.0f} }, { { 1.0f,  1.0f, -1.0f} }, { { 1.0f,  1.0f,  1.0f} },
        { {-1.0f,  1.0f, -1.0f} }, { { 1.0f,  1.0f,  1.0f} }, { {-1.0f,  1.0f,  1.0f} },
        { { 1.0f, -1.0f, -1.0f} }, { { 1.0f, -1.0f,  1.0f} }, { { 1.0f,  1.0f,  1.0f} },
        { { 1.0f, -1.0f, -1.0f} }, { { 1.0f,  1.0f,  1.0f} }, { { 1.0f,  1.0f, -1.0f} },
        { {-1.0f, -1.0f,  1.0f} }, { {-1.0f, -1.0f, -1.0f} }, { {-1.0f,  1.0f, -1.0f} },
        { {-1.0f, -1.0f,  1.0f} }, { {-1.0f,  1.0f, -1.0f} }, { {-1.0f,  1.0f,  1.0f} }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;
    if (FAILED(s_device->CreateBuffer(&vbDesc, &vbData, &s_vertexBuffer)))
        return false;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(CB_Skybox);
    if (FAILED(s_device->CreateBuffer(&cbDesc, nullptr, &s_cbSkybox)))
        return false;

    cbDesc.ByteWidth = sizeof(CB_SkyboxParams);
    if (FAILED(s_device->CreateBuffer(&cbDesc, nullptr, &s_cbParams)))
        return false;

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(s_device->CreateSamplerState(&samplerDesc, &s_sampler)))
        return false;

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    if (FAILED(s_device->CreateRasterizerState(&rasterDesc, &s_rasterCullFront)))
        return false;

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(s_device->CreateDepthStencilState(&depthDesc, &s_depthReadOnly)))
        return false;

    Skybox_ReloadFromSettings();
    return true;
}

void Skybox_Finalize() {
    // Cubemap SRV lifetime owned by texture cache (CubemapReleaseAll).
    s_cubemapSrv = nullptr;
    s_loadedCubemapPath.clear();

    SAFE_RELEASE(s_depthReadOnly);
    SAFE_RELEASE(s_rasterCullFront);
    SAFE_RELEASE(s_sampler);
    SAFE_RELEASE(s_cbParams);
    SAFE_RELEASE(s_cbSkybox);
    SAFE_RELEASE(s_vertexBuffer);
    SAFE_RELEASE(s_inputLayout);
    SAFE_RELEASE(s_ps);
    SAFE_RELEASE(s_vs);
    s_device = nullptr;
    s_context = nullptr;
}

void Skybox_ReloadFromSettings() {
    const RenderSettingsData& settings = RenderSettings_GetConst();
    const std::wstring path = ToWString(settings.skyboxCubemapPath);

    if (!settings.skyboxEnabled || path.empty()) {
        s_cubemapSrv = nullptr;
        s_loadedCubemapPath.clear();
        return;
    }



    s_loadedCubemapPath.clear();

    s_cubemapSrv = CubemapLoad(path.c_str());
    if (s_cubemapSrv)
        s_loadedCubemapPath = path;
}

void Skybox_Draw(const XMMATRIX& view, const XMMATRIX& proj) {
    if (!s_context || !s_vs || !s_ps || !s_cubemapSrv)
        return;

    const RenderSettingsData& settings = RenderSettings_GetConst();
    if (!settings.skyboxEnabled)
        return;

    ID3D11VertexShader* prevVs = nullptr;
    ID3D11PixelShader* prevPs = nullptr;
    ID3D11InputLayout* prevIl = nullptr;
    ID3D11Buffer* prevVb = nullptr;
    UINT prevStride = 0;
    UINT prevOffset = 0;
    ID3D11RasterizerState* prevRaster = nullptr;
    ID3D11DepthStencilState* prevDepth = nullptr;
    UINT prevStencilRef = 0;
    ID3D11Buffer* prevVsCb0 = nullptr;
    ID3D11Buffer* prevPsCb0 = nullptr;
    ID3D11SamplerState* prevSampler = nullptr;
    ID3D11ShaderResourceView* prevSrv = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY prevTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

    s_context->VSGetShader(&prevVs, nullptr, nullptr);
    s_context->PSGetShader(&prevPs, nullptr, nullptr);
    s_context->IAGetInputLayout(&prevIl);
    s_context->IAGetPrimitiveTopology(&prevTopology);
    s_context->IAGetVertexBuffers(0, 1, &prevVb, &prevStride, &prevOffset);
    s_context->RSGetState(&prevRaster);
    s_context->OMGetDepthStencilState(&prevDepth, &prevStencilRef);
    s_context->VSGetConstantBuffers(0, 1, &prevVsCb0);
    s_context->PSGetConstantBuffers(0, 1, &prevPsCb0);
    s_context->PSGetSamplers(0, 1, &prevSampler);
    s_context->PSGetShaderResources(0, 1, &prevSrv);

    const float rotationRad = XMConvertToRadians(settings.skyboxRotationYDeg);
    const XMMATRIX rotationY = XMMatrixRotationY(rotationRad);

    CB_Skybox cb = {};
    XMStoreFloat4x4(&cb.viewRot, XMMatrixTranspose(StripTranslation(view)));
    XMStoreFloat4x4(&cb.proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&cb.rotationY, XMMatrixTranspose(rotationY));
    s_context->UpdateSubresource(s_cbSkybox, 0, nullptr, &cb, 0, 0);

    CB_SkyboxParams params = {};
    params.tint = settings.skyboxTint;
    params.exposure = settings.skyboxExposure;
    s_context->UpdateSubresource(s_cbParams, 0, nullptr, &params, 0, 0);

    s_context->VSSetShader(s_vs, nullptr, 0);
    s_context->PSSetShader(s_ps, nullptr, 0);
    s_context->IASetInputLayout(s_inputLayout);
    s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = sizeof(SkyboxVertex);
    UINT offset = 0;
    s_context->IASetVertexBuffers(0, 1, &s_vertexBuffer, &stride, &offset);

    if (s_rasterCullFront)
        s_context->RSSetState(s_rasterCullFront);
    if (s_depthReadOnly)
        s_context->OMSetDepthStencilState(s_depthReadOnly, 0);

    s_context->VSSetConstantBuffers(0, 1, &s_cbSkybox);
    s_context->PSSetConstantBuffers(0, 1, &s_cbParams);
    s_context->PSSetSamplers(0, 1, &s_sampler);
    s_context->PSSetShaderResources(0, 1, &s_cubemapSrv);

    s_context->Draw(36, 0);

    s_context->VSSetShader(prevVs, nullptr, 0);
    s_context->PSSetShader(prevPs, nullptr, 0);
    s_context->IASetInputLayout(prevIl);
    s_context->IASetPrimitiveTopology(prevTopology);
    s_context->IASetVertexBuffers(0, 1, &prevVb, &prevStride, &prevOffset);
    s_context->RSSetState(prevRaster);
    s_context->OMSetDepthStencilState(prevDepth, prevStencilRef);
    s_context->VSSetConstantBuffers(0, 1, &prevVsCb0);
    s_context->PSSetConstantBuffers(0, 1, &prevPsCb0);
    s_context->PSSetSamplers(0, 1, &prevSampler);
    s_context->PSSetShaderResources(0, 1, &prevSrv);

    SAFE_RELEASE(prevVs);
    SAFE_RELEASE(prevPs);
    SAFE_RELEASE(prevIl);
    SAFE_RELEASE(prevVb);
    SAFE_RELEASE(prevRaster);
    SAFE_RELEASE(prevDepth);
    SAFE_RELEASE(prevVsCb0);
    SAFE_RELEASE(prevPsCb0);
    SAFE_RELEASE(prevSampler);
    SAFE_RELEASE(prevSrv);
}
