/**===========================================================================

 File   :  [grid.cpp]

 Author : Terry Yeung
 Date   : 2025/10/23
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "grid.h"
#include <vector>
#include <memory>
#include <cmath>
#include "engine/rendering/d3d11/renderer.h" // VERTEX_3D, MATERIAL, GetDevice*, SCREEN_*
#include "engine/rendering/d3d11/shader.h"
#include "camera.h"
#include "engine/rendering/model/model_util.h" // Uses CreateSolidWhiteSRV
#include "engine/physics/collider/collider.h"
#include "scene_manager.h"

using namespace DirectX;

// Destroy the GameObject while engine singletons are still alive.
static std::unique_ptr<Grid> g_grid;
static bool g_gridVisible = true;

static ID3D11Buffer* g_pVB = nullptr;
static ID3D11Buffer* g_pFillVB = nullptr;           // VB for background fill
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;
static UINT g_vertexCount = 0;
static ID3D11ShaderResourceView* g_pWhiteSRV = nullptr; // 1x1 white texture

static constexpr int GRID_COUNT = 5000;
static const XMFLOAT4 GRID_FILL_COLOR = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); // Gray matching the reference image

void Grid::Initialize() {
	g_pDevice = GetDevice();
	g_pContext = GetDeviceContext();

	SetTag(Tag::TAG_GROUND);

	const int gridCells = GRID_COUNT;
	const int half = gridCells / 2;
	const int lineCount = gridCells + 1;
	const float minC = -static_cast<float>(half);
	const float maxC = static_cast<float>(half);

	std::vector<VERTEX_3D> vtx;
	vtx.reserve(lineCount * 2 * 2); // 11 lines x 2 verts x 2 axes

	const XMFLOAT4 WHITE = XMFLOAT4(1, 1, 1, 1);
	const XMFLOAT4 RED = XMFLOAT4(1, 0, 0, 1);
	const XMFLOAT4 BLUE = XMFLOAT4(0, 0, 1, 1);

	const XMFLOAT3 upNormal = XMFLOAT3(0, 1, 0);
	const XMFLOAT2 uv0 = XMFLOAT2(0, 0);

	// Lines along X at fixed Z; world Z=0 line in red
	for (int i = 0; i < lineCount; ++i) {
		float zLocal = static_cast<float>(i - half);
		const bool isWorldZ0Line = (i == half);
		const XMFLOAT4 col = isWorldZ0Line ? RED : WHITE;

		VERTEX_3D a {}, b {};
		a.Position = XMFLOAT3(minC, 0.0f, zLocal);
		a.Normal = upNormal;
		a.Diffuse = col;
		a.TexCoord = uv0;

		b.Position = XMFLOAT3(maxC, 0.0f, zLocal);
		b.Normal = upNormal;
		b.Diffuse = col;
		b.TexCoord = uv0;

		vtx.push_back(a);
		vtx.push_back(b);
	}

	// Lines along Z at fixed X; world X=0 line in blue
	for (int i = 0; i < lineCount; ++i) {
		float xLocal = static_cast<float>(i - half);
		const bool isWorldX0Line = (i == half);
		const XMFLOAT4 col = isWorldX0Line ? BLUE : WHITE;

		VERTEX_3D a {}, b {};
		a.Position = XMFLOAT3(xLocal, 0.0f, minC);
		a.Normal = upNormal;
		a.Diffuse = col;
		a.TexCoord = uv0;

		b.Position = XMFLOAT3(xLocal, 0.0f, maxC);
		b.Normal = upNormal;
		b.Diffuse = col;
		b.TexCoord = uv0;

		vtx.push_back(a);
		vtx.push_back(b);
	}

	g_vertexCount = static_cast<UINT>(vtx.size());

	// Create vertex buffer (lines)
	{
		D3D11_BUFFER_DESC bd {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * g_vertexCount;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = vtx.data();

		if (FAILED(g_pDevice->CreateBuffer(&bd, &init, &g_pVB))) {
			g_pVB = nullptr;
		}
	}

	// Background quad VB (two triangles covering minC..maxC)
	{
		VERTEX_3D fill[6];
		const XMFLOAT3 n = upNormal;
		const XMFLOAT2 uv = {0, 0};
		// Two CCW triangles
		fill[0] = {{minC, 0.0f, minC}, n, GRID_FILL_COLOR, uv};
		fill[1] = {{minC, 0.0f, maxC}, n, GRID_FILL_COLOR, uv};
		fill[2] = {{maxC, 0.0f, maxC}, n, GRID_FILL_COLOR, uv};
		fill[3] = {{minC, 0.0f, minC}, n, GRID_FILL_COLOR, uv};
		fill[4] = {{maxC, 0.0f, maxC}, n, GRID_FILL_COLOR, uv};
		fill[5] = {{maxC, 0.0f, minC}, n, GRID_FILL_COLOR, uv};

		D3D11_BUFFER_DESC bd {};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = sizeof(fill);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA init {};
		init.pSysMem = fill;

		if (FAILED(g_pDevice->CreateBuffer(&bd, &init, &g_pFillVB))) {
			g_pFillVB = nullptr;
		}
	}

	if (!g_pWhiteSRV) {
		g_pWhiteSRV = CreateSolidWhiteSRV();
	}

	// Collider setup and scene registration
	{
		// const float width = maxC - minC; // = GRID_COUNT
		// const float depth = maxC - minC; // = GRID_COUNT
		// const float thickness = 0.2f;    // Thin floor
		//
		// if (!m_collider) {
		// 	m_collider = new BoxCollider(this);
		// }
		// static_cast<BoxCollider*>(m_collider)->SetSize({width, thickness, depth});
		// m_collider->SetCenter({0.0f, 0.0f, 0.0f});
		//
		// m_rigidbody.SetUseGravity(false);
		// m_rigidbody.SetIsKinematic(true);
		//
		// m_debugCollider = true;
		//
		// if (auto* scene = SceneManager::Instance().GetCurrentScene()) {
		// 	scene->AddObject(this);
		// }
	}

	m_visible = true;
	m_isActive = true;
}

void Grid::Finalize() {
	if (auto* scene = SceneManager::Instance().GetCurrentScene()) {
		scene->RemoveObject(this);
	}

	if (g_pFillVB) { g_pFillVB->Release(); g_pFillVB = nullptr; }
	if (g_pVB) { g_pVB->Release();      g_pVB = nullptr; }
	if (g_pWhiteSRV) { g_pWhiteSRV->Release(); g_pWhiteSRV = nullptr; }
}

void Grid::LogicUpdate(double /*elapsed_time*/) {
	// Grid is static; nothing to update
}

