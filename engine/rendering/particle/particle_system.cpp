// particle_system.cpp
// Implements ParticleVertex.

#include "particle_system.h"

#include "engine/ecs/string_utils.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/core/content_path.h"
#include "engine/rendering/culling/frustum_cull_math.h"
#include "engine/rendering/culling/fx_cull.h"
#include "engine/rendering/culling/fx_cull_math.h"
#include "engine/scene/render_settings.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>

using namespace DirectX;

namespace engine {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGravity = -9.8f;

std::mt19937& ParticleRng() {
	static std::mt19937 rng{ std::random_device{}() };
	return rng;
}

float Clamp01(float value) {
	return std::clamp(value, 0.0f, 1.0f);
}

XMFLOAT4 Lerp(const XMFLOAT4& a, const XMFLOAT4& b, float t) {
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
		a.w + (b.w - a.w) * t
	};
}

float Lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

XMFLOAT3 NormalizeOrUp(const XMFLOAT3& value) {
	const float lenSq = value.x * value.x + value.y * value.y + value.z * value.z;
	if (lenSq <= 0.000001f)
		return { 0.0f, 1.0f, 0.0f };
	const float invLen = 1.0f / sqrtf(lenSq);
	return { value.x * invLen, value.y * invLen, value.z * invLen };
}

bool LoadCompiledShader(const char* path, std::vector<char>& data) {
	std::ifstream file(content_path::ReadableFileCurrent(path), std::ios::binary);
	if (!file)
		return false;
	file.seekg(0, std::ios::end);
	data.resize(static_cast<size_t>(file.tellg()));
	file.seekg(0, std::ios::beg);
	file.read(data.data(), static_cast<std::streamsize>(data.size()));
	return !data.empty();
}

struct ParticleVertex {
	XMFLOAT3 position;
	XMFLOAT2 texcoord;
};

struct ParticleInstanceData {
	XMFLOAT3 position;
	float size;
	XMFLOAT4 color;
	XMFLOAT4 uv; // xy = offset, zw = scale
	float rotation;
	XMFLOAT3 padding;
};

struct CBViewProj {
	XMFLOAT4X4 view;
	XMFLOAT4X4 proj;
};

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11InputLayout* g_inputLayout = nullptr;
ID3D11Buffer* g_vertexBuffer = nullptr;
ID3D11Buffer* g_instanceBuffer = nullptr;
ID3D11Buffer* g_viewProjBuffer = nullptr;
ID3D11SamplerState* g_sampler = nullptr;
ID3D11RasterizerState* g_rasterNoCull = nullptr;
ID3D11DepthStencilState* g_depthReadOnly = nullptr;
ID3D11BlendState* g_alphaBlend = nullptr;
ID3D11BlendState* g_additiveBlend = nullptr;
std::vector<ParticleInstanceData> g_instances;

bool CreateBlendState(D3D11_BLEND srcBlend, D3D11_BLEND dstBlend, ID3D11BlendState** outState) {
	D3D11_BLEND_DESC desc = {};
	desc.RenderTarget[0].BlendEnable = TRUE;
	desc.RenderTarget[0].SrcBlend = srcBlend;
	desc.RenderTarget[0].DestBlend = dstBlend;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return SUCCEEDED(g_device->CreateBlendState(&desc, outState));
}
} // namespace

void ParticlePool::Reset(int maxParticles) {
	m_aliveCount = 0;
	m_maxParticles = std::clamp(maxParticles, 1, kMaxParticles);
	m_alive.fill(false);
}

bool ParticlePool::Spawn(const XMFLOAT3& position,
	const XMFLOAT3& velocity,
	float lifetime,
	float startSize,
	float endSize,
	float rotation,
	const XMFLOAT4& startColor,
	const XMFLOAT4& endColor) {
	if (m_aliveCount >= m_maxParticles)
		return false;

	const int i = m_aliveCount++;
	m_positions[i] = position;
	m_velocities[i] = velocity;
	m_lifetimes[i] = (std::max)(0.01f, lifetime);
	m_startLifetimes[i] = m_lifetimes[i];
	m_startSizes[i] = startSize;
	m_endSizes[i] = endSize;
	m_sizes[i] = startSize;
	m_rotations[i] = rotation;
	m_startColors[i] = startColor;
	m_endColors[i] = endColor;
	m_colors[i] = startColor;
	m_frames[i] = 0;
	m_alive[i] = true;
	return true;
}

