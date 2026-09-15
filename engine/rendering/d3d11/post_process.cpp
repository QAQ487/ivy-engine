/**===========================================================================

 File   :  [post_process.cpp]

 Author : Terry Yeung
 Date   : 2026/3/28
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "post_process.h"

#include "renderer.h"
#include "engine/core/engine_gui.h"

#include <fstream>
#include <vector>
#include <algorithm>

namespace {

ID3D11VertexShader* s_vs = nullptr;
ID3D11PixelShader* s_ps = nullptr;
ID3D11Buffer* s_cb = nullptr;
ID3D11SamplerState* s_sampler = nullptr;

float s_baseRgbShift = 0.0f;
float s_baseMosaicBlocks = 0.0f;
float s_burstRgbShift = 0.0f;
float s_burstDuration = 0.0f;
float s_burstRemaining = 0.0f;

struct PostProcessCB {
	float PP_Mode;
	float PP_RGBShift;
	float PP_MosaicBlocks;
	float PP_Padding;
	float PP_UVOffsetX;
	float PP_UVOffsetY;
	float PP_UVScaleX;
	float PP_UVScaleY;
};
static_assert(sizeof(PostProcessCB) == 32, "PostProcessCB must be 32 bytes");

bool LoadShaderFile(const char* path, std::vector<char>& outData) {
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;
	file.seekg(0, std::ios::end);
	const std::streamsize size = file.tellg();
	if (size <= 0)
		return false;
	file.seekg(0, std::ios::beg);
	outData.resize(static_cast<size_t>(size));
	file.read(outData.data(), size);
	return static_cast<bool>(file) || file.eof();
}

} // namespace

void PostProcess_Initialize() {
	ID3D11Device* device = GetDevice();
	if (!device)
		return;

	std::vector<char> vsData;
	if (!LoadShaderFile("rom/shader/FullscreenVS.cso", vsData)) {
		MessageBoxA(nullptr,
			"Cannot open rom/shader/FullscreenVS.cso\nPlease build the project to generate .cso files.",
			"PostProcess VS Load Error", MB_OK | MB_ICONERROR);
		return;
	}
	if (FAILED(device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &s_vs)))
		return;

	std::vector<char> psData;
	if (!LoadShaderFile("rom/shader/PostProcessPS.cso", psData)) {
		MessageBoxA(nullptr,
			"Cannot open rom/shader/PostProcessPS.cso\nPlease build the project to generate .cso files.",
			"PostProcess PS Load Error", MB_OK | MB_ICONERROR);
		return;
	}
	if (FAILED(device->CreatePixelShader(psData.data(), psData.size(), nullptr, &s_ps)))
		return;

	D3D11_BUFFER_DESC cbd = {};
	cbd.ByteWidth = sizeof(PostProcessCB);
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(device->CreateBuffer(&cbd, nullptr, &s_cb)))
		return;

	D3D11_SAMPLER_DESC sd = {};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0.0f;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sd, &s_sampler);
}

void PostProcess_Shutdown() {
	SAFE_RELEASE(s_sampler);
	SAFE_RELEASE(s_cb);
	SAFE_RELEASE(s_ps);
	SAFE_RELEASE(s_vs);
	s_baseRgbShift = 0.0f;
	s_baseMosaicBlocks = 0.0f;
	s_burstRgbShift = 0.0f;
	s_burstDuration = 0.0f;
	s_burstRemaining = 0.0f;
}

void PostProcess_SetEffects(float rgbShift, float mosaicBlocks) {
	s_baseRgbShift = (std::max)(0.0f, rgbShift);
	s_baseMosaicBlocks = (std::max)(0.0f, mosaicBlocks);
}

void PostProcess_GetEffects(float& outRgbShift, float& outMosaicBlocks) {
	outRgbShift = s_baseRgbShift;
	outMosaicBlocks = s_baseMosaicBlocks;
}

void PostProcess_TriggerBurst(float rgbShift, float durationSeconds) {
	s_burstRgbShift = (std::max)(0.0f, rgbShift);
	s_burstDuration = (std::max)(0.0f, durationSeconds);
	s_burstRemaining = s_burstDuration;
}

void PostProcess_Update(float dt) {
	if (s_burstRemaining <= 0.0f) {
		s_burstRemaining = 0.0f;
		return;
	}
	s_burstRemaining -= dt;
	if (s_burstRemaining < 0.0f)
		s_burstRemaining = 0.0f;
}

void PostProcess_Draw() {
	ID3D11DeviceContext* ctx = GetDeviceContext();
	ID3D11ShaderResourceView* srv = PostRT_GetSRV();
	if (!ctx || !srv || !s_vs || !s_ps || !s_cb || !s_sampler)
		return;

	float burst = 0.0f;
	if (s_burstDuration > 1e-6f && s_burstRemaining > 0.0f)
		burst = s_burstRgbShift * (s_burstRemaining / s_burstDuration);

	const float rgb = (std::max)(s_baseRgbShift, burst);
	const float mosaic = s_baseMosaicBlocks;

	float mode = 0.0f;
	if (rgb > 0.0f && mosaic > 0.0f)
		mode = 3.0f;
	else if (rgb > 0.0f)
		mode = 1.0f;
	else if (mosaic > 0.0f)
		mode = 2.0f;

	Renderer_BindBackBuffer();

	float vx, vy, vw, vh;
	EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
	const float bbW = GetBackBufferWidth();
	const float bbH = GetBackBufferHeight();
	const float uvOffX = (bbW > 1e-3f) ? (vx / bbW) : 0.0f;
	const float uvOffY = (bbH > 1e-3f) ? (vy / bbH) : 0.0f;
	const float uvScaleX = (bbW > 1e-3f) ? (vw / bbW) : 1.0f;
	const float uvScaleY = (bbH > 1e-3f) ? (vh / bbH) : 1.0f;

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(ctx->Map(s_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		auto* cb = static_cast<PostProcessCB*>(mapped.pData);
		cb->PP_Mode = mode;
		cb->PP_RGBShift = rgb;
		cb->PP_MosaicBlocks = mosaic;
		cb->PP_Padding = 0.0f;
		cb->PP_UVOffsetX = uvOffX;
		cb->PP_UVOffsetY = uvOffY;
		cb->PP_UVScaleX = uvScaleX;
		cb->PP_UVScaleY = uvScaleY;
		ctx->Unmap(s_cb, 0);
	}

	ctx->IASetInputLayout(nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->VSSetShader(s_vs, nullptr, 0);
	ctx->PSSetShader(s_ps, nullptr, 0);
	ctx->PSSetConstantBuffers(10, 1, &s_cb);
	ctx->PSSetShaderResources(0, 1, &srv);
	ctx->PSSetSamplers(0, 1, &s_sampler);
	SetDepthEnable(false);

	SetRenderViewport(vx, vy, vw, vh);

	ctx->Draw(3, 0);

	// Leave game viewport active so SceneCanvas / editor overlays draw into it.
	ID3D11ShaderResourceView* nullSrv = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSrv);
	SetDepthEnable(true);
}
