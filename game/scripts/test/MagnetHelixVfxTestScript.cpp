#include "MagnetHelixVfxTestScript.h"

#include "MagnetHelixPath.h"
#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <DirectXMath.h>

using namespace DirectX;
using namespace MagnetHelixPath;

REGISTER_SCRIPT(MagnetHelixVfxTestScript)

namespace {
constexpr float kSpawnT = 0.005f;
constexpr float kHeightBias = 1.0f;

XMFLOAT3 LinkAnchor(const engine::GameObject& obj) {
	XMFLOAT3 pos = obj.GetPosition();
	pos.y += kHeightBias;
	return pos;
}

engine::ParticleEmitterConfig BuildHelixEmitterConfig(const MagnetHelixSettings& settings) {
	engine::ParticleEmitterConfig cfg;
	cfg.emissionRate = 0.0f;
	cfg.burstCount = 0;
	cfg.shape = engine::ParticleShape::Sphere;
	cfg.radius = 0.0f;
	cfg.startLifetimeMin = settings.travelTime;
	cfg.startLifetimeMax = settings.travelTime;
	cfg.startSpeedMin = 0.0f;
	cfg.startSpeedMax = 0.0f;
	cfg.startSizeMin = settings.particleSize;
	cfg.startSizeMax = settings.particleSize;
	cfg.startColorMin = { 0.4f, 0.8f, 1.0f, 1.0f };
	cfg.startColorMax = { 0.4f, 0.8f, 1.0f, 1.0f };
	cfg.gravityModifier = 0.0f;
	cfg.colorOverLifetimeStart = { 0.4f, 0.8f, 1.0f, 1.0f };
	cfg.colorOverLifetimeEnd = { 0.4f, 0.8f, 1.0f, 0.0f };
	cfg.sizeOverLifetimeStart = 1.0f;
	cfg.sizeOverLifetimeEnd = 0.25f;
	cfg.simulationSpace = engine::ParticleSimulationSpace::World;
	cfg.maxParticles = 256;
	cfg.looping = false;
	cfg.billboardBlendMode = engine::ParticleBillboardBlendMode::Additive;
	cfg.texturePath = "rom/texture/particle.png";
	cfg.textureSheetFrames = 1;
	cfg.textureSheetCycles = 1.0f;
	return cfg;
}
} // namespace

MagnetHelixPath::MagnetHelixSettings MagnetHelixVfxTestScript::BuildSettings() const {
	MagnetHelixSettings settings;
	settings.ampMin = helixAmpMin;
	settings.ampMax = helixAmpMax;
	settings.turnsMin = helixTurnsMin;
	settings.turnsMax = helixTurnsMax;
	settings.rotateSpeed = helixRotateSpeed;
	settings.travelTime = helixTravelTime;
	settings.particleSize = helixParticleSize;
	return settings;
}

void MagnetHelixVfxTestScript::OnCreate(engine::GameObject& self) {
	ResolveLinkEndpoints(self);
	EnsureEmitter();
}

void MagnetHelixVfxTestScript::OnDestroy(engine::GameObject& self) {
	(void)self;
	m_playing = false;
	m_spawnAccumulator = 0.0f;
	m_helixPhase = 0.0f;
	m_nextStrand = 0;
	if (m_emitterId != 0) {
		ScriptApi_ParticleDestroyEmitter(m_emitterId);
		m_emitterId = 0;
	}
	m_source = nullptr;
	m_target = nullptr;
}

void MagnetHelixVfxTestScript::ResolveLinkEndpoints(engine::GameObject& self) {
	m_source = nullptr;
	m_target = nullptr;

	for (const auto& child : self.GetChildren()) {
		if (!child)
			continue;
		if (child->GetName() == "Player" || child->GetName() == "LinkSource")
			m_source = child.get();
		else if (child->GetName() == "LinkTarget")
			m_target = child.get();
	}
}

void MagnetHelixVfxTestScript::EnsureEmitter() {
	if (m_emitterId != 0)
		return;

	const MagnetHelixSettings settings = BuildSettings();
	const XMFLOAT3 origin = m_source ? LinkAnchor(*m_source) : XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	m_emitterId = ScriptApi_ParticleCreateEmitter(BuildHelixEmitterConfig(settings), origin);
	ScriptApi_ParticleSetEmitterEmitting(m_emitterId, false);
}

void MagnetHelixVfxTestScript::FireHelixSpawn() {
	if (!m_source || !m_target || m_emitterId == 0)
		return;

	const MagnetHelixSettings settings = BuildSettings();

	LinkFrame frame;
	if (!BuildLinkFrame(LinkAnchor(*m_source), LinkAnchor(*m_target), frame))
		return;

	const HelixParams helix = ComputeHelixParams(frame.linkLen, settings);
	const int strand = m_nextStrand;
	m_nextStrand = 1 - m_nextStrand;

	const XMFLOAT3 pos = EvalHelixPoint(frame, kSpawnT, strand, m_helixPhase, helix.amplitude, helix.turns);
	const float axialSpeed = frame.linkLen / settings.travelTime;
	const XMFLOAT3 velocity = EvalAxialVelocity(frame, axialSpeed);
	const float lifetime = (1.0f - kSpawnT) * settings.travelTime;
	const XMFLOAT4 color = { 0.4f, 0.8f, 1.0f, 1.0f };

	ScriptApi_ParticleSpawnAt(m_emitterId, pos, velocity, color, settings.particleSize, lifetime);
}

void MagnetHelixVfxTestScript::OnUpdate(engine::GameObject& self, float dt) {
	(void)self;
	if (KeyLogger_IsTrigger(KK_SPACE))
		m_playing = !m_playing;

	if (!m_playing)
		return;

	const MagnetHelixSettings settings = BuildSettings();
	m_helixPhase += settings.rotateSpeed * dt;

	m_spawnAccumulator += dt;
	while (m_spawnAccumulator >= helixSpawnInterval) {
		m_spawnAccumulator -= helixSpawnInterval;
		FireHelixSpawn();
	}
}