void ParticlePool::Tick(float dt, float gravityModifier, int textureSheetFrames, float textureSheetCycles) {
	if (dt <= 0.0f)
		return;

	const int frameCount = (std::max)(1, textureSheetFrames);
	const float cycles = (std::max)(0.0f, textureSheetCycles);
	for (int i = 0; i < m_aliveCount; ++i) {
		if (!m_alive[i])
			continue;

		const float startLifetime = (std::max)(0.01f, m_startLifetimes[i]);
		const float normalizedAge = Clamp01(1.0f - (m_lifetimes[i] / startLifetime));
		m_velocities[i].y += kGravity * gravityModifier * dt;
		m_positions[i].x += m_velocities[i].x * dt;
		m_positions[i].y += m_velocities[i].y * dt;
		m_positions[i].z += m_velocities[i].z * dt;
		m_sizes[i] = Lerp(m_startSizes[i], m_endSizes[i], normalizedAge);
		m_colors[i] = Lerp(m_startColors[i], m_endColors[i], normalizedAge);
		const int frame = static_cast<int>(normalizedAge * cycles * static_cast<float>(frameCount));
		m_frames[i] = std::clamp(frame, 0, frameCount - 1);

		m_lifetimes[i] -= dt;
		if (m_lifetimes[i] <= 0.0f)
			m_alive[i] = false;
	}
}

void ParticlePool::Compact() {
	int write = 0;
	for (int read = 0; read < m_aliveCount; ++read) {
		if (!m_alive[read])
			continue;
		if (write != read)
			CopyParticle(write, read);
		m_alive[write] = true;
		++write;
	}
	for (int i = write; i < m_aliveCount; ++i)
		m_alive[i] = false;
	m_aliveCount = write;
}

void ParticlePool::CopyParticle(int dst, int src) {
	m_positions[dst] = m_positions[src];
	m_velocities[dst] = m_velocities[src];
	m_lifetimes[dst] = m_lifetimes[src];
	m_startLifetimes[dst] = m_startLifetimes[src];
	m_startSizes[dst] = m_startSizes[src];
	m_endSizes[dst] = m_endSizes[src];
	m_sizes[dst] = m_sizes[src];
	m_rotations[dst] = m_rotations[src];
	m_startColors[dst] = m_startColors[src];
	m_endColors[dst] = m_endColors[src];
	m_colors[dst] = m_colors[src];
	m_frames[dst] = m_frames[src];
	m_alive[dst] = m_alive[src];
}

ParticleEmitter::ParticleEmitter(std::uint32_t id, const ParticleEmitterConfig& config, const XMFLOAT3& position)
	: m_id(id), m_config(config), m_position(position) {
	SetConfig(config);
}

void ParticleEmitter::SetConfig(const ParticleEmitterConfig& config) {
	m_config = config;
	m_config.maxParticles = std::clamp(m_config.maxParticles, 1, ParticlePool::kMaxParticles);
	m_config.startLifetimeMin = (std::max)(0.01f, m_config.startLifetimeMin);
	m_config.startLifetimeMax = (std::max)(m_config.startLifetimeMin, m_config.startLifetimeMax);
	m_config.startSpeedMin = (std::max)(0.0f, m_config.startSpeedMin);
	m_config.startSpeedMax = (std::max)(m_config.startSpeedMin, m_config.startSpeedMax);
	m_config.startSizeMin = (std::max)(0.0f, m_config.startSizeMin);
	m_config.startSizeMax = (std::max)(m_config.startSizeMin, m_config.startSizeMax);
	m_config.textureSheetFrames = (std::max)(1, m_config.textureSheetFrames);
	m_pool.Reset(m_config.maxParticles);
}

