/**===========================================================================

 File   :  [shader.h]

 Author : Terry Yeung
 Date   : 2025/10/29
 -----------------------------------------------------------------------------


 ===========================================================================**/
#ifndef SHADER_H
#define SHADER_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include "renderer.h"

constexpr int SHADER_MAX_LIGHTS = 4;
constexpr int SHADER_MAX_BONES = 128;

enum class Shader3DVariant {
	Standard,
	Hemisphere,
	Rim,
	SurfaceFX,
	Toon,
	PBR,
	DirectionalPixel,
	PointPixel,
	SpotPixel,
	Count
};

// CPU mirror of SurfaceFXBuffer (b4). Must stay 16-byte aligned.
struct SURFACE_FX_PARAMS {
	DirectX::XMFLOAT4 Color1 = { 0.15f, 0.55f, 0.75f, 0.85f };
	DirectX::XMFLOAT4 Color2 = { 0.02f, 0.12f, 0.28f, 0.95f };
	DirectX::XMFLOAT2 ScrollSpeed1 = { 0.03f, 0.01f };
	DirectX::XMFLOAT2 ScrollSpeed2 = { -0.02f, 0.025f };
	float UvScale = 0.08f;
	float FresnelPower = 3.0f;
	float EmissionStrength = 0.2f;
	// Gradient / edge fade are 0-1 fractions of mesh XZ radius (not absolute OS units).
	float GradientStart = 0.0f;
	float GradientEnd = 0.85f;
	float EdgeFadeStart = 0.85f;
	float EdgeFadeEnd = 1.0f;
};
static_assert(sizeof(SURFACE_FX_PARAMS) % 4 == 0, "SURFACE_FX_PARAMS packing");

bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_Finalize();

void Shader_SetProjectionMatrix(const DirectX::XMMATRIX& matrix);
void Shader_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_SetColor(const DirectX::XMFLOAT4& color);
void Shader_SetMaterial(const MATERIAL& material);
void Shader_SetSkinPalette(const DirectX::XMFLOAT4X4* bones, int boneCount, bool enabled);

void Shader_SetSurfaceFXParams(const SURFACE_FX_PARAMS& params);
void Shader_SetSurfaceFXMeshRadius(float radiusOS);
void Shader_CommitSurfaceFX(); // packs World + Time into b4 (call after SetWorldMatrix)

DirectX::XMMATRIX Shader_GetProjectionMatrix();
DirectX::XMMATRIX Shader_GetViewMatrix();
DirectX::XMMATRIX Shader_GetWorldMatrix();

void Shader_SetShadowMatrix(const DirectX::XMMATRIX& lightViewProj, float bias = 0.0015f);
void Shader_SetShadowMatrices(const DirectX::XMMATRIX* lightViewProjArray, int cascadeCount, float bias = 0.0015f, float slope = 1.5f);
// splits: view-space far depth per cascade; fadeLength: depth over which shadows fade to
// lit before the last split (Shadow::GetShadowFadeLength()).
void Shader_SetShadowCascadeParams(const DirectX::XMFLOAT4& splits, float fadeLength);
// Per-cascade ShadowParams (Shadow::GetCascadeReceiverParams): x const depth bias,
// y slope depth bias, z normal offset (world units), w blend width (view depth).
void Shader_SetShadowReceiverParams(const DirectX::XMFLOAT4* perCascade, int count);
// Which Lights[] slot EvalShadow shadows with the cascades (Shadow::GetCasterLightIndex()); -1 = none.
void Shader_SetShadowLightIndex(int lightIndex);
// Ambient kept inside a CSM shadow, 0..1 (render.shadowAmbientStrength); clamped.
void Shader_SetShadowAmbient(float ambientInShadow);

void Shader_BindShadowMap(ID3D11ShaderResourceView* shadowSRV);
void Shader_BindShadowMaps(ID3D11ShaderResourceView* const* shadowSRVs, int lightCount);
// Forgets the shadow SRVs remembered by the bind calls and unbinds them. Shadow::Finalize
// calls it before releasing the cascade textures (shadow quality change, scene teardown),
// so Shader3D never rebinds a released SRV.
void Shader_ClearShadowMaps();

void Shader_SetUseExternalVP(bool enable);
bool Shader_IsExternalVP();

void Shader2D_Begin();
void Shader3D_Begin(Shader3DVariant variant = Shader3DVariant::Standard);
void Shader3D_BeginOutline();
void Shader3D_UseGpuSkin();

Shader3DVariant Shader3DVariantFromId(const std::string& id);
const char* Shader3DVariantLabel(Shader3DVariant v);
const char* Shader3DVariantId(Shader3DVariant v);
int Shader3DVariantCount();
Shader3DVariant Shader3DVariantByIndex(int i);
bool Shader3DVariantIsUserVisible(Shader3DVariant v);

#endif // SHADER_H
