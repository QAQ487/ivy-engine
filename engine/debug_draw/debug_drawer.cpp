/**===========================================================================

 File   :  [debug_drawer.cpp]

 Author : Hayato Otsuki
 Date   : 2025/10/24
 -----------------------------------------------------------------------------

 ===========================================================================**/

#include "debug_drawer.h"
#include <cmath>

VERTEX_3D DebugDrawer::m_boxVerts[8] = {
	{{-0.5f, -0.5f, -0.5f}, {0,1,0}, {0,1,0,1}, {0,0}},
	{{ 0.5f, -0.5f, -0.5f}, {0,1,0}, {0,1,0,1}, {1,0}},
	{{ 0.5f,  0.5f, -0.5f}, {0,1,0}, {0,1,0,1}, {1,1}},
	{{-0.5f,  0.5f, -0.5f}, {0,1,0}, {0,1,0,1}, {0,1}},
	{{-0.5f, -0.5f,  0.5f}, {0,1,0}, {0,1,0,1}, {0,0}},
	{{ 0.5f, -0.5f,  0.5f}, {0,1,0}, {0,1,0,1}, {1,0}},
	{{ 0.5f,  0.5f,  0.5f}, {0,1,0}, {0,1,0,1}, {1,1}},
	{{-0.5f,  0.5f,  0.5f}, {0,1,0}, {0,1,0,1}, {0,1}},
};

UINT DebugDrawer::m_boxIndices[24] = {
	0,1, 1,2, 2,3, 3,0, // Back face
	4,5, 5,6, 6,7, 7,4, // Front face
	0,4, 1,5, 2,6, 3,7, // Connecting edges
};

VERTEX_3D DebugDrawer::m_sphereVerts[60];
UINT DebugDrawer::m_sphereIndices[120];

VERTEX_3D DebugDrawer::m_capsuleVerts[8];
UINT DebugDrawer::m_capsuleIndices[8];

ID3D11Buffer* DebugDrawer::m_boxVB = nullptr;
ID3D11Buffer* DebugDrawer::m_boxIB = nullptr;
ID3D11Buffer* DebugDrawer::m_sphereVB = nullptr;
ID3D11Buffer* DebugDrawer::m_sphereIB = nullptr;
ID3D11Buffer* DebugDrawer::m_capsuleVB = nullptr;
ID3D11Buffer* DebugDrawer::m_capsuleIB = nullptr;
ID3D11ShaderResourceView* DebugDrawer::m_debugTexture = nullptr;
bool DebugDrawer::m_initialized = false;

ID3D11Buffer* DebugDrawer::m_linesVB         = nullptr;
UINT          DebugDrawer::m_linesVBCapacity  = 0;

ID3D11DepthStencilState* DebugDrawer::m_depthNoTest = nullptr;
std::vector<DebugDrawer::SphereDrawRequest> DebugDrawer::s_sphereQueue;
std::vector<DebugDrawer::LineDrawRequest> DebugDrawer::s_lineQueue;

DebugDrawer::~DebugDrawer() {
	Release();
}

