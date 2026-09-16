/**===========================================================================
 File   : line_sprite.cpp
 Brief  : Billboarded world-space quad via 2D sprite path (depth ON).
 ===========================================================================**/
#include "line_sprite.h"

#include "texture.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/shader.h"
#include "camera.h"

#include <cmath>
#include <cstring>

using namespace DirectX;

namespace {

constexpr int kNumVertex = 4;
constexpr UINT kLineIndices[6] = { 0, 1, 2, 0, 2, 3 };

struct Vertex {
	XMFLOAT3 position;
	XMFLOAT4 color;
	XMFLOAT2 texcoord;
};

ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;

} // namespace

void LineSprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
	g_pDevice = pDevice;
	g_pContext = pContext;

	if (!g_pVertexBuffer) {
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * kNumVertex;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.StructureByteStride = sizeof(Vertex);
		g_pDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);
	}

	if (!g_pIndexBuffer) {
		D3D11_BUFFER_DESC ibd = {};
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(kLineIndices);
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = kLineIndices;
		g_pDevice->CreateBuffer(&ibd, &initData, &g_pIndexBuffer);
	}
}

void LineSprite_Finalize() {
	if (g_pVertexBuffer) {
		g_pVertexBuffer->Release();
		g_pVertexBuffer = nullptr;
	}
	if (g_pIndexBuffer) {
		g_pIndexBuffer->Release();
		g_pIndexBuffer = nullptr;
	}
	g_pDevice = nullptr;
	g_pContext = nullptr;
}

void LineSprite_Draw(
	const DirectX::XMFLOAT3& a,
	const DirectX::XMFLOAT3& b,
	float thicknessWorld,
	const DirectX::XMFLOAT4& color) {
	if (!g_pContext || !g_pVertexBuffer || !g_pIndexBuffer)
		return;
	if (thicknessWorld <= 0.0f)
		return;

	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	const float dz = b.z - a.z;
	const float lenSq = dx * dx + dy * dy + dz * dz;
	if (lenSq < 1.0e-12f)
		return;

	const float invLen = 1.0f / sqrtf(lenSq);
	XMVECTOR axis = XMVectorSet(dx * invLen, dy * invLen, dz * invLen, 0.0f);

	const Camera& cam = GetCamera();
	XMFLOAT3 fwd = cam.GetForward();
	XMVECTOR camForward = XMLoadFloat3(&fwd);
	XMVECTOR camUp = XMLoadFloat3(&cam.up);

	XMVECTOR perp = XMVector3Cross(axis, camForward);
	float perpLen = 0.0f;
	XMStoreFloat(&perpLen, XMVector3LengthSq(perp));
	if (perpLen < 1.0e-8f) {
		perp = XMVector3Cross(axis, camUp);
		XMStoreFloat(&perpLen, XMVector3LengthSq(perp));
		if (perpLen < 1.0e-8f)
			return;
	}
	perp = XMVector3Normalize(perp);

	const float halfW = thicknessWorld * 0.5f;
	XMVECTOR aV = XMLoadFloat3(&a);
	XMVECTOR bV = XMLoadFloat3(&b);
	XMVECTOR offset = XMVectorScale(perp, halfW);

	XMFLOAT3 c0, c1, c2, c3;
	XMStoreFloat3(&c0, XMVectorSubtract(aV, offset));
	XMStoreFloat3(&c1, XMVectorAdd(aV, offset));
	XMStoreFloat3(&c2, XMVectorAdd(bV, offset));
	XMStoreFloat3(&c3, XMVectorSubtract(bV, offset));

	Vertex vtx[4] = {
		{ c0, color, { 0.0f, 0.0f } },
		{ c1, color, { 1.0f, 0.0f } },
		{ c2, color, { 1.0f, 1.0f } },
		{ c3, color, { 0.0f, 1.0f } },
	};

	D3D11_MAPPED_SUBRESOURCE msr;
	if (FAILED(g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
		return;
	memcpy(msr.pData, vtx, sizeof(vtx));
	g_pContext->Unmap(g_pVertexBuffer, 0);

	ID3D11DepthStencilState* prevDepth = nullptr;
	UINT prevStencilRef = 0;
	g_pContext->OMGetDepthStencilState(&prevDepth, &prevStencilRef);

	Shader2D_Begin();
	SetDepthEnable(true);
	Shader_SetColor(color);

	const int whiteId = TextureGetWhiteTexture();
	if (whiteId >= 0)
		TextureSetTexture(whiteId);

	const float vw = GetRenderViewportWidth();
	const float vh = GetRenderViewportHeight();
	Shader_SetViewMatrix(cam.GetViewMatrix());
	Shader_SetProjectionMatrix(cam.GetProjMatrix(vw, vh));
	Shader_SetWorldMatrix(XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offsetBytes = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offsetBytes);
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_pContext->DrawIndexed(6, 0, 0);

	g_pContext->OMSetDepthStencilState(prevDepth, prevStencilRef);
	if (prevDepth)
		prevDepth->Release();
}