void ParticleEmitter::Tick(float dt) {
	if (dt <= 0.0f)
		return;

	if (m_emitting) {
		if (!m_burstEmitted) {
			Emit((std::max)(0, m_config.burstCount));
			m_burstEmitted = true;
		}

		if (m_config.looping || m_burstEmitted) {
			m_emitAccumulator += (std::max)(0.0f, m_config.emissionRate) * dt;
			const int emitCount = static_cast<int>(m_emitAccumulator);
			if (emitCount > 0) {
				Emit(emitCount);
				m_emitAccumulator -= static_cast<float>(emitCount);
			}
		}
	}

	m_pool.Tick(dt, m_config.gravityModifier, m_config.textureSheetFrames, m_config.textureSheetCycles);
	m_pool.Compact();
}

void ParticleEmitter::Render() const {
	// Rendered by ParticleRenderer_RenderEmitter.
}

void ParticleEmitter::Stop(bool destroyWhenEmpty) {
	m_emitting = false;
	m_destroyWhenEmpty = destroyWhenEmpty;
}

void ParticleEmitter::Emit(int count) {
	for (int i = 0; i < count; ++i) {
		XMFLOAT3 localPos = RandomShapePosition();
		XMFLOAT3 position = localPos;
		if (m_config.simulationSpace == ParticleSimulationSpace::World) {
			position.x += m_position.x;
			position.y += m_position.y;
			position.z += m_position.z;
		}

		const XMFLOAT3 direction = RandomDirection();
		const float speed = RandomRange(m_config.startSpeedMin, m_config.startSpeedMax);
		const XMFLOAT3 velocity = { direction.x * speed, direction.y * speed, direction.z * speed };
		const float lifetime = RandomRange(m_config.startLifetimeMin, m_config.startLifetimeMax);
		const float startSize = RandomRange(m_config.startSizeMin, m_config.startSizeMax) * m_config.sizeOverLifetimeStart;
		const float endSize = RandomRange(m_config.startSizeMin, m_config.startSizeMax) * m_config.sizeOverLifetimeEnd;
		const XMFLOAT4 randomColor = RandomRange(m_config.startColorMin, m_config.startColorMax);
		const XMFLOAT4 startColor = {
			randomColor.x * m_config.colorOverLifetimeStart.x,
			randomColor.y * m_config.colorOverLifetimeStart.y,
			randomColor.z * m_config.colorOverLifetimeStart.z,
			randomColor.w * m_config.colorOverLifetimeStart.w
		};
		const XMFLOAT4 endColor = {
			randomColor.x * m_config.colorOverLifetimeEnd.x,
			randomColor.y * m_config.colorOverLifetimeEnd.y,
			randomColor.z * m_config.colorOverLifetimeEnd.z,
			randomColor.w * m_config.colorOverLifetimeEnd.w
		};
		const float rotation = RandomRange(0.0f, kPi * 2.0f);
		if (!m_pool.Spawn(position, velocity, lifetime, startSize, endSize, rotation, startColor, endColor))
			break;
	}
}

XMFLOAT3 ParticleEmitter::RandomShapePosition() {
	switch (m_config.shape) {
	case ParticleShape::Box:
		return {
			RandomRange(-m_config.halfExtents.x, m_config.halfExtents.x),
			RandomRange(-m_config.halfExtents.y, m_config.halfExtents.y),
			RandomRange(-m_config.halfExtents.z, m_config.halfExtents.z)
		};
	case ParticleShape::Edge:
		return { RandomRange(-m_config.halfExtents.x, m_config.halfExtents.x), 0.0f, 0.0f };
	case ParticleShape::Cone:
	case ParticleShape::Sphere:
	default: {
		const float radius = (std::max)(0.0f, m_config.radius);
		const XMFLOAT3 dir = RandomDirection();
		const float r = radius * cbrtf(RandomRange(0.0f, 1.0f));
		return { dir.x * r, dir.y * r, dir.z * r };
	}
	}
}