bool DebugDrawer::Initialize() {
	if (m_initialized) return true;

	// Initialize box buffers
	D3D11_BUFFER_DESC vbDesc{};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(m_boxVerts);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData{};
	vbData.pSysMem = m_boxVerts;
	HRESULT hr = GetDevice()->CreateBuffer(&vbDesc, &vbData, &m_boxVB);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create box vertex buffer\n");
		return false;
	}

	D3D11_BUFFER_DESC ibDesc{};
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(m_boxIndices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData{};
	ibData.pSysMem = m_boxIndices;
	hr = GetDevice()->CreateBuffer(&ibDesc, &ibData, &m_boxIB);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create box index buffer\n");
		m_boxVB->Release();
		m_boxVB = nullptr;
		return false;
	}

	// Initialize sphere buffers (3 circles: XY, XZ, YZ, 20 segments each)
	const int segments = 20;
	const float pi = 3.1415926535f;
	int vertIndex = 0;
	int indexIndex = 0;

	// XY plane circle
	for (int i = 0; i < segments; ++i) {
		float angle = 2.0f * pi * i / segments;
		m_sphereVerts[vertIndex] = {
			{ std::cos(angle), std::sin(angle), 0.0f }, // Position
			{ std::cos(angle), std::sin(angle), 0 },    // Normal
			{ 0, 1, 0, 1 },                            // Green
			{ 0, 0 }                                   // TexCoord
		};
		m_sphereIndices[indexIndex++] = vertIndex;
		m_sphereIndices[indexIndex++] = (i + 1) % segments; // Corrected modulo
		vertIndex++;
	}

	// XZ plane circle
	for (int i = 0; i < segments; ++i) {
		float angle = 2.0f * pi * i / segments;
		m_sphereVerts[vertIndex] = {
			{ std::cos(angle), 0.0f, std::sin(angle) },
			{ std::cos(angle), 0, std::sin(angle) },
			{ 0, 1, 0, 1 },
			{ 0, 0 }
		};
		m_sphereIndices[indexIndex++] = vertIndex;
		m_sphereIndices[indexIndex++] = segments + (i + 1) % segments; // Corrected modulo
		vertIndex++;
	}

	// YZ plane circle
	for (int i = 0; i < segments; ++i) {
		float angle = 2.0f * pi * i / segments;
		m_sphereVerts[vertIndex] = {
			{ 0.0f, std::cos(angle), std::sin(angle) },
			{ 0, std::cos(angle), std::sin(angle) },
			{ 0, 1, 0, 1 },
			{ 0, 0 }
		};
		m_sphereIndices[indexIndex++] = vertIndex;
		m_sphereIndices[indexIndex++] = 2 * segments + (i + 1) % segments; // Corrected modulo
		vertIndex++;
	}

	// Log sphere vertices for debugging
	// char log[256];
	// for (int i = 0; i < vertIndex; ++i) {
	//     sprintf_s(log, "Sphere vert[%d]: Pos=(%.2f, %.2f, %.2f)\n",
	//         i, m_sphereVerts[i].Position.x, m_sphereVerts[i].Position.y, m_sphereVerts[i].Position.z);
	//     OutputDebugStringA(log);
	// }

	vbDesc.ByteWidth = sizeof(m_sphereVerts);
	vbData.pSysMem = m_sphereVerts;
	hr = GetDevice()->CreateBuffer(&vbDesc, &vbData, &m_sphereVB);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create sphere vertex buffer\n");
		Release();
		return false;
	}

	ibDesc.ByteWidth = sizeof(m_sphereIndices);
	ibData.pSysMem = m_sphereIndices;
	hr = GetDevice()->CreateBuffer(&ibDesc, &ibData, &m_sphereIB);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create sphere index buffer\n");
		Release();
		return false;
	}

	// Initialize capsule buffers (4 longitudinal lines)
	vertIndex = 0;
	indexIndex = 0;
	const float angles[4] = { 0.0f, pi / 2.0f, pi, 3.0f * pi / 2.0f }; // 0??, 90??, 180??, 270??
	for (int i = 0; i < 4; ++i) {
		float x = std::cos(angles[i]);
		float z = std::sin(angles[i]);
		// Bottom vertex (y = -0.5)
		m_capsuleVerts[vertIndex] = {
			{ x, -0.5f, z }, // Unit radius, y = -0.5
			{ x, 0, z },     // Outward normal
			{ 0, 1, 0, 1 },  // Green
			{ 0, 0 }
		};
		m_capsuleIndices[indexIndex++] = vertIndex++;
		// Top vertex (y = 0.5)
		m_capsuleVerts[vertIndex] = {
			{ x, 0.5f, z },  // Unit radius, y = 0.5
			{ x, 0, z },
			{ 0, 1, 0, 1 },
			{ 0, 0 }
		};
		m_capsuleIndices[indexIndex++] = vertIndex++;
	}

	// Log capsule vertices
	// for (int i = 0; i < vertIndex; ++i) {
	//     sprintf_s(log, "Capsule vert[%d]: Pos=(%.2f, %.2f, %.2f)\n",
	//         i, m_capsuleVerts[i].Position.x, m_capsuleVerts[i].Position.y, m_capsuleVerts[i].Position.z);
	//     OutputDebugStringA(log);
	// }

	vbDesc.ByteWidth = sizeof(m_capsuleVerts);
	vbData.pSysMem = m_capsuleVerts;
	hr = GetDevice()->CreateBuffer(&vbDesc, &vbData, &m_capsuleVB);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create capsule vertex buffer\n");
		Release();
		return false;
	}

	ibDesc.ByteWidth = sizeof(m_capsuleIndices);
	ibData.pSysMem = m_capsuleIndices;
	hr = GetDevice()->CreateBuffer(&ibDesc, &ibData, &m_capsuleIB);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create capsule index buffer\n");
		Release();
		return false;
	}

	// Initialize debug texture
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	UINT pixel = 0xFFFFFFFF; // White
	D3D11_SUBRESOURCE_DATA texData{};
	texData.pSysMem = &pixel;
	texData.SysMemPitch = sizeof(UINT);
	ID3D11Texture2D* tex;
	hr = GetDevice()->CreateTexture2D(&texDesc, &texData, &tex);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create debug texture\n");
		Release();
		return false;
	}
	hr = GetDevice()->CreateShaderResourceView(tex, nullptr, &m_debugTexture);
	tex->Release();
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create debug texture SRV\n");
		Release();
		return false;
	}

	// Log initialization summary
	// sprintf_s(log, "DebugDrawer::Initialize: Sphere vertices=%d, indices=%d, Capsule vertices=%d, indices=%d\n",
	//     60, 120, 8, 8);
	// OutputDebugStringA(log);

	// Depth stencil state with depth testing disabled (for deferred debug overlays)
	D3D11_DEPTH_STENCIL_DESC noDepthDesc = {};
	noDepthDesc.DepthEnable    = FALSE;
	noDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	noDepthDesc.DepthFunc      = D3D11_COMPARISON_ALWAYS;
	noDepthDesc.StencilEnable  = FALSE;
	hr = GetDevice()->CreateDepthStencilState(&noDepthDesc, &m_depthNoTest);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create no-depth stencil state\n");
		Release();
		return false;
	}

	m_initialized = true;
	return true;
}

