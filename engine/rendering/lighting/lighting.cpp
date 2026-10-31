/**===========================================================================

 File   :  [lighting.cpp]

 Author : Terry Yeung
 Date   : 2025/11/6
 -----------------------------------------------------------------------------


 ===========================================================================**/
#include "lighting.h"
#include <DirectXMath.h>
#include "camera.h"
#include "engine/scene/render_settings.h"
#include "engine/rendering/environment/ibl.h"
using namespace DirectX;

namespace {
    ID3D11Device* s_dev = nullptr;
    ID3D11DeviceContext* s_ctx = nullptr;
    ID3D11Buffer* s_cb = nullptr;

    Lighting::Params     s_params {};

    struct LightParamGPU {
        XMFLOAT3 DirOS;     float Type;     // 0:Directional, 1:Spot, 2:Point
        XMFLOAT3 Color;     float _pad1;
        XMFLOAT3 SpecColor; float SpecPower;
        XMFLOAT3 PosOS;     float Range;
        XMFLOAT3 Atten;     float InnerCos;
        float    OuterCos;  XMFLOAT3 _pad2;
    };
    struct CB_LIGHTING {
        LightParamGPU Lights[Lighting::MAX_LIGHTS];
        XMFLOAT3 Ambient; float _padA;
        XMFLOAT3 ViewDirOS; float LightCountF;
        XMFLOAT3 CameraPosWS; float FogEnabled;
        XMFLOAT3 FogColor; float FogDensity;
        float FogStart;
        float FogHeight;
        float FogHeightFalloff;
        float IblEnabled;
        float IblIntensity;
        float EnvRotationYRad;
        float _padIbl0;
        float _padIbl1;
        XMFLOAT4X4 World;
    } s_cbData {};
    static_assert(sizeof(CB_LIGHTING) % 16 == 0, "CB_LIGHTING must be 16-byte aligned");
}

namespace Lighting {

    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        Finalize();
        s_dev = dev; s_ctx = ctx;
        if (!s_dev || !s_ctx) return false;

