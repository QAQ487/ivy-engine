/**===========================================================================

 File   :  [shader.cpp]

 Author : Terry
 Date   : 2025/10/16
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

#include "renderer.h"
#include "engine/debug_draw/debug_ofstream.h"
#include <fstream>
#include <vector>
#include <cstdio>
#include "shader.h"
#include "../lighting/shadow.h"
#include "../lighting/lighting.h"
#include "engine/core/system_timer.h"

// MATERIAL �T�C�Y����
static_assert(sizeof(MATERIAL) % 16 == 0, "MATERIAL constant buffer must be 16-byte aligned.");

static constexpr int MAX_LIGHTS = SHADER_MAX_LIGHTS;

/**---------------------------------------------------------------------------
// �f�o�C�X�^�R���e�L�X�g
---------------------------------------------------------------------------**/
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

// ���ʃT���v���[
static ID3D11SamplerState* g_pSamplerState = nullptr;

/**---------------------------------------------------------------------------
// 2D
---------------------------------------------------------------------------**/
static ID3D11VertexShader* g_pVS2D = nullptr;
static ID3D11PixelShader* g_pPS2D = nullptr;
static ID3D11InputLayout* g_pIL2D = nullptr;
static ID3D11Buffer* g_pCB2D_VP = nullptr;
static ID3D11Buffer* g_pCB2D_World = nullptr;
static ID3D11Buffer* g_pCB2D_Color = nullptr;

static XMMATRIX s_View2D = XMMatrixIdentity();
static XMMATRIX s_Proj2D = XMMatrixIdentity();
static XMMATRIX s_World2D = XMMatrixIdentity();

/**---------------------------------------------------------------------------
// 3D
---------------------------------------------------------------------------**/
static ID3D11VertexShader* g_pVS3D = nullptr;
static ID3D11VertexShader* g_pVS3DOutline = nullptr;
static ID3D11PixelShader* g_pPS3DOutline = nullptr;
static ID3D11PixelShader* g_pPS3DVariants[static_cast<int>(Shader3DVariant::Count)] = {};
static ID3D11InputLayout* g_pIL3D = nullptr;
static ID3D11InputLayout* g_pIL3DOutline = nullptr;

struct Shader3DVariantInfo {
	Shader3DVariant variant;
	const char* id;
	const char* label;
	const char* csoFile;
	bool userVisible;
};

static const Shader3DVariantInfo kShader3DVariants[] = {
	{ Shader3DVariant::Standard,         "standard",          "Standard (All Lights)", "rom/shader/PixelShader3D.cso",                 true },
	{ Shader3DVariant::Hemisphere,       "hemisphere",        "Hemisphere",            "rom/shader/PixelShader3D_Hemisphere.cso",      true },
	{ Shader3DVariant::Rim,              "rim",               "Rim",                   "rom/shader/PixelShader3D_Rim.cso",             true },
	{ Shader3DVariant::SurfaceFX,        "surface_fx",        "Surface FX",            "rom/shader/PixelShader3D_SurfaceFX.cso",       true },
	{ Shader3DVariant::Toon,             "toon",              "Toon",                  "rom/shader/PixelShader3D_Toon.cso",            true },
	{ Shader3DVariant::DirectionalPixel, "directional_pixel", "Directional (Pixel)",   "rom/shader/PixelShader3D_DirectionalPixel.cso", false },
	{ Shader3DVariant::PointPixel,       "point_pixel",       "Point (Pixel)",         "rom/shader/PixelShader3D_PointPixel.cso",       false },
	{ Shader3DVariant::SpotPixel,        "spot_pixel",        "Spot (Pixel)",          "rom/shader/PixelShader3D_SpotPixel.cso",        false },
};
static_assert(ARRAYSIZE(kShader3DVariants) == static_cast<int>(Shader3DVariant::Count),
	"kShader3DVariants must match Shader3DVariant::Count");

static bool LoadPixelShaderFromFile(const char* path, ID3D11PixelShader** outPS) {
	std::ifstream psFile(path, std::ios::binary);
	if (!psFile) return false;
	psFile.seekg(0, std::ios::end);
	std::vector<char> psData(static_cast<size_t>(psFile.tellg()));
	psFile.seekg(0, std::ios::beg);
	psFile.read(psData.data(), static_cast<std::streamsize>(psData.size()));
	return SUCCEEDED(g_pDevice->CreatePixelShader(psData.data(), psData.size(), nullptr, outPS));
}
static ID3D11Buffer* g_pCB3D_WVP = nullptr;
static ID3D11Buffer* g_pCB3D_Material = nullptr;
static ID3D11Buffer* g_pCB3D_SurfaceFX = nullptr;