void DebugDrawer::Release() {
	if (m_boxVB) { m_boxVB->Release(); m_boxVB = nullptr; }
	if (m_boxIB) { m_boxIB->Release(); m_boxIB = nullptr; }
	if (m_sphereVB) { m_sphereVB->Release(); m_sphereVB = nullptr; }
	if (m_sphereIB) { m_sphereIB->Release(); m_sphereIB = nullptr; }
	if (m_capsuleVB) { m_capsuleVB->Release(); m_capsuleVB = nullptr; }
	if (m_capsuleIB) { m_capsuleIB->Release(); m_capsuleIB = nullptr; }
	if (m_debugTexture) { m_debugTexture->Release(); m_debugTexture = nullptr; }
	if (m_linesVB) { m_linesVB->Release(); m_linesVB = nullptr; m_linesVBCapacity = 0; }
	if (m_depthNoTest) { m_depthNoTest->Release(); m_depthNoTest = nullptr; }
	m_initialized = false;
}

void DebugDrawer::DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color) {
	if (!m_boxVB || !m_boxIB) {
		OutputDebugStringA("DrawBox: Vertex or index buffer null\n");
		return;
	}

	Shader3D_Begin();
	SetDepthEnable(true);

	Camera& cam = GetCamera();
	Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
	Shader_SetViewMatrix(cam.GetViewMatrix());

	using namespace DirectX;

	XMMATRIX Rotation = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(rotation.x),
		XMConvertToRadians(rotation.y),
		XMConvertToRadians(rotation.z)
	);

	// Rotate the local center offset by the rotation, then translate
	XMVECTOR vCenter = XMLoadFloat3(&center);
	XMVECTOR vRotCenter = XMVector3TransformCoord(vCenter, Rotation);
	XMVECTOR vPosition = XMLoadFloat3(&position);
	XMVECTOR vWorldPos = XMVectorAdd(vPosition, vRotCenter);

	XMFLOAT3 worldPosF;
	XMStoreFloat3(&worldPosF, vWorldPos);

	XMMATRIX Translation = XMMatrixTranslation(worldPosF.x, worldPosF.y, worldPosF.z);
	XMMATRIX Scaling = XMMatrixScaling(size.x, size.y, size.z);
	XMMATRIX World = Scaling * Rotation * Translation;
	Shader_SetWorldMatrix(World);

	// Use a temporary vertex buffer when a non-default colour is requested so
	// that the static shared VB (always green) is left unchanged.
	ID3D11Buffer* vbToUse = m_boxVB;
	ID3D11Buffer* tempVB  = nullptr;
	static const XMFLOAT4 kDefaultColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	if (color.x != kDefaultColor.x || color.y != kDefaultColor.y || color.z != kDefaultColor.z || color.w != kDefaultColor.w) {
		VERTEX_3D verts[8];
		memcpy(verts, m_boxVerts, sizeof(m_boxVerts));
		for (auto& v : verts) v.Diffuse = color;

		D3D11_BUFFER_DESC bd = {};
		bd.Usage           = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth       = sizeof(verts);
		bd.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = verts;
		if (SUCCEEDED(GetDevice()->CreateBuffer(&bd, &sd, &tempVB))) {
			vbToUse = tempVB;
		}
	}

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &vbToUse, &stride, &offset);
	GetDeviceContext()->IASetIndexBuffer(m_boxIB, DXGI_FORMAT_R32_UINT, 0);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	MATERIAL material{};
	material.Diffuse = DirectX::XMFLOAT4(0, 1, 0, 0); // Green
	Shader_SetMaterial(material);

	GetDeviceContext()->DrawIndexed(24, 0, 0);

	if (tempVB) tempVB->Release();

	// Log box draw call
	// char log[256];
	// sprintf_s(log, "DrawBox: Pos=(%.2f, %.2f, %.2f), Center=(%.2f, %.2f, %.2f), Size=(%.2f, %.2f, %.2f), Rot=(%.2f, %.2f, %.2f)\n",
	//     position.x, position.y, position.z, center.x, center.y, center.z, size.x, size.y, size.z,
	//     rotation.x, rotation.y, rotation.z);
	// OutputDebugStringA(log);
}

