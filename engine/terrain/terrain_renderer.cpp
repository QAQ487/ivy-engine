// terrain_renderer.cpp
// See terrain_renderer.h (ADR 0011).

#include "engine/terrain/terrain_renderer.h"

#include "engine/ecs/terrain_component.h"
#include "engine/terrain/terrain_draw_plan.h"
#include "engine/core/content_path.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/rendering/culling/frustum_cull.h"
#include "engine/rendering/culling/lod_state.h"
#include "engine/rendering/culling/shadow_cascade_cull.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/lighting/shadow.h"
#include "engine/rendering/lighting/shadow_volume.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/scene/render_settings.h"
#include "camera.h"

#include <d3d11.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>

using namespace DirectX;

namespace {

// Must match TerrainBuffer in VertexShaderTerrain.hlsl.
struct TerrainCB {
    float cellSizeX;
    float cellSizeZ;
    float heightScale;
    float heightOffset;
    float lastSample;
    float tileSize;
    float pad[2];
};
static_assert(sizeof(TerrainCB) % 16 == 0, "TerrainCB must be 16-byte aligned");

// Must match TerrainPixelBuffer in LightingCommon.hlsli (TERRAIN_SPLAT).
struct TerrainPixelCB {
    float layerTileInv[4];
    float sizeInvX;
    float sizeInvZ;
    float layerCount;
    float splatTexels;
};
static_assert(sizeof(TerrainPixelCB) % 16 == 0, "TerrainPixelCB must be 16-byte aligned");

constexpr UINT kTerrainCBSlot = 6;
constexpr UINT kHeightSrvSlot = 10;
constexpr UINT kTerrainPixelCBSlot = 7;
constexpr UINT kSplatSrvSlot = 11;   // layers 1..3 at 12..14, layer 0 at t0

struct ChunkMesh {
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    std::vector<UINT> levelStart; // first index of each LOD level in ib
    std::vector<UINT> levelCount; // index count of each LOD level
};

ID3D11VertexShader* s_vs = nullptr;
ID3D11InputLayout* s_il = nullptr;
ID3D11Buffer* s_cb = nullptr;
ID3D11Buffer* s_pixelCb = nullptr;
// Terrain pixel shaders: Standard, PBR, Toon (other scene shaders use Standard).
ID3D11PixelShader* s_ps[3] = {};
ID3D11Buffer* s_instances = nullptr;
UINT s_instanceCapacity = 0;
bool s_sharedFailed = false;
std::map<int, ChunkMesh> s_meshes; // by chunkQuads

std::vector<std::vector<terrain::ChunkInstance>> s_perLevel; // scratch

template <class T> void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

bool EnsureShared() {
    if (s_vs && s_il && s_cb && s_pixelCb && s_ps[0] && s_ps[1] && s_ps[2])
        return true;
    if (s_sharedFailed)
        return false;
    ID3D11Device* dev = GetDevice();
    if (!dev)
        return false;

    std::ifstream f(content_path::ReadableFileCurrent("rom/shader/VertexShaderTerrain.cso"),
                    std::ios::binary | std::ios::ate);
    std::vector<char> code;
    if (f) {
        code.resize(static_cast<size_t>(f.tellg()));
        f.seekg(0, std::ios::beg);
        f.read(code.data(), static_cast<std::streamsize>(code.size()));
    }
    if (!f || code.empty()) {
        hal::DebugWriteA("[Terrain] cannot open rom/shader/VertexShaderTerrain.cso "
                         "(build the project to generate .cso files); terrain is not drawn\n");
        s_sharedFailed = true;
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    D3D11_BUFFER_DESC cbd {};
    cbd.ByteWidth = sizeof(TerrainCB);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_BUFFER_DESC pcbd = cbd;
    pcbd.ByteWidth = sizeof(TerrainPixelCB);
    bool ok = SUCCEEDED(dev->CreateVertexShader(code.data(), code.size(), nullptr, &s_vs))
        && SUCCEEDED(dev->CreateInputLayout(layout, ARRAYSIZE(layout), code.data(), code.size(), &s_il))
        && SUCCEEDED(dev->CreateBuffer(&cbd, nullptr, &s_cb))
        && SUCCEEDED(dev->CreateBuffer(&pcbd, nullptr, &s_pixelCb));
    const char* psFiles[3] = { "rom/shader/PixelShaderTerrain.cso", "rom/shader/PixelShaderTerrain_PBR.cso",
                               "rom/shader/PixelShaderTerrain_Toon.cso" };
    for (int i = 0; ok && i < 3; ++i) {
        std::ifstream pf(content_path::ReadableFileCurrent(psFiles[i]), std::ios::binary | std::ios::ate);
        std::vector<char> ps;
        if (pf) {
            ps.resize(static_cast<size_t>(pf.tellg()));
            pf.seekg(0, std::ios::beg);
            pf.read(ps.data(), static_cast<std::streamsize>(ps.size()));
        }
        ok = pf && !ps.empty() && SUCCEEDED(dev->CreatePixelShader(ps.data(), ps.size(), nullptr, &s_ps[i]));
        if (!ok)
            hal::DebugWriteA((std::string("[Terrain] cannot load ") + psFiles[i] + "\n").c_str());
    }
    if (!ok) {
        hal::DebugWriteA("[Terrain] creating the terrain shaders failed; terrain is not drawn\n");
        SafeRelease(s_vs);
        SafeRelease(s_il);
        SafeRelease(s_cb);
        SafeRelease(s_pixelCb);
        for (auto*& p : s_ps) SafeRelease(p);
        s_sharedFailed = true;
        return false;
    }
    return true;
}

const ChunkMesh* EnsureMesh(int chunkQuads) {
    auto it = s_meshes.find(chunkQuads);
    if (it != s_meshes.end())
        return &it->second;
    ID3D11Device* dev = GetDevice();
    if (!dev)
        return nullptr;

    // Vertices: (x, skirt, z) for the surface grid, then the same grid flagged as skirt.
    const int row = chunkQuads + 1;
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(terrain::ChunkVertexCount(chunkQuads)) * 3);
    for (int skirt = 0; skirt < 2; ++skirt)
        for (int z = 0; z < row; ++z)
            for (int x = 0; x < row; ++x)
                vertices.insert(vertices.end(), {static_cast<float>(x), static_cast<float>(skirt), static_cast<float>(z)});

    ChunkMesh mesh;
    std::vector<std::uint32_t> indices;
    const int maxLevel = terrain::MaxLodLevel(chunkQuads);
    for (int level = 0; level <= maxLevel; ++level) {
        std::vector<std::uint32_t> levelIndices;
        if (!terrain::BuildChunkMeshIndices(chunkQuads, 1 << level, levelIndices))
            return nullptr;
        mesh.levelStart.push_back(static_cast<UINT>(indices.size()));
        mesh.levelCount.push_back(static_cast<UINT>(levelIndices.size()));
        indices.insert(indices.end(), levelIndices.begin(), levelIndices.end());
    }

    D3D11_BUFFER_DESC vbd {};
    vbd.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(float));
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit {};
    vinit.pSysMem = vertices.data();
    D3D11_BUFFER_DESC ibd {};
    ibd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit {};
    iinit.pSysMem = indices.data();
    if (FAILED(dev->CreateBuffer(&vbd, &vinit, &mesh.vb)) || FAILED(dev->CreateBuffer(&ibd, &iinit, &mesh.ib))) {
        SafeRelease(mesh.vb);
        SafeRelease(mesh.ib);
        hal::DebugWriteA("[Terrain] creating the chunk mesh failed\n");
        return nullptr;
    }
    return &s_meshes.emplace(chunkQuads, mesh).first->second;
}