struct SURFACE_FX_CB {
	XMFLOAT4 Color1;
	XMFLOAT4 Color2;
	XMFLOAT2 ScrollSpeed1;
	XMFLOAT2 ScrollSpeed2;
	float Time;
	float UvScale;
	float FresnelPower;
	float EmissionStrength;
	float GradientStart;
	float GradientEnd;
	float EdgeFadeStart;
	float EdgeFadeEnd;
	float MeshRadius;
	XMFLOAT3 PadMesh;
	XMFLOAT4X4 World;
};
static_assert(sizeof(SURFACE_FX_CB) % 16 == 0, "SURFACE_FX_CB must be 16-byte aligned");

static SURFACE_FX_PARAMS s_SurfaceFXParams{};
static float s_SurfaceFXMeshRadius = 1.0f;

static XMMATRIX s_View3D = XMMatrixIdentity();
static XMMATRIX s_Proj3D = XMMatrixIdentity();
static XMMATRIX s_World3D = XMMatrixIdentity();

/**---------------------------------------------------------------------------
// �V���h�E�z��
---------------------------------------------------------------------------**/
static ID3D11Buffer* g_pCB3D_Shadow = nullptr;
static ID3D11SamplerState* g_pShadowSampler = nullptr;

struct SHADOW_CB {
	XMFLOAT4X4 LightVP[MAX_LIGHTS];
	XMFLOAT4   ShadowParams[MAX_LIGHTS]; // x=bias, y=slope
	UINT       CascadeCount;
	float      CascadeBlend;
	float      _padCascade[2];
	XMFLOAT4   CascadeSplits;
	XMFLOAT4X4 WorldView;
};

// Per-cascade LightVP + cascade selection params
static XMMATRIX s_LightVPs[MAX_LIGHTS];
static float    s_ShadowBias = 0.0015f;
static float    s_ShadowSlope = 1.5f;
static int      s_CascadeCount = 0;
static float    s_CascadeBlend = 2.0f;
static XMFLOAT4 s_CascadeSplits = { 0, 0, 0, 0 };

// Shadow SRV array (t1..)
static ID3D11ShaderResourceView* s_pShadowSRVs[MAX_LIGHTS] = {};
static int s_ShadowSRVCount = 0;

static void UpdateShadowCB() {
	if (!g_pContext || !g_pCB3D_Shadow) return;

	SHADOW_CB cb {};
	const int n = (s_CascadeCount > MAX_LIGHTS) ? MAX_LIGHTS : s_CascadeCount;
	for (int i = 0; i < n; ++i) {
		XMMATRIX wlv = s_World3D * s_LightVPs[i];
		XMStoreFloat4x4(&cb.LightVP[i], XMMatrixTranspose(wlv));
		cb.ShadowParams[i] = XMFLOAT4(s_ShadowBias, s_ShadowSlope, 0, 0);
	}
	cb.CascadeCount = static_cast<UINT>(n);
	cb.CascadeBlend = s_CascadeBlend;
	cb._padCascade[0] = 0.0f;
	cb._padCascade[1] = 0.0f;
	cb.CascadeSplits = s_CascadeSplits;
	XMStoreFloat4x4(&cb.WorldView, XMMatrixTranspose(s_World3D * s_View3D));

	g_pContext->UpdateSubresource(g_pCB3D_Shadow, 0, nullptr, &cb, 0, 0);
}

static bool s_useExternalVP = false;

/**===========================================================================
// ����X�V�w���p
===========================================================================**/
static void Update2D_VP() {
	if (!g_pContext || !g_pCB2D_VP) return;
	XMMATRIX VP = s_View2D * s_Proj2D;
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(VP));
	g_pContext->UpdateSubresource(g_pCB2D_VP, 0, nullptr, &transpose, 0, 0);
}

static void Update2D_World() {
	if (!g_pContext || !g_pCB2D_World) return;
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(s_World2D));
	g_pContext->UpdateSubresource(g_pCB2D_World, 0, nullptr, &transpose, 0, 0);
}