void Grid::Draw() {
	if (!m_visible) return;
	if ((!g_pFillVB && !g_pVB) || g_vertexCount == 0) return;

	// Editor overlay only: never use the scene default shader (IBL showcase = PBR).
	// PBR + bound IBL made the fill quad reflect the skybox (birds as grey blobs).
	Shader3D_Begin(Shader3DVariant::Standard);
	SetDepthEnable(false);

	Camera& cam = GetCamera();
	Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
	Shader_SetViewMatrix(cam.GetViewMatrix());

	const auto pos = GetPosition();
	XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
	Shader_SetWorldMatrix(T);

	// Skip the opaque grey fill quad. It was a 5000x5000 PBR plane and sampled IBL.
	if (g_pVB) {
		MATERIAL mat {}; mat.Diffuse = XMFLOAT4(1, 1, 1, 1);
		mat.Diffuse = XMFLOAT4(1, 1, 1, 1);
		mat.Ambient = XMFLOAT4(1, 1, 1, 1);
		Shader_SetMaterial(mat);
		if (g_pWhiteSRV) {
			g_pContext->PSSetShaderResources(0, 1, &g_pWhiteSRV);
		}
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		g_pContext->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);
		g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		g_pContext->Draw(g_vertexCount, 0);
	}

	// 3) Debug draw (optional)
	if (m_debugCollider && m_collider) {
		m_collider->DebugDraw();
	}
}

void Grid_Initialize() {
    if (g_grid) return;
    g_grid = std::make_unique<Grid>();
    g_grid->Initialize();
    g_grid->SetVisible(g_gridVisible);
}

void Grid_Finalize() {
    if (!g_grid) return;
    g_grid->Finalize();
    g_grid.reset();
}

void Grid_Draw() {
    if (g_grid && g_grid->IsVisible()) g_grid->Draw();
}

bool Grid_IsVisible() {
    return g_gridVisible;
}

void Grid_SetVisible(bool visible) {
    g_gridVisible = visible;
    if (g_grid) g_grid->SetVisible(visible);
}
