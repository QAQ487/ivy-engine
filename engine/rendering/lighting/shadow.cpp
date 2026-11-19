/**===========================================================================

 File   :  [shadow.cpp]

 Author : Terry Yeung
 Date   : 2025/11/9
 -----------------------------------------------------------------------------
 Cascaded shadow maps: practical splits, tight ortho fit, texel snapping.

 ===========================================================================**/
#include "shadow.h"
#include "engine/rendering/d3d11/shader.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace {
	ID3D11Device* g_dev = nullptr;
	ID3D11DeviceContext* g_ctx = nullptr;
	UINT g_size = 1024;
	DXGI_FORMAT g_texFormat = DXGI_FORMAT_R32_TYPELESS;
	DXGI_FORMAT g_dsvFormat = DXGI_FORMAT_D32_FLOAT;
	DXGI_FORMAT g_srvFormat = DXGI_FORMAT_R32_FLOAT;

	struct CascadeMap {
		ID3D11Texture2D* tex = nullptr;
		ID3D11DepthStencilView* dsv = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
		DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();
		float splitFar = 0.0f;
	};
	std::vector<CascadeMap> g_cascades;

	DirectX::XMFLOAT3 g_lightDirWS {0.0f, -1.0f, 0.0f};
	float g_cascadeBlend = 2.0f;
	float g_shadowDistance = 60.0f;

	D3D11_VIEWPORT g_vp {};

	ID3D11RenderTargetView* g_savedRTV = nullptr;
	ID3D11DepthStencilView* g_savedDSV = nullptr;
	std::vector<D3D11_VIEWPORT> g_savedVPs;

	ID3D11RasterizerState* g_rsShadowFront = nullptr;
	ID3D11RasterizerState* g_rsShadowBack = nullptr;
	ID3D11RasterizerState* g_rsSaved = nullptr;

	bool g_backfaceDepth = true;

	float g_receiverBias = 0.0015f;
	float g_texelSize = 0.0f;

	int g_currentRenderingCascade = -1;

	constexpr float kSplitLambda = 0.65f;
	constexpr float kZPadding = 25.0f;

	struct ShadowConfig {
		UINT size;
		DXGI_FORMAT texFormat;
		DXGI_FORMAT dsvFormat;
		DXGI_FORMAT srvFormat;
		INT   depthBias;
		FLOAT slopeScaledDepthBias;
		FLOAT depthBiasClamp;
		FLOAT receiverBias;
	};

	DirectX::XMVECTOR SafeNormalize3(DirectX::XMVECTOR v, DirectX::XMVECTOR fallback) {
		DirectX::XMVECTOR lenSq = DirectX::XMVector3LengthSq(v);
		if (DirectX::XMVectorGetX(lenSq) < 1e-8f)
			return DirectX::XMVector3Normalize(fallback);
		return DirectX::XMVector3Normalize(v);
	}

	void BuildFrustumCorners(
		const Shadow::CameraShadowParams& cam,
		float nearZ,
		float farZ,
		DirectX::XMVECTOR outCorners[8])
	{
		using namespace DirectX;
		XMVECTOR eye = XMLoadFloat3(&cam.eye);
		XMVECTOR at = XMLoadFloat3(&cam.at);
		XMVECTOR upIn = XMLoadFloat3(&cam.up);

		XMVECTOR forward = SafeNormalize3(at - eye, XMVectorSet(0, 0, 1, 0));
		XMVECTOR right = SafeNormalize3(XMVector3Cross(upIn, forward), XMVectorSet(1, 0, 0, 0));
		// Re-orthogonalize up in case look-at is near up axis
		XMVECTOR up = SafeNormalize3(XMVector3Cross(forward, right), XMVectorSet(0, 1, 0, 0));
		right = XMVector3Cross(up, forward);

		const float fovY = XMConvertToRadians(cam.fovYDegrees);
		const float tanHalf = tanf(fovY * 0.5f);
		const float nearH = tanHalf * nearZ;
		const float nearW = nearH * cam.aspect;
		const float farH = tanHalf * farZ;
		const float farW = farH * cam.aspect;

		const XMVECTOR nc = eye + forward * nearZ;
		const XMVECTOR fc = eye + forward * farZ;

		outCorners[0] = nc + up * nearH - right * nearW;
		outCorners[1] = nc + up * nearH + right * nearW;
		outCorners[2] = nc - up * nearH - right * nearW;
		outCorners[3] = nc - up * nearH + right * nearW;
		outCorners[4] = fc + up * farH - right * farW;
		outCorners[5] = fc + up * farH + right * farW;
		outCorners[6] = fc - up * farH - right * farW;
		outCorners[7] = fc - up * farH + right * farW;
	}

	DirectX::XMMATRIX BuildLightView(DirectX::XMVECTOR focus, DirectX::XMVECTOR lightDir) {
		using namespace DirectX;
		lightDir = SafeNormalize3(lightDir, XMVectorSet(0, -1, 0, 0));
		// Directional shine dir points toward scene; eye looks from opposite side.
		XMVECTOR eye = focus - lightDir * 200.0f;
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		if (fabsf(XMVectorGetX(XMVector3Dot(lightDir, up))) > 0.95f)
			up = XMVectorSet(0, 0, 1, 0);
		return XMMatrixLookAtLH(eye, focus, up);
	}

	void FitOrthoCascade(
		CascadeMap& cascade,
		DirectX::XMVECTOR cornersWS[8],
		DirectX::XMVECTOR lightDir,
		UINT mapSize)
	{
		using namespace DirectX;

		XMVECTOR center = XMVectorZero();
		for (int i = 0; i < 8; ++i)
			center += cornersWS[i];
		center *= (1.0f / 8.0f);

		XMMATRIX lightView = BuildLightView(center, lightDir);

		float minX = FLT_MAX, maxX = -FLT_MAX;
		float minY = FLT_MAX, maxY = -FLT_MAX;
		float minZ = FLT_MAX, maxZ = -FLT_MAX;
		for (int i = 0; i < 8; ++i) {
			XMVECTOR ls = XMVector3TransformCoord(cornersWS[i], lightView);
			XMFLOAT3 p; XMStoreFloat3(&p, ls);
			minX = (std::min)(minX, p.x); maxX = (std::max)(maxX, p.x);
			minY = (std::min)(minY, p.y); maxY = (std::max)(maxY, p.y);
			minZ = (std::min)(minZ, p.z); maxZ = (std::max)(maxZ, p.z);
		}

		float radius = 0.0f;
		const XMVECTOR aabbCenter = XMVectorSet((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f, 0);
		for (int i = 0; i < 8; ++i) {
			XMVECTOR ls = XMVector3TransformCoord(cornersWS[i], lightView);
			float d = XMVectorGetX(XMVector3Length(ls - aabbCenter));
			radius = (std::max)(radius, d);
		}
		radius = ceilf(radius * 16.0f) / 16.0f;
		if (radius < 0.5f) radius = 0.5f;

		XMVECTOR lsCenter = aabbCenter;
		{
			const float texelWorld = (radius * 2.0f) / static_cast<float>((std::max)(1u, mapSize));
			XMFLOAT3 c; XMStoreFloat3(&c, lsCenter);
			c.x = floorf(c.x / texelWorld) * texelWorld;
			c.y = floorf(c.y / texelWorld) * texelWorld;
			lsCenter = XMLoadFloat3(&c);
		}

		XMMATRIX invLightView = XMMatrixInverse(nullptr, lightView);
		XMVECTOR snappedFocus = XMVector3TransformCoord(lsCenter, invLightView);
		lightView = BuildLightView(snappedFocus, lightDir);

		minZ = FLT_MAX; maxZ = -FLT_MAX;
		for (int i = 0; i < 8; ++i) {
			XMVECTOR ls = XMVector3TransformCoord(cornersWS[i], lightView);
			float z = XMVectorGetZ(ls);
			minZ = (std::min)(minZ, z);
			maxZ = (std::max)(maxZ, z);
		}

		minZ -= kZPadding;
		maxZ += kZPadding;
		if (maxZ <= minZ + 0.1f)
			maxZ = minZ + 0.1f;

		cascade.view = lightView;
		cascade.proj = XMMatrixOrthographicOffCenterLH(
			-radius, radius,
			-radius, radius,
			minZ, maxZ);
	}
}

