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
float s_basePosterize = 0.0f;
float s_baseNoise = 0.0f;
float s_baseGrayscale = 0.0f;
float s_baseContrast = 0.0f;
float s_baseVignette = 0.0f;
float s_baseScanline = 0.0f;
float s_baseFlicker = 0.0f;
float s_burstRgbShift = 0.0f;
float s_burstDuration = 0.0f;
float s_burstRemaining = 0.0f;
float s_time = 0.0f;

struct PostProcessCB {
	float PP_RGBShift;
	float PP_MosaicBlocks;
	float PP_Posterize;
	float PP_Noise;
	float PP_Grayscale;
	float PP_Contrast;
	float PP_Vignette;
	float PP_Scanline;
	float PP_Time;
	float PP_Flicker;
	float PP_UVOffsetX;
	float PP_UVOffsetY;
	float PP_UVScaleX;
	float PP_UVScaleY;
	float PP_Pad0;
	float PP_Pad1;
};
static_assert(sizeof(PostProcessCB) == 64, "PostProcessCB must be 64 bytes");

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
	s_basePosterize = 0.0f;
	s_baseNoise = 0.0f;
	s_baseGrayscale = 0.0f;
	s_baseContrast = 0.0f;
	s_baseVignette = 0.0f;
	s_baseScanline = 0.0f;
	s_baseFlicker = 0.0f;
	s_burstRgbShift = 0.0f;
	s_burstDuration = 0.0f;
	s_burstRemaining = 0.0f;
	s_time = 0.0f;
}

void PostProcess_SetEffects(float rgbShift, float mosaicBlocks) {
	s_baseRgbShift = (std::max)(0.0f, rgbShift);
	s_baseMosaicBlocks = (std::max)(0.0f, mosaicBlocks);
}

void PostProcess_SetEffects(const PostFXParams& params) {
	s_baseRgbShift = (std::max)(0.0f, params.rgbShift);
	s_baseMosaicBlocks = (std::max)(0.0f, params.mosaicBlocks);
	s_basePosterize = (std::max)(0.0f, params.posterize);
	s_baseNoise = (std::max)(0.0f, params.noise);
	s_baseGrayscale = (std::max)(0.0f, params.grayscale);
	s_baseContrast = (std::max)(0.0f, params.contrast);
	s_baseVignette = (std::max)(0.0f, params.vignette);
	s_baseScanline = (std::max)(0.0f, params.scanline);
	s_baseFlicker = (std::max)(0.0f, params.flicker);
}

void PostProcess_GetEffects(float& outRgbShift, float& outMosaicBlocks) {
	outRgbShift = s_baseRgbShift;
	outMosaicBlocks = s_baseMosaicBlocks;
}

void PostProcess_GetEffects(PostFXParams& out) {
	out.rgbShift = s_baseRgbShift;
	out.mosaicBlocks = s_baseMosaicBlocks;
	out.posterize = s_basePosterize;
	out.noise = s_baseNoise;
	out.grayscale = s_baseGrayscale;
	out.contrast = s_baseContrast;
	out.vignette = s_baseVignette;
	out.scanline = s_baseScanline;
	out.flicker = s_baseFlicker;
}

void PostProcess_TriggerBurst(float rgbShift, float durationSeconds) {
	s_burstRgbShift = (std::max)(0.0f, rgbShift);
	s_burstDuration = (std::max)(0.0f, durationSeconds);
	s_burstRemaining = s_burstDuration;
}

void PostProcess_Update(float dt) {
	s_time += dt;
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
		cb->PP_RGBShift = rgb;
		cb->PP_MosaicBlocks = mosaic;
		cb->PP_Posterize = s_basePosterize;
		cb->PP_Noise = s_baseNoise;
		cb->PP_Grayscale = s_baseGrayscale;
		cb->PP_Contrast = s_baseContrast;
		cb->PP_Vignette = s_baseVignette;
		cb->PP_Scanline = s_baseScanline;
		cb->PP_Time = s_time;
		cb->PP_Flicker = s_baseFlicker;
		cb->PP_UVOffsetX = uvOffX;
		cb->PP_UVOffsetY = uvOffY;
		cb->PP_UVScaleX = uvScaleX;
		cb->PP_UVScaleY = uvScaleY;
		cb->PP_Pad0 = 0.0f;
		cb->PP_Pad1 = 0.0f;
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