XMFLOAT3 ParticleEmitter::RandomDirection() {
	if (m_config.shape == ParticleShape::Cone) {
		const float maxAngle = std::clamp(m_config.angle, 0.0f, 180.0f) * (kPi / 180.0f);
		const float cosTheta = RandomRange(cosf(maxAngle), 1.0f);
		const float sinTheta = sqrtf((std::max)(0.0f, 1.0f - cosTheta * cosTheta));
		const float phi = RandomRange(0.0f, kPi * 2.0f);
		return NormalizeOrUp({ cosf(phi) * sinTheta, cosTheta, sinf(phi) * sinTheta });
	}

	const float z = RandomRange(-1.0f, 1.0f);
	const float theta = RandomRange(0.0f, kPi * 2.0f);
	const float r = sqrtf((std::max)(0.0f, 1.0f - z * z));
	return { r * cosf(theta), z, r * sinf(theta) };
}

float ParticleEmitter::RandomRange(float minValue, float maxValue) {
	if (maxValue <= minValue)
		return minValue;
	std::uniform_real_distribution<float> dist(minValue, maxValue);
	return dist(ParticleRng());
}

XMFLOAT4 ParticleEmitter::RandomRange(const XMFLOAT4& minValue, const XMFLOAT4& maxValue) {
	return {
		RandomRange(minValue.x, maxValue.x),
		RandomRange(minValue.y, maxValue.y),
		RandomRange(minValue.z, maxValue.z),
		RandomRange(minValue.w, maxValue.w)
	};
}

ID3D11ShaderResourceView* ParticleEmitter::GetTextureSRV() const {
	if (m_textureId >= 0)
		return TextureGetSRV(m_textureId);
	if (m_config.texturePath.empty())
		return nullptr;

	const std::wstring wpath = ToWString(m_config.texturePath);
	m_textureId = TextureLoad(wpath.c_str(), true);
	if (m_textureId >= 0)
		m_textureSrv = TextureGetSRV(m_textureId);
	return m_textureSrv;
}

ParticleSystem& ParticleSystem::Instance() {
	static ParticleSystem system;
	return system;
}

bool ParticleSystem::Initialize() {
	return ParticleRenderer_Initialize();
}

void ParticleSystem::Finalize() {
	Reset();
	ParticleRenderer_Finalize();
}

void ParticleSystem::Reset() {
	m_emitters.clear();
	m_nextEmitterId = 1;
}

void ParticleSystem::Tick(float dt) {
	for (auto& emitter : m_emitters) {
		if (emitter)
			emitter->Tick(dt);
	}

	m_emitters.erase(
		std::remove_if(m_emitters.begin(), m_emitters.end(),
			[](const std::unique_ptr<ParticleEmitter>& emitter) {
				return !emitter || emitter->ShouldDestroy();
			}),
		m_emitters.end());
}

void ParticleSystem::Render(const XMMATRIX& view, const XMMATRIX& proj) {
	for (const auto& emitter : m_emitters) {
		if (emitter && emitter->IsAlive())
			ParticleRenderer_RenderEmitter(*emitter, view, proj);
	}
}

std::uint32_t ParticleSystem::CreateEmitter(const ParticleEmitterConfig& config, const XMFLOAT3& position) {
	const std::uint32_t id = m_nextEmitterId++;
	m_emitters.push_back(std::make_unique<ParticleEmitter>(id, config, position));
	return id;
}

void ParticleSystem::SetEmitterPosition(std::uint32_t emitterId, const XMFLOAT3& position) {
	if (ParticleEmitter* emitter = FindEmitter(emitterId))
		emitter->SetPosition(position);
}

void ParticleSystem::SetEmitterEmitting(std::uint32_t emitterId, bool emitting) {
	if (ParticleEmitter* emitter = FindEmitter(emitterId))
		emitter->SetEmitting(emitting);
}

void ParticleSystem::StopEmitter(std::uint32_t emitterId, bool destroyWhenEmpty) {
	if (ParticleEmitter* emitter = FindEmitter(emitterId))
		emitter->Stop(destroyWhenEmpty);
}

void ParticleSystem::DestroyEmitter(std::uint32_t emitterId) {
	m_emitters.erase(
		std::remove_if(m_emitters.begin(), m_emitters.end(),
			[emitterId](const std::unique_ptr<ParticleEmitter>& emitter) {
				return emitter && emitter->GetId() == emitterId;
			}),
		m_emitters.end());
}

