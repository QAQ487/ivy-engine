/**===========================================================================
 File   : LockOnUI.cpp
 Brief  : Billboard lock-on reticle at the magnet target.
============================================================================**/
#include "LockOnUI.h"

#include "engine/ecs/game_object.h"
#include "engine/scripting/script_runtime_api.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX;

GameObject* LockOnUI::s_target = nullptr;
LockOnTargetState LockOnUI::s_target_state = LockOnTargetState::None;
bool LockOnUI::s_is_active = false;

float LockOnUI::s_animation_timer = 0.0f;
float LockOnUI::s_rotation_angle = 0.0f;
float LockOnUI::s_pulse_scale = 1.0f;
float LockOnUI::s_lock_on_progress = 0.0f;
bool LockOnUI::s_is_locking_on = false;
bool LockOnUI::s_is_releasing = false;

float LockOnUI::s_outer_ring_rotation = 0.0f;
float LockOnUI::s_outer_ring_scale = 0.0f;
float LockOnUI::s_inner_ring_rotation = 0.0f;
float LockOnUI::s_inner_ring_scale = 0.0f;
float LockOnUI::s_corner_offset = 2.0f;
float LockOnUI::s_corner_scale = 0.0f;
float LockOnUI::s_pulse_timer = 0.0f;
float LockOnUI::s_pulse_alpha = 0.0f;
float LockOnUI::s_diamond_rotation = 0.0f;
float LockOnUI::s_diamond_scale = 0.0f;

ID3D11VertexShader* LockOnUI::s_ptr_vs = nullptr;
ID3D11PixelShader* LockOnUI::s_ptr_ps = nullptr;
ID3D11InputLayout* LockOnUI::s_ptr_il = nullptr;
ID3D11Buffer* LockOnUI::s_ptr_vertex_buffer = nullptr;
ID3D11Buffer* LockOnUI::s_ptr_index_buffer = nullptr;
ID3D11Buffer* LockOnUI::s_ptr_cb_transform = nullptr;
ID3D11Buffer* LockOnUI::s_ptr_cb_color = nullptr;
ID3D11BlendState* LockOnUI::s_ptr_blend_state = nullptr;
ID3D11DepthStencilState* LockOnUI::s_ptr_depth_state = nullptr;
ID3D11RasterizerState* LockOnUI::s_ptr_raster_state = nullptr;

ID3D11Device* LockOnUI::s_ptr_device = nullptr;
ID3D11DeviceContext* LockOnUI::s_ptr_context = nullptr;

DirectX::XMFLOAT3 LockOnUI::s_last_target_pos = {0.0f, 0.0f, 0.0f};

static const XMFLOAT4 AVAILABLE_COLOR = {220.0f / 255.0f, 20.0f / 255.0f, 60.0f / 255.0f, 1.0f};
static const XMFLOAT4 AVAILABLE_GLOW = {220.0f / 255.0f, 20.0f / 255.0f, 60.0f / 255.0f, 0.6f};
static const XMFLOAT4 COOLDOWN_COLOR = {1.0f, 0.92f, 0.15f, 1.0f};
static const XMFLOAT4 COOLDOWN_GLOW = {1.0f, 0.92f, 0.15f, 0.6f};
static const XMFLOAT4 TOO_FAR_COLOR = {0.25f, 0.9f, 0.35f, 1.0f};
static const XMFLOAT4 TOO_FAR_GLOW = {0.25f, 0.9f, 0.35f, 0.6f};

namespace {

static const char* VS_SOURCE = R"(
cbuffer CB_Transform : register(b0) {
    float4x4 world_view_proj;
};

struct VS_INPUT {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.position = mul(float4(input.position, 1.0f), world_view_proj);
    output.texcoord = input.texcoord;
    return output;
}
)";

