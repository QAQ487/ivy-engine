#pragma once

#include "engine/scene/scene_node.h"

#include <DirectXMath.h>
#include <cstdint>

struct MagnetOrbitInputState {
	bool startRequested = false;
	bool held = false;
	bool pullHeld = false;
	bool pushHeld = false;
	float radiusInput = 0.0f;
};

struct MagnetFieldConfig {
	float maxRange = 47.0f;
	float attractAcceleration = 100.0f;
	float repulseAcceleration = 80.0f;
	float attractMaxSpeed = 20.0f;
	float repulseMaxSpeed = 18.0f;
	float attractFalloffDistance = 0.5f;
	float orbitMinRadius = 0.5f;
	float orbitMaxRadius = 80.0f;
	float orbitStartSpeed = 7.0f;
	float orbitAcceleration = 5.0f;
	float orbitLaunchSpeedMult = 5.0f;
	float orbitRadiusAdjustSpeed = 12.0f;
	float orbitRadialSpring = 35.0f;
	float orbitRadialDamping = 10.0f;
	float orbitTangentialDamping = 8.0f;
};

class MagnetFieldSystem {
public:
	static MagnetFieldSystem& Instance();

	void Tick(float dt);
	void Reset();
	void SetConfig(const MagnetFieldConfig& config);

	void Attract(engine::GameObject& player, engine::GameObject* target, float dt);
	void Repulse(engine::GameObject& player, engine::GameObject* target, float dt);
	void ShowMagnetLink(const engine::GameObject& source,
		const engine::GameObject& target,
		int type);
	void HideMagnetLink();
	void Orbit(engine::GameObject& player, engine::GameObject* target,
		const MagnetOrbitInputState& inputState, float dt);

	bool IsOrbiting() const { return m_isOrbiting; }
	int GetOrbitTargetIndex() const { return m_orbitTargetIndex; }
	DirectX::XMFLOAT3 GetOrbitFacingDirectionXZ() const;
	float ConsumeReleasedOrbitDuration();
	void ForceReleaseOrbit(engine::GameObject& player);
	void CancelOrbit();

private:
	MagnetFieldSystem() = default;

	bool TryGetTargetPosition(engine::GameObject* target, DirectX::XMFLOAT3& outPosition) const;
	void ApplyLinearMagnetForce(engine::GameObject& player, engine::GameObject* target,
		float acceleration, float maxSpeed, float sign);
	void StartOrbit(engine::GameObject& player, engine::GameObject* target);
	void ReleaseOrbit(engine::GameObject& player);

	bool m_isOrbiting = false;
	engine::GameObject* m_orbitPlayer = nullptr;
	engine::GameObject* m_orbitTarget = nullptr;
	int m_orbitTargetIndex = -1;
	float m_orbitRadius = 0.0f;
	float m_orbitAngle = 0.0f;
	float m_orbitHeldDuration = 0.0f;
	float m_lastReleasedOrbitDuration = 0.0f;
	float m_orbitAngularVelocity = 0.0f;
	float m_orbitTangentialSpeed = 0.0f;
	float m_orbitReleaseYVelocity = 0.0f;
	MagnetFieldConfig m_config;
	DirectX::XMFLOAT3 m_orbitTargetPosition = { 0.0f, 0.0f, 0.0f };
};
