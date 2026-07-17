#include "ImpactSparkVfx.h"

#include "engine/rendering/particle/particle_system.h"
#include "engine/scripting/script_runtime_api.h"

#include <cmath>
#include <cstdlib>

using namespace DirectX;

namespace {
float Rand01() {
	return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float RandRange(float lo, float hi) {
	return lo + (hi - lo) * Rand01();
}

XMFLOAT3 NormalizeOrFallback(const XMFLOAT3& v, const XMFLOAT3& fallback) {
	const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq < 1.0e-8f)
		return fallback;
	const float inv = 1.0f / sqrtf(lenSq);
	return { v.x * inv, v.y * inv, v.z * inv };
}

// Random direction biased along the contact normal, with upward spray.
XMFLOAT3 RandomSparkDirection(const XMFLOAT3& normalXZ) {
	const XMFLOAT3 basis = NormalizeOrFallback(
		{ normalXZ.x, 0.0f, normalXZ.z },
		{ 1.0f, 0.0f, 0.0f });

	const float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
	const XMFLOAT3 tangent = { -basis.z * side, 0.0f, basis.x * side };

	const float along = RandRange(-0.35f, 1.0f);
	const float across = RandRange(-0.85f, 0.85f);
	const float up = RandRange(0.35f, 1.15f);

	return NormalizeOrFallback(
		{
			basis.x * along + tangent.x * across,
			up,
			basis.z * along + tangent.z * across
		},
		{ 0.0f, 1.0f, 0.0f });
}

XMFLOAT4 LerpColor(const XMFLOAT4& a, const XMFLOAT4& b, float t) {
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
		a.w + (b.w - a.w) * t
	};
}
} // namespace

ImpactSparkVfx& ImpactSparkVfx::Instance() {
	static ImpactSparkVfx vfx;
	return vfx;
}

void ImpactSparkVfx::EnsureEmitter() {
	if (m_emitterId != 0)
		return;

	engine::ParticleEmitterConfig cfg;
	cfg.emissionRate = 0.0f;
	cfg.burstCount = 0;
	cfg.shape = engine::ParticleShape::Sphere;
	cfg.radius = 0.0f;
	cfg.startLifetimeMin = 0.2f;
	cfg.startLifetimeMax = 0.35f;
	cfg.startSpeedMin = 0.0f;
	cfg.startSpeedMax = 0.0f;
	cfg.startSizeMin = 0.12f;
	cfg.startSizeMax = 0.2f;
	cfg.startColorMin = { 1.0f, 1.0f, 1.0f, 1.0f };
	cfg.startColorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
	cfg.gravityModifier = 1.6f;
	cfg.colorOverLifetimeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
	cfg.colorOverLifetimeEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
	cfg.sizeOverLifetimeStart = 1.0f;
	cfg.sizeOverLifetimeEnd = 0.35f;
	cfg.simulationSpace = engine::ParticleSimulationSpace::World;
	cfg.maxParticles = 512;
	cfg.looping = false;
	cfg.billboardBlendMode = engine::ParticleBillboardBlendMode::Additive;
	cfg.texturePath = "rom/texture/particle.png";
	cfg.textureSheetFrames = 1;
	cfg.textureSheetCycles = 1.0f;

	m_emitterId = ScriptApi_ParticleCreateEmitter(cfg, { 0.0f, 0.0f, 0.0f });
	ScriptApi_ParticleSetEmitterEmitting(m_emitterId, false);
}

void ImpactSparkVfx::Spawn(const XMFLOAT3& position,
	const XMFLOAT3& normalXZ,
	bool charged) {
	EnsureEmitter();
	if (m_emitterId == 0)
		return;

	ScriptApi_ParticleSetEmitterPosition(m_emitterId, position);

	const int count = charged ? 40 : 22;
	const float speedMin = charged ? 8.0f : 4.0f;
	const float speedMax = charged ? 18.0f : 9.0f;
	const float sizeMin = charged ? 0.45f : 0.28f;
	const float sizeMax = charged ? 0.95f : 0.55f;
	const float lifeMin = charged ? 0.35f : 0.22f;
	const float lifeMax = charged ? 0.7f : 0.42f;

	// Overbright RGB so additive billboards punch through the scene.
	const XMFLOAT4 colorA = charged
		? XMFLOAT4{ 0.75f, 1.4f, 1.8f, 1.0f }
		: XMFLOAT4{ 1.6f, 1.35f, 0.55f, 1.0f };
	const XMFLOAT4 colorB = charged
		? XMFLOAT4{ 1.8f, 1.8f, 2.0f, 1.0f }
		: XMFLOAT4{ 1.8f, 1.0f, 0.35f, 1.0f };

	for (int i = 0; i < count; ++i) {
		const XMFLOAT3 dir = RandomSparkDirection(normalXZ);
		const float speed = RandRange(speedMin, speedMax);
		const XMFLOAT3 velocity = {
			dir.x * speed,
			dir.y * speed,
			dir.z * speed
		};
		const XMFLOAT4 color = LerpColor(colorA, colorB, Rand01());
		const float size = RandRange(sizeMin, sizeMax);
		const float lifetime = RandRange(lifeMin, lifeMax);

		// Slight positional jitter so the burst reads as volume, not a point.
		const float jitter = charged ? 0.28f : 0.16f;
		const XMFLOAT3 spawnPos = {
			position.x + RandRange(-jitter, jitter),
			position.y + RandRange(0.0f, jitter * 0.6f),
			position.z + RandRange(-jitter, jitter)
		};

		ScriptApi_ParticleSpawnAt(m_emitterId, spawnPos, velocity, color, size, lifetime);
	}
}

void ImpactSparkVfx::Reset() {
	if (m_emitterId != 0) {
		ScriptApi_ParticleDestroyEmitter(m_emitterId);
		m_emitterId = 0;
	}
}
