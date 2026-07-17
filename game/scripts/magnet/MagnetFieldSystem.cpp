#include "MagnetFieldSystem.h"

#include "ImpactSparkVfx.h"
#include "MagnetLinkVfx.h"
#include "CenterPoleScript.h"
#include "engine/core/game_object.h"
#include "engine/physics/collider/collider.h"
#include "engine/scripting/script_runtime_api.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace {
float LengthXZ(float x, float z) {
	return sqrtf(x * x + z * z);
}

float SmoothStep01(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

float ColliderRadiusXZ(const engine::GameObject* obj) {
	if (!obj || !obj->GetCollider())
		return 0.0f;

	const Collider* collider = obj->GetCollider();
	if (const auto* sphere = dynamic_cast<const SphereCollider*>(collider))
		return sphere->GetRadius();
	if (const auto* capsule = dynamic_cast<const CapsuleCollider*>(collider))
		return capsule->GetRadius();
	if (const auto* cylinder = dynamic_cast<const CylinderCollider*>(collider))
		return cylinder->GetRadius();
	if (const auto* box = dynamic_cast<const BoxCollider*>(collider)) {
		const XMFLOAT3 size = ScriptApi_GetBoxColliderSize(*box);
		return 0.5f * (std::max)(size.x, size.z);
	}
	return 0.0f;
}
} // namespace

MagnetFieldSystem& MagnetFieldSystem::Instance() {
	static MagnetFieldSystem system;
	return system;
}

void MagnetFieldSystem::Tick(float dt) {
	MagnetLinkVfx::Instance().Tick(dt);
}

void MagnetFieldSystem::Reset() {
	CancelOrbit();
	MagnetLinkVfx::Instance().Reset();
	ImpactSparkVfx::Instance().Reset();
}

void MagnetFieldSystem::ShowMagnetLink(const engine::GameObject& source,
	const engine::GameObject& target,
	int type) {
	MagnetLinkVfx::Instance().Show(source, target, type);
}

void MagnetFieldSystem::HideMagnetLink() {
	MagnetLinkVfx::Instance().Hide();
}

void MagnetFieldSystem::SetConfig(const MagnetFieldConfig& config) {
	m_config = config;
	m_config.maxRange = (std::max)(0.01f, m_config.maxRange);
	m_config.attractAcceleration = (std::max)(0.0f, m_config.attractAcceleration);
	m_config.repulseAcceleration = (std::max)(0.0f, m_config.repulseAcceleration);
	m_config.attractMaxSpeed = (std::max)(0.0f, m_config.attractMaxSpeed);
	m_config.repulseMaxSpeed = (std::max)(0.0f, m_config.repulseMaxSpeed);
	m_config.attractFalloffDistance = (std::max)(0.01f, m_config.attractFalloffDistance);
	m_config.orbitMinRadius = (std::max)(0.01f, m_config.orbitMinRadius);
	m_config.orbitMaxRadius = (std::max)(m_config.orbitMinRadius, m_config.orbitMaxRadius);
	m_config.orbitStartSpeed = (std::max)(0.0f, m_config.orbitStartSpeed);
	m_config.orbitAcceleration = (std::max)(0.0f, m_config.orbitAcceleration);
	m_config.orbitRadiusAdjustSpeed = (std::max)(0.0f, m_config.orbitRadiusAdjustSpeed);
	m_config.orbitRadialSpring = (std::max)(0.0f, m_config.orbitRadialSpring);
	m_config.orbitRadialDamping = (std::max)(0.0f, m_config.orbitRadialDamping);
	m_config.orbitTangentialDamping = (std::max)(0.0f, m_config.orbitTangentialDamping);
}

float MagnetFieldSystem::ConsumeReleasedOrbitDuration() {
	const float duration = m_lastReleasedOrbitDuration;
	m_lastReleasedOrbitDuration = 0.0f;
	return duration;
}

XMFLOAT3 MagnetFieldSystem::GetOrbitFacingDirectionXZ() const {
	if (!m_isOrbiting)
		return { 0.0f, 0.0f, 0.0f };
	const float tangentX = -sinf(m_orbitAngle);
	const float tangentZ = cosf(m_orbitAngle);
	const float sign = (m_orbitAngularVelocity < 0.0f) ? -1.0f : 1.0f;
	return { tangentX * sign, 0.0f, tangentZ * sign };
}