static void Update3D_WVP() {
	if (!g_pContext || !g_pCB3D_WVP) return;
	XMMATRIX WVP = s_World3D * s_View3D * s_Proj3D;
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(WVP));
	// XMStoreFloat4x4(&transpose, WVP);
	g_pContext->UpdateSubresource(g_pCB3D_WVP, 0, nullptr, &transpose, 0, 0);
}

static bool CreateBuffer(ID3D11Buffer** ppBuf, UINT byteWidth, D3D11_BIND_FLAG bind = D3D11_BIND_CONSTANT_BUFFER) {
	D3D11_BUFFER_DESC desc {};
	desc.ByteWidth = byteWidth;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind;
	desc.CPUAccessFlags = 0;
	return SUCCEEDED(g_pDevice->CreateBuffer(&desc, nullptr, ppBuf));
}

/**===========================================================================
// ������
===========================================================================**/
bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
	HRESULT hr;

	if (!pDevice || !pContext) return false;
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 2D VS/PS ��R���p�C���ς� .cso ����ǂݍ���
	{
		// Vertex Shader
		std::ifstream vsFile2D("rom/shader/VertexShader2D.cso", std::ios::binary);
		if (!vsFile2D) {
			MessageBox(nullptr, "Cannot open rom/shader/VertexShader2D.cso\nPlease build the project to generate .cso files.", "2D VS Load Error", MB_OK | MB_ICONERROR);
			return false;
		}
		vsFile2D.seekg(0, std::ios::end);
		std::vector<char> vsData2D(static_cast<size_t>(vsFile2D.tellg()));
		vsFile2D.seekg(0, std::ios::beg);
		vsFile2D.read(vsData2D.data(), static_cast<std::streamsize>(vsData2D.size()));

		hr = g_pDevice->CreateVertexShader(vsData2D.data(), vsData2D.size(), nullptr, &g_pVS2D);
		if (FAILED(hr)) return false;

		D3D11_INPUT_ELEMENT_DESC layout2D[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
		hr = g_pDevice->CreateInputLayout(layout2D, ARRAYSIZE(layout2D), vsData2D.data(), vsData2D.size(), &g_pIL2D);
		if (FAILED(hr)) return false;

		// Pixel Shader
		std::ifstream psFile2D("rom/shader/PixelShader2D.cso", std::ios::binary);
		if (!psFile2D) {
			MessageBox(nullptr, "Cannot open rom/shader/PixelShader2D.cso\nPlease build the project to generate .cso files.", "2D PS Load Error", MB_OK | MB_ICONERROR);
			return false;
		}
		psFile2D.seekg(0, std::ios::end);
		std::vector<char> psData2D(static_cast<size_t>(psFile2D.tellg()));
		psFile2D.seekg(0, std::ios::beg);
		psFile2D.read(psData2D.data(), static_cast<std::streamsize>(psData2D.size()));

		hr = g_pDevice->CreatePixelShader(psData2D.data(), psData2D.size(), nullptr, &g_pPS2D);
		if (FAILED(hr)) return false;
	}

	// 3D VS/PS ��R���p�C���ς� .cso ����ǂݍ���
	{
		// Vertex Shader
		std::ifstream vsFile("rom/shader/VertexShader3D.cso", std::ios::binary);
		if (!vsFile) {
			MessageBox(nullptr, "Cannot open rom/shader/VertexShader3D.cso\nPlease build the project to generate .cso files.", "3D VS Load Error", MB_OK | MB_ICONERROR);
			return false;
		}
		vsFile.seekg(0, std::ios::end);
		std::vector<char> vsData(static_cast<size_t>(vsFile.tellg()));
		vsFile.seekg(0, std::ios::beg);
		vsFile.read(vsData.data(), static_cast<std::streamsize>(vsData.size()));

		hr = g_pDevice->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &g_pVS3D);
		if (FAILED(hr)) return false;

		D3D11_INPUT_ELEMENT_DESC layout3D[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
		hr = g_pDevice->CreateInputLayout(layout3D, ARRAYSIZE(layout3D), vsData.data(), vsData.size(), &g_pIL3D);
		if (FAILED(hr)) return false;

		for (const auto& info : kShader3DVariants) {
			const int idx = static_cast<int>(info.variant);
			if (!LoadPixelShaderFromFile(info.csoFile, &g_pPS3DVariants[idx])) {
				char msg[512];
				snprintf(msg, sizeof(msg),
					"Cannot open %s\nPlease build the project to generate .cso files.", info.csoFile);
				MessageBox(nullptr, msg, "3D PS Load Error", MB_OK | MB_ICONERROR);
				return false;
			}
		}

		// Outline inverted-hull shaders
		{
			std::ifstream vsOutline("rom/shader/VertexShader3D_Outline.cso", std::ios::binary);
			if (!vsOutline) {
				MessageBox(nullptr, "Cannot open rom/shader/VertexShader3D_Outline.cso\nPlease build the project to generate .cso files.", "3D Outline VS Load Error", MB_OK | MB_ICONERROR);
				return false;
			}
			vsOutline.seekg(0, std::ios::end);
			std::vector<char> vsOutlineData(static_cast<size_t>(vsOutline.tellg()));
			vsOutline.seekg(0, std::ios::beg);
			vsOutline.read(vsOutlineData.data(), static_cast<std::streamsize>(vsOutlineData.size()));

			hr = g_pDevice->CreateVertexShader(vsOutlineData.data(), vsOutlineData.size(), nullptr, &g_pVS3DOutline);
			if (FAILED(hr)) return false;

			D3D11_INPUT_ELEMENT_DESC layoutOutline[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
			};
			hr = g_pDevice->CreateInputLayout(layoutOutline, ARRAYSIZE(layoutOutline), vsOutlineData.data(), vsOutlineData.size(), &g_pIL3DOutline);
			if (FAILED(hr)) return false;

			if (!LoadPixelShaderFromFile("rom/shader/PixelShader3D_Outline.cso", &g_pPS3DOutline)) {
				MessageBox(nullptr, "Cannot open rom/shader/PixelShader3D_Outline.cso\nPlease build the project to generate .cso files.", "3D Outline PS Load Error", MB_OK | MB_ICONERROR);
				return false;
			}
		}
	}

	// �萔�o�b�t�@�쐬
	if (!CreateBuffer(&g_pCB2D_VP, sizeof(XMFLOAT4X4))) return false;
	if (!CreateBuffer(&g_pCB2D_World, sizeof(XMFLOAT4X4))) return false;
	if (!CreateBuffer(&g_pCB3D_WVP, sizeof(XMFLOAT4X4))) return false;

	{
		D3D11_BUFFER_DESC md {};
		md.ByteWidth = sizeof(MATERIAL);
		md.Usage = D3D11_USAGE_DEFAULT;
		md.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_pDevice->CreateBuffer(&md, nullptr, &g_pCB3D_Material))) return false;
	}

	{
		D3D11_BUFFER_DESC sfx {};
		sfx.ByteWidth = sizeof(SURFACE_FX_CB);
		sfx.Usage = D3D11_USAGE_DEFAULT;
		sfx.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_pDevice->CreateBuffer(&sfx, nullptr, &g_pCB3D_SurfaceFX))) return false;
	}

	// 2D PS �萔�o�b�t�@�i�F�j
	{
		D3D11_BUFFER_DESC psd {}; psd.ByteWidth = sizeof(XMFLOAT4); psd.Usage = D3D11_USAGE_DEFAULT; psd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g_pDevice->CreateBuffer(&psd, nullptr, &g_pCB2D_Color))) return false;
	}

	// �T���v���[
	{
		D3D11_SAMPLER_DESC sd {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MipLODBias = 0;
		sd.MaxAnisotropy = 16;
		sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sd.MinLOD = 0;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		if (FAILED(g_pDevice->CreateSamplerState(&sd, &g_pSamplerState))) return false;
	}

	// �V���h�E: CB �Ɣ�r�T���v���[
	{
		if (!CreateBuffer(&g_pCB3D_Shadow, sizeof(SHADOW_CB))) return false;

		D3D11_SAMPLER_DESC sd {};
		sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		sd.MinLOD = 0.0f;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 1.0f;
		if (FAILED(g_pDevice->CreateSamplerState(&sd, &g_pShadowSampler))) return false;
	}

	// Defaults
	s_CascadeCount = 0;
	s_LightVPs[0] = XMMatrixIdentity();
	s_ShadowBias = 0.0015f;
	s_ShadowSlope = 1.5f;
	s_CascadeBlend = 2.0f;
	s_CascadeSplits = XMFLOAT4(0, 0, 0, 0);
	Update2D_VP();
	Update2D_World();
	Update3D_WVP();

	// ���C�e�B���O������
	Lighting::Initialize(g_pDevice, g_pContext);

	return true;
}

