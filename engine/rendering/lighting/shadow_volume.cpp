/**===========================================================================

 File   :  [shadow_volume.cpp]
 -----------------------------------------------------------------------------
 Stencil shadow volumes: z-fail (Carmack's reverse) count with depth clamp, then a
 fullscreen multiply over stencil != 0. See shadow_volume.h for the frame flow.

 ===========================================================================**/
#include "shadow_volume.h"

#include "engine/core/content_path.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/rendering/d3d11/shader.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
	ID3D11Device* g_dev = nullptr;
	ID3D11DeviceContext* g_ctx = nullptr;

	ID3D11VertexShader* g_vsVolume = nullptr;
	ID3D11InputLayout* g_ilVolume = nullptr;
	ID3D11VertexShader* g_vsFullscreen = nullptr;
	ID3D11PixelShader* g_psDarken = nullptr;
	ID3D11Buffer* g_cbObject = nullptr;  // VS b7
	ID3D11Buffer* g_cbDarken = nullptr;  // PS b8

	ID3D11DepthStencilState* g_dsCount = nullptr;          // z-fail, outward = front
	ID3D11DepthStencilState* g_dsCountMirrored = nullptr;  // negative-determinant world
	ID3D11DepthStencilState* g_dsDarken = nullptr;
	ID3D11RasterizerState* g_rsVolume = nullptr;           // cull none, depth clip off
	ID3D11BlendState* g_bsNoColor = nullptr;
	ID3D11BlendState* g_bsMultiply = nullptr;

	bool g_passActive = false;
	DirectX::XMFLOAT3 g_shineWS {0.0f, -1.0f, 0.0f};
	ShadowVolume::Stats g_stats {};
	ShadowVolume::Stats g_lastStats {};

	// Saved pipeline state (restored by EndPass).
	ID3D11DepthStencilState* g_savedDS = nullptr;
	UINT g_savedStencilRef = 0;
	ID3D11RasterizerState* g_savedRS = nullptr;
	ID3D11BlendState* g_savedBS = nullptr;
	FLOAT g_savedBlendFactor[4] = {};
	UINT g_savedSampleMask = 0xffffffff;
	ID3D11VertexShader* g_savedVS = nullptr;
	ID3D11PixelShader* g_savedPS = nullptr;
	ID3D11InputLayout* g_savedIL = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY g_savedTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// World-space push of the near cap along the light, so it sits just inside the
	// caster's lit faces and never wins the depth test against them (self-shadow acne).
	constexpr float kCapBiasWorld = 0.01f;

	struct ObjectCB {
		DirectX::XMFLOAT4 shineDirOS;
		DirectX::XMFLOAT4 capBiasOS;
		DirectX::XMFLOAT4 extrudeParams;  // x: extrusion length (world units)
	};
	float g_extrusionLength = 1000.0f;
	struct DarkenCB {
		DirectX::XMFLOAT4 color;
	};

	template <class T> void SafeRelease(T*& p) {
		if (p) { p->Release(); p = nullptr; }
	}

	bool ReadCso(const char* romPath, std::vector<char>& out) {
		std::ifstream f(content_path::ReadableFileCurrent(romPath), std::ios::binary | std::ios::ate);
		if (!f) {
			char msg[256];
			std::snprintf(msg, sizeof(msg),
				"[ShadowVolume] cannot open %s (build the project to generate .cso files); shadow volumes disabled\n",
				romPath);
			hal::DebugWriteA(msg);
			return false;
		}
		out.resize(static_cast<size_t>(f.tellg()));
		f.seekg(0, std::ios::beg);
		f.read(out.data(), static_cast<std::streamsize>(out.size()));
		return static_cast<bool>(f) && !out.empty();
	}

	bool CreateCB(UINT bytes, ID3D11Buffer** out) {
		D3D11_BUFFER_DESC bd {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = bytes;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		return SUCCEEDED(g_dev->CreateBuffer(&bd, nullptr, out));
	}

	D3D11_DEPTH_STENCILOP_DESC StencilOps(D3D11_STENCIL_OP depthFail) {
		D3D11_DEPTH_STENCILOP_DESC op {};
		op.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		op.StencilDepthFailOp = depthFail;
		op.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		op.StencilFunc = D3D11_COMPARISON_ALWAYS;
		return op;
	}

	bool CreateStates() {
		D3D11_DEPTH_STENCIL_DESC ds {};
		ds.DepthEnable = TRUE;
		ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		// LESS_EQUAL: the far cap is clamped to depth 1.0 and must not count against
		// the cleared (sky) depth of 1.0.
		ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		ds.StencilEnable = TRUE;
		ds.StencilReadMask = 0xff;
		ds.StencilWriteMask = 0xff;
		// Volume faces built outward are D3D front faces (clockwise).
		ds.FrontFace = StencilOps(D3D11_STENCIL_OP_DECR);
		ds.BackFace = StencilOps(D3D11_STENCIL_OP_INCR);
		if (FAILED(g_dev->CreateDepthStencilState(&ds, &g_dsCount))) return false;
		ds.FrontFace = StencilOps(D3D11_STENCIL_OP_INCR);
		ds.BackFace = StencilOps(D3D11_STENCIL_OP_DECR);
		if (FAILED(g_dev->CreateDepthStencilState(&ds, &g_dsCountMirrored))) return false;

		D3D11_DEPTH_STENCIL_DESC dd {};
		dd.DepthEnable = FALSE;
		dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dd.StencilEnable = TRUE;
		dd.StencilReadMask = 0xff;
		dd.StencilWriteMask = 0x00;
		D3D11_DEPTH_STENCILOP_DESC test {};
		test.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		test.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		test.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		test.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;  // ref 0
		dd.FrontFace = test;
		dd.BackFace = test;
		if (FAILED(g_dev->CreateDepthStencilState(&dd, &g_dsDarken))) return false;

		D3D11_RASTERIZER_DESC rd {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.FrontCounterClockwise = FALSE;
		rd.DepthClipEnable = FALSE;  // depth clamp: far cap at infinity lands on the far plane
		if (FAILED(g_dev->CreateRasterizerState(&rd, &g_rsVolume))) return false;

		D3D11_BLEND_DESC bd {};
		bd.RenderTarget[0].BlendEnable = FALSE;
		bd.RenderTarget[0].RenderTargetWriteMask = 0;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		if (FAILED(g_dev->CreateBlendState(&bd, &g_bsNoColor))) return false;

		bd.RenderTarget[0].BlendEnable = TRUE;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;  // dest * src
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;     // keep alpha
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(g_dev->CreateBlendState(&bd, &g_bsMultiply))) return false;
		return true;
	}

	void SaveState() {
		g_ctx->OMGetDepthStencilState(&g_savedDS, &g_savedStencilRef);
		g_ctx->RSGetState(&g_savedRS);
		g_ctx->OMGetBlendState(&g_savedBS, g_savedBlendFactor, &g_savedSampleMask);
		g_ctx->VSGetShader(&g_savedVS, nullptr, nullptr);
		g_ctx->PSGetShader(&g_savedPS, nullptr, nullptr);
		g_ctx->IAGetInputLayout(&g_savedIL);
		g_ctx->IAGetPrimitiveTopology(&g_savedTopology);
	}

	void RestoreState() {
		g_ctx->OMSetDepthStencilState(g_savedDS, g_savedStencilRef);
		g_ctx->RSSetState(g_savedRS);
		g_ctx->OMSetBlendState(g_savedBS, g_savedBlendFactor, g_savedSampleMask);
		g_ctx->VSSetShader(g_savedVS, nullptr, 0);
		g_ctx->PSSetShader(g_savedPS, nullptr, 0);
		g_ctx->IASetInputLayout(g_savedIL);
		g_ctx->IASetPrimitiveTopology(g_savedTopology);
		SafeRelease(g_savedDS);
		SafeRelease(g_savedRS);
		SafeRelease(g_savedBS);
		SafeRelease(g_savedVS);
		SafeRelease(g_savedPS);
		SafeRelease(g_savedIL);
	}
}