bool EnsureInstanceCapacity(UINT count) {
    if (count <= s_instanceCapacity && s_instances)
        return true;
    ID3D11Device* dev = GetDevice();
    if (!dev)
        return false;
    SafeRelease(s_instances);
    s_instanceCapacity = 0;
    const UINT capacity = std::max<UINT>(count, 256);
    D3D11_BUFFER_DESC bd {};
    bd.ByteWidth = capacity * static_cast<UINT>(sizeof(terrain::ChunkInstance));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &s_instances)))
        return false;
    s_instanceCapacity = capacity;
    return true;
}

ID3D11ShaderResourceView* EnsureHeightSrv(TerrainComponent& t) {
    if (ID3D11ShaderResourceView* srv = t.GetHeightSRV()) {
        // Sculpt edits: upload only the changed rectangle.
        const terrain::SampleRect r = terrain::ClipRect(t.PendingGpuRect(), t.GetHeightmap());
        if (!r.Empty()) {
            ID3D11Resource* res = nullptr;
            srv->GetResource(&res);
            if (res) {
                const terrain::Heightmap& map = t.GetHeightmap();
                D3D11_BOX box {};
                box.left = static_cast<UINT>(r.x0);
                box.top = static_cast<UINT>(r.z0);
                box.front = 0;
                box.right = static_cast<UINT>(r.x1 + 1);
                box.bottom = static_cast<UINT>(r.z1 + 1);
                box.back = 1;
                const std::uint16_t* first =
                    &map.samples[static_cast<size_t>(r.z0) * map.resolution + r.x0];
                GetDeviceContext()->UpdateSubresource(res, 0, &box, first,
                    static_cast<UINT>(map.resolution) * sizeof(std::uint16_t), 0);
                res->Release();
            }
        }
        t.ClearPendingGpuRect();
        return srv;
    }
    t.ClearPendingGpuRect(); // the new texture below holds every sample
    ID3D11Device* dev = GetDevice();
    const terrain::Heightmap& map = t.GetHeightmap();
    if (!dev || !map.IsValid())
        return nullptr;

    D3D11_TEXTURE2D_DESC td {};
    td.Width = static_cast<UINT>(map.resolution);
    td.Height = static_cast<UINT>(map.resolution);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; // step 4 (sculpting) updates rectangles in place
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init {};
    init.pSysMem = map.samples.data();
    init.SysMemPitch = static_cast<UINT>(map.resolution) * sizeof(std::uint16_t);

    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &init, &tex)))
        return nullptr;
    const HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release(); // the view keeps the texture alive
    if (FAILED(hr))
        return nullptr;
    t.SetHeightSRV(srv);
    return srv;
}