void MagnetFieldSystem::ForceReleaseOrbit(engine::GameObject& player) {
	if (m_isOrbiting)
		ReleaseOrbit(player);
}

bool MagnetFieldSystem::TryGetTargetPosition(engine::GameObject* target, XMFLOAT3& outPosition) const {
	if (!target || !target->IsActive())
		return false;

	if (CenterPole_HasAimed() && target == CenterPole_GetAimedNode()) {
		outPosition = CenterPole_GetAimedPosition();
		return true;
	}

	outPosition = target->GetPosition();
	return true;
}

void MagnetFieldSystem::ApplyLinearMagnetForce(engine::GameObject& player,
	engine::GameObject* target,
	float acceleration,
	float maxSpeed,
	float sign) {
	XMFLOAT3 targetPos = {};
	if (!TryGetTargetPosition(target, targetPos))
		return;

	const XMFLOAT3 playerPos = player.GetPosition();
	const float dx = targetPos.x - playerPos.x;
	const float dz = targetPos.z - playerPos.z;
	const float distance = LengthXZ(dx, dz);
	if (distance < 0.001f || distance > m_config.maxRange)
		return;

	float forceScale = 1.0f;
	if (sign > 0.0f) {
		const float surfaceGap = (std::max)(0.0f,
			distance - ColliderRadiusXZ(&player) - ColliderRadiusXZ(target));
		forceScale = SmoothStep01(surfaceGap / m_config.attractFalloffDistance);
		if (forceScale <= 0.001f)
			return;
	}

	const float invDistance = 1.0f / distance;
	const float dirX = dx * invDistance * sign;
	const float dirZ = dz * invDistance * sign;

	const XMFLOAT3 velocity = player.GetRigidbody().GetVelocity();
	const float along = velocity.x * dirX + velocity.z * dirZ;
	const float targetSpeed = maxSpeed * forceScale;
	if (targetSpeed <= 0.001f)
		return;

	// Ramp from acceleration (at rest) down to targetSpeed (at max). Hold at target.
	const float rampAccel = (std::max)(acceleration, targetSpeed);
	const float t = std::clamp(along / targetSpeed, 0.0f, 1.0f);
	const float applied = rampAccel + (targetSpeed - rampAccel) * t;
	if (applied <= 0.001f)
		return;

	player.GetRigidbody().AddForce(
		{ dirX * applied, 0.0f, dirZ * applied },
		ForceMode::Acceleration);
}

void MagnetFieldSystem::Attract(engine::GameObject& player, engine::GameObject* target, float dt) {
	(void)dt;
	ApplyLinearMagnetForce(player, target,
		m_config.attractAcceleration, m_config.attractMaxSpeed, +1.0f);
}

void MagnetFieldSystem::Repulse(engine::GameObject& player, engine::GameObject* target, float dt) {
	(void)dt;
	ApplyLinearMagnetForce(player, target,
		m_config.repulseAcceleration, m_config.repulseMaxSpeed, -1.0f);
}

void MagnetFieldSystem::StartOrbit(engine::GameObject& player, engine::GameObject* target) {
	XMFLOAT3 targetPos = {};
	if (!TryGetTargetPosition(target, targetPos))
		return;

	const XMFLOAT3 playerPos = player.GetPosition();
	const float dx = playerPos.x - targetPos.x;
	const float dz = playerPos.z - targetPos.z;
	const float distance = LengthXZ(dx, dz);
	if (distance < m_config.orbitMinRadius || distance > m_config.maxRange)
		return;

	m_isOrbiting = true;
	m_orbitPlayer = &player;
	m_orbitTarget = target;
	m_orbitTargetIndex = CenterPole_GetAimedIndex();
	m_orbitTargetPosition = targetPos;
	m_orbitRadius = std::clamp(distance, m_config.orbitMinRadius, m_config.orbitMaxRadius);
	m_orbitAngle = atan2f(dz, dx);
	m_orbitHeldDuration = 0.0f;
	m_lastReleasedOrbitDuration = 0.0f;

	const float invDistance = 1.0f / distance;
	const float tangentX = -dz * invDistance;
	const float tangentZ = dx * invDistance;
	const XMFLOAT3 velocity = player.GetRigidbody().GetVelocity();
	const float currentTangential = velocity.x * tangentX + velocity.z * tangentZ;
	const float sign = (currentTangential < -0.1f) ? -1.0f : 1.0f;
	m_orbitTangentialSpeed = sign * m_config.orbitStartSpeed;
	m_orbitAngularVelocity = m_orbitTangentialSpeed / m_orbitRadius;
	m_orbitReleaseYVelocity = velocity.y;

	Rigidbody& rb = player.GetRigidbody();
	rb.SetVelocity({ 0.0f, 0.0f, 0.0f });
	rb.SetIsKinematic(true);
	CenterPole_SetOrbitIndex(m_orbitTargetIndex);
}