void DebugDrawer::DrawSphere(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color, bool enableDepth) {
	if (!m_sphereVB || !m_sphereIB) {
		OutputDebugStringA("DrawSphere: Vertex or index buffer null\n");
		return;
	}

	Shader3D_Begin();
	if (enableDepth) {
		SetDepthEnable(true);
	} else if (m_depthNoTest) {
		GetDeviceContext()->OMSetDepthStencilState(m_depthNoTest, 0);
	}

	Camera& cam = GetCamera();
	Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
	Shader_SetViewMatrix(cam.GetViewMatrix());

	using namespace DirectX;

	XMMATRIX Rotation = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(rotation.x),
		XMConvertToRadians(rotation.y),
		XMConvertToRadians(rotation.z)
	);

	// Rotate the local center offset by rotation and then translate by position
	XMVECTOR vCenter = XMLoadFloat3(&center);
	XMVECTOR vRotCenter = XMVector3TransformCoord(vCenter, Rotation);
	XMVECTOR vPosition = XMLoadFloat3(&position);
	XMVECTOR vWorldPos = XMVectorAdd(vPosition, vRotCenter);

	XMFLOAT3 worldPosF;
	XMStoreFloat3(&worldPosF, vWorldPos);
	XMMATRIX Translation = XMMatrixTranslation(worldPosF.x, worldPosF.y, worldPosF.z);

	XMMATRIX Scaling = XMMatrixScaling(radius, radius, radius);

	// World matrix = scale * rotation * translation
	XMMATRIX World = Scaling * Rotation * Translation;
	Shader_SetWorldMatrix(World);

	// Use a temporary vertex buffer when a non-default colour is requested.
	ID3D11Buffer* vbToUse = m_sphereVB;
	ID3D11Buffer* tempVB  = nullptr;
	static const XMFLOAT4 kDefaultColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	if (color.x != kDefaultColor.x || color.y != kDefaultColor.y || color.z != kDefaultColor.z || color.w != kDefaultColor.w) {
		VERTEX_3D verts[60];
		memcpy(verts, m_sphereVerts, sizeof(m_sphereVerts));
		for (auto& v : verts) v.Diffuse = color;

		D3D11_BUFFER_DESC bd = {};
		bd.Usage           = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth       = sizeof(verts);
		bd.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = verts;
		if (SUCCEEDED(GetDevice()->CreateBuffer(&bd, &sd, &tempVB))) {
			vbToUse = tempVB;
		}
	}

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &vbToUse, &stride, &offset);
	GetDeviceContext()->IASetIndexBuffer(m_sphereIB, DXGI_FORMAT_R32_UINT, 0);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(0, 1, 0, 0); // Green wireframe
	Shader_SetMaterial(material);

	GetDeviceContext()->DrawIndexed(120, 0, 0);

	if (tempVB) tempVB->Release();

	// char log[256];
	// sprintf_s(log,
	//     "DrawSphere: Pos=(%.2f, %.2f, %.2f), Center=(%.2f, %.2f, %.2f), Radius=%.2f, Rot=(%.2f, %.2f, %.2f)\n",
	//     position.x, position.y, position.z,
	//     center.x, center.y, center.z,
	//     radius, rotation.x, rotation.y, rotation.z
	// );
	// OutputDebugStringA(log);
}