namespace Shadow {
	bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, UINT size, int cascadeCount) {
		Finalize();
		g_dev = dev; g_ctx = ctx;
		if (!g_dev || !g_ctx) return false;
		if (cascadeCount <= 0) cascadeCount = 1;
		if (cascadeCount > MAX_CASCADES) cascadeCount = MAX_CASCADES;

		ShadowConfig cfg {};
		cfg.size = 1024;
		cfg.texFormat = DXGI_FORMAT_R32_TYPELESS;
		cfg.dsvFormat = DXGI_FORMAT_D32_FLOAT;
		cfg.srvFormat = DXGI_FORMAT_R32_FLOAT;
		cfg.depthBias = 0;
		cfg.slopeScaledDepthBias = 1.5f;
		cfg.depthBiasClamp = 1.0f;
		cfg.receiverBias = 0.0015f;
		if (size != 0) cfg.size = size;

		g_size = cfg.size;
		g_texFormat = cfg.texFormat;
		g_dsvFormat = cfg.dsvFormat;
		g_srvFormat = cfg.srvFormat;
		g_receiverBias = cfg.receiverBias;
		g_texelSize = 1.0f / static_cast<float>(g_size);

		g_vp = {};
		g_vp.Width = static_cast<FLOAT>(g_size);
		g_vp.Height = static_cast<FLOAT>(g_size);
		g_vp.MinDepth = 0.0f;
		g_vp.MaxDepth = 1.0f;
		g_vp.TopLeftX = 0.0f;
		g_vp.TopLeftY = 0.0f;

		{
			D3D11_RASTERIZER_DESC rd {};
			rd.FillMode = D3D11_FILL_SOLID;
			rd.DepthClipEnable = TRUE;
			rd.MultisampleEnable = FALSE;
			rd.AntialiasedLineEnable = FALSE;
			rd.DepthBias = cfg.depthBias;
			rd.SlopeScaledDepthBias = cfg.slopeScaledDepthBias;
			rd.DepthBiasClamp = cfg.depthBiasClamp;

			rd.CullMode = D3D11_CULL_FRONT;
			if (FAILED(g_dev->CreateRasterizerState(&rd, &g_rsShadowFront))) { Finalize(); return false; }

			rd.CullMode = D3D11_CULL_BACK;
			if (FAILED(g_dev->CreateRasterizerState(&rd, &g_rsShadowBack))) { Finalize(); return false; }
		}

		g_cascades.resize(cascadeCount);
		for (int i = 0; i < cascadeCount; ++i) {
			CascadeMap& C = g_cascades[i];

			D3D11_TEXTURE2D_DESC td {};
			td.Width = g_size; td.Height = g_size;
			td.MipLevels = 1; td.ArraySize = 1;
			td.Format = g_texFormat;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
			// On failure, release any cascades already created in earlier loop iterations
			// (Finalize() also tears down the rasterizer states created just above).
			if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &C.tex))) { Finalize(); return false; }

			D3D11_DEPTH_STENCIL_VIEW_DESC dsvd {};
			dsvd.Format = g_dsvFormat;
			dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			if (FAILED(g_dev->CreateDepthStencilView(C.tex, &dsvd, &C.dsv))) { Finalize(); return false; }

			D3D11_SHADER_RESOURCE_VIEW_DESC srvd {};
			srvd.Format = g_srvFormat;
			srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvd.Texture2D.MipLevels = 1;
			if (FAILED(g_dev->CreateShaderResourceView(C.tex, &srvd, &C.srv))) { Finalize(); return false; }
		}

		g_currentRenderingCascade = -1;
		return true;
	}

	bool Reconfigure(UINT size, int cascadeCount) {
		if (!g_dev || !g_ctx) return false;
		if (cascadeCount <= 0) cascadeCount = 1;
		if (cascadeCount > MAX_CASCADES) cascadeCount = MAX_CASCADES;
		if (size == 0) size = 1024;

		if (size == g_size && cascadeCount == (int)g_cascades.size() && !g_cascades.empty())
			return true;

		ID3D11Device* dev = g_dev;
		ID3D11DeviceContext* ctx = g_ctx;
		const DirectX::XMFLOAT3 dir = g_lightDirWS;
		const bool backface = g_backfaceDepth;

		if (!Initialize(dev, ctx, size, cascadeCount))
			return false;

		g_lightDirWS = dir;
		g_backfaceDepth = backface;
		return true;
	}

	void Finalize() {
		// The shader keeps raw pointers to the cascade SRVs for rebinding: drop them
		// before the SRVs are released (quality change, scene teardown).
		Shader_ClearShadowMaps();
		if (g_ctx) {
			if (g_savedRTV || g_savedDSV) g_ctx->OMSetRenderTargets(0, nullptr, nullptr);
			if (g_rsSaved) { g_ctx->RSSetState(g_rsSaved); g_rsSaved->Release(); g_rsSaved = nullptr; }
		}
		if (g_rsShadowFront) { g_rsShadowFront->Release(); g_rsShadowFront = nullptr; }
		if (g_rsShadowBack) { g_rsShadowBack->Release();  g_rsShadowBack = nullptr; }

		for (auto& C : g_cascades) {
			if (C.srv) { C.srv->Release(); C.srv = nullptr; }
			if (C.dsv) { C.dsv->Release(); C.dsv = nullptr; }
			if (C.tex) { C.tex->Release(); C.tex = nullptr; }
		}
		g_cascades.clear();

		g_dev = nullptr; g_ctx = nullptr;
		g_savedRTV = nullptr; g_savedDSV = nullptr;
		g_savedVPs.clear();
		g_currentRenderingCascade = -1;
	}

	void SetLightDirection(int index, const DirectX::XMFLOAT3& dir) {
		(void)index;
		DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir));
		DirectX::XMStoreFloat3(&g_lightDirWS, v);
	}

	DirectX::XMFLOAT3 GetLightDirection(int index) {
		(void)index;
		return g_lightDirWS;
	}

	int GetCascadeCount() { return (int)g_cascades.size(); }
	int GetLightCount() { return GetCascadeCount(); }
	int GetMaxCasters() { return MAX_CASTERS; }

	DirectX::XMFLOAT4 GetCascadeSplits() {
		DirectX::XMFLOAT4 s {0, 0, 0, 0};
		const int n = GetCascadeCount();
		if (n > 0) s.x = g_cascades[0].splitFar;
		if (n > 1) s.y = g_cascades[1].splitFar;
		if (n > 2) s.z = g_cascades[2].splitFar;
		if (n > 3) s.w = g_cascades[3].splitFar;
		return s;
	}

	float GetCascadeBlend() { return g_cascadeBlend; }

	void UpdateForCamera(const CameraShadowParams& cam, float maxShadowDistance) {
		using namespace DirectX;
		if (g_cascades.empty()) return;

		g_shadowDistance = (std::max)(1.0f, maxShadowDistance);
		const int N = (int)g_cascades.size();

		const float nearZ = (std::max)(0.05f, cam.nearClip);
		const float farZ = (std::min)(cam.farClip, g_shadowDistance);
		const float shadowFar = (std::max)(nearZ + 0.5f, farZ);

		float splits[MAX_CASCADES] = {};
		for (int i = 0; i < N; ++i) {
			const float p = static_cast<float>(i + 1) / static_cast<float>(N);
			const float logSplit = nearZ * powf(shadowFar / nearZ, p);
			const float uniSplit = nearZ + (shadowFar - nearZ) * p;
			splits[i] = uniSplit * (1.0f - kSplitLambda) + logSplit * kSplitLambda;
		}

		XMVECTOR lightDir = XMLoadFloat3(&g_lightDirWS);

		for (int i = 0; i < N; ++i) {
			const float sliceNear = (i == 0) ? nearZ : splits[i - 1];
			const float sliceFar = splits[i];
			g_cascades[i].splitFar = sliceFar;

			XMVECTOR corners[8];
			BuildFrustumCorners(cam, sliceNear, sliceFar, corners);
			FitOrthoCascade(g_cascades[i], corners, lightDir, g_size);
		}

		// Blend band ~ a small fraction of the first cascade depth span
		const float firstSpan = (std::max)(0.5f, splits[0] - nearZ);
		g_cascadeBlend = firstSpan * 0.35f;
		g_cascadeBlend = (std::max)(1.0f, (std::min)(g_cascadeBlend, 4.0f));
	}

	DirectX::XMMATRIX GetLightView(int cascadeIndex) {
		if (cascadeIndex < 0 || cascadeIndex >= (int)g_cascades.size())
			return DirectX::XMMatrixIdentity();
		return g_cascades[cascadeIndex].view;
	}

	DirectX::XMMATRIX GetLightProj(int cascadeIndex) {
		if (cascadeIndex < 0 || cascadeIndex >= (int)g_cascades.size())
			return DirectX::XMMatrixIdentity();
		return g_cascades[cascadeIndex].proj;
	}

	DirectX::XMMATRIX GetLightViewProj(int cascadeIndex) {
		return GetLightView(cascadeIndex) * GetLightProj(cascadeIndex);
	}

	void Begin(int cascadeIndex) {
		if (cascadeIndex < 0 || cascadeIndex >= (int)g_cascades.size()) return;

		g_currentRenderingCascade = cascadeIndex;

		g_ctx->OMGetRenderTargets(1, &g_savedRTV, &g_savedDSV);
		UINT vpCount = 0; g_ctx->RSGetViewports(&vpCount, nullptr);
		g_savedVPs.resize(vpCount);
		if (vpCount) g_ctx->RSGetViewports(&vpCount, g_savedVPs.data());
		if (g_rsSaved) { g_rsSaved->Release(); g_rsSaved = nullptr; }
		g_ctx->RSGetState(&g_rsSaved);

		ID3D11ShaderResourceView* nullSRV[Shadow::MAX_CASCADES] = {};
		g_ctx->PSSetShaderResources(1, Shadow::MAX_CASCADES, nullSRV);
		g_ctx->VSSetShaderResources(1, Shadow::MAX_CASCADES, nullSRV);

		ID3D11RasterizerState* rs = g_backfaceDepth ? g_rsShadowFront : g_rsShadowBack;
		if (rs) g_ctx->RSSetState(rs);

		g_ctx->OMSetRenderTargets(0, nullptr, g_cascades[cascadeIndex].dsv);
		g_ctx->ClearDepthStencilView(g_cascades[cascadeIndex].dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		g_ctx->RSSetViewports(1, &g_vp);
	}

	void End() {
		if (g_savedRTV || g_savedDSV) {
			g_ctx->OMSetRenderTargets(1, &g_savedRTV, g_savedDSV);
		}
		if (!g_savedVPs.empty()) {
			g_ctx->RSSetViewports(static_cast<UINT>(g_savedVPs.size()), g_savedVPs.data());
		}
		if (g_rsSaved) {
			g_ctx->RSSetState(g_rsSaved);
			g_rsSaved->Release();
			g_rsSaved = nullptr;
		}
		if (g_savedRTV) { g_savedRTV->Release(); g_savedRTV = nullptr; }
		if (g_savedDSV) { g_savedDSV->Release(); g_savedDSV = nullptr; }
		g_savedVPs.clear();

		g_currentRenderingCascade = -1;
	}

	ID3D11ShaderResourceView* GetShadowSRV(int cascadeIndex) {
		if (cascadeIndex < 0 || cascadeIndex >= (int)g_cascades.size()) return nullptr;
		return g_cascades[cascadeIndex].srv;
	}

	UINT GetShadowMapSize() { return g_size; }
	float GetReceiverBias() { return g_receiverBias; }
	float GetTexelSize() { return g_texelSize; }

	int GetRenderingLightIndex() {
		return g_currentRenderingCascade;
	}

	void SetBackfaceDepth(bool enable) { g_backfaceDepth = enable; }
	bool IsBackfaceDepthEnabled() { return g_backfaceDepth; }
} // namespace Shadow
