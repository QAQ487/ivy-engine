/**===========================================================================

 File   :  [lighting.h]

 Author : Terry Yeung
 Date   : 2025/11/6
 -----------------------------------------------------------------------------


 ===========================================================================**/
#ifndef LIGHTING_H
#define LIGHTING_H

#include <d3d11.h>
#include <DirectXMath.h>

namespace Lighting {

    constexpr int MAX_LIGHTS = 4;

    enum LightType {
        DIRECTIONAL = 0,
        SPOT = 1,
        POINT = 2,
    };

    struct Light {
        // Directional
        DirectX::XMFLOAT3 directionWS {0, 0, 1}; // Light travel direction (incoming negated for directional)
        DirectX::XMFLOAT3 color {1, 1, 1};
        DirectX::XMFLOAT3 specColor {1, 1, 1};
        float             specPower = 32.0f;

        // Type
        int               type = DIRECTIONAL;

        // Spot / point
        DirectX::XMFLOAT3 positionWS {0, 0, 0};
        float             range = 100.0f;     // Falloff range (spot/point)
        DirectX::XMFLOAT3 attenuation {1.0f, 0.0f, 1.0f}; // (kc, kl, kq)
        float             innerCos = 0.95f;      // Inner cone angle cosine
        float             outerCos = 0.85f;      // Outer cone angle cosine
    };

    struct Params {
        DirectX::XMFLOAT3 ambientColor {0.15f, 0.15f, 0.15f};
        Light             lights[MAX_LIGHTS];
        int               lightCount = 1;
    };

    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void Finalize();

    // Lights
    void ClearLights();
    void AddDirectionalLight(const Light& l);
    void AddPointLight(const Light& l);
    void AddSpotLight(const Light& l);
    void SetAmbientColor(const DirectX::XMFLOAT3& col);

    // Accessors
    Params GetParams();
    DirectX::XMFLOAT3 GetDirection(); // Primary light direction

    // Per-object lighting (object-space)
    void UpdatePerObject(const DirectX::XMMATRIX& world);

    // Bind constant buffer b3
    void Bind();

} // namespace Lighting

#endif // !LIGHTING_H