void DebugDrawer::DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, float height, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color) {
	if (!m_sphereVB || !m_sphereIB || !m_capsuleVB || !m_capsuleIB) {
		OutputDebugStringA("DrawCapsule: Vertex or index buffer null\n");
		return;
	}

	Shader3D_Begin();
	SetDepthEnable(true);

	Camera& cam = GetCamera();
	Shader_SetProjectionMatrix(cam.GetProjMatrix(GetRenderViewportWidth(), GetRenderViewportHeight()));
	Shader_SetViewMatrix(cam.GetViewMatrix());

	using namespace DirectX;

	XMMATRIX Rotation = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(rotation.x),
		XMConvertToRadians(rotation.y),
		XMConvertToRadians(rotation.z)
	);

	// Rotate the local center offset before translating so the body lines follow rotation
	XMVECTOR vCenter = XMLoadFloat3(&center);
	XMVECTOR vRotCenter = XMVector3TransformCoord(vCenter, Rotation);
	XMVECTOR vPosition = XMLoadFloat3(&position);
	XMVECTOR vWorldPos = XMVectorAdd(vPosition, vRotCenter);
	XMFLOAT3 worldPosF;
	XMStoreFloat3(&worldPosF, vWorldPos);

	XMMATRIX Translation = XMMatrixTranslation(
		worldPosF.x,
		worldPosF.y,
		worldPosF.z
	);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(0, 1, 0, 0); // Green
	Shader_SetMaterial(material);

	// find top and bottom in local space
	XMFLOAT3 localBottom = { 0.0f, -height * 0.5f, 0.0f };
	XMFLOAT3 localTop = { 0.0f,  height * 0.5f, 0.0f };

	// Pass local-space cap centers to DrawSphere so DrawSphere rotates them consistently.
	XMFLOAT3 bottomCenterLocal = { center.x + localBottom.x, center.y + localBottom.y, center.z + localBottom.z };
	XMFLOAT3 topCenterLocal = { center.x + localTop.x,    center.y + localTop.y,    center.z + localTop.z };

	// draw both cap spheres (propagate colour so caps match the body)
	DrawSphere(position, bottomCenterLocal, radius, rotation, color);
	DrawSphere(position, topCenterLocal, radius, rotation, color);

	// draw the body (lines) with rotated center applied via Translation
	XMMATRIX Scaling = XMMatrixScaling(radius, height, radius);
	XMMATRIX World = Scaling * Rotation * Translation;
	Shader_SetWorldMatrix(World);

	// Use a temporary vertex buffer when a non-default colour is requested.
	ID3D11Buffer* vbToUse = m_capsuleVB;
	ID3D11Buffer* tempVB  = nullptr;
	static const XMFLOAT4 kDefaultColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	if (color.x != kDefaultColor.x || color.y != kDefaultColor.y || color.z != kDefaultColor.z || color.w != kDefaultColor.w) {
		VERTEX_3D verts[8];
		memcpy(verts, m_capsuleVerts, sizeof(m_capsuleVerts));
		for (auto& v : verts) v.Diffuse = color;

		D3D11_BUFFER_DESC bd = {};
		bd.Usage           = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth       = sizeof(verts);
		bd.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = verts;
		if (SUCCEEDED(GetDevice()->CreateBuffer(&bd, &sd, &tempVB))) {
			vbToUse = tempVB;
		}
	}

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &vbToUse, &stride, &offset);
	GetDeviceContext()->IASetIndexBuffer(m_capsuleIB, DXGI_FORMAT_R32_UINT, 0);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	GetDeviceContext()->DrawIndexed(8, 0, 0); // 4 lines

	if (tempVB) tempVB->Release();
}