ID3D11ShaderResourceView* EnsureSplatSrv(TerrainComponent& t) {
    if (!t.EnsureSplatLoaded())
        return nullptr;
    const terrain::SplatMap& map = t.GetSplat();
    if (ID3D11ShaderResourceView* srv = t.GetSplatSRV()) {
        const terrain::SampleRect r = terrain::ClipSplatRect(t.PendingSplatRect(), map);
        if (!r.Empty()) {
            ID3D11Resource* res = nullptr;
            srv->GetResource(&res);
            if (res) {
                D3D11_BOX box {};
                box.left = static_cast<UINT>(r.x0);
                box.top = static_cast<UINT>(r.z0);
                box.right = static_cast<UINT>(r.x1 + 1);
                box.bottom = static_cast<UINT>(r.z1 + 1);
                box.back = 1;
                GetDeviceContext()->UpdateSubresource(res, 0, &box,
                    &map.rgba[(static_cast<size_t>(r.z0) * map.resolution + r.x0) * 4],
                    static_cast<UINT>(map.resolution) * 4, 0);
                res->Release();
            }
        }
        t.ClearPendingSplatRect();
        return srv;
    }
    t.ClearPendingSplatRect();
    ID3D11Device* dev = GetDevice();
    if (!dev)
        return nullptr;
    D3D11_TEXTURE2D_DESC td {};
    td.Width = static_cast<UINT>(map.resolution);
    td.Height = static_cast<UINT>(map.resolution);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; // paint strokes update rectangles in place
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init {};
    init.pSysMem = map.rgba.data();
    init.SysMemPitch = static_cast<UINT>(map.resolution) * 4;
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &init, &tex)))
        return nullptr;
    const HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    if (FAILED(hr))
        return nullptr;
    t.SetSplatSRV(srv);
    return srv;
}

MATERIAL TerrainMaterial(const terrain::Settings& s, bool hasTexture) {
    MATERIAL m {};
    m.Diffuse = XMFLOAT4(s.colorR, s.colorG, s.colorB, hasTexture ? 1.0f : 0.0f);
    m.Ambient = XMFLOAT4(0.30f, 0.28f, 0.25f, 0.6f);
    // Same roughness / metallic mapping as MaterialComponent.
    m.Shininess = 2.0f + (1.0f - s.roughness) * 254.0f;
    m.Roughness = s.roughness;
    m.Metallic = s.metallic;
    const float f0 = 0.04f;
    m.Specular = XMFLOAT4(f0 + (s.colorR - f0) * s.metallic, f0 + (s.colorG - f0) * s.metallic,
                          f0 + (s.colorB - f0) * s.metallic, 0.5f);
    m.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    m.NormalStrength = 0.0f;
    m.ShadeSteps = 3.0f;
    m.ShadeSmoothness = 0.05f;
    m.SpecularThreshold = 0.85f;
    m.OutlineWidth = 0.0f;
    m.OutlineColor = XMFLOAT4(0.05f, 0.05f, 0.08f, 1.0f);
    m.ShadowTint = XMFLOAT4(0.6f, 0.65f, 0.85f, 1.0f);
    m.UseRamp = 0.0f;
    m.RimStrength = 0.25f;
    return m;
}

} // namespace