void ParticleSystem::SetEmitterConfig(std::uint32_t emitterId, const ParticleEmitterConfig& config) {
	if (ParticleEmitter* emitter = FindEmitter(emitterId))
		emitter->SetConfig(config);
}

void ParticleSystem::SpawnParticle(std::uint32_t emitterId,
	const XMFLOAT3& position,
	const XMFLOAT3& velocity,
	const XMFLOAT4& color,
	float size,
	float lifetime) {
	if (emitterId == 0)
		return;

	ParticleEmitter* emitter = FindEmitter(emitterId);
	if (!emitter)
		return;

	const float startSize = (std::max)(0.0f, size);
	const XMFLOAT4 endColor = { color.x, color.y, color.z, 0.0f };
	emitter->GetPool().Spawn(
		position,
		velocity,
		lifetime,
		startSize,
		0.0f,
		0.0f,
		color,
		endColor);
}

ParticleEmitter* ParticleSystem::FindEmitter(std::uint32_t emitterId) {
	for (auto& emitter : m_emitters) {
		if (emitter && emitter->GetId() == emitterId)
			return emitter.get();
	}
	return nullptr;
}

bool ParticleRenderer_Initialize() {
	g_device = GetDevice();
	g_context = GetDeviceContext();
	if (!g_device || !g_context)
		return false;

	std::vector<char> vsData;
	std::vector<char> psData;
	if (!LoadCompiledShader("rom/shader/ParticleVS.cso", vsData)) {
		MessageBoxA(nullptr, "Cannot open rom/shader/ParticleVS.cso\nPlease build the project to generate .cso files.", "Particle VS Load Error", MB_OK | MB_ICONERROR);
		return false;
	}
	if (!LoadCompiledShader("rom/shader/ParticlePS.cso", psData)) {
		MessageBoxA(nullptr, "Cannot open rom/shader/ParticlePS.cso\nPlease build the project to generate .cso files.", "Particle PS Load Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (FAILED(g_device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &g_vs)))
		return false;
	if (FAILED(g_device->CreatePixelShader(psData.data(), psData.size(), nullptr, &g_ps)))
		return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "INSTANCEPOS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCESIZE", 0, DXGI_FORMAT_R32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCECOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCEUV", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCEROT", 0, DXGI_FORMAT_R32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	if (FAILED(g_device->CreateInputLayout(layout, _countof(layout), vsData.data(), vsData.size(), &g_inputLayout)))
		return false;

	ParticleVertex vertices[] = {
		{ { -0.5f, +0.5f, 0.0f }, { 0.0f, 0.0f } },
		{ { +0.5f, +0.5f, 0.0f }, { 1.0f, 0.0f } },
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		{ { +0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
	};
	D3D11_BUFFER_DESC vertexDesc = {};
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = sizeof(vertices);
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices;
	if (FAILED(g_device->CreateBuffer(&vertexDesc, &vertexData, &g_vertexBuffer)))
		return false;

	D3D11_BUFFER_DESC instanceDesc = {};
	instanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	instanceDesc.ByteWidth = sizeof(ParticleInstanceData) * ParticlePool::kMaxParticles;
	instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(g_device->CreateBuffer(&instanceDesc, nullptr, &g_instanceBuffer)))
		return false;

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.ByteWidth = sizeof(CBViewProj);
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(g_device->CreateBuffer(&cbDesc, nullptr, &g_viewProjBuffer)))
		return false;

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(g_device->CreateSamplerState(&samplerDesc, &g_sampler)))
		return false;

	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthClipEnable = TRUE;
	if (FAILED(g_device->CreateRasterizerState(&rasterDesc, &g_rasterNoCull)))
		return false;

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(g_device->CreateDepthStencilState(&depthDesc, &g_depthReadOnly)))
		return false;

	if (!CreateBlendState(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, &g_alphaBlend))
		return false;
	if (!CreateBlendState(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, &g_additiveBlend))
		return false;

	return true;
}