void DebugDrawer::DrawCylinder(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, float height, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT4& color) {
	using namespace DirectX;

	XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(rotation.x),
		XMConvertToRadians(rotation.y),
		XMConvertToRadians(rotation.z));

	XMVECTOR vCenter = XMLoadFloat3(&center);
	XMVECTOR vRotCenter = XMVector3TransformCoord(vCenter, rotM);
	XMVECTOR vPosition = XMLoadFloat3(&position);
	XMFLOAT3 origin;
	XMStoreFloat3(&origin, XMVectorAdd(vPosition, vRotCenter));

	auto transformLocal = [&](float lx, float ly, float lz) -> XMFLOAT3 {
		XMVECTOR local = XMVectorSet(lx, ly, lz, 1.0f);
		XMVECTOR world = XMVector3TransformCoord(local, rotM) + XMLoadFloat3(&origin);
		XMFLOAT3 out;
		XMStoreFloat3(&out, world);
		return out;
	};

	const float halfH = height * 0.5f;
	const int segments = 24;
	std::vector<XMFLOAT3> lineVerts;
	lineVerts.reserve(static_cast<size_t>(segments) * 4 + 8);

	for (int i = 0; i < segments; ++i) {
		const float a0 = (static_cast<float>(i) / segments) * XM_2PI;
		const float a1 = (static_cast<float>(i + 1) / segments) * XM_2PI;
		const float x0 = std::cos(a0) * radius;
		const float z0 = std::sin(a0) * radius;
		const float x1 = std::cos(a1) * radius;
		const float z1 = std::sin(a1) * radius;

		const XMFLOAT3 b0 = transformLocal(x0, -halfH, z0);
		const XMFLOAT3 b1 = transformLocal(x1, -halfH, z1);
		const XMFLOAT3 t0 = transformLocal(x0, halfH, z0);
		const XMFLOAT3 t1 = transformLocal(x1, halfH, z1);

		lineVerts.push_back(b0);
		lineVerts.push_back(b1);
		lineVerts.push_back(t0);
		lineVerts.push_back(t1);
	}

	// Vertical struts at cardinal points
	lineVerts.push_back(transformLocal(radius, -halfH, 0.0f));
	lineVerts.push_back(transformLocal(radius, halfH, 0.0f));
	lineVerts.push_back(transformLocal(-radius, -halfH, 0.0f));
	lineVerts.push_back(transformLocal(-radius, halfH, 0.0f));
	lineVerts.push_back(transformLocal(0.0f, -halfH, radius));
	lineVerts.push_back(transformLocal(0.0f, halfH, radius));
	lineVerts.push_back(transformLocal(0.0f, -halfH, -radius));
	lineVerts.push_back(transformLocal(0.0f, halfH, -radius));

	DrawLines(lineVerts, color);
}

void DebugDrawer::DrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color) {
	if (!m_initialized) return;

	// Create temporary vertex buffer for the line
	VERTEX_3D lineVerts[2];
	lineVerts[0].Position = start;
	lineVerts[0].Normal = { 0, 1, 0 };
	lineVerts[0].Diffuse = color;
	lineVerts[0].TexCoord = { 0, 0 };

	lineVerts[1].Position = end;
	lineVerts[1].Normal = { 0, 1, 0 };
	lineVerts[1].Diffuse = color;
	lineVerts[1].TexCoord = { 0, 0 };

	// Create temporary vertex buffer
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * 2;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = lineVerts;

	ID3D11Buffer* lineVB = nullptr;
	HRESULT hr = GetDevice()->CreateBuffer(&bd, &sd, &lineVB);
	if (FAILED(hr)) return;

	// Set shader state
	// Shader_SetTexture(m_debugTexture);

	// Set identity world matrix (line vertices are already in world space)
	XMMATRIX identity = XMMatrixIdentity();
	Shader_SetWorldMatrix(identity);

	// Draw the line
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &lineVB, &stride, &offset);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	GetDeviceContext()->Draw(2, 0);

	// Clean up
	lineVB->Release();
}