void MagnetFieldSystem::ReleaseOrbit(engine::GameObject& player) {
	const float tangentX = -sinf(m_orbitAngle);
	const float tangentZ = cosf(m_orbitAngle);

	Rigidbody& rb = player.GetRigidbody();
	rb.SetIsKinematic(false);
	XMFLOAT3 velocity = rb.GetVelocity();
	velocity.x = tangentX * m_orbitAngularVelocity * m_orbitRadius * m_config.orbitLaunchSpeedMult;
	velocity.y = m_orbitReleaseYVelocity;
	velocity.z = tangentZ * m_orbitAngularVelocity * m_orbitRadius * m_config.orbitLaunchSpeedMult;
	rb.SetVelocity(velocity);
	m_lastReleasedOrbitDuration = m_orbitHeldDuration;

	CancelOrbit();
}

void MagnetFieldSystem::Orbit(engine::GameObject& player, engine::GameObject* target,
	const MagnetOrbitInputState& inputState, float dt) {
	if (dt <= 0.0f)
		return;

	if (!m_isOrbiting && inputState.startRequested)
		StartOrbit(player, target);

	if (!m_isOrbiting)
		return;

	if (!inputState.held) {
		ReleaseOrbit(player);
		return;
	}

	if (!m_orbitTarget || !m_orbitTarget->IsActive()) {
		CancelOrbit();
		return;
	}

	TryGetTargetPosition(m_orbitTarget, m_orbitTargetPosition);

	m_orbitHeldDuration += dt;
	m_orbitRadius += inputState.radiusInput * m_config.orbitRadiusAdjustSpeed * dt;
	m_orbitRadius = std::clamp(m_orbitRadius, m_config.orbitMinRadius, m_config.orbitMaxRadius);

	// Accelerate in the spin direction chosen at StartOrbit (no A/D hold required).
	const float spinSign = (m_orbitTangentialSpeed < 0.0f) ? -1.0f : 1.0f;
	m_orbitTangentialSpeed += spinSign * m_config.orbitAcceleration * dt;
	m_orbitAngularVelocity = (m_orbitRadius > 0.001f)
		? (m_orbitTangentialSpeed / m_orbitRadius)
		: 0.0f;
	m_orbitAngle += m_orbitAngularVelocity * dt;

	const XMFLOAT3 playerPos = player.GetPosition();
	XMFLOAT3 orbitPos = playerPos;
	orbitPos.x = m_orbitTargetPosition.x + cosf(m_orbitAngle) * m_orbitRadius;
	orbitPos.z = m_orbitTargetPosition.z + sinf(m_orbitAngle) * m_orbitRadius;
	player.SetPosition(orbitPos);
}

void MagnetFieldSystem::CancelOrbit() {
	if (!m_isOrbiting)
		return;

	if (m_orbitPlayer)
		m_orbitPlayer->GetRigidbody().SetIsKinematic(false);
	m_isOrbiting = false;
	m_orbitPlayer = nullptr;
	m_orbitTarget = nullptr;
	m_orbitTargetIndex = -1;
	m_orbitRadius = 0.0f;
	m_orbitAngle = 0.0f;
	m_orbitHeldDuration = 0.0f;
	m_orbitAngularVelocity = 0.0f;
	m_orbitTangentialSpeed = 0.0f;
	m_orbitReleaseYVelocity = 0.0f;
	m_orbitTargetPosition = { 0.0f, 0.0f, 0.0f };
	CenterPole_SetOrbitIndex(-1);
}
