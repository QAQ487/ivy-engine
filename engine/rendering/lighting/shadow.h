/**===========================================================================

 File   :  [shadow.h]

 Author : Terry Yeung
 Date   : 2025/10/29
 -----------------------------------------------------------------------------
 Cascaded directional shadow maps (camera-fitted ortho volumes + texel snap).

 ===========================================================================**/
#ifndef SHADOW_H
#define SHADOW_H
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

namespace Shadow {

    // Texture / matrix slots are cascades for a single directional caster.
    constexpr int MAX_CASCADES = 4;
    constexpr int MAX_LIGHTS = MAX_CASCADES; // legacy alias (casters maps historically)

    constexpr int MAX_CASTERS = 1;

    struct CameraShadowParams {
        DirectX::XMFLOAT3 eye {0, 0, 0};
        DirectX::XMFLOAT3 at {0, 0, 1};
        DirectX::XMFLOAT3 up {0, 1, 0};
        float fovYDegrees = 45.0f;
        float nearClip = 0.5f;
        float farClip = 1000.0f;
        float aspect = 16.0f / 9.0f;
    };

    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, UINT size = 1024, int cascadeCount = MAX_CASCADES);
    // Rebuild maps if size/cascade count changed (keeps existing device/context).
    bool Reconfigure(UINT size, int cascadeCount);
    void Finalize();

    void SetLightDirection(int index, const DirectX::XMFLOAT3& dir);
    // Whether a light that casts shadows was found by the last lighting sync (a
    // DirectionalLight with castShadows, or the fallback light of a scene without
    // lights). Read by the shadow-volume pass; the CSM path does not use it yet.
    void SetCasterActive(bool active);
    bool IsCasterActive();
    DirectX::XMFLOAT3 GetLightDirection(int index = 0);

    // Rebuild cascade view/proj from the active camera (call once per frame before shadow passes).
    void UpdateForCamera(const CameraShadowParams& cam, float maxShadowDistance = 60.0f);

    int GetCascadeCount();
    int GetLightCount(); // alias: cascade count (depth-pass loop)
    int GetMaxCasters();

    DirectX::XMFLOAT4 GetCascadeSplits(); // view-space far Z per cascade (xyzw = 0..3)
    float GetCascadeBlend();

    DirectX::XMMATRIX GetLightView(int cascadeIndex);
    DirectX::XMMATRIX GetLightProj(int cascadeIndex);
    DirectX::XMMATRIX GetLightViewProj(int cascadeIndex);

    void Begin(int cascadeIndex);
    void End();

    ID3D11ShaderResourceView* GetShadowSRV(int cascadeIndex);
    UINT GetShadowMapSize();
    float GetReceiverBias();
    float GetTexelSize();

    // Cascade currently being rendered during the shadow pass; -1 when not in a shadow pass
    int GetRenderingLightIndex();

    void SetBackfaceDepth(bool enable);
    bool IsBackfaceDepthEnabled();
}

#endif // !SHADOW_H
