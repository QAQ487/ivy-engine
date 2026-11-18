// SampleParticleEmitterScript.h
// Declares SampleParticleEmitterScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

#include <cstdint>

// Teaching / verification script: owns one particle emitter at the object's world
// position (untextured soft quads drifting outward). Used by the testbed
// particle-culling scene; shows how a script drives ScriptApi_Particle*.
class SampleParticleEmitterScript : public Script {
public:
#define SAMPLE_PARTICLE_FIELDS(X) \
    X(float, emissionRate, 20.0f) \
    X(float, lifetime, 5.0f) \
    X(float, startSpeed, 1.5f) \
    X(float, startSize, 0.35f) \
    X(float, spawnRadius, 0.4f) \
    X(float, colorR, 1.0f) \
    X(float, colorG, 0.6f) \
    X(float, colorB, 0.2f) \
    X(int, maxParticles, 128)

    SAMPLE_PARTICLE_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_PARTICLE_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnDestroy(engine::GameObject& self) override;

private:
    std::uint32_t m_emitterId = 0;
};
