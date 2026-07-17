#include "MagnetLinkVfx.h"

#include "engine/core/game_object.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/scripting/script_runtime_api.h"

#include <cmath>

using namespace DirectX;
using namespace MagnetHelixPath;

namespace {
constexpr float kLinkParticleLifetime = 20.0f;
constexpr float kLinkParticleSize = 0.22f;

XMFLOAT3 LinkAnchor(const engine::GameObject& obj, float heightBias) {
	XMFLOAT3 pos = obj.GetPosition();
	pos.y += heightBias;
	return pos;
}

XMFLOAT4 ScaleColor(const XMFLOAT4& baseColor, float intensity) {
	return {
		baseColor.x * intensity,
		baseColor.y * intensity,
		baseColor.z * intensity,
		baseColor.w * intensity
	};
}

XMFLOAT4 MagnetLinkBaseColor(int linkType) {
	switch (linkType) {
		case 1: return { 0.35f, 0.85f, 1.0f, 1.0f };
		case 2: return { 1.0f, 0.4f, 0.15f, 1.0f };
		case 3: return { 0.35f, 1.0f, 0.5f, 1.0f };
		default: return { 1.0f, 1.0f, 1.0f, 1.0f };
	}
}

XMFLOAT3 FrameCarryVelocity(const XMFLOAT3& cur, const XMFLOAT3& prev, float dt) {
	if (dt <= 0.0f)
		return { 0.0f, 0.0f, 0.0f };
	return {
		(cur.x - prev.x) / dt,
		(cur.y - prev.y) / dt,
		(cur.z - prev.z) / dt
	};
}

XMFLOAT3 EvalSpawnVelocity(const LinkFrame& frame,
	const LinkFrame* prevFrame,
	float t,
	int strand,
	float phase,
	float prevPhase,
	float amplitude,
	float turns,
	float prevAmplitude,
	float prevTurns,
	float dt) {
	if (!prevFrame)
		return { 0.0f, 0.0f, 0.0f };

	const XMFLOAT3 pos = EvalHelixPoint(frame, t, strand, phase, amplitude, turns);
	const XMFLOAT3 prevPos = EvalHelixPoint(
		*prevFrame, t, strand, prevPhase, prevAmplitude, prevTurns);
	return FrameCarryVelocity(pos, prevPos, dt);
}
} // namespace

MagnetLinkVfx& MagnetLinkVfx::Instance() {
	static MagnetLinkVfx vfx;
	return vfx;
}

void MagnetLinkVfx::Tick(float dt) {
	if (dt <= 0.0f || !m_showMagnetLink || m_linkEmitterId == 0)
		return;

	if (m_magnetLinkSource && m_magnetLinkTarget) {
		m_magnetLinkStart = LinkAnchor(*m_magnetLinkSource, m_settings.heightBias);
		m_magnetLinkEnd = LinkAnchor(*m_magnetLinkTarget, m_settings.heightBias);
		ScriptApi_ParticleSetEmitterPosition(m_linkEmitterId, {
			(m_magnetLinkStart.x + m_magnetLinkEnd.x) * 0.5f,
			(m_magnetLinkStart.y + m_magnetLinkEnd.y) * 0.5f,
			(m_magnetLinkStart.z + m_magnetLinkEnd.z) * 0.5f
		});
	}

	const float phase = m_magnetLinkPhase;
	m_magnetLinkPhase += m_settings.rotateSpeed * dt;

	LinkFrame frame;
	if (!BuildLinkFrame(m_magnetLinkStart, m_magnetLinkEnd, frame))
		return;

	const HelixParams helix = ComputeHelixParams(frame.linkLen, m_settings);
	const HelixParams prevHelix = m_hasPrevFrame
		? ComputeHelixParams(m_prevLinkFrame.linkLen, m_settings)
		: helix;
	const LinkFrame* prevFrame = m_hasPrevFrame ? &m_prevLinkFrame : nullptr;
	const XMFLOAT4 baseColor = MagnetLinkBaseColor(m_magnetLinkType);

	const int pointCount = (std::max)(2, m_settings.skeletonPointCount);
	for (int strand = 0; strand < 2; ++strand) {
		for (int i = 0; i < pointCount; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
			const XMFLOAT3 pos = EvalHelixPoint(frame, t, strand, phase, helix.amplitude, helix.turns);
			const XMFLOAT3 velocity = EvalSpawnVelocity(
				frame, prevFrame, t, strand, phase, m_prevPhase,
				helix.amplitude, helix.turns, prevHelix.amplitude, prevHelix.turns, dt);
			const float intensity = HelixEndIntensity(t);
			const XMFLOAT4 col = ScaleColor(baseColor, 0.35f + 0.65f * intensity);
			ScriptApi_ParticleSpawnAt(
				m_linkEmitterId, pos, velocity, col,
				m_settings.skeletonParticleSize, m_settings.skeletonLifetime);
		}
	}

	m_prevLinkFrame = frame;
	m_prevPhase = phase;
	m_hasPrevFrame = true;
}