void Shader_Finalize() {
	SAFE_RELEASE(g_pSamplerState);

	SAFE_RELEASE(g_pShadowSampler);
	SAFE_RELEASE(g_pCB3D_Shadow);
	for (int i = 0; i < MAX_LIGHTS; ++i) s_pShadowSRVs[i] = nullptr;
	s_ShadowSRVCount = 0;

	Lighting::Finalize();

	SAFE_RELEASE(g_pCB2D_Color);
	SAFE_RELEASE(g_pCB2D_World);
	SAFE_RELEASE(g_pCB2D_VP);
	SAFE_RELEASE(g_pIL2D);
	SAFE_RELEASE(g_pPS2D);
	SAFE_RELEASE(g_pVS2D);

	SAFE_RELEASE(g_pCB3D_Material);
	SAFE_RELEASE(g_pCB3D_SurfaceFX);
	SAFE_RELEASE(g_pCB3D_WVP);
	SAFE_RELEASE(g_pIL3DOutline);
	SAFE_RELEASE(g_pIL3D);
	for (int i = 0; i < static_cast<int>(Shader3DVariant::Count); ++i)
		SAFE_RELEASE(g_pPS3DVariants[i]);
	SAFE_RELEASE(g_pPS3DOutline);
	SAFE_RELEASE(g_pVS3DOutline);
	SAFE_RELEASE(g_pVS3D);

	g_pDevice = nullptr;
	g_pContext = nullptr;
}

