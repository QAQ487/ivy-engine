#pragma once

#include <DirectXMath.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

namespace engine {

enum class ParticleShape {
	Sphere,
	Cone,
	Box,
	Edge
};

enum class ParticleSimulationSpace {
	Local,
	World
};

enum class ParticleBillboardBlendMode {
	Additive,
	Alpha
};

struct ParticleEmitterConfig {
	float emissionRate = 0.0f;
	int burstCount = 0;
	ParticleShape shape = ParticleShape::Sphere;
	float radius = 0.0f;
	float angle = 25.0f;
	DirectX::XMFLOAT3 halfExtents = { 0.0f, 0.0f, 0.0f };
	float startLifetimeMin = 1.0f;
	float startLifetimeMax = 1.0f;
	float startSpeedMin = 0.0f;
	float startSpeedMax = 0.0f;
	float startSizeMin = 1.0f;
	float startSizeMax = 1.0f;
	DirectX::XMFLOAT4 startColorMin = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 startColorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
	float gravityModifier = 0.0f;
	DirectX::XMFLOAT4 colorOverLifetimeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 colorOverLifetimeEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
	float sizeOverLifetimeStart = 1.0f;
	float sizeOverLifetimeEnd = 0.0f;
	ParticleSimulationSpace simulationSpace = ParticleSimulationSpace::World;
	int maxParticles = 1024;
	bool looping = true;
	std::string texturePath;
	ParticleBillboardBlendMode billboardBlendMode = ParticleBillboardBlendMode::Alpha;
	int textureSheetFrames = 1;
	float textureSheetCycles = 1.0f;
};

class ParticlePool {
public:
	static constexpr int kMaxParticles = 1024;

	void Reset(int maxParticles);
	bool Spawn(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity,
		float lifetime,
		float startSize,
		float endSize,
		float rotation,
		const DirectX::XMFLOAT4& startColor,
		const DirectX::XMFLOAT4& endColor);
	void Tick(float dt, float gravityModifier, int textureSheetFrames, float textureSheetCycles);
	void Compact();

	int GetAliveCount() const { return m_aliveCount; }
	int GetMaxParticles() const { return m_maxParticles; }

	const std::array<DirectX::XMFLOAT3, kMaxParticles>& Positions() const { return m_positions; }
	const std::array<float, kMaxParticles>& Sizes() const { return m_sizes; }
	const std::array<float, kMaxParticles>& Rotations() const { return m_rotations; }
	const std::array<DirectX::XMFLOAT4, kMaxParticles>& Colors() const { return m_colors; }
	const std::array<int, kMaxParticles>& Frames() const { return m_frames; }

private:
	void CopyParticle(int dst, int src);

	int m_aliveCount = 0;
	int m_maxParticles = kMaxParticles;
	std::array<DirectX::XMFLOAT3, kMaxParticles> m_positions = {};
	std::array<DirectX::XMFLOAT3, kMaxParticles> m_velocities = {};
	std::array<float, kMaxParticles> m_lifetimes = {};
	std::array<float, kMaxParticles> m_startLifetimes = {};
	std::array<float, kMaxParticles> m_startSizes = {};
	std::array<float, kMaxParticles> m_endSizes = {};
	std::array<float, kMaxParticles> m_sizes = {};
	std::array<float, kMaxParticles> m_rotations = {};
	std::array<DirectX::XMFLOAT4, kMaxParticles> m_startColors = {};
	std::array<DirectX::XMFLOAT4, kMaxParticles> m_endColors = {};
	std::array<DirectX::XMFLOAT4, kMaxParticles> m_colors = {};
	std::array<int, kMaxParticles> m_frames = {};
	std::array<bool, kMaxParticles> m_alive = {};
};

class ParticleEmitter {
public:
	ParticleEmitter(std::uint32_t id, const ParticleEmitterConfig& config, const DirectX::XMFLOAT3& position);

	std::uint32_t GetId() const { return m_id; }
	const ParticleEmitterConfig& GetConfig() const { return m_config; }
	const ParticlePool& GetPool() const { return m_pool; }
	ParticlePool& GetPool() { return m_pool; }
	const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
	ID3D11ShaderResourceView* GetTextureSRV() const;
	void Tick(float dt);
	void Render() const;
	void SetPosition(const DirectX::XMFLOAT3& position) { m_position = position; }
	void SetEmitting(bool emitting) { m_emitting = emitting; }
	void Stop(bool destroyWhenEmpty);
	bool ShouldDestroy() const { return m_destroyWhenEmpty && m_pool.GetAliveCount() == 0; }
	bool IsAlive() const { return m_pool.GetAliveCount() > 0 || m_emitting; }
	void SetConfig(const ParticleEmitterConfig& config);

private:
	void Emit(int count);
	DirectX::XMFLOAT3 RandomShapePosition();
	DirectX::XMFLOAT3 RandomDirection();
	float RandomRange(float minValue, float maxValue);
	DirectX::XMFLOAT4 RandomRange(const DirectX::XMFLOAT4& minValue, const DirectX::XMFLOAT4& maxValue);

	std::uint32_t m_id = 0;
	ParticleEmitterConfig m_config;
	ParticlePool m_pool;
	DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
	float m_emitAccumulator = 0.0f;
	bool m_emitting = true;
	bool m_burstEmitted = false;
	bool m_destroyWhenEmpty = false;
	mutable int m_textureId = -1;
	mutable ID3D11ShaderResourceView* m_textureSrv = nullptr;
};

class ParticleSystem {
public:
	static ParticleSystem& Instance();

	bool Initialize();
	void Finalize();
	void Reset();
	void Tick(float dt);
	void Render(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

	std::uint32_t CreateEmitter(const ParticleEmitterConfig& config, const DirectX::XMFLOAT3& position);
	void SetEmitterPosition(std::uint32_t emitterId, const DirectX::XMFLOAT3& position);
	void SetEmitterEmitting(std::uint32_t emitterId, bool emitting);
	void StopEmitter(std::uint32_t emitterId, bool destroyWhenEmpty);
	void DestroyEmitter(std::uint32_t emitterId);
	void SetEmitterConfig(std::uint32_t emitterId, const ParticleEmitterConfig& config);
	void SpawnParticle(std::uint32_t emitterId,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity,
		const DirectX::XMFLOAT4& color,
		float size,
		float lifetime);

private:
	ParticleSystem() = default;
	ParticleEmitter* FindEmitter(std::uint32_t emitterId);

	std::vector<std::unique_ptr<ParticleEmitter>> m_emitters;
	std::uint32_t m_nextEmitterId = 1;
};

bool ParticleRenderer_Initialize();
void ParticleRenderer_Finalize();
void ParticleRenderer_RenderEmitter(const ParticleEmitter& emitter,
	const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj);

} // namespace engine
