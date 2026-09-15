/**===========================================================================

 File   :  [renderer.h]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef RENDERER_H
#define RENDERER_H

#include "main.h"

 // ・ｽZ・ｽ[・ｽt・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽX
#define SAFE_RELEASE(o) if (o) { (o)->Release(); o = NULL; }

// ・ｽ・ｽ・ｽ_・ｽ・ｽ・ｽC・ｽA・ｽE・ｽg
struct VERTEX_3D {
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;
};

// ・ｽ}・ｽe・ｽ・ｽ・ｽA・ｽ・ｽ
struct MATERIAL {
    XMFLOAT4    Ambient;
    XMFLOAT4    Diffuse;
    XMFLOAT4    Specular;
    XMFLOAT4    Emission;
    float       Shininess;
    float       Roughness;      // 0..1 (MaterialComponent)
    float       Metallic;       // 0..1 (MaterialComponent)
    float       NormalStrength; // 0 = no normal map; >0 = strength
    // Toon / outline (ignored by non-toon shaders)
    float       ShadeSteps;
    float       ShadeSmoothness;
    float       SpecularThreshold;
    float       OutlineWidth;
    XMFLOAT4    OutlineColor;
    XMFLOAT4    ShadowTint;
    float       UseRamp;
    float       RimStrength;
    float       _padToon0;
    float       _padToon1;
};

// ・ｽ關費ｿｽo・ｽb・ｽt・ｽ@・ｽT・ｽC・ｽY・ｽ・ｽ 16 ・ｽo・ｽC・ｽg・ｽ・ｽ・ｽE・ｽ・ｽ・ｽ・ｽ
static_assert(sizeof(MATERIAL) % 16 == 0, "MATERIAL must be 16-byte aligned.");

// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ/・ｽI・ｽ・ｽ
HRESULT InitRenderer(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void    UninitRenderer(void);

// ・ｽ・ｽ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ
void    Clear(void);
void    Present(void);

// ・ｽf・ｽo・ｽC・ｽX・ｽ謫ｾ
ID3D11Device* GetDevice(void);
ID3D11DeviceContext* GetDeviceContext(void);

// ・ｽ[・ｽx・ｽe・ｽX・ｽg・ｽﾘゑｿｽﾖゑｿｽ
void    SetDepthEnable(bool Enable);
void    SetCullFront(bool enable);

// ・ｽ・ｽﾊサ・ｽC・ｽY
float   GetBackBufferWidth();
float   GetBackBufferHeight();
float   GetClientWidth();
float   GetClientHeight();

void    Renderer_ClientToBackbuffer(float clientX, float clientY, float& bufX, float& bufY);
void    Renderer_BackbufferToClient(float bufX, float bufY, float& clientX, float& clientY);
HRESULT Renderer_UpdateClientSize();
HRESULT Renderer_ResizeToClient();
HRESULT Renderer_SetWindowMode(BOOL bWindow);
BOOL    Renderer_IsWindowed();

void    SetRenderViewport(float x, float y, float width, float height);
void    ResetRenderViewport();
float   GetRenderViewportWidth();
float   GetRenderViewportHeight();

// Offscreen post-process color target (matches swapchain format/size).
HRESULT PostRT_Initialize();
void    PostRT_Shutdown();
HRESULT PostRT_Resize(UINT w, UINT h);
ID3D11RenderTargetView* PostRT_GetRTV();
ID3D11ShaderResourceView* PostRT_GetSRV();
void    Renderer_BindPostTarget();
void    Renderer_BindBackBuffer();

// ・ｽE・ｽB・ｽ・ｽ・ｽh・ｽE・ｽn・ｽ・ｽ・ｽh・ｽ・ｽ・ｽ謫ｾ
HWND    GetWindowHandle();

#endif // !RENDERER_H