void DebugDrawer::DrawLines(const std::vector<DirectX::XMFLOAT3>& lineVerts,
                             const DirectX::XMFLOAT4& color) {
	if (!m_initialized) return;
	if (lineVerts.empty() || lineVerts.size() % 2 != 0) return;

	// Build a flat VERTEX_3D array for all segments in one shot.
	std::vector<VERTEX_3D> verts(lineVerts.size());
	for (size_t i = 0; i < lineVerts.size(); ++i) {
		verts[i].Position = lineVerts[i];
		verts[i].Normal   = { 0.0f, 1.0f, 0.0f };
		verts[i].Diffuse  = color;
		verts[i].TexCoord = { 0.0f, 0.0f };
	}

	D3D11_BUFFER_DESC bd = {};
	bd.Usage          = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth      = static_cast<UINT>(sizeof(VERTEX_3D) * verts.size());
	bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = verts.data();

	ID3D11Buffer* batchVB = nullptr;
	if (FAILED(GetDevice()->CreateBuffer(&bd, &sd, &batchVB))) return;

	XMMATRIX identity = XMMatrixIdentity();
	Shader_SetWorldMatrix(identity);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &batchVB, &stride, &offset);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	GetDeviceContext()->Draw(static_cast<UINT>(verts.size()), 0);

	batchVB->Release();
}

void DebugDrawer::DrawLinesCached(const std::vector<DirectX::XMFLOAT3>& lineVerts,
                                   const DirectX::XMFLOAT4& color) {
	if (!m_initialized) return;
	if (lineVerts.empty() || lineVerts.size() % 2 != 0) return;

	const UINT count = static_cast<UINT>(lineVerts.size());

	// Grow the persistent buffer only when the current capacity is exceeded.
	if (count > m_linesVBCapacity) {
		if (m_linesVB) { m_linesVB->Release(); m_linesVB = nullptr; }
		D3D11_BUFFER_DESC bd = {};
		bd.Usage          = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth      = sizeof(VERTEX_3D) * count;
		bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(GetDevice()->CreateBuffer(&bd, nullptr, &m_linesVB))) return;
		m_linesVBCapacity = count;
	}

	// Upload vertex data via Map/Unmap — no allocation.
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(GetDeviceContext()->Map(m_linesVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	auto* dst = static_cast<VERTEX_3D*>(mapped.pData);
	for (UINT i = 0; i < count; ++i) {
		dst[i].Position = lineVerts[i];
		dst[i].Normal   = { 0.0f, 1.0f, 0.0f };
		dst[i].Diffuse  = color;
		dst[i].TexCoord = { 0.0f, 0.0f };
	}
	GetDeviceContext()->Unmap(m_linesVB, 0);

	XMMATRIX identity = XMMatrixIdentity();
	Shader_SetWorldMatrix(identity);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &m_linesVB, &stride, &offset);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	GetDeviceContext()->Draw(count, 0);
}

void DebugDrawer::QueueSphere(const DirectX::XMFLOAT3& position,
                               const DirectX::XMFLOAT3& center,
                               float radius,
                               const DirectX::XMFLOAT3& rotation,
                               const DirectX::XMFLOAT4& color) {
	s_sphereQueue.push_back({position, center, radius, rotation, color});
}

void DebugDrawer::FlushSphereQueue() {
	if (s_sphereQueue.empty()) return;

	static DebugDrawer drawer;
	if (!drawer.Initialize()) {
		s_sphereQueue.clear();
		return;
	}

	for (const auto& req : s_sphereQueue) {
		drawer.DrawSphere(req.position, req.center, req.radius, req.rotation, req.color, false);
	}
	s_sphereQueue.clear();

	// Restore default depth state after the no-depth draws.
	SetDepthEnable(true);
}

void DebugDrawer::QueueLine(const DirectX::XMFLOAT3& start,
                            const DirectX::XMFLOAT3& end,
                            const DirectX::XMFLOAT4& color) {
	s_lineQueue.push_back({ start, end, color });
}

void DebugDrawer::FlushLineQueue() {
	if (s_lineQueue.empty()) return;

	static DebugDrawer drawer;
	if (!drawer.Initialize()) {
		s_lineQueue.clear();
		return;
	}

	for (const auto& req : s_lineQueue) {
		drawer.DrawLine(req.start, req.end, req.color);
	}
	s_lineQueue.clear();
}