void ParticleRenderer_Finalize() {
	g_instances.clear();
	if (g_additiveBlend) { g_additiveBlend->Release(); g_additiveBlend = nullptr; }
	if (g_alphaBlend) { g_alphaBlend->Release(); g_alphaBlend = nullptr; }
	if (g_depthReadOnly) { g_depthReadOnly->Release(); g_depthReadOnly = nullptr; }
	if (g_rasterNoCull) { g_rasterNoCull->Release(); g_rasterNoCull = nullptr; }
	if (g_sampler) { g_sampler->Release(); g_sampler = nullptr; }
	if (g_viewProjBuffer) { g_viewProjBuffer->Release(); g_viewProjBuffer = nullptr; }
	if (g_instanceBuffer) { g_instanceBuffer->Release(); g_instanceBuffer = nullptr; }
	if (g_vertexBuffer) { g_vertexBuffer->Release(); g_vertexBuffer = nullptr; }
	if (g_inputLayout) { g_inputLayout->Release(); g_inputLayout = nullptr; }
	if (g_ps) { g_ps->Release(); g_ps = nullptr; }
	if (g_vs) { g_vs->Release(); g_vs = nullptr; }
	g_context = nullptr;
	g_device = nullptr;
}

void ParticleRenderer_RenderEmitter(const ParticleEmitter& emitter, const XMMATRIX& view, const XMMATRIX& proj) {
	if (!g_context || !g_instanceBuffer || !g_vertexBuffer)
		return;

	const ParticlePool& pool = emitter.GetPool();
	const ParticleEmitterConfig& config = emitter.GetConfig();
	const XMFLOAT3& emitterPosition = emitter.GetPosition();

	const int aliveCount = pool.GetAliveCount();
	if (aliveCount <= 0)
		return;

	const float cellWidth = 1.0f / static_cast<float>((std::max)(1, config.textureSheetFrames));
	g_instances.clear();
	g_instances.reserve(static_cast<size_t>(aliveCount));

	const bool cullParticles = RenderSettings_GetConst().billboardCullingEnabled;
	frustum_cull::FrustumPlanes planes;
	if (cullParticles)
		frustum_cull::ExtractFrustumPlanesD3D(view * proj, planes);
	int culledParticles = 0;

	const auto& positions = pool.Positions();
	const auto& sizes = pool.Sizes();
	const auto& rotations = pool.Rotations();
	const auto& colors = pool.Colors();
	const auto& frames = pool.Frames();
	for (int i = 0; i < aliveCount; ++i) {
		XMFLOAT3 position = positions[i];
		if (config.simulationSpace == ParticleSimulationSpace::Local) {
			position.x += emitterPosition.x;
			position.y += emitterPosition.y;
			position.z += emitterPosition.z;
		}

		if (cullParticles && !fx_cull::SphereIntersectsFrustum(
				position.x, position.y, position.z, fx_cull::ParticleRadius(sizes[i]), planes)) {
			++culledParticles;
			continue;
		}

		const int frame = frames[i] % (std::max)(1, config.textureSheetFrames);
		ParticleInstanceData instance = {};
		instance.position = position;
		instance.size = sizes[i];
		instance.color = colors[i];
		instance.uv = { static_cast<float>(frame) * cellWidth, 0.0f, cellWidth, 1.0f };
		instance.rotation = rotations[i];
		g_instances.push_back(instance);
	}

	if (cullParticles)
		FxCull_RecordParticles(aliveCount, culledParticles);
	if (g_instances.empty())
		return;

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(g_context->Map(g_instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;
	memcpy(mapped.pData, g_instances.data(), sizeof(ParticleInstanceData) * g_instances.size());
	g_context->Unmap(g_instanceBuffer, 0);

	ID3D11VertexShader* prevVs = nullptr;
	ID3D11PixelShader* prevPs = nullptr;
	ID3D11InputLayout* prevLayout = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY prevTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	ID3D11Buffer* prevBuffers[2] = {};
	UINT prevStrides[2] = {};
	UINT prevOffsets[2] = {};
	ID3D11RasterizerState* prevRaster = nullptr;
	ID3D11BlendState* prevBlend = nullptr;
	FLOAT prevBlendFactor[4] = {};
	UINT prevSampleMask = 0;
	ID3D11DepthStencilState* prevDepth = nullptr;
	UINT prevStencilRef = 0;
	ID3D11Buffer* prevVsCb0 = nullptr;
	ID3D11SamplerState* prevSampler = nullptr;
	ID3D11ShaderResourceView* prevSrv = nullptr;

	g_context->VSGetShader(&prevVs, nullptr, nullptr);
	g_context->PSGetShader(&prevPs, nullptr, nullptr);
	g_context->IAGetInputLayout(&prevLayout);
	g_context->IAGetPrimitiveTopology(&prevTopology);
	g_context->IAGetVertexBuffers(0, 2, prevBuffers, prevStrides, prevOffsets);
	g_context->RSGetState(&prevRaster);
	g_context->OMGetBlendState(&prevBlend, prevBlendFactor, &prevSampleMask);
	g_context->OMGetDepthStencilState(&prevDepth, &prevStencilRef);
	g_context->VSGetConstantBuffers(0, 1, &prevVsCb0);
	g_context->PSGetSamplers(0, 1, &prevSampler);
	g_context->PSGetShaderResources(0, 1, &prevSrv);

	CBViewProj cb = {};
	XMStoreFloat4x4(&cb.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&cb.proj, XMMatrixTranspose(proj));
	g_context->UpdateSubresource(g_viewProjBuffer, 0, nullptr, &cb, 0, 0);

	ID3D11Buffer* buffers[2] = { g_vertexBuffer, g_instanceBuffer };
	UINT strides[2] = { sizeof(ParticleVertex), sizeof(ParticleInstanceData) };
	UINT offsets[2] = { 0, 0 };
	g_context->VSSetShader(g_vs, nullptr, 0);
	g_context->PSSetShader(g_ps, nullptr, 0);
	g_context->IASetInputLayout(g_inputLayout);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	g_context->VSSetConstantBuffers(0, 1, &g_viewProjBuffer);
	g_context->PSSetSamplers(0, 1, &g_sampler);
	g_context->RSSetState(g_rasterNoCull);
	g_context->OMSetDepthStencilState(g_depthReadOnly, 0);

	const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ID3D11BlendState* blend = config.billboardBlendMode == ParticleBillboardBlendMode::Additive
		? g_additiveBlend
		: g_alphaBlend;
	g_context->OMSetBlendState(blend, blendFactor, 0xffffffff);

	ID3D11ShaderResourceView* srv = emitter.GetTextureSRV();
	if (!srv) {
		const int whiteId = TextureGetWhiteTexture();
		if (whiteId >= 0)
			srv = TextureGetSRV(whiteId);
	}
	if (srv)
		g_context->PSSetShaderResources(0, 1, &srv);

	g_context->DrawInstanced(4, static_cast<UINT>(g_instances.size()), 0, 0);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	g_context->PSSetShaderResources(0, 1, &nullSrv);
	g_context->VSSetShader(prevVs, nullptr, 0);
	g_context->PSSetShader(prevPs, nullptr, 0);
	g_context->IASetInputLayout(prevLayout);
	g_context->IASetPrimitiveTopology(prevTopology);
	g_context->IASetVertexBuffers(0, 2, prevBuffers, prevStrides, prevOffsets);
	g_context->RSSetState(prevRaster);
	g_context->OMSetBlendState(prevBlend, prevBlendFactor, prevSampleMask);
	g_context->OMSetDepthStencilState(prevDepth, prevStencilRef);
	g_context->VSSetConstantBuffers(0, 1, &prevVsCb0);
	g_context->PSSetSamplers(0, 1, &prevSampler);
	g_context->PSSetShaderResources(0, 1, &prevSrv);

	if (prevVs) prevVs->Release();
	if (prevPs) prevPs->Release();
	if (prevLayout) prevLayout->Release();
	for (ID3D11Buffer* buffer : prevBuffers)
		if (buffer) buffer->Release();
	if (prevRaster) prevRaster->Release();
	if (prevBlend) prevBlend->Release();
	if (prevDepth) prevDepth->Release();
	if (prevVsCb0) prevVsCb0->Release();
	if (prevSampler) prevSampler->Release();
	if (prevSrv) prevSrv->Release();
}

} // namespace engine
