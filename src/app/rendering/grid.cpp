/**===========================================================================

 File   :  [grid.cpp]

 Author : Terry Yeung
 Date   : 2025/10/23
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "grid.h"
#include <vector>
#include <cmath>
#include "engine/rendering/d3d11/renderer.h" // VERTEX_3D, MATERIAL, GetDevice*, SCREEN_*
#include "engine/rendering/d3d11/shader.h"
#include "engine/scene/render_settings.h"
#include "camera.h"
#include "engine/rendering/model/model_util.h" // CreateSolidWhiteSRV を利用
#include "engine/physics/collider/collider.h"
#include "scene_manager.h"

using namespace DirectX;

static Grid g_grid;

static ID3D11Buffer* g_pVB = nullptr;
static ID3D11Buffer* g_pFillVB = nullptr;           // 背景塗りつぶし用のVB
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;
static UINT g_vertexCount = 0;
static ID3D11ShaderResourceView* g_pWhiteSRV = nullptr; // 1x1 白テクスチャ

static constexpr int GRID_COUNT = 5000;
static const XMFLOAT4 GRID_FILL_COLOR = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); // 画像のようなグレー

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
	vtx.reserve(lineCount * 2 * 2); // 11本×各2頂点×2方向

	const XMFLOAT4 WHITE = XMFLOAT4(1, 1, 1, 1);
	const XMFLOAT4 RED = XMFLOAT4(1, 0, 0, 1);
	const XMFLOAT4 BLUE = XMFLOAT4(0, 0, 1, 1);

	const XMFLOAT3 upNormal = XMFLOAT3(0, 1, 0);
	const XMFLOAT2 uv0 = XMFLOAT2(0, 0);

	// Z一定でXを引く（X方向の線）-> 世界座標Z=0のラインを赤に
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

	// X一定でZを引く（Z方向の線）-> 世界座標X=0のラインを青に
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

	// 頂点バッファ作成（ライン）
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

	// 背景塗りつぶし用の四角形VB（minC..maxC を覆う2三角形）
	{
		VERTEX_3D fill[6];
		const XMFLOAT3 n = upNormal;
		const XMFLOAT2 uv = {0, 0};
		// 反時計回りで2枚
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

	// ここからコライダー設定とシーン登録
	{
		// const float width = maxC - minC; // = GRID_COUNT
		// const float depth = maxC - minC; // = GRID_COUNT
		// const float thickness = 0.2f;    // 薄めの床
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
	// グリッドは静的なので特に更新なし
}

void Grid::Draw() {
	if ((!g_pFillVB && !g_pVB) || g_vertexCount == 0) return;

	Shader3D_Begin(RenderSettings_GetDefaultShader());
	SetDepthEnable(false);

	Camera& cam = GetCamera();
	Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
	Shader_SetViewMatrix(cam.GetViewMatrix());

	const auto pos = GetPosition();
	XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
	Shader_SetWorldMatrix(T);

	// 1) 背景塗り（グレー）
	if (g_pFillVB) {
		MATERIAL mat {}; mat.Diffuse = XMFLOAT4(1, 1, 1, 1);
		mat.Diffuse = XMFLOAT4(1, 1, 1, 1);
		mat.Ambient = XMFLOAT4(1, 1, 1, 1);
		Shader_SetMaterial(mat);
		if (g_pWhiteSRV) {
			g_pContext->PSSetShaderResources(0, 1, &g_pWhiteSRV);
		}
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		g_pContext->IASetVertexBuffers(0, 1, &g_pFillVB, &stride, &offset);
		g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		g_pContext->Draw(6, 0);
	}

	// 2) ライン（従来どおり）
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

	// 3) デバッグ表示（任意）
	if (m_debugCollider && m_collider) {
		m_collider->DebugDraw();
	}
}

void Grid_Initialize() {
	g_grid.Initialize();
}

void Grid_Finalize() {
	g_grid.Finalize();
}

void Grid_Draw() {
	g_grid.Draw();
}