namespace ShadowVolume {

	bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
		Finalize();
		g_dev = dev;
		g_ctx = ctx;
		if (!g_dev || !g_ctx) return false;

		std::vector<char> vs, fsvs, ps;
		if (!ReadCso("rom/shader/VertexShaderShadowVolume.cso", vs)
			|| !ReadCso("rom/shader/FullscreenVS.cso", fsvs)
			|| !ReadCso("rom/shader/PixelShaderShadowVolumeDarken.cso", ps)) {
			Finalize();
			return false;
		}

		bool ok = SUCCEEDED(g_dev->CreateVertexShader(vs.data(), vs.size(), nullptr, &g_vsVolume))
			&& SUCCEEDED(g_dev->CreateVertexShader(fsvs.data(), fsvs.size(), nullptr, &g_vsFullscreen))
			&& SUCCEEDED(g_dev->CreatePixelShader(ps.data(), ps.size(), nullptr, &g_psDarken));
		if (ok) {
			// Same layout as Model3D::VertexPNTC (and the skinned VS).
			const D3D11_INPUT_ELEMENT_DESC layout[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
			};
			ok = SUCCEEDED(g_dev->CreateInputLayout(layout, ARRAYSIZE(layout), vs.data(), vs.size(), &g_ilVolume));
		}
		ok = ok && CreateCB(sizeof(ObjectCB), &g_cbObject) && CreateCB(sizeof(DarkenCB), &g_cbDarken)
			&& CreateStates();
		if (!ok) {
			hal::DebugWriteA("[ShadowVolume] D3D11 resource creation failed; shadow volumes disabled\n");
			Finalize();
			return false;
		}
		return true;
	}

	void Finalize() {
		if (g_passActive && g_ctx) {
			RestoreState();
			g_passActive = false;
		}
		SafeRelease(g_vsVolume);
		SafeRelease(g_ilVolume);
		SafeRelease(g_vsFullscreen);
		SafeRelease(g_psDarken);
		SafeRelease(g_cbObject);
		SafeRelease(g_cbDarken);
		SafeRelease(g_dsCount);
		SafeRelease(g_dsCountMirrored);
		SafeRelease(g_dsDarken);
		SafeRelease(g_rsVolume);
		SafeRelease(g_bsNoColor);
		SafeRelease(g_bsMultiply);
		g_dev = nullptr;
		g_ctx = nullptr;
	}

	bool IsReady() {
		return g_ctx && g_vsVolume && g_ilVolume && g_vsFullscreen && g_psDarken && g_dsCount
			&& g_dsDarken && g_rsVolume && g_bsNoColor && g_bsMultiply;
	}

	bool BeginPass(const DirectX::XMFLOAT3& shineDirWS, float extrusionLength) {
		g_stats = {};
		if (!IsReady() || g_passActive) {
			g_lastStats = {};
			return false;
		}

		using namespace DirectX;
		XMVECTOR shine = XMLoadFloat3(&shineDirWS);
		if (XMVectorGetX(XMVector3LengthSq(shine)) < 1e-12f) {
			g_lastStats = {};
			return false;
		}
		XMStoreFloat3(&g_shineWS, XMVector3Normalize(shine));
		g_extrusionLength = (std::isfinite(extrusionLength) && extrusionLength > 0.0f)
			? extrusionLength : 1000.0f;

		SaveState();
		g_passActive = true;

		// Binds the shared 3D constant buffers (WVP b0, skin b5, ...) to the VS.
		Shader3D_Begin(Shader3DVariant::Standard);

		g_ctx->VSSetShader(g_vsVolume, nullptr, 0);
		g_ctx->IASetInputLayout(g_ilVolume);
		g_ctx->VSSetConstantBuffers(7, 1, &g_cbObject);
		g_ctx->PSSetShader(nullptr, nullptr, 0);
		g_ctx->RSSetState(g_rsVolume);
		const FLOAT zero[4] = {};
		g_ctx->OMSetBlendState(g_bsNoColor, zero, 0xffffffff);
		g_ctx->OMSetDepthStencilState(g_dsCount, 0);
		g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		return true;
	}

	bool IsPassActive() { return g_passActive; }

	void DrawVolume(const DirectX::XMMATRIX& world, UINT indexCount) {
		if (!g_passActive || indexCount == 0) return;
		using namespace DirectX;

		XMVECTOR det;
		const XMMATRIX invWorld = XMMatrixInverse(&det, world);
		const float d = XMVectorGetX(det);
		if (!std::isfinite(d) || std::fabs(d) < 1e-12f) return;  // zero scale: nothing to cast

		// Directions map to object space through the inverse linear part. A geometric
		// normal n_os faces the light iff dot(n_os, invWorld * L_ws) > 0.
		const XMVECTOR shineWS = XMLoadFloat3(&g_shineWS);
		ObjectCB cb {};
		XMStoreFloat4(&cb.shineDirOS, XMVector3TransformNormal(shineWS, invWorld));
		XMStoreFloat4(&cb.capBiasOS, XMVector3TransformNormal(XMVectorScale(shineWS, kCapBiasWorld), invWorld));
		cb.shineDirOS.w = 0.0f;
		cb.capBiasOS.w = 0.0f;
		cb.extrudeParams = XMFLOAT4(g_extrusionLength, 0.0f, 0.0f, 0.0f);
		g_ctx->UpdateSubresource(g_cbObject, 0, nullptr, &cb, 0, 0);

		// Re-assert our VS/IL: Model3D only updates constant buffers, but keep this
		// robust against anything that ran between draws.
		g_ctx->VSSetShader(g_vsVolume, nullptr, 0);
		g_ctx->IASetInputLayout(g_ilVolume);
		g_ctx->VSSetConstantBuffers(7, 1, &g_cbObject);
		// A mirrored transform flips screen winding, so outward faces become back faces.
		g_ctx->OMSetDepthStencilState(d < 0.0f ? g_dsCountMirrored : g_dsCount, 0);

		g_ctx->DrawIndexed(indexCount, 0, 0);
		++g_stats.casters;
		g_stats.triangles += static_cast<int>(indexCount / 3);
	}

	void RecordOpenMesh() { ++g_stats.openMeshes; }

	void EndPass(float strength) {
		if (!g_passActive) return;

		if (g_stats.casters > 0 && strength > 0.0f) {
			const float k = 1.0f - (strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength));
			DarkenCB cb { DirectX::XMFLOAT4(k, k, k, 1.0f) };
			g_ctx->UpdateSubresource(g_cbDarken, 0, nullptr, &cb, 0, 0);

			g_ctx->VSSetShader(g_vsFullscreen, nullptr, 0);
			g_ctx->IASetInputLayout(nullptr);
			g_ctx->PSSetShader(g_psDarken, nullptr, 0);
			g_ctx->PSSetConstantBuffers(8, 1, &g_cbDarken);
			g_ctx->RSSetState(g_savedRS);  // normal clip; fullscreen triangle in the current viewport
			const FLOAT zero[4] = {};
			g_ctx->OMSetBlendState(g_bsMultiply, zero, 0xffffffff);
			g_ctx->OMSetDepthStencilState(g_dsDarken, 0);
			g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			g_ctx->Draw(3, 0);
		}

		RestoreState();
		g_passActive = false;
		g_lastStats = g_stats;
	}

	Stats GetLastStats() { return g_lastStats; }
}