static const char* PS_SOURCE = R"(
cbuffer CB_Color : register(b0) {
    float4 color;
    float time;
    float3 padding;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float2 centered = input.texcoord - float2(0.5f, 0.5f);
    float dist = length(centered) * 2.0f;
    float alpha = color.a * saturate(1.0f - dist * 0.5f);
    float glow = exp(-dist * 2.0f) * 0.5f;
    return float4(color.rgb + glow * 0.2f, alpha);
}
)";

} // namespace

XMFLOAT4 LockOnUI::PrimaryColor() {
	switch (s_target_state) {
	case LockOnTargetState::OnCooldown: return COOLDOWN_COLOR;
	case LockOnTargetState::TooFar: return TOO_FAR_COLOR;
	case LockOnTargetState::Available: return AVAILABLE_COLOR;
	default: return AVAILABLE_COLOR;
	}
}

XMFLOAT4 LockOnUI::GlowColor() {
	switch (s_target_state) {
	case LockOnTargetState::OnCooldown: return COOLDOWN_GLOW;
	case LockOnTargetState::TooFar: return TOO_FAR_GLOW;
	case LockOnTargetState::Available: return AVAILABLE_GLOW;
	default: return AVAILABLE_GLOW;
	}
}

bool LockOnUI::Initialize() {
	s_ptr_device = ScriptApi_GetD3DDevice();
	s_ptr_context = ScriptApi_GetD3DDeviceContext();

	if (!s_ptr_device || !s_ptr_context) return false;

	HRESULT hr;
	ID3DBlob* ptr_blob = nullptr;
	ID3DBlob* ptr_err = nullptr;

	hr = D3DCompile(VS_SOURCE, strlen(VS_SOURCE), nullptr, nullptr, nullptr,
		"main", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ptr_blob, &ptr_err);
	if (FAILED(hr)) {
		if (ptr_err) ptr_err->Release();
		return false;
	}
	hr = s_ptr_device->CreateVertexShader(ptr_blob->GetBufferPointer(), ptr_blob->GetBufferSize(), nullptr, &s_ptr_vs);
	if (FAILED(hr)) { ptr_blob->Release(); return false; }

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	hr = s_ptr_device->CreateInputLayout(layout, 2, ptr_blob->GetBufferPointer(), ptr_blob->GetBufferSize(), &s_ptr_il);
	ptr_blob->Release();
	if (FAILED(hr)) return false;

	hr = D3DCompile(PS_SOURCE, strlen(PS_SOURCE), nullptr, nullptr, nullptr,
		"main", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ptr_blob, &ptr_err);
	if (FAILED(hr)) {
		if (ptr_err) ptr_err->Release();
		return false;
	}
	hr = s_ptr_device->CreatePixelShader(ptr_blob->GetBufferPointer(), ptr_blob->GetBufferSize(), nullptr, &s_ptr_ps);
	ptr_blob->Release();
	if (FAILED(hr)) return false;

	Vertex vertices[] = {
		{{-0.5f, +0.5f, 0.0f}, {0.0f, 0.0f}},
		{{+0.5f, +0.5f, 0.0f}, {1.0f, 0.0f}},
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
		{{+0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
	};

	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(vertices);
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA init_data = {};
	init_data.pSysMem = vertices;
	hr = s_ptr_device->CreateBuffer(&vbd, &init_data, &s_ptr_vertex_buffer);
	if (FAILED(hr)) return false;

	UINT indices[] = {0, 1, 2, 1, 3, 2};
	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(indices);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	init_data.pSysMem = indices;
	hr = s_ptr_device->CreateBuffer(&ibd, &init_data, &s_ptr_index_buffer);
	if (FAILED(hr)) return false;

	D3D11_BUFFER_DESC cbd = {};
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	cbd.ByteWidth = sizeof(CB_Transform);
	hr = s_ptr_device->CreateBuffer(&cbd, nullptr, &s_ptr_cb_transform);
	if (FAILED(hr)) return false;

	cbd.ByteWidth = sizeof(CB_Color);
	hr = s_ptr_device->CreateBuffer(&cbd, nullptr, &s_ptr_cb_color);
	if (FAILED(hr)) return false;

	D3D11_BLEND_DESC blend_desc = {};
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = s_ptr_device->CreateBlendState(&blend_desc, &s_ptr_blend_state);
	if (FAILED(hr)) return false;

	D3D11_DEPTH_STENCIL_DESC depth_desc = {};
	depth_desc.DepthEnable = FALSE;
	depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	hr = s_ptr_device->CreateDepthStencilState(&depth_desc, &s_ptr_depth_state);
	if (FAILED(hr)) return false;

	D3D11_RASTERIZER_DESC raster_desc = {};
	raster_desc.FillMode = D3D11_FILL_SOLID;
	raster_desc.CullMode = D3D11_CULL_NONE;
	raster_desc.DepthClipEnable = TRUE;
	hr = s_ptr_device->CreateRasterizerState(&raster_desc, &s_ptr_raster_state);
	if (FAILED(hr)) return false;

	return true;
}

void LockOnUI::Finalize() {
	ResetIdleState();

	if (s_ptr_raster_state) { s_ptr_raster_state->Release(); s_ptr_raster_state = nullptr; }
	if (s_ptr_depth_state) { s_ptr_depth_state->Release(); s_ptr_depth_state = nullptr; }
	if (s_ptr_blend_state) { s_ptr_blend_state->Release(); s_ptr_blend_state = nullptr; }
	if (s_ptr_cb_color) { s_ptr_cb_color->Release(); s_ptr_cb_color = nullptr; }
	if (s_ptr_cb_transform) { s_ptr_cb_transform->Release(); s_ptr_cb_transform = nullptr; }
	if (s_ptr_index_buffer) { s_ptr_index_buffer->Release(); s_ptr_index_buffer = nullptr; }
	if (s_ptr_vertex_buffer) { s_ptr_vertex_buffer->Release(); s_ptr_vertex_buffer = nullptr; }
	if (s_ptr_il) { s_ptr_il->Release(); s_ptr_il = nullptr; }
	if (s_ptr_ps) { s_ptr_ps->Release(); s_ptr_ps = nullptr; }
	if (s_ptr_vs) { s_ptr_vs->Release(); s_ptr_vs = nullptr; }

	s_ptr_device = nullptr;
	s_ptr_context = nullptr;
}

void LockOnUI::ResetIdleState() {
	s_target = nullptr;
	s_target_state = LockOnTargetState::None;
	s_is_active = false;
	s_is_releasing = false;
	s_is_locking_on = false;
	s_lock_on_progress = 0.0f;
}

void LockOnUI::SetTarget(GameObject* target, LockOnTargetState state) {
	if (target) {
		s_target_state = state;

		if (!s_target && !s_is_releasing) {
			TriggerLockOnEffect();
			s_target = target;
		}
		else if (s_is_releasing) {
			s_is_releasing = false;
			TriggerLockOnEffect();
			s_target = target;
		}
		else if (s_target != target) {
			s_target = target;
			s_lock_on_progress = 1.0f;
		}
		else {
			s_target = target;
		}
		return;
	}

	if (s_is_releasing)
		return;

	if (s_target || s_is_active)
		TriggerReleaseEffect();
	else
		ResetIdleState();
}

void LockOnUI::TriggerLockOnEffect() {
	s_is_locking_on = true;
	s_is_releasing = false;
	s_lock_on_progress = 0.0f;
	s_is_active = true;
	s_outer_ring_scale = 3.0f;
	s_inner_ring_scale = 0.0f;
	s_corner_offset = 2.0f;
	s_corner_scale = 0.0f;
	s_diamond_scale = 0.0f;
	s_pulse_timer = 0.0f;
}

void LockOnUI::TriggerReleaseEffect() {
	s_is_releasing = true;
	s_is_locking_on = false;
	s_lock_on_progress = 1.0f;
}

void LockOnUI::Update(double elapsed_time) {
	float dt = static_cast<float>(elapsed_time);
	s_animation_timer += dt;

	if (s_target && !s_target->IsActive() && !s_is_releasing) {
		TriggerReleaseEffect();
	}

	if (s_is_locking_on) {
		s_lock_on_progress += dt * 4.0f;
		if (s_lock_on_progress >= 1.0f) {
			s_lock_on_progress = 1.0f;
			s_is_locking_on = false;
		}

		float ease = EaseOutBack(s_lock_on_progress);
		float elastic = EaseOutElastic(s_lock_on_progress);

		s_outer_ring_scale = 3.0f - 2.0f * ease;
		s_inner_ring_scale = elastic;
		s_corner_offset = 2.0f - 1.5f * ease;
		s_corner_scale = ease;

		float diamond_progress = (std::max)(0.0f, (s_lock_on_progress - 0.3f) / 0.7f);
		s_diamond_scale = EaseOutBack(diamond_progress);
	}

	if (s_is_releasing) {
		s_lock_on_progress -= dt * 6.0f;
		if (s_lock_on_progress <= 0.0f) {
			ResetIdleState();
			return;
		}

		float progress = s_lock_on_progress;

		s_outer_ring_scale = 1.0f + (1.0f - progress) * 2.0f;
		s_inner_ring_scale = progress;
		s_corner_offset = 0.5f + (1.0f - progress) * 1.5f;
		s_corner_scale = progress;
		s_diamond_scale = progress;
		return;
	}

	if (s_is_active && !s_is_locking_on && !s_is_releasing) {
		s_outer_ring_rotation += dt * 30.0f;
		if (s_outer_ring_rotation >= 360.0f) s_outer_ring_rotation -= 360.0f;

		s_inner_ring_rotation -= dt * 60.0f;
		if (s_inner_ring_rotation <= -360.0f) s_inner_ring_rotation += 360.0f;

		s_diamond_rotation += dt * 90.0f;
		if (s_diamond_rotation >= 360.0f) s_diamond_rotation -= 360.0f;

		s_pulse_timer += dt * 3.0f;
		s_pulse_alpha = (std::sin(s_pulse_timer) + 1.0f) * 0.3f + 0.2f;
		s_pulse_scale = 1.0f + std::sin(s_pulse_timer) * 0.1f;

		float breath = std::sin(s_animation_timer * 2.0f) * 0.05f;
		s_corner_offset = 0.5f + breath;
	}
}

static XMMATRIX CreateBillboardMatrix(const XMMATRIX& view, const XMFLOAT3& position) {
	XMMATRIX inv_view = XMMatrixInverse(nullptr, view);

	XMVECTOR cam_right = XMVector3Normalize(inv_view.r[0]);
	XMVECTOR cam_up = XMVector3Normalize(inv_view.r[1]);
	XMVECTOR cam_forward = XMVector3Normalize(inv_view.r[2]);

	XMMATRIX billboard = XMMatrixIdentity();
	billboard.r[0] = cam_right;
	billboard.r[1] = cam_up;
	billboard.r[2] = cam_forward;
	billboard.r[3] = XMVectorSet(position.x, position.y, position.z, 1.0f);

	return billboard;
}

void LockOnUI::Draw() {
	if (!s_is_active && !s_is_releasing) return;
	if (!s_target && !s_is_releasing) return;

	DirectX::XMFLOAT3 target_pos;
	if (s_target) {
		target_pos = s_target->GetPosition();
		target_pos.y += 1.0f;
		s_last_target_pos = target_pos;
	}
	else {
		target_pos = s_last_target_pos;
	}

	ID3D11BlendState* prevBlendState = nullptr;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	s_ptr_context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

	ID3D11DepthStencilState* prevDepthState = nullptr;
	UINT prevStencilRef;
	s_ptr_context->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);

	ID3D11RasterizerState* prevRasterState = nullptr;
	s_ptr_context->RSGetState(&prevRasterState);

	const float viewport_w = ScriptApi_GetRenderViewportWidth();
	const float viewport_h = ScriptApi_GetRenderViewportHeight();

	XMFLOAT4X4 view_store;
	XMFLOAT4X4 proj_store;
	ScriptApi_GetCameraViewMatrix(&view_store);
	ScriptApi_GetCameraProjMatrix(viewport_w, viewport_h, &proj_store);

	XMMATRIX view = XMLoadFloat4x4(&view_store);
	XMMATRIX proj = XMLoadFloat4x4(&proj_store);

	float base_size = 2.0f;

	s_ptr_context->VSSetShader(s_ptr_vs, nullptr, 0);
	s_ptr_context->PSSetShader(s_ptr_ps, nullptr, 0);
	s_ptr_context->IASetInputLayout(s_ptr_il);
	s_ptr_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	s_ptr_context->IASetVertexBuffers(0, 1, &s_ptr_vertex_buffer, &stride, &offset);
	s_ptr_context->IASetIndexBuffer(s_ptr_index_buffer, DXGI_FORMAT_R32_UINT, 0);

	s_ptr_context->OMSetBlendState(s_ptr_blend_state, nullptr, 0xFFFFFFFF);
	s_ptr_context->OMSetDepthStencilState(s_ptr_depth_state, 0);
	s_ptr_context->RSSetState(s_ptr_raster_state);

	s_ptr_context->VSSetConstantBuffers(0, 1, &s_ptr_cb_transform);
	s_ptr_context->PSSetConstantBuffers(0, 1, &s_ptr_cb_color);

	DrawOuterRing(view, proj, target_pos, base_size);
	DrawInnerRing(view, proj, target_pos, base_size);
	DrawCornerMarkers(view, proj, target_pos, base_size);
	DrawDiamond(view, proj, target_pos, base_size);
	DrawCrosshair(view, proj, target_pos, base_size);

	s_ptr_context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
	s_ptr_context->OMSetDepthStencilState(prevDepthState, prevStencilRef);
	s_ptr_context->RSSetState(prevRasterState);

	if (prevBlendState) prevBlendState->Release();
	if (prevDepthState) prevDepthState->Release();
	if (prevRasterState) prevRasterState->Release();
}

void LockOnUI::DrawQuad(const XMMATRIX& world_view_proj, const XMFLOAT4& color) {
	CB_Transform cb_transform;
	XMStoreFloat4x4(&cb_transform.world_view_proj, XMMatrixTranspose(world_view_proj));
	s_ptr_context->UpdateSubresource(s_ptr_cb_transform, 0, nullptr, &cb_transform, 0, 0);

	CB_Color cb_color;
	cb_color.color = color;
	cb_color.time = s_animation_timer;
	s_ptr_context->UpdateSubresource(s_ptr_cb_color, 0, nullptr, &cb_color, 0, 0);

	s_ptr_context->DrawIndexed(6, 0, 0);
}

void LockOnUI::DrawOuterRing(const XMMATRIX& view, const XMMATRIX& proj,
	const XMFLOAT3& position, float base_size) {
	float size = base_size * 1.2f * s_outer_ring_scale;
	float rot = XMConvertToRadians(s_outer_ring_rotation);

	for (int i = 0; i < 8; i++) {
		float angle = rot + i * XM_PI / 4.0f;
		float segment_size = size * 0.15f;
		float offset_dist = size * 0.5f;

		XMMATRIX billboard = CreateBillboardMatrix(view, position);
		XMVECTOR cam_right = XMVector3Normalize(billboard.r[0]);
		XMVECTOR cam_up = XMVector3Normalize(billboard.r[1]);

		float local_x = std::cos(angle) * offset_dist;
		float local_y = std::sin(angle) * offset_dist;

		XMVECTOR offset_vec = XMVectorAdd(
			XMVectorScale(cam_right, local_x),
			XMVectorScale(cam_up, local_y)
		);

		XMFLOAT3 final_pos;
		final_pos.x = position.x + XMVectorGetX(offset_vec);
		final_pos.y = position.y + XMVectorGetY(offset_vec);
		final_pos.z = position.z + XMVectorGetZ(offset_vec);

		XMMATRIX scale = XMMatrixScaling(segment_size, segment_size * 0.3f, 1.0f);
		XMMATRIX local_rot = XMMatrixRotationZ(angle + XM_PIDIV2);

		XMMATRIX final_billboard = CreateBillboardMatrix(view, final_pos);

		XMMATRIX final_world = scale * local_rot * final_billboard;
		XMMATRIX wvp = final_world * view * proj;

		XMFLOAT4 color = PrimaryColor();
		color.w = 0.8f * (std::min)(s_lock_on_progress, 1.0f);
		DrawQuad(wvp, color);
	}
}

void LockOnUI::DrawInnerRing(const XMMATRIX& view, const XMMATRIX& proj,
	const XMFLOAT3& position, float base_size) {
	float size = base_size * 0.6f * s_inner_ring_scale;
	float rot = XMConvertToRadians(s_inner_ring_rotation);

	for (int i = 0; i < 4; i++) {
		float angle = rot + i * XM_PIDIV2;
		float segment_size = size * 0.25f;
		float offset_dist = size * 0.4f;

		XMMATRIX billboard = CreateBillboardMatrix(view, position);
		XMVECTOR cam_right = XMVector3Normalize(billboard.r[0]);
		XMVECTOR cam_up = XMVector3Normalize(billboard.r[1]);

		float local_x = std::cos(angle) * offset_dist;
		float local_y = std::sin(angle) * offset_dist;

		XMVECTOR offset_vec = XMVectorAdd(
			XMVectorScale(cam_right, local_x),
			XMVectorScale(cam_up, local_y)
		);

		XMFLOAT3 final_pos;
		final_pos.x = position.x + XMVectorGetX(offset_vec);
		final_pos.y = position.y + XMVectorGetY(offset_vec);
		final_pos.z = position.z + XMVectorGetZ(offset_vec);

		XMMATRIX scale = XMMatrixScaling(segment_size, segment_size * 0.4f, 1.0f);
		XMMATRIX local_rot = XMMatrixRotationZ(angle + XM_PIDIV2);

		XMMATRIX final_billboard = CreateBillboardMatrix(view, final_pos);

		XMMATRIX final_world = scale * local_rot * final_billboard;
		XMMATRIX wvp = final_world * view * proj;

		XMFLOAT4 color = GlowColor();
		color.w = 0.9f * s_inner_ring_scale;
		DrawQuad(wvp, color);
	}
}

void LockOnUI::DrawCornerMarkers(const XMMATRIX& view, const XMMATRIX& proj,
	const XMFLOAT3& position, float base_size) {
	float offset = base_size * s_corner_offset;
	float marker_size = base_size * 0.2f * s_corner_scale;

	XMMATRIX billboard = CreateBillboardMatrix(view, position);
	XMVECTOR cam_right = XMVector3Normalize(billboard.r[0]);
	XMVECTOR cam_up = XMVector3Normalize(billboard.r[1]);

	XMFLOAT2 corners[] = {
		{-1.0f, +1.0f},
		{+1.0f, +1.0f},
		{-1.0f, -1.0f},
		{+1.0f, -1.0f}
	};

	for (int i = 0; i < 4; i++) {
		XMVECTOR offset_vec = XMVectorAdd(
			XMVectorScale(cam_right, corners[i].x * offset),
			XMVectorScale(cam_up, corners[i].y * offset)
		);

		XMFLOAT3 corner_pos;
		corner_pos.x = position.x + XMVectorGetX(offset_vec);
		corner_pos.y = position.y + XMVectorGetY(offset_vec);
		corner_pos.z = position.z + XMVectorGetZ(offset_vec);

		for (int j = 0; j < 2; j++) {
			float rot_angle = (i * XM_PIDIV2) + (j * XM_PIDIV2);

			XMMATRIX scale = XMMatrixScaling(marker_size, marker_size * 0.2f, 1.0f);
			XMMATRIX local_rot = XMMatrixRotationZ(rot_angle);

			XMMATRIX final_billboard = CreateBillboardMatrix(view, corner_pos);

			XMMATRIX final_world = scale * local_rot * final_billboard;
			XMMATRIX wvp = final_world * view * proj;

			XMFLOAT4 color = PrimaryColor();
			color.w = s_corner_scale;
			DrawQuad(wvp, color);
		}
	}
}

void LockOnUI::DrawDiamond(const XMMATRIX& view, const XMMATRIX& proj,
	const XMFLOAT3& position, float base_size) {
	if (s_diamond_scale < 0.01f) return;

	float size = base_size * 0.15f * s_diamond_scale;
	float rot = XMConvertToRadians(s_diamond_rotation + 45.0f);

	XMMATRIX scale = XMMatrixScaling(size, size, 1.0f);
	XMMATRIX local_rot = XMMatrixRotationZ(rot);

	XMMATRIX final_billboard = CreateBillboardMatrix(view, position);

	XMMATRIX final_world = scale * local_rot * final_billboard;
	XMMATRIX wvp = final_world * view * proj;

	XMFLOAT4 color = PrimaryColor();
	color.w = s_diamond_scale;
	DrawQuad(wvp, color);
}

void LockOnUI::DrawCrosshair(const XMMATRIX& view, const XMMATRIX& proj,
	const XMFLOAT3& position, float base_size) {
	if (s_lock_on_progress < 0.5f) return;

	float progress = (s_lock_on_progress - 0.5f) * 2.0f;
	float size = base_size * 0.08f * EaseOutBack(progress);

	XMMATRIX scale = XMMatrixScaling(size, size, 1.0f);
	XMMATRIX final_billboard = CreateBillboardMatrix(view, position);

	XMMATRIX final_world = scale * final_billboard;
	XMMATRIX wvp = final_world * view * proj;

	XMFLOAT4 color = PrimaryColor();
	color.w = progress;
	DrawQuad(wvp, color);

	XMVECTOR cam_right = XMVector3Normalize(final_billboard.r[0]);
	XMVECTOR cam_up = XMVector3Normalize(final_billboard.r[1]);

	float line_size = base_size * 0.1f * progress;
	float line_offset = base_size * 0.15f;

	XMFLOAT2 directions[] = {
		{0.0f, +1.0f}, {0.0f, -1.0f}, {+1.0f, 0.0f}, {-1.0f, 0.0f}
	};

	for (int i = 0; i < 4; i++) {
		XMVECTOR offset_vec = XMVectorAdd(
			XMVectorScale(cam_right, directions[i].x * line_offset),
			XMVectorScale(cam_up, directions[i].y * line_offset)
		);

		XMFLOAT3 line_pos;
		line_pos.x = position.x + XMVectorGetX(offset_vec);
		line_pos.y = position.y + XMVectorGetY(offset_vec);
		line_pos.z = position.z + XMVectorGetZ(offset_vec);

		float rot = (i < 2) ? 0.0f : XM_PIDIV2;

		XMMATRIX line_scale = XMMatrixScaling(line_size * 0.3f, line_size, 1.0f);
		XMMATRIX line_rot = XMMatrixRotationZ(rot);

		XMMATRIX line_billboard = CreateBillboardMatrix(view, line_pos);

		XMMATRIX line_final = line_scale * line_rot * line_billboard;
		XMMATRIX line_wvp = line_final * view * proj;

		color.w = progress * 0.8f;
		DrawQuad(line_wvp, color);
	}
}

float LockOnUI::EaseOutBack(float t) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

float LockOnUI::EaseOutElastic(float t) {
	if (t == 0.0f || t == 1.0f) return t;
	const float c4 = (2.0f * XM_PI) / 3.0f;
	return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}