void Shader_SetProjectionMatrix(const DirectX::XMMATRIX& matrix) {
	s_Proj2D = matrix; Update2D_VP();
	s_Proj3D = matrix; Update3D_WVP();
}
void Shader_SetViewMatrix(const DirectX::XMMATRIX& matrix) {
	s_View2D = matrix; Update2D_VP();
	s_View3D = matrix; Update3D_WVP();
}
void Shader_SetWorldMatrix(const DirectX::XMMATRIX& matrix) {
	s_World2D = matrix; Update2D_World();
	s_World3D = matrix; Update3D_WVP();
	UpdateShadowCB();            // World*LightVP ��Čv�Z
	Lighting::UpdatePerObject(s_World3D);
}
void Shader_SetColor(const DirectX::XMFLOAT4& color) {
	if (g_pContext && g_pCB2D_Color) {
		g_pContext->UpdateSubresource(g_pCB2D_Color, 0, nullptr, &color, 0, 0);
	}
}
void Shader_SetMaterial(const MATERIAL& material) {
	if (g_pContext && g_pCB3D_Material) {
		g_pContext->UpdateSubresource(g_pCB3D_Material, 0, nullptr, &material, 0, 0);
	}
}

void Shader_SetSurfaceFXParams(const SURFACE_FX_PARAMS& params) {
	s_SurfaceFXParams = params;
}

void Shader_SetSurfaceFXMeshRadius(float radiusOS) {
	s_SurfaceFXMeshRadius = (radiusOS > 1e-4f) ? radiusOS : 1.0f;
}

void Shader_CommitSurfaceFX() {
	if (!g_pContext || !g_pCB3D_SurfaceFX) return;

	SURFACE_FX_CB cb {};
	cb.Color1 = s_SurfaceFXParams.Color1;
	cb.Color2 = s_SurfaceFXParams.Color2;
	cb.ScrollSpeed1 = s_SurfaceFXParams.ScrollSpeed1;
	cb.ScrollSpeed2 = s_SurfaceFXParams.ScrollSpeed2;
	cb.Time = static_cast<float>(SystemTimer_GetTime());
	cb.UvScale = s_SurfaceFXParams.UvScale;
	cb.FresnelPower = s_SurfaceFXParams.FresnelPower;
	cb.EmissionStrength = s_SurfaceFXParams.EmissionStrength;
	cb.GradientStart = s_SurfaceFXParams.GradientStart;
	cb.GradientEnd = s_SurfaceFXParams.GradientEnd;
	cb.EdgeFadeStart = s_SurfaceFXParams.EdgeFadeStart;
	cb.EdgeFadeEnd = s_SurfaceFXParams.EdgeFadeEnd;
	cb.MeshRadius = s_SurfaceFXMeshRadius;
	cb.PadMesh = XMFLOAT3(0, 0, 0);
	XMStoreFloat4x4(&cb.World, XMMatrixTranspose(s_World3D));

	g_pContext->UpdateSubresource(g_pCB3D_SurfaceFX, 0, nullptr, &cb, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pCB3D_SurfaceFX);
}

