/**===========================================================================

 File   :  [billboard.cpp]

 Author : Terry Yeung
 Date   : 2025/12/4
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "billboard.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/core/content_path.h"
#include "engine/rendering/culling/frustum_cull_math.h"
#include "engine/rendering/culling/fx_cull.h"
#include "engine/rendering/culling/fx_cull_math.h"
#include "engine/scene/render_settings.h"
#include <fstream>
#include <vector>
#include <algorithm>

 // Static member initialization
ID3D11VertexShader* Billboard::s_ptr_vs = nullptr;
ID3D11PixelShader* Billboard::s_ptr_ps = nullptr;
ID3D11InputLayout* Billboard::s_ptr_il = nullptr;
ID3D11Buffer* Billboard::s_ptr_cb_vp = nullptr;
ID3D11Buffer* Billboard::s_ptr_cb_billboard = nullptr;
ID3D11SamplerState* Billboard::s_ptr_sampler = nullptr;
ID3D11RasterizerState* Billboard::s_ptr_raster_no_cull = nullptr;
ID3D11DepthStencilState* Billboard::s_ptr_depth_read_only = nullptr;
ID3D11Buffer* Billboard::s_ptr_vertex_buffer = nullptr;

ID3D11Device* Billboard::s_ptr_device = nullptr;
ID3D11DeviceContext* Billboard::s_ptr_context = nullptr;

XMMATRIX Billboard::s_view_matrix = XMMatrixIdentity();
XMMATRIX Billboard::s_proj_matrix = XMMatrixIdentity();

std::vector<BillboardData> Billboard::s_draw_list;

bool Billboard::Initialize() {
	s_ptr_device = GetDevice();
	s_ptr_context = GetDeviceContext();

	if (!s_ptr_device || !s_ptr_context) return false;

	HRESULT hr;

	// Load pre-compiled vertex shader from .cso file
	std::ifstream vs_file(content_path::ReadableFileCurrent("rom/shader/VertexShaderUnlit.cso"), std::ios::binary);
	if (!vs_file) {
		MessageBoxA(nullptr, "Cannot open rom/shader/VertexShaderUnlit.cso\nPlease build the project to generate .cso files.", "VS Load Error", MB_OK | MB_ICONERROR);
		return false;
	}
	vs_file.seekg(0, std::ios::end);
	std::vector<char> vs_data(static_cast<size_t>(vs_file.tellg()));
	vs_file.seekg(0, std::ios::beg);
	vs_file.read(vs_data.data(), static_cast<std::streamsize>(vs_data.size()));

	hr = s_ptr_device->CreateVertexShader(vs_data.data(), vs_data.size(), nullptr, &s_ptr_vs);
	if (FAILED(hr)) return false;

	// Input layout
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	hr = s_ptr_device->CreateInputLayout(layout, 2, vs_data.data(), vs_data.size(), &s_ptr_il);
	if (FAILED(hr)) return false;

	// Load pre-compiled pixel shader from .cso file
	std::ifstream ps_file(content_path::ReadableFileCurrent("rom/shader/PixelShaderUnlit.cso"), std::ios::binary);
	if (!ps_file) {
		MessageBoxA(nullptr, "Cannot open rom/shader/PixelShaderUnlit.cso\nPlease build the project to generate .cso files.", "PS Load Error", MB_OK | MB_ICONERROR);
		return false;
	}
	ps_file.seekg(0, std::ios::end);
	std::vector<char> ps_data(static_cast<size_t>(ps_file.tellg()));
	ps_file.seekg(0, std::ios::beg);
	ps_file.read(ps_data.data(), static_cast<std::streamsize>(ps_data.size()));

	hr = s_ptr_device->CreatePixelShader(ps_data.data(), ps_data.size(), nullptr, &s_ptr_ps);
	if (FAILED(hr)) return false;

	// Constant buffer creation
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	bd.ByteWidth = sizeof(CB_ViewProj);
	hr = s_ptr_device->CreateBuffer(&bd, nullptr, &s_ptr_cb_vp);
	if (FAILED(hr)) return false;

	bd.ByteWidth = sizeof(CB_Billboard);
	hr = s_ptr_device->CreateBuffer(&bd, nullptr, &s_ptr_cb_billboard);
	if (FAILED(hr)) return false;

	// Sampler
	D3D11_SAMPLER_DESC sd = {};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	hr = s_ptr_device->CreateSamplerState(&sd, &s_ptr_sampler);
	if (FAILED(hr)) return false;

	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthClipEnable = TRUE;
	hr = s_ptr_device->CreateRasterizerState(&rasterDesc, &s_ptr_raster_no_cull);
	if (FAILED(hr)) return false;

	D3D11_DEPTH_STENCIL_DESC depthReadDesc = {};
	depthReadDesc.DepthEnable = TRUE;
	depthReadDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthReadDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = s_ptr_device->CreateDepthStencilState(&depthReadDesc, &s_ptr_depth_read_only);
	if (FAILED(hr)) return false;

	// Vertex buffer (quad: 4 vertices)
	VertexBillboard vertices[] = {
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

	return true;
}

void Billboard::Finalize() {
	s_draw_list.clear();

	if (s_ptr_vertex_buffer) { s_ptr_vertex_buffer->Release(); s_ptr_vertex_buffer = nullptr; }
	if (s_ptr_depth_read_only) { s_ptr_depth_read_only->Release(); s_ptr_depth_read_only = nullptr; }
	if (s_ptr_raster_no_cull) { s_ptr_raster_no_cull->Release(); s_ptr_raster_no_cull = nullptr; }
	if (s_ptr_sampler) { s_ptr_sampler->Release(); s_ptr_sampler = nullptr; }
	if (s_ptr_cb_billboard) { s_ptr_cb_billboard->Release(); s_ptr_cb_billboard = nullptr; }
	if (s_ptr_cb_vp) { s_ptr_cb_vp->Release(); s_ptr_cb_vp = nullptr; }
	if (s_ptr_il) { s_ptr_il->Release(); s_ptr_il = nullptr; }
	if (s_ptr_ps) { s_ptr_ps->Release(); s_ptr_ps = nullptr; }
	if (s_ptr_vs) { s_ptr_vs->Release(); s_ptr_vs = nullptr; }

	s_ptr_device = nullptr;
	s_ptr_context = nullptr;
}

void Billboard::Update(double elapsed_time) {
	(void) elapsed_time;  // Reserved for future expansion (animations, etc.)

	// Sort by distance from camera (draw far to near)
	XMMATRIX inv_view = XMMatrixInverse(nullptr, s_view_matrix);
	XMFLOAT3 camera_position;
	XMStoreFloat3(&camera_position, inv_view.r[3]);

	std::sort(s_draw_list.begin(), s_draw_list.end(),
		[&camera_position] (const BillboardData& a, const BillboardData& b) {
		float dist_a = (a.position.x - camera_position.x) * (a.position.x - camera_position.x)
			+ (a.position.y - camera_position.y) * (a.position.y - camera_position.y)
			+ (a.position.z - camera_position.z) * (a.position.z - camera_position.z);
		float dist_b = (b.position.x - camera_position.x) * (b.position.x - camera_position.x)
			+ (b.position.y - camera_position.y) * (b.position.y - camera_position.y)
			+ (b.position.z - camera_position.z) * (b.position.z - camera_position.z);
		return dist_a > dist_b;
	});
}

void Billboard::Draw() {
	if (s_draw_list.empty()) return;

	// Frustum-cull the list (order preserved, so the far-to-near sort stays valid).
	if (RenderSettings_GetConst().billboardCullingEnabled) {
		frustum_cull::FrustumPlanes planes;
		frustum_cull::ExtractFrustumPlanesD3D(s_view_matrix * s_proj_matrix, planes);
		const int before = static_cast<int>(s_draw_list.size());
		s_draw_list.erase(
			std::remove_if(s_draw_list.begin(), s_draw_list.end(),
				[&planes] (const BillboardData& d) {
					return !fx_cull::SphereIntersectsFrustum(
						d.position.x, d.position.y, d.position.z,
						fx_cull::BillboardRadius(d.size.x, d.size.y), planes);
				}),
			s_draw_list.end());
		FxCull_RecordBillboards(before, before - static_cast<int>(s_draw_list.size()));
		if (s_draw_list.empty()) return;
	}

	ID3D11VertexShader* prevVs = nullptr;
	ID3D11PixelShader* prevPs = nullptr;
	ID3D11InputLayout* prevIl = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY prevTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	ID3D11Buffer* prevVb = nullptr;
	UINT prevStride = 0;
	UINT prevOffset = 0;
	ID3D11RasterizerState* prevRaster = nullptr;
	ID3D11BlendState* prevBlend = nullptr;
	FLOAT prevBlendFactor[4] = {};
	UINT prevSampleMask = 0;
	ID3D11DepthStencilState* prevDepth = nullptr;
	UINT prevStencilRef = 0;
	ID3D11Buffer* prevVsCb0 = nullptr;
	ID3D11Buffer* prevVsCb1 = nullptr;
	ID3D11SamplerState* prevPsSampler0 = nullptr;
	ID3D11ShaderResourceView* prevPsSrv0 = nullptr;

	s_ptr_context->VSGetShader(&prevVs, nullptr, nullptr);
	s_ptr_context->PSGetShader(&prevPs, nullptr, nullptr);
	s_ptr_context->IAGetInputLayout(&prevIl);
	s_ptr_context->IAGetPrimitiveTopology(&prevTopology);
	s_ptr_context->IAGetVertexBuffers(0, 1, &prevVb, &prevStride, &prevOffset);
	s_ptr_context->RSGetState(&prevRaster);
	s_ptr_context->OMGetBlendState(&prevBlend, prevBlendFactor, &prevSampleMask);
	s_ptr_context->OMGetDepthStencilState(&prevDepth, &prevStencilRef);
	s_ptr_context->VSGetConstantBuffers(0, 1, &prevVsCb0);
	s_ptr_context->VSGetConstantBuffers(1, 1, &prevVsCb1);
	s_ptr_context->PSGetSamplers(0, 1, &prevPsSampler0);
	s_ptr_context->PSGetShaderResources(0, 1, &prevPsSrv0);

	s_ptr_context->VSSetShader(s_ptr_vs, nullptr, 0);
	s_ptr_context->PSSetShader(s_ptr_ps, nullptr, 0);
	s_ptr_context->IASetInputLayout(s_ptr_il);
	s_ptr_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	UINT stride = sizeof(VertexBillboard);
	UINT offset = 0;
	s_ptr_context->IASetVertexBuffers(0, 1, &s_ptr_vertex_buffer, &stride, &offset);

	CB_ViewProj cb_vp;
	XMStoreFloat4x4(&cb_vp.view, XMMatrixTranspose(s_view_matrix));
	XMStoreFloat4x4(&cb_vp.proj, XMMatrixTranspose(s_proj_matrix));
	s_ptr_context->UpdateSubresource(s_ptr_cb_vp, 0, nullptr, &cb_vp, 0, 0);

	s_ptr_context->VSSetConstantBuffers(0, 1, &s_ptr_cb_vp);
	s_ptr_context->VSSetConstantBuffers(1, 1, &s_ptr_cb_billboard);
	s_ptr_context->PSSetSamplers(0, 1, &s_ptr_sampler);

	if (s_ptr_raster_no_cull)
		s_ptr_context->RSSetState(s_ptr_raster_no_cull);

	ID3D11ShaderResourceView* whiteSrv = nullptr;
	{
		const int whiteId = TextureGetWhiteTexture();
		if (whiteId >= 0)
			whiteSrv = TextureGetSRV(whiteId);
	}

	for (const auto& data : s_draw_list) {
		CB_Billboard cb;
		cb.position = data.position;
		cb.padding1 = 0.0f;
		cb.size = data.size;
		cb.rotation = data.rotation;
		cb.groundAligned = data.groundAligned ? 1.0f : 0.0f;
		cb.color = data.color;
		cb.uvOffset = data.uvOffset;
		cb.uvScale = data.uvScale;
		s_ptr_context->UpdateSubresource(s_ptr_cb_billboard, 0, nullptr, &cb, 0, 0);

		if (data.groundAligned && s_ptr_depth_read_only)
			s_ptr_context->OMSetDepthStencilState(s_ptr_depth_read_only, 0);
		else
			SetDepthEnable(true);

		ID3D11ShaderResourceView* srv = data.ptr_texture ? data.ptr_texture : whiteSrv;
		if (srv)
			s_ptr_context->PSSetShaderResources(0, 1, &srv);

		s_ptr_context->Draw(4, 0);
	}

	s_ptr_context->VSSetShader(prevVs, nullptr, 0);
	s_ptr_context->PSSetShader(prevPs, nullptr, 0);
	s_ptr_context->IASetInputLayout(prevIl);
	s_ptr_context->IASetPrimitiveTopology(prevTopology);
	s_ptr_context->IASetVertexBuffers(0, 1, &prevVb, &prevStride, &prevOffset);
	s_ptr_context->RSSetState(prevRaster);
	s_ptr_context->OMSetBlendState(prevBlend, prevBlendFactor, prevSampleMask);
	s_ptr_context->OMSetDepthStencilState(prevDepth, prevStencilRef);
	s_ptr_context->VSSetConstantBuffers(0, 1, &prevVsCb0);
	s_ptr_context->VSSetConstantBuffers(1, 1, &prevVsCb1);
	s_ptr_context->PSSetSamplers(0, 1, &prevPsSampler0);
	s_ptr_context->PSSetShaderResources(0, 1, &prevPsSrv0);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	s_ptr_context->PSSetShaderResources(0, 1, &nullSrv);

	if (prevVs) prevVs->Release();
	if (prevPs) prevPs->Release();
	if (prevIl) prevIl->Release();
	if (prevVb) prevVb->Release();
	if (prevRaster) prevRaster->Release();
	if (prevBlend) prevBlend->Release();
	if (prevDepth) prevDepth->Release();
	if (prevVsCb0) prevVsCb0->Release();
	if (prevVsCb1) prevVsCb1->Release();
	if (prevPsSampler0) prevPsSampler0->Release();
	if (prevPsSrv0) prevPsSrv0->Release();

	s_draw_list.clear();
}

void Billboard::Add(const XMFLOAT3& position, const XMFLOAT2& size, const XMFLOAT4& color, ID3D11ShaderResourceView* ptr_texture, float rotation, bool groundAligned) {
	Add(position, size, color, ptr_texture, rotation, groundAligned, { 0.0f, 0.0f }, { 1.0f, 1.0f });
}

void Billboard::Add(const XMFLOAT3& position, const XMFLOAT2& size, const XMFLOAT4& color, ID3D11ShaderResourceView* ptr_texture, float rotation, bool groundAligned, const XMFLOAT2& uvOffset, const XMFLOAT2& uvScale) {
	BillboardData data;
	data.position = position;
	data.size = size;
	data.color = color;
	data.ptr_texture = ptr_texture;
	data.rotation = rotation;
	data.groundAligned = groundAligned;
	data.uvOffset = uvOffset;
	data.uvScale = uvScale;
	s_draw_list.push_back(data);
}

void Billboard::Add(const BillboardData& data) {
	s_draw_list.push_back(data);
}