        D3D11_BUFFER_DESC bd {};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(CB_LIGHTING);
        bd.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(s_dev->CreateBuffer(&bd, nullptr, &s_cb))) return false;

        // デフォルト（1本のディレクショナル）
        s_params.ambientColor = XMFLOAT3(0.15f, 0.15f, 0.15f);
        s_params.lightCount = 1;
        s_params.lights[0].type = DIRECTIONAL;
        s_params.lights[0].directionWS = XMFLOAT3(0, 0, 1);
        s_params.lights[0].color = XMFLOAT3(1, 1, 1);
        s_params.lights[0].specColor = XMFLOAT3(1, 1, 1);
        s_params.lights[0].specPower = 32.0f;

        // 一旦クリア
        ZeroMemory(&s_cbData, sizeof(s_cbData));
        s_cbData.Ambient = s_params.ambientColor;
        s_cbData.ViewDirOS = XMFLOAT3(0, 0, -1);
        s_cbData.LightCountF = float(s_params.lightCount);
        s_ctx->UpdateSubresource(s_cb, 0, nullptr, &s_cbData, 0, 0);

        return true;
    }

    void Finalize() {
        if (s_cb) { s_cb->Release(); s_cb = nullptr; }
        s_dev = nullptr; s_ctx = nullptr;
    }

    void ClearLights() {
        s_params.lightCount = 0;
    }
    void AddDirectionalLight(const Light& lIn) {
        if (s_params.lightCount >= MAX_LIGHTS) return;
        Light l = lIn;
        if (l.type != SPOT && l.type != POINT)
            l.type = DIRECTIONAL;
        s_params.lights[s_params.lightCount] = l;
        s_params.lightCount++;
    }

    void AddPointLight(const Light& lIn) {
        if (s_params.lightCount >= MAX_LIGHTS) return;
        Light l = lIn;
        l.type = POINT;
        s_params.lights[s_params.lightCount] = l;
        s_params.lightCount++;
    }

    void AddSpotLight(const Light& lIn) {
        if (s_params.lightCount >= MAX_LIGHTS) return;
        Light l = lIn;
        l.type = SPOT;
        s_params.lights[s_params.lightCount] = l;
        s_params.lightCount++;
    }

    void SetAmbientColor(const XMFLOAT3& col) { s_params.ambientColor = col; }

    XMFLOAT3 GetDirection() { return s_params.lights[0].directionWS; }
    Params   GetParams() { return s_params; }

    void UpdatePerObject(const XMMATRIX& world) {
        if (!s_ctx) return;

        // inv(world) : WS->OS
        XMVECTOR det;
        XMMATRIX invW = XMMatrixInverse(&det, world);

        // View（WS）
        const auto& cam = GetCamera();
        XMVECTOR camPos = XMLoadFloat3(&cam.position);
        XMVECTOR objOriginWS = world.r[3];
        XMVECTOR Vw = XMVector3Normalize(XMVectorSubtract(camPos, objOriginWS));
        XMVECTOR Vos = XMVector3Normalize(XMVector3TransformNormal(Vw, invW));
        XMStoreFloat3(&s_cbData.ViewDirOS, Vos);

        const int n = (s_params.lightCount > MAX_LIGHTS) ? MAX_LIGHTS : s_params.lightCount;
        for (int i = 0; i < n; ++i) {
            const Light& src = s_params.lights[i];

            // Direction（光が進む向き）。Directionalは -dir を使ってきた互換維持。
            XMVECTOR Lw = XMLoadFloat3(&src.directionWS);
            XMVECTOR Lp = XMVectorNegate(Lw); // 光が進む向き
            XMVECTOR Los = XMVector3Normalize(XMVector3TransformNormal(Lp, invW));

            // Position（Spot のみ使用）
            XMVECTOR Pw = XMLoadFloat3(&src.positionWS);
            // OS 座標は原点（オブジェクト座標）基準で変換
            XMVECTOR PosOS = XMVector3TransformCoord(Pw, invW);

            XMStoreFloat3(&s_cbData.Lights[i].DirOS, Los);
            s_cbData.Lights[i].Type = float(src.type);
            s_cbData.Lights[i].Color = src.color;
            s_cbData.Lights[i].SpecColor = src.specColor;
            s_cbData.Lights[i].SpecPower = src.specPower;
            XMStoreFloat3(&s_cbData.Lights[i].PosOS, PosOS);
            s_cbData.Lights[i].Range = src.range;
            s_cbData.Lights[i].Atten = src.attenuation;
            s_cbData.Lights[i].InnerCos = src.innerCos;
            s_cbData.Lights[i].OuterCos = src.outerCos;
        }
        // 残りはゼロクリア
        for (int i = n; i < MAX_LIGHTS; ++i) {
            s_cbData.Lights[i] = {};
        }

        s_cbData.Ambient = s_params.ambientColor;
        s_cbData.LightCountF = float(s_params.lightCount);

        const RenderSettingsData& rs = RenderSettings_GetConst();
        s_cbData.CameraPosWS = cam.position;
        s_cbData.FogEnabled = rs.fogEnabled ? 1.0f : 0.0f;
        s_cbData.FogColor = rs.fogColor;
        s_cbData.FogDensity = rs.fogDensity;
        s_cbData.FogStart = rs.fogStart;
        s_cbData.FogHeight = rs.fogHeight;
        s_cbData.FogHeightFalloff = rs.fogHeightFalloff;
        s_cbData.IblEnabled = Ibl_IsReady() ? 1.0f : 0.0f;
        s_cbData.IblIntensity = Ibl_Intensity();
        s_cbData.EnvRotationYRad = XMConvertToRadians(rs.skyboxRotationYDeg);
        s_cbData._padIbl0 = 0.0f;
        s_cbData._padIbl1 = 0.0f;
        XMStoreFloat4x4(&s_cbData.World, XMMatrixTranspose(world));

        s_ctx->UpdateSubresource(s_cb, 0, nullptr, &s_cbData, 0, 0);
    }

    void Bind() {
        if (!s_ctx || !s_cb) return;
        s_ctx->VSSetConstantBuffers(3, 1, &s_cb);
        s_ctx->PSSetConstantBuffers(3, 1, &s_cb);
    }

} // namespace Lighting