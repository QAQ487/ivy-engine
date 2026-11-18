// SampleParticleEmitterScript.cpp
// Sample script: SampleParticleEmitterScript implementation.

#include "SampleParticleEmitterScript.h"

#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <algorithm>

REGISTER_SCRIPT(SampleParticleEmitterScript)

void SampleParticleEmitterScript::OnCreate(engine::GameObject& self) {
    DirectX::XMFLOAT3 position = self.GetPosition();
    if (const engine::Transform* t = ScriptApi_FindTransform(self))
        position = t->GetWorldPosition();

    engine::ParticleEmitterConfig cfg;
    cfg.emissionRate = std::max(emissionRate, 0.0f);
    cfg.shape = engine::ParticleShape::Sphere;
    cfg.radius = std::max(spawnRadius, 0.0f);
    cfg.startLifetimeMin = std::max(lifetime * 0.8f, 0.1f);
    cfg.startLifetimeMax = std::max(lifetime, cfg.startLifetimeMin);
    cfg.startSpeedMin = std::max(startSpeed * 0.5f, 0.0f);
    cfg.startSpeedMax = std::max(startSpeed, cfg.startSpeedMin);
    cfg.startSizeMin = std::max(startSize * 0.7f, 0.01f);
    cfg.startSizeMax = std::max(startSize, cfg.startSizeMin);
    cfg.startColorMin = { colorR, colorG, colorB, 1.0f };
    cfg.startColorMax = { colorR, colorG, colorB, 1.0f };
    cfg.colorOverLifetimeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
    cfg.colorOverLifetimeEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
    cfg.sizeOverLifetimeStart = 1.0f;
    cfg.sizeOverLifetimeEnd = 0.4f;
    cfg.simulationSpace = engine::ParticleSimulationSpace::World;
    cfg.maxParticles = std::clamp(maxParticles, 1, engine::ParticlePool::kMaxParticles);
    cfg.looping = true;
    cfg.billboardBlendMode = engine::ParticleBillboardBlendMode::Additive;

    m_emitterId = ScriptApi_ParticleCreateEmitter(cfg, position);
}

void SampleParticleEmitterScript::OnDestroy(engine::GameObject& /*self*/) {
    if (m_emitterId != 0) {
        ScriptApi_ParticleDestroyEmitter(m_emitterId);
        m_emitterId = 0;
    }
}