void TerrainRenderer_Draw(TerrainComponent& t, const XMFLOAT3& worldPosition) {
    if (!t.settings.enabled || ShadowVolume::IsPassActive())
        return;
    const bool shadowPass = Shadow::GetRenderingLightIndex() >= 0;
    if (shadowPass && !t.settings.castShadows)
        return;
    if (!t.EnsureLoaded() || !EnsureShared())
        return;
    const terrain::ChunkGrid& grid = t.GetChunkGrid();
    const ChunkMesh* mesh = EnsureMesh(grid.chunkQuads);
    ID3D11ShaderResourceView* heightSrv = EnsureHeightSrv(t);
    ID3D11DeviceContext* ctx = GetDeviceContext();
    if (!mesh || !heightSrv || !ctx)
        return;

    const terrain::Layout layout = terrain::SanitizeLayout(t.settings.layout);
    const terrain::Heightmap& map = t.GetHeightmap();
    const RenderSettingsData& rs = RenderSettings_GetConst();
    const terrain::Float3 origin{worldPosition.x, worldPosition.y, worldPosition.z};

    // Visibility: camera frustum in the main pass, open-near light frustum per cascade.
    const bool mainCull = !shadowPass && rs.frustumCullingEnabled && FrustumCull_IsMainPassActive();
    const bool cascadeCull = shadowPass && rs.shadowCascadeCullingEnabled
        && ShadowCascadeCull_IsCascadePassActive();
    auto visible = [&](const terrain::Float3& mn, const terrain::Float3& mx) {
        frustum_cull::Aabb box;
        box.min = XMFLOAT3(mn.x, mn.y, mn.z);
        box.max = XMFLOAT3(mx.x, mx.y, mx.z);
        box.valid = true;
        if (mainCull)
            return frustum_cull::AabbIntersectsFrustum(box, FrustumCull_GetPlanes());
        if (cascadeCull)
            return frustum_cull::AabbIntersectsFrustumOpenNear(box, ShadowCascadeCull_GetPlanes());
        return true;
    };

    terrain::PassOptions options;
    const XMFLOAT3 cam = Lod_IsFrameActive() ? Lod_GetCameraPosition() : GetCamera().position;
    options.camera = {cam.x, cam.y, cam.z};
    options.lodEnabled = rs.lodEnabled;
    options.lodBias = t.settings.lodBias * rs.lodBias;
    options.levelOffset = shadowPass ? rs.shadowLodOffset : 0;
    options.storeLevels = !shadowPass;

    const int drawn = terrain::PlanChunkDraws(layout, map, grid, t.GetChunkBounds(), origin, options,
                                              visible, t.LodMemory(), s_perLevel);
    if (drawn <= 0 || !EnsureInstanceCapacity(static_cast<UINT>(drawn)))
        return;

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (FAILED(ctx->Map(s_instances, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::vector<UINT> firstInstance(s_perLevel.size(), 0);
    UINT written = 0;
    for (size_t level = 0; level < s_perLevel.size(); ++level) {
        firstInstance[level] = written;
        const auto& list = s_perLevel[level];
        if (list.empty())
            continue;
        std::memcpy(static_cast<terrain::ChunkInstance*>(mapped.pData) + written, list.data(),
                    list.size() * sizeof(terrain::ChunkInstance));
        written += static_cast<UINT>(list.size());
    }
    ctx->Unmap(s_instances, 0);

    // Regular 3D state (camera / light matrices, lights, shadows, IBL, the scene's pixel
    // shader), then swap in the terrain vertex shader.
    SetDepthEnable(true);
    if (!Shader_IsExternalVP()) {
        const auto& camera = GetCamera();
        Shader_SetProjectionMatrix(camera.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
        Shader_SetViewMatrix(camera.GetViewMatrix());
    }
    Shader_SetWorldMatrix(XMMatrixTranslation(worldPosition.x, worldPosition.y, worldPosition.z));
    Shader_SetSkinPalette(nullptr, 0, false);

    Shader3DVariant variant = RenderSettings_GetDefaultShader();
    if (variant == Shader3DVariant::SurfaceFX) // needs per-mesh SurfaceFX parameters
        variant = Shader3DVariant::Standard;
    Shader3D_Begin(variant);

    TerrainCB cb {};
    cb.cellSizeX = terrain::CellSizeX(layout, map);
    cb.cellSizeZ = terrain::CellSizeZ(layout, map);
    cb.heightScale = layout.heightScale;
    cb.heightOffset = layout.heightOffset;
    cb.lastSample = static_cast<float>(map.resolution - 1);
    cb.tileSize = t.settings.textureTileSize;
    ctx->UpdateSubresource(s_cb, 0, nullptr, &cb, 0, 0);

    ctx->VSSetShader(s_vs, nullptr, 0);
    ctx->IASetInputLayout(s_il);
    ctx->VSSetConstantBuffers(kTerrainCBSlot, 1, &s_cb);
    ctx->VSSetShaderResources(kHeightSrvSlot, 1, &heightSrv);

    if (!shadowPass) {
        // Terrain pixel shader for the scene's lighting model; layers blended by the splat map.
        const int psIndex = (variant == Shader3DVariant::PBR) ? 1 : (variant == Shader3DVariant::Toon ? 2 : 0);
        ctx->PSSetShader(s_ps[psIndex], nullptr, 0);

        const std::vector<terrain::LayerSettings> layers = terrain::EffectiveLayers(t.settings);
        ID3D11ShaderResourceView* white = TextureGetSRV(TextureGetWhiteTexture());
        ID3D11ShaderResourceView* layerSrv[terrain::kMaxLayers] = {};
        TerrainPixelCB pcb {};
        for (int i = 0; i < terrain::kMaxLayers; ++i) {
            ID3D11ShaderResourceView* srv = i < static_cast<int>(layers.size()) ? t.GetLayerSRV(i) : nullptr;
            layerSrv[i] = srv ? srv : white;
            const float tile = i < static_cast<int>(layers.size()) ? layers[i].tileSize : 8.0f;
            pcb.layerTileInv[i] = 1.0f / (tile > 0.0f ? tile : 8.0f);
        }
        ID3D11ShaderResourceView* splat = EnsureSplatSrv(t);
        pcb.sizeInvX = 1.0f / layout.sizeX;
        pcb.sizeInvZ = 1.0f / layout.sizeZ;
        // Without a usable splat map only layer 0 shows.
        pcb.layerCount = splat ? static_cast<float>(layers.size()) : 1.0f;
        pcb.splatTexels = splat ? static_cast<float>(t.GetSplat().resolution) : 2.0f;
        ctx->UpdateSubresource(s_pixelCb, 0, nullptr, &pcb, 0, 0);
        ctx->PSSetConstantBuffers(kTerrainPixelCBSlot, 1, &s_pixelCb);

        Shader_SetMaterial(TerrainMaterial(t.settings, true));
        ID3D11ShaderResourceView* nullSrv = nullptr;
        ctx->PSSetShaderResources(0, 1, &layerSrv[0]);
        ctx->PSSetShaderResources(5, 1, &nullSrv); // no normal map
        ctx->PSSetShaderResources(6, 1, &nullSrv); // no toon ramp
        ID3D11ShaderResourceView* extra[4] = { splat ? splat : white, layerSrv[1], layerSrv[2], layerSrv[3] };
        ctx->PSSetShaderResources(kSplatSrvSlot, 4, extra);
    }

    ID3D11Buffer* buffers[2] = {mesh->vb, s_instances};
    const UINT strides[2] = {3 * sizeof(float), sizeof(terrain::ChunkInstance)};
    const UINT offsets[2] = {0, 0};
    ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    ctx->IASetIndexBuffer(mesh->ib, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (size_t level = 0; level < s_perLevel.size() && level < mesh->levelCount.size(); ++level) {
        const UINT count = static_cast<UINT>(s_perLevel[level].size());
        if (count == 0)
            continue;
        ctx->DrawIndexedInstanced(mesh->levelCount[level], count, mesh->levelStart[level], 0,
                                  firstInstance[level]);
    }

    // Leave nothing terrain-specific bound for the next draw.
    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->VSSetShaderResources(kHeightSrvSlot, 1, &nullSrv);
    ID3D11ShaderResourceView* nullExtra[4] = {};
    ctx->PSSetShaderResources(kSplatSrvSlot, 4, nullExtra);
    ID3D11Buffer* nullBuffer = nullptr;
    const UINT zero = 0;
    ctx->IASetVertexBuffers(1, 1, &nullBuffer, &zero, &zero);
    Shader3D_Begin(RenderSettings_GetDefaultShader());
}

void TerrainRenderer_Finalize() {
    for (auto& kv : s_meshes) {
        SafeRelease(kv.second.vb);
        SafeRelease(kv.second.ib);
    }
    s_meshes.clear();
    SafeRelease(s_instances);
    s_instanceCapacity = 0;
    SafeRelease(s_cb);
    SafeRelease(s_pixelCb);
    for (auto*& p : s_ps) SafeRelease(p);
    SafeRelease(s_il);
    SafeRelease(s_vs);
    s_sharedFailed = false;
    s_perLevel.clear();
}