// Single-matrix compatibility
void Shader_SetShadowMatrix(const XMMATRIX& lightViewProj, float bias) {
	s_CascadeCount = 1;
	s_LightVPs[0] = lightViewProj;
	s_ShadowBias = bias;
	s_CascadeSplits = XMFLOAT4(1e6f, 1e6f, 1e6f, 1e6f);
	s_CascadeBlend = 0.0f;
	UpdateShadowCB();
}

// Cascades (matrices[0..count) + view-space split distances)
void Shader_SetShadowMatrices(const XMMATRIX* lightViewProjArray, int cascadeCount, float bias, float slope) {
	s_CascadeCount = (cascadeCount <= 0) ? 0 : (cascadeCount > MAX_LIGHTS ? MAX_LIGHTS : cascadeCount);
	for (int i = 0; i < s_CascadeCount; ++i) {
		s_LightVPs[i] = lightViewProjArray[i];
	}
	s_ShadowBias = bias;
	s_ShadowSlope = slope;
	UpdateShadowCB();
}

void Shader_SetShadowCascadeParams(const DirectX::XMFLOAT4& splits, float blend) {
	s_CascadeSplits = splits;
	s_CascadeBlend = blend;
	UpdateShadowCB();
}

DirectX::XMMATRIX Shader_GetProjectionMatrix() {
	return s_Proj3D;
}

DirectX::XMMATRIX Shader_GetViewMatrix() {
	return s_View3D;
}

DirectX::XMMATRIX Shader_GetWorldMatrix() {
	return s_World3D;
}

void Shader_BindShadowMap(ID3D11ShaderResourceView* shadowSRV) {
	s_ShadowSRVCount = 0;
	if (shadowSRV) {
		s_pShadowSRVs[0] = shadowSRV;
		s_ShadowSRVCount = 1;
		g_pContext->PSSetShaderResources(1, 1, &s_pShadowSRVs[0]);
	}
}

void Shader_BindShadowMaps(ID3D11ShaderResourceView* const* shadowSRVs, int lightCount) {
	constexpr UINT kMax = MAX_LIGHTS;

	// �܂��S�X���b�g��N���A
	ID3D11ShaderResourceView* nullSRV[kMax] = {};
	g_pContext->PSSetShaderResources(1, kMax, nullSRV);

	// ����N�����v
	UINT reqCount = (lightCount <= 0) ? 0u : (lightCount > (int) kMax ? kMax : (UINT) lightCount);

	// �O�l�߂ŗL���� SRV ������R�s�[�iAddRef/Release �Ō��S�������j
	UINT bindCount = 0;
	for (UINT i = 0; i < reqCount; ++i) {
		ID3D11ShaderResourceView* srv = (shadowSRVs ? shadowSRVs[i] : nullptr);
		if (!srv) break;
		HRESULT hrAdd = srv->AddRef();  // ���s�Ȃ疳��
		srv->Release();
		if (FAILED(hrAdd)) break;       // ���Ă���̂ł����őł��؂�
		s_pShadowSRVs[bindCount++] = srv;
	}
	for (UINT i = bindCount; i < kMax; ++i) s_pShadowSRVs[i] = nullptr;
	s_ShadowSRVCount = (int) bindCount;

	if (bindCount > 0) {
		g_pContext->PSSetShaderResources(1, bindCount, s_pShadowSRVs);
	}
}

void Shader_SetUseExternalVP(bool enable) { s_useExternalVP = enable; }
bool Shader_IsExternalVP() { return s_useExternalVP; }

void Shader2D_Begin() {
	g_pContext->VSSetShader(g_pVS2D, nullptr, 0);
	g_pContext->PSSetShader(g_pPS2D, nullptr, 0);
	g_pContext->IASetInputLayout(g_pIL2D);
	g_pContext->VSSetConstantBuffers(0, 1, &g_pCB2D_VP);
	g_pContext->VSSetConstantBuffers(1, 1, &g_pCB2D_World);
	g_pContext->PSSetConstantBuffers(0, 1, &g_pCB2D_Color);
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerState);
}

Shader3DVariant Shader3DVariantFromId(const std::string& id) {
	for (const auto& info : kShader3DVariants) {
		if (id == info.id)
			return info.variant;
	}
	return Shader3DVariant::Standard;
}

