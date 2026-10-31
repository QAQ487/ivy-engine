/**===========================================================================

 File   :  [sprite.cpp]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include <d3d11.h>
#include <DirectXMath.h>
 // #include "direct3d.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/shader.h"
#include "texture.h"
#include <math.h>
#include <float.h>
#include "engine/debug_draw/debug_ofstream.h"
#include "sprite.h"
#include "engine/core/frame_capture.h"

#include <algorithm>
#include <vector>
using namespace DirectX;

static constexpr int NUM_VERTEX = 4;  // Vertex count

static ID3D11RasterizerState* g_worldRasterizer = nullptr;
static ID3D11Buffer* g_pVertexBuffer = nullptr; // Vertex buffer
static ID3D11Buffer* g_pRadialVertexBuffer = nullptr; // Radial fill vertex buffer
static ID3D11Buffer* g_pRadialIndexBuffer = nullptr; // Radial fill index buffer

// Device / context (set at Initialize)
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

// Vertex data
struct Vertex {
	XMFLOAT3 position;
	XMFLOAT4 color;
	XMFLOAT2 texcoord;
};

// Quad index list (2 triangles)
static const UINT SpriteIndices[6] = {0, 1, 2, 0, 2, 3};
static ID3D11Buffer* g_pIndexBuffer = nullptr;

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
	g_pDevice = pDevice;
	g_pContext = pContext;

	// Vertex buffer (dynamic; one quad)
	if (!g_pVertexBuffer) {
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		bd.StructureByteStride = sizeof(Vertex);
		g_pDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);
	}

	// Index buffer (quad = 2 triangles)
	if (!g_pIndexBuffer) {
		D3D11_BUFFER_DESC ibd = {};
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(SpriteIndices);
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = SpriteIndices;
		g_pDevice->CreateBuffer(&ibd, &initData, &g_pIndexBuffer);
	}
}

void Sprite_Finalize(void) {
    if(g_worldRasterizer) { g_worldRasterizer->Release(); g_worldRasterizer=nullptr; }
	if (g_pVertexBuffer) { g_pVertexBuffer->Release(); g_pVertexBuffer = nullptr; }
	if (g_pIndexBuffer) { g_pIndexBuffer->Release(); g_pIndexBuffer = nullptr; }
	if (g_pRadialVertexBuffer) { g_pRadialVertexBuffer->Release(); g_pRadialVertexBuffer = nullptr; }
	if (g_pRadialIndexBuffer) { g_pRadialIndexBuffer->Release(); g_pRadialIndexBuffer = nullptr; }
}

void Sprite_Draw(int texid, float X, float Y, XMFLOAT4 color) {
	// Texture size
	auto size = TextureGetSize(texid);

	Sprite_Draw(texid, X, Y, (float) size.x, (float) size.y, 0, 0, (int) size.x, (int) size.y, color);
}

void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, XMFLOAT4 color) {
	Sprite_Draw(texid, X, Y, width, height, tx, ty, tw, th, 0.0f, color);
}

void Sprite_Draw(int texid, float X, float Y, float width, float height, int tx, int ty, int tw, int th, float angle, XMFLOAT4 color) {
	if (texid < 0) return;

	// Texture size
	auto texWidth = (float) TextureGetWidth(texid);
	auto texHeight = (float) TextureGetHeight(texid);
	if (texWidth <= 0.0f || texHeight <= 0.0f) return;

	// Compute UVs
	float u0 = (float) tx / texWidth;
	float v0 = (float) ty / texHeight;
	float u1 = (float) (tx + tw) / texWidth;
	float v1 = (float) (ty + th) / texHeight;

	// Pivot at center; build verts relative to center
	float cx = X + width * 0.5f;
	float cy = Y + height * 0.5f;

	// Local verts (TL -> TR -> BR -> BL)
	XMFLOAT3 pos[4] = {
		{-width * 0.5f, -height * 0.5f, 0.0f}, // Top-left
		{width * 0.5f, -height * 0.5f, 0.0f}, // Top-right
		{width * 0.5f, height * 0.5f, 0.0f}, // Bottom-right
		{-width * 0.5f, height * 0.5f, 0.0f}, // Bottom-left
	};
	XMFLOAT2 uv[4] = {
		{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}
	};

	// Rotate
	float rad = XMConvertToRadians(angle);
	float s = sinf(rad);
	float c = cosf(rad);

	Vertex vtx[NUM_VERTEX] {};
	for (int i = 0; i < NUM_VERTEX; ++i) {
		float px = pos[i].x;
		float py = pos[i].y;
		// Rotate
		float rx = px * c - py * s;
		float ry = px * s + py * c;
		// Translate into screen space
		vtx[i].position = XMFLOAT3(cx + rx, cy + ry, 0.0f);
		vtx[i].color = color;
		vtx[i].texcoord = uv[i];
	}

	// Update vertex buffer
	D3D11_MAPPED_SUBRESOURCE msr;
	if (SUCCEEDED(g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
		memcpy(msr.pData, vtx, sizeof(vtx));
		g_pContext->Unmap(g_pVertexBuffer, 0);
	}

	// Bind shaders / buffers / texture
	Shader2D_Begin();
	SetDepthEnable(false);
	Shader_SetColor(color);

	// Bind texture
	TextureSetTexture(texid);

	// Set world / view / projection (2D)
	Shader_SetViewMatrix(DirectX::XMMatrixIdentity());
	Shader_SetProjectionMatrix(DirectX::XMMatrixOrthographicOffCenterLH(
		0.0f, GetRenderViewportWidth(), GetRenderViewportHeight(), 0.0f, 0.0f, 1.0f));
	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());

	// Bind vertex / index buffers
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw
	FrameCapture_RecordSpriteDraw();
    g_pContext->DrawIndexed(6, 0, 0);
}

void Sprite_DrawRadialFill(int texid, float X, float Y, float width, float height,
	int tx, int ty, int tw, int th, float angle,
	float fillAmount, float startAngle, bool clockwise,
	XMFLOAT4 color) {
	if (texid < 0 || fillAmount <= 0.0f) return;

	// Clamp fill amount
	fillAmount = (fillAmount < 0.0f) ? 0.0f : (fillAmount > 1.0f ? 1.0f : fillAmount);

	// Texture and UV setup
	auto texWidth = (float) TextureGetWidth(texid);
	auto texHeight = (float) TextureGetHeight(texid);
	if (texWidth <= 0.0f || texHeight <= 0.0f) return;
	float u0 = (float) tx / texWidth;
	float v0 = (float) ty / texHeight;
	float u1 = (float) (tx + tw) / texWidth;
	float v1 = (float) (ty + th) / texHeight;
	float uCenter = (u0 + u1) * 0.5f;
	float vCenter = (v0 + v1) * 0.5f;

	// Center position
	float cx = X + width * 0.5f;
	float cy = Y + height * 0.5f;

	// Rotation cache (Sprite Rotation)
	float rotRad = XMConvertToRadians(angle);
	float rotSin = sinf(rotRad);
	float rotCos = cosf(rotRad);

	// --- 1. Calculate Draw Angles ---
	float startRad = XMConvertToRadians(startAngle);
	float totalRad = XMConvertToRadians(fillAmount * 360.0f);
	float dir = clockwise ? 1.0f : -1.0f;

	std::vector<float> renderAngles;
	renderAngles.reserve(64);

	// A. Start and End angles
	renderAngles.push_back(startRad);
	renderAngles.push_back(startRad + totalRad * dir);

	// B. Intermediate smoothness steps
	const int smoothnessSteps = 32;
	for (int i = 1; i < smoothnessSteps; ++i) {
		float t = (float) i / (float) smoothnessSteps;
		renderAngles.push_back(startRad + (totalRad * dir * t));
	}

	// C. Inject Corners (Fixes the cropping issue)
	float halfW = width * 0.5f;
	float halfH = height * 0.5f;
	struct Corner { float x, y; };
	Corner corners[4] = {
		{halfW, -halfH}, {halfW, halfH}, {-halfW, halfH}, {-halfW, -halfH}
	};

	for (const auto& c : corners) {
		// Calculate angle of this corner relative to center
		float cornerAng = atan2f(c.y, c.x);

		// Normalize cornerAng relative to startRad
		float diff = cornerAng - startRad;

		// Unwrap wrap-around angles
		if (clockwise) {
			while (diff < 0.0f) diff += XM_2PI;
			while (diff >= XM_2PI) diff -= XM_2PI;
		}
		else {
			while (diff > 0.0f) diff -= XM_2PI;
			while (diff <= -XM_2PI) diff += XM_2PI;
		}

		// If corner is within the fill arc, add it
		if (std::abs(diff) <= std::abs(totalRad) && std::abs(diff) > 0.0001f) {
			renderAngles.push_back(startRad + diff);
		}
	}

	// D. Sort angles to form a clean fan
	if (clockwise) {
		std::sort(renderAngles.begin(), renderAngles.end());
	}
	else {
		std::sort(renderAngles.begin(), renderAngles.end(), std::greater<float>());
	}

	// Remove duplicates (e.g. if start angle matches a corner exactly)
	auto last = std::unique(renderAngles.begin(), renderAngles.end(),
		[] (float a, float b) { return std::abs(a - b) < 0.001f; });
	renderAngles.erase(last, renderAngles.end());

	// Need at least 2 perimeter points to form a triangle fan.
	if (renderAngles.size() < 2) return;

	// --- 2. Buffer Management ---
	int numVertices = (int) renderAngles.size() + 1; // +1 for Center
	int numIndices = ((int) renderAngles.size() - 1) * 3;
	static int currentMaxVerts = 0;

	// Recreate buffers if needed (simple dynamic growth)
	if (!g_pRadialVertexBuffer || currentMaxVerts < numVertices) {
		if (g_pRadialVertexBuffer) g_pRadialVertexBuffer->Release();
		if (g_pRadialIndexBuffer) g_pRadialIndexBuffer->Release();

		currentMaxVerts = numVertices + 64;

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * currentMaxVerts;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		g_pDevice->CreateBuffer(&bd, nullptr, &g_pRadialVertexBuffer);

		D3D11_BUFFER_DESC ibd = {};
		ibd.Usage = D3D11_USAGE_DYNAMIC;
		ibd.ByteWidth = sizeof(UINT) * (currentMaxVerts * 3);
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		g_pDevice->CreateBuffer(&ibd, nullptr, &g_pRadialIndexBuffer);
	}

	// --- 3. Build Vertices ---
	std::vector<Vertex> vertices(numVertices);

	// Center Vertex
	vertices[0].position = XMFLOAT3(cx, cy, 0.0f);
	vertices[0].color = color;
	vertices[0].texcoord = XMFLOAT2(uCenter, vCenter);

	// Perimeter Vertices
	for (size_t i = 0; i < renderAngles.size(); ++i) {
		float currentAngle = renderAngles[i];
		float dx = cosf(currentAngle);
		float dy = sinf(currentAngle);

		// Precise Ray-Box Intersection for rectangular edge
		float tRight = (dx > 0.0f) ? (halfW / dx) : FLT_MAX;
		float tLeft = (dx < 0.0f) ? (-halfW / dx) : FLT_MAX;
		float tBottom = (dy > 0.0f) ? (halfH / dy) : FLT_MAX;
		float tTop = (dy < 0.0f) ? (-halfH / dy) : FLT_MAX;

		float tMin = FLT_MAX;
		if (tRight > 0.0f) tMin = min(tMin, tRight);
		if (tLeft > 0.0f) tMin = min(tMin, tLeft);
		if (tBottom > 0.0f) tMin = min(tMin, tBottom);
		if (tTop > 0.0f) tMin = min(tMin, tTop);

		// Calculate local pos and clamp
		float localX = dx * tMin;
		float localY = dy * tMin;
		localX = (localX > halfW) ? halfW : (localX < -halfW ? -halfW : localX);
		localY = (localY > halfH) ? halfH : (localY < -halfH ? -halfH : localY);

		// Apply Sprite Rotation
		float rx = localX * rotCos - localY * rotSin;
		float ry = localX * rotSin + localY * rotCos;

		vertices[i + 1].position = XMFLOAT3(cx + rx, cy + ry, 0.0f);
		vertices[i + 1].color = color;

		// Map position to UV
		float uvX = uCenter + (localX / width) * (u1 - u0);
		float uvY = vCenter + (localY / height) * (v1 - v0);
		vertices[i + 1].texcoord = XMFLOAT2(uvX, uvY);
	}

	// --- 4. Build Indices ---
	std::vector<UINT> indices;
	indices.reserve(numIndices);
	for (size_t i = 0; i < renderAngles.size() - 1; ++i) {
		indices.push_back(0); // Center

		if (clockwise) {
			indices.push_back((UINT) i + 1);
			indices.push_back((UINT) i + 2);
		}
		else {
			indices.push_back((UINT) i + 2);
			indices.push_back((UINT) i + 1);
		}
	}

	// --- 5. Draw ---
	D3D11_MAPPED_SUBRESOURCE msr;
	if (SUCCEEDED(g_pContext->Map(g_pRadialVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
		memcpy(msr.pData, vertices.data(), sizeof(Vertex) * vertices.size());
		g_pContext->Unmap(g_pRadialVertexBuffer, 0);
	}
	if (SUCCEEDED(g_pContext->Map(g_pRadialIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
		memcpy(msr.pData, indices.data(), sizeof(UINT) * indices.size());
		g_pContext->Unmap(g_pRadialIndexBuffer, 0);
	}

	Shader2D_Begin();
	SetDepthEnable(false);
	Shader_SetColor(color);
	TextureSetTexture(texid);

	// Ensure matrices are set
	Shader_SetViewMatrix(DirectX::XMMatrixIdentity());
	Shader_SetProjectionMatrix(DirectX::XMMatrixOrthographicOffCenterLH(
		0.0f, GetRenderViewportWidth(), GetRenderViewportHeight(), 0.0f, 0.0f, 1.0f));
	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pRadialVertexBuffer, &stride, &offset);
	g_pContext->IASetIndexBuffer(g_pRadialIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	FrameCapture_RecordSpriteDraw();
    g_pContext->DrawIndexed((UINT) indices.size(), 0, 0);
}
void Sprite_DrawWorld(int texture, float width, float height, XMFLOAT2 pivot,
    XMFLOAT4 uv, XMFLOAT4 color, const XMMATRIX& world,
    const XMMATRIX& view, const XMMATRIX& projection) {
    if(texture<0 || !g_pVertexBuffer || width<=0 || height<=0) return;
    const float l=-pivot.x*width, r=l+width, b=-pivot.y*height, t=b+height;
    const Vertex vertices[4]={
        {{l,t,0},{1,1,1,1},{uv.x,uv.y}},{{r,t,0},{1,1,1,1},{uv.z,uv.y}},
        {{r,b,0},{1,1,1,1},{uv.z,uv.w}},{{l,b,0},{1,1,1,1},{uv.x,uv.w}}};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(g_pContext->Map(g_pVertexBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return;
    memcpy(mapped.pData,vertices,sizeof(vertices));
    g_pContext->Unmap(g_pVertexBuffer,0);
    if(!g_worldRasterizer) {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode=D3D11_FILL_SOLID; desc.CullMode=D3D11_CULL_NONE;
        desc.DepthClipEnable=TRUE;
        if(FAILED(g_pDevice->CreateRasterizerState(&desc,&g_worldRasterizer))) return;
    }
    ID3D11RasterizerState* previousRasterizer=nullptr;
    g_pContext->RSGetState(&previousRasterizer);
    g_pContext->RSSetState(g_worldRasterizer);
    Shader2D_Begin(); SetDepthEnable(false); Shader_SetColor(color);
    TextureSetTexture(texture); Shader_SetViewMatrix(view);
    Shader_SetProjectionMatrix(projection); Shader_SetWorldMatrix(world);
    UINT stride=sizeof(Vertex), offset=0;
    g_pContext->IASetVertexBuffers(0,1,&g_pVertexBuffer,&stride,&offset);
    g_pContext->IASetIndexBuffer(g_pIndexBuffer,DXGI_FORMAT_R32_UINT,0);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    FrameCapture_RecordSpriteDraw();
    g_pContext->DrawIndexed(6,0,0);
    g_pContext->RSSetState(previousRasterizer);
    if(previousRasterizer) previousRasterizer->Release();
}
