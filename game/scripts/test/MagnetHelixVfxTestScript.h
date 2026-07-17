#pragma once

#include "MagnetHelixPath.h"
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

class MagnetHelixVfxTestScript : public Script {
#define MAGNET_HELIX_VFX_TEST_SCRIPT_FIELDS(X) \
	X(float, helixAmpMin, 0.15f) \
	X(float, helixAmpMax, 0.6f) \
	X(float, helixTurnsMin, 2.0f) \
	X(float, helixTurnsMax, 6.5f) \
	X(float, helixRotateSpeed, 1.75f) \
	X(float, helixTravelTime, 1.2f) \
	X(float, helixSpawnInterval, 0.05f) \
	X(float, helixParticleSize, 0.3f)

public:
	MAGNET_HELIX_VFX_TEST_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

	void OnCreate(engine::GameObject& self) override;
	void OnDestroy(engine::GameObject& self) override;
	void OnUpdate(engine::GameObject& self, float dt) override;

	SCRIPT_SERIALIZE_METHODS(MAGNET_HELIX_VFX_TEST_SCRIPT_FIELDS)

private:
	void ResolveLinkEndpoints(engine::GameObject& self);
	void EnsureEmitter();
	void FireHelixSpawn();
	MagnetHelixPath::MagnetHelixSettings BuildSettings() const;

	engine::GameObject* m_source = nullptr;
	engine::GameObject* m_target = nullptr;
	std::uint32_t m_emitterId = 0;
	bool m_playing = false;
	float m_spawnAccumulator = 0.0f;
	float m_helixPhase = 0.0f;
	int m_nextStrand = 0;
};