void MagnetLinkVfx::Reset() {
	Hide();
	m_linkEmitterId = 0;
	m_magnetLinkPhase = 0.0f;
	m_helixRefreshTimer = 0.0f;
	m_hasPrevFrame = false;
}

void MagnetLinkVfx::Show(const engine::GameObject& source,
	const engine::GameObject& target,
	int type) {
	Show(LinkAnchor(source, m_settings.heightBias), LinkAnchor(target, m_settings.heightBias), type);
	m_magnetLinkSource = &source;
	m_magnetLinkTarget = &target;
}

void MagnetLinkVfx::Show(const XMFLOAT3& from, const XMFLOAT3& to, int type) {
	m_magnetLinkSource = nullptr;
	m_magnetLinkTarget = nullptr;

	const int clampedType = (type == 1 || type == 2 || type == 3) ? type : 0;
	if (clampedType == 0) {
		Hide();
		return;
	}

	if (m_magnetLinkType != clampedType)
		m_magnetLinkPhase = 0.0f;

	m_magnetLinkType = clampedType;
	m_showMagnetLink = true;
	m_hasPrevFrame = false;
	m_magnetLinkStart = from;
	m_magnetLinkEnd = to;

	if (m_linkEmitterId == 0) {
		engine::ParticleEmitterConfig cfg;
		cfg.emissionRate = 0.0f;
		cfg.burstCount = 0;
		cfg.shape = engine::ParticleShape::Sphere;
		cfg.radius = 0.0f;
		cfg.startLifetimeMin = kLinkParticleLifetime;
		cfg.startLifetimeMax = kLinkParticleLifetime;
		cfg.startSpeedMin = 0.0f;
		cfg.startSpeedMax = 0.0f;
		cfg.startSizeMin = kLinkParticleSize;
		cfg.startSizeMax = kLinkParticleSize;
		cfg.startColorMin = { 1.0f, 1.0f, 1.0f, 1.0f };
		cfg.startColorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
		cfg.gravityModifier = 0.0f;
		cfg.colorOverLifetimeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
		cfg.colorOverLifetimeEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
		cfg.sizeOverLifetimeStart = 1.0f;
		cfg.sizeOverLifetimeEnd = 0.85f;
		cfg.simulationSpace = engine::ParticleSimulationSpace::World;
		cfg.maxParticles = 768;
		cfg.looping = false;
		cfg.billboardBlendMode = engine::ParticleBillboardBlendMode::Additive;
		cfg.texturePath = "rom/texture/particle.png";
		cfg.textureSheetFrames = 1;
		cfg.textureSheetCycles = 1.0f;

		const XMFLOAT3 midpoint = {
			(m_magnetLinkStart.x + m_magnetLinkEnd.x) * 0.5f,
			(m_magnetLinkStart.y + m_magnetLinkEnd.y) * 0.5f,
			(m_magnetLinkStart.z + m_magnetLinkEnd.z) * 0.5f
		};
		m_linkEmitterId = ScriptApi_ParticleCreateEmitter(cfg, midpoint);
		ScriptApi_ParticleSetEmitterEmitting(m_linkEmitterId, false);
	}
	else {
		ScriptApi_ParticleSetEmitterPosition(m_linkEmitterId, {
			(m_magnetLinkStart.x + m_magnetLinkEnd.x) * 0.5f,
			(m_magnetLinkStart.y + m_magnetLinkEnd.y) * 0.5f,
			(m_magnetLinkStart.z + m_magnetLinkEnd.z) * 0.5f
		});
		ScriptApi_ParticleSetEmitterEmitting(m_linkEmitterId, false);
	}
}

void MagnetLinkVfx::Hide() {
	m_showMagnetLink = false;
	m_magnetLinkType = 0;
	m_magnetLinkSource = nullptr;
	m_magnetLinkTarget = nullptr;
	m_hasPrevFrame = false;
}