const char* Shader3DVariantLabel(Shader3DVariant v) {
	const int idx = static_cast<int>(v);
	if (idx >= 0 && idx < static_cast<int>(Shader3DVariant::Count))
		return kShader3DVariants[idx].label;
	return kShader3DVariants[0].label;
}

const char* Shader3DVariantId(Shader3DVariant v) {
	const int idx = static_cast<int>(v);
	if (idx >= 0 && idx < static_cast<int>(Shader3DVariant::Count))
		return kShader3DVariants[idx].id;
	return kShader3DVariants[0].id;
}

int Shader3DVariantCount() {
	return static_cast<int>(Shader3DVariant::Count);
}

Shader3DVariant Shader3DVariantByIndex(int i) {
	if (i >= 0 && i < static_cast<int>(Shader3DVariant::Count))
		return kShader3DVariants[i].variant;
	return Shader3DVariant::Standard;
}

bool Shader3DVariantIsUserVisible(Shader3DVariant v) {
	const int idx = static_cast<int>(v);
	if (idx >= 0 && idx < static_cast<int>(Shader3DVariant::Count))
		return kShader3DVariants[idx].userVisible;
	return false;
}

void Shader3D_Begin(Shader3DVariant variant) {
	g_pContext->VSSetShader(g_pVS3D, nullptr, 0);
	g_pContext->IASetInputLayout(g_pIL3D);

	g_pContext->VSSetConstantBuffers(0, 1, &g_pCB3D_WVP);
	g_pContext->VSSetConstantBuffers(1, 1, &g_pCB3D_Material);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pCB3D_Material);

	g_pContext->VSSetConstantBuffers(2, 1, &g_pCB3D_Shadow);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pCB3D_Shadow);

	Lighting::Bind();

	g_pContext->PSSetSamplers(0, 1, &g_pSamplerState);
	g_pContext->PSSetSamplers(1, 1, &g_pShadowSampler);

	if (Shadow::GetRenderingLightIndex() >= 0) {
		g_pContext->PSSetShader(nullptr, nullptr, 0);
		ID3D11ShaderResourceView* nullSRV[MAX_LIGHTS] = {};
		g_pContext->PSSetShaderResources(1, MAX_LIGHTS, nullSRV);
		return;
	}

	const int variantIdx = static_cast<int>(variant);
	if (variantIdx < 0 || variantIdx >= static_cast<int>(Shader3DVariant::Count))
		variant = Shader3DVariant::Standard;
	ID3D11PixelShader* ps = g_pPS3DVariants[static_cast<int>(variant)];
	if (!ps) ps = g_pPS3DVariants[static_cast<int>(Shader3DVariant::Standard)];
	g_pContext->PSSetShader(ps, nullptr, 0);

	// �O�̂��ߐ�ɃN���A
	ID3D11ShaderResourceView* nullSRV[MAX_LIGHTS] = {};
	g_pContext->PSSetShaderResources(1, MAX_LIGHTS, nullSRV);

	// �O�l�ߍς� SRV ��Č��؂��A�����Ȃ�o�C���h���Ȃ�
	UINT bindCount = (s_ShadowSRVCount > MAX_LIGHTS) ? MAX_LIGHTS : (UINT) s_ShadowSRVCount;
	for (UINT i = 0; i < bindCount; ++i) {
		ID3D11ShaderResourceView* srv = s_pShadowSRVs[i];
		if (!srv) { bindCount = i; break; }
		HRESULT hrAdd = srv->AddRef();
		srv->Release();
		if (FAILED(hrAdd)) { bindCount = i; break; }
	}
	s_ShadowSRVCount = (int) bindCount;

	if (bindCount > 0) {
		g_pContext->PSSetShaderResources(1, bindCount, s_pShadowSRVs);
	}
}

void Shader3D_BeginOutline() {
	g_pContext->VSSetShader(g_pVS3DOutline, nullptr, 0);
	g_pContext->PSSetShader(g_pPS3DOutline, nullptr, 0);
	g_pContext->IASetInputLayout(g_pIL3DOutline ? g_pIL3DOutline : g_pIL3D);

	g_pContext->VSSetConstantBuffers(0, 1, &g_pCB3D_WVP);
	g_pContext->VSSetConstantBuffers(1, 1, &g_pCB3D_Material);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pCB3D_Material);
}
