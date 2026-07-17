/*****************************************************
 * File: game/scripts/PlayerScript.cpp
 * Project: d3d11_engine
 *****************************************************/
#include "PlayerColor.h"
#include "PlayerScript.h"
#include "GameMode.h"
#include "CenterPoleScript.h"
#include "ChaseTestEnemyScript.h"
#include "EnemyAiScript.h"
#include "../magnet/MagnetFieldSystem.h"
#include "../magnet/ImpactSparkVfx.h"
#include "../magnet/MagnetLinkVfx.h"
#include "../ui/LockOnUI.h"
#include "../ui/PlayerNametag.h"
#include "RoundManagerScript.h"
#include "ItemWorldApi.h"
#include "engine/core/game_object.h"
#include "engine/ecs/collider_component.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/key_logger.h"
#include "engine/input/controller.h"
#include "third_person_camera.h"
#include "scene_manager.h"
#include "engine/physics/collider/collider.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/material_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/core/engine_gui.h"
#include "camera.h"
#include "engine/ui/scene_canvas.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

REGISTER_SCRIPT(PlayerScript)

static XMFLOAT4 PlayerEmissionGlow() {
	return PlayerColor_GetEmissionGlow(PlayerColor_GetLobbySlot(0));
}

static XMFLOAT3 PlayerTintRgb() {
	const XMFLOAT4 tint = PlayerColor_GetTint(PlayerColor_GetLobbySlot(0));
	return {tint.x, tint.y, tint.z};
}

static void SyncPlayerVisualAppearance(engine::GameObject& self, bool stunActive) {
	if (auto* mat = self.GetComponent<MaterialComponent>())
		mat->color = PlayerTintRgb();
	if (auto* vc = ScriptApi_FindVisualComponent(self)) {
		vc->emissionGlow = stunActive
			? XMFLOAT4{1.0f, 0.92f, 0.15f, 1.0f}
			: XMFLOAT4{0.0f, 0.0f, 0.0f, 0.0f};
	}
}

static constexpr float kJumpForce               = 10.0f;
static constexpr float kImpactForce             = 15.0f;
static constexpr float kImpactLiftForce         = 1.0f;
static constexpr float kImpactCooldown          = 2.0f;
static constexpr float kPlayerKnockbackImpulse  = 6.0f;
static constexpr float kKnockbackDuration       = 0.45f;
static constexpr float kStunSpinSpeed             = 720.0f;
static constexpr float kChargeImpactForce       = 40.0f;
static constexpr float kChargeImpactLift        = 6.0f;
static constexpr float kChargePlayerRecoilScale = 0.3f;
static constexpr float kChargeKnockbackDuration = 0.55f;
static constexpr float kChargeMinApproachSpeed  = 5.0f;
static constexpr float kChargeKnockbackCooldown = 3.0f;
static constexpr float kLocomotionSpeedSqThreshold = 0.25f;
static constexpr float kDanceDuration              = 1.0f;
static constexpr float kOrbitLTActivationThresh    = 0.5f;
static constexpr float kFacingLerpSpeed            = 14.0f;

namespace {
	static PlayerScript* g_playerForMagnetGlow = nullptr;
	static GameObject* g_playerBody = nullptr;
	static std::unordered_set<GameObject*> g_chargeImpactBodiesThisFrame;

	static bool HostShouldSuppressAimGlow(const engine::GameObject* node) {
		if (!g_playerForMagnetGlow || !node) return false;
		return g_playerForMagnetGlow->IsEnemyMagnetCooldownActive(node);
	}
} // namespace

GameObject* PlayerScript_GetBody() {
	return g_playerBody;
}

void PlayerScript_ResetSessionState() {
	MagnetFieldSystem::Instance().Reset();
	if (g_playerForMagnetGlow) {
		CenterPole_SetShouldSuppressAimGlow(nullptr);
		LockOnUI::SetTarget(nullptr, LockOnTargetState::None);
		ThirdPersonCamera_SetTarget(nullptr, nullptr);
	}
	g_playerForMagnetGlow = nullptr;
	g_playerBody = nullptr;
}

bool PlayerScript_IsRespawning() {
	return g_playerForMagnetGlow && g_playerForMagnetGlow->IsRespawning();
}

void PlayerScript_BeginRespawnAt(const DirectX::XMFLOAT3& holdPosition) {
	if (g_playerForMagnetGlow)
		g_playerForMagnetGlow->BeginRespawnAt(holdPosition);
}

void PlayerScript_TickRespawnHold() {
	if (g_playerForMagnetGlow)
		g_playerForMagnetGlow->TickRespawnHold();
}

void PlayerScript_EndRespawn() {
	if (g_playerForMagnetGlow)
		g_playerForMagnetGlow->EndRespawn();
}

bool PlayerScript_TryGetSpawnPosition(DirectX::XMFLOAT3& outPosition) {
	if (!g_playerForMagnetGlow)
		return false;
	outPosition = g_playerForMagnetGlow->GetSpawnPosition();
	return true;
}

void PlayerScript::BeginRespawnAt(const XMFLOAT3& holdPosition) {
	if (!g_playerBody) return;

	m_respawnHoldPosition = holdPosition;
	m_isRespawning = true;
	CancelOrbit();

	g_playerBody->GetRigidbody().SetUseGravity(false);
	g_playerBody->GetRigidbody().SetVelocity({ 0.0f, 0.0f, 0.0f });
	g_playerBody->SetPosition(m_respawnHoldPosition);
	SyncTransformFromPhysics(*g_playerBody);
}

void PlayerScript::TickRespawnHold() {
	if (!g_playerBody || !m_isRespawning) return;

	g_playerBody->GetRigidbody().SetUseGravity(false);
	g_playerBody->GetRigidbody().SetVelocity({ 0.0f, 0.0f, 0.0f });
	g_playerBody->SetPosition(m_respawnHoldPosition);
	SyncTransformFromPhysics(*g_playerBody);
}

void PlayerScript::EndRespawn() {
	if (!g_playerBody) return;

	g_playerBody->SetPosition(m_respawnHoldPosition);
	g_playerBody->GetRigidbody().SetVelocity({ 0.0f, 0.0f, 0.0f });
	g_playerBody->GetRigidbody().SetUseGravity(true);

	m_isRespawning = false;
	m_controlState = PlayerControlState::Normal;
	m_knockbackTimer = 0.0f;
	m_moveSpeedScale = 1.0f;
	m_knockbackGroundStick = false;
	m_stunImmobilize = false;
	m_stunSpinAngle = 0.0f;
	m_enemyMagnetCooldown.clear();
	m_enemyContactSet.clear();
	m_chargeKnockbackCooldownRemaining = 0.0f;
	m_magneticEnergy = (std::max)(0.01f, magnetEnergyMax);
	m_magneticOverheated = false;
	m_postOrbitShieldTimer = 0.0f;
	SyncChargeKnockbackCooldownUI();
	SyncMagneticEnergyUI();
	SyncTransformFromPhysics(*g_playerBody);
	if (g_playerBody)
		SyncPlayerVisualAppearance(*g_playerBody, false);
}

void PlayerScript_BeginKnockback(float durationSeconds) {
	if (g_playerForMagnetGlow)
		g_playerForMagnetGlow->BeginKnockback(durationSeconds, false);
}

void PlayerScript_BeginKnockbackForBody(GameObject* body, float durationSeconds) {
	if (body && body == g_playerBody)
		PlayerScript_BeginKnockback(durationSeconds);
}

void PlayerScript_BeginStunForBody(GameObject* body, float durationSeconds) {
	if (body && body == g_playerBody && g_playerForMagnetGlow)
		g_playerForMagnetGlow->BeginStun(durationSeconds);
}

void PlayerScript_SetMoveSpeedScaleForBody(GameObject* body, float scale) {
	if (!body || body != g_playerBody || !g_playerForMagnetGlow) return;
	g_playerForMagnetGlow->SetMoveSpeedScale(scale);
}

bool PlayerScript_IsKnockbackActiveForBody(GameObject* body) {
	return body && body == g_playerBody && g_playerForMagnetGlow
		&& g_playerForMagnetGlow->IsKnockbackActive();
}

void PlayerScript_ScaleKnockbackVelocityForBody(GameObject* body, float scale) {
	if (!body || body != g_playerBody || !g_playerForMagnetGlow) return;
	g_playerForMagnetGlow->ScaleKnockbackVelocity(scale);
}

void PlayerScript_SetKnockbackGroundStickForBody(GameObject* body, bool enabled) {
	if (!body || body != g_playerBody || !g_playerForMagnetGlow) return;
	g_playerForMagnetGlow->SetKnockbackGroundStick(enabled);
}

void PlayerScript_ApplyKnockbackImpulseForBody(GameObject* body,
	DirectX::XMFLOAT3 impulse,
	float stunDurationSeconds) {
	if (!body || body != g_playerBody || !g_playerForMagnetGlow) return;
	g_playerForMagnetGlow->BeginKnockback(stunDurationSeconds, false);
	body->GetRigidbody().AddForce(impulse, ForceMode::Impulse);
}

void PlayerScript_ApplyChargeKnockbackToBody(GameObject* body,
	const DirectX::XMFLOAT3& dirFromAttackerToBody,
	float horizontalForce,
	float lift,
	float stunDurationSeconds) {
	if (!body || body != g_playerBody || !g_playerForMagnetGlow) return;

	float dirLenSq = dirFromAttackerToBody.x * dirFromAttackerToBody.x
		+ dirFromAttackerToBody.z * dirFromAttackerToBody.z;
	XMFLOAT3 dir = { 0.0f, 0.0f, 1.0f };
	if (dirLenSq > 0.0001f) {
		const float invLen = 1.0f / sqrtf(dirLenSq);
		dir = { dirFromAttackerToBody.x * invLen, 0.0f, dirFromAttackerToBody.z * invLen };
	}

	const float horiz = (std::max)(0.0f, horizontalForce);
	const float useLift = (std::max)(0.0f, lift);

	g_playerForMagnetGlow->BeginKnockback(stunDurationSeconds, false);
	Rigidbody& rb = body->GetRigidbody();
	if (!rb.IsKinematic() && rb.GetMass() > 0.0f) {
		rb.AddForce(
			{ dir.x * horiz, useLift, dir.z * horiz },
			ForceMode::VelocityChange);
	}
}

bool PlayerScript_IsMagneticChargeActive() {
	return g_playerForMagnetGlow && g_playerForMagnetGlow->IsMagneticChargeActive();
}

bool PlayerScript_WasChargeImpactAppliedToBody(const GameObject* body) {
	if (!body) return false;
	return g_chargeImpactBodiesThisFrame.count(const_cast<GameObject*>(body)) > 0;
}

bool PlayerScript::IsMagneticChargeActive() const {
	if (m_magneticOverheated)
		return false;
	if (m_isRespawning || m_isOrbiting)
		return false;
	if (m_controlState != PlayerControlState::Normal)
		return false;
	if (m_postOrbitShieldTimer > 0.0f)
		return true;
	if (m_chargeKnockbackCooldownRemaining > 0.0f)
		return false;
	if (IsAimedTargetLocked())
		return false;
	if (!CenterPole_HasAimed())
		return false;
	if (Mouse_IsLeftDown())
		return true;
	return Controller_GetRightTrigger() > 0.5f;
}

void PlayerScript::BindChargeKnockbackCooldownUI() {
	UICanvas* canvas = SceneCanvas_GetCanvas();
	if (!canvas)
		return;

	m_chargeKnockbackCooldownSprite =
		ScriptApi_UICanvasGetSprite(*canvas, kChargeKnockbackCooldownSpriteName);
	if (!m_chargeKnockbackCooldownSprite)
		return;

	ScriptApi_UISpriteSetFillNone(*m_chargeKnockbackCooldownSprite);
	m_chargeKnockbackCooldownSprite->SetVisible(true);
}

void PlayerScript::TickChargeKnockbackCooldown(float dt) {
	if (m_chargeKnockbackCooldownRemaining <= 0.0f)
		return;
	m_chargeKnockbackCooldownRemaining -= dt;
	if (m_chargeKnockbackCooldownRemaining < 0.0f)
		m_chargeKnockbackCooldownRemaining = 0.0f;
}

void PlayerScript::SyncChargeKnockbackCooldownUI() {
	if (!m_chargeKnockbackCooldownSprite)
		BindChargeKnockbackCooldownUI();
	if (!m_chargeKnockbackCooldownSprite)
		return;

	const float maxCd = (std::max)(0.01f, kChargeKnockbackCooldown);
	if (m_chargeKnockbackCooldownRemaining <= 0.0f) {
		m_chargeKnockbackCooldownSprite->SetVisible(true);
		ScriptApi_UISpriteSetFillNone(*m_chargeKnockbackCooldownSprite);
		return;
	}

	const float elapsed = 1.0f - (m_chargeKnockbackCooldownRemaining / maxCd);
	const float fill = (std::max)(0.0f, (std::min)(1.0f, elapsed));
	m_chargeKnockbackCooldownSprite->SetVisible(true);
	ScriptApi_UISpriteSetRadial360Fill(*m_chargeKnockbackCooldownSprite, fill, false);
}

void PlayerScript::BeginChargeKnockbackCooldown() {
	m_chargeKnockbackCooldownRemaining = (std::max)(0.0f, kChargeKnockbackCooldown);
	SyncChargeKnockbackCooldownUI();
}

void PlayerScript::BindMagneticEnergyUI() {
	UICanvas* canvas = SceneCanvas_GetCanvas();
	if (!canvas)
		return;

	if (!m_magneticEnergyBar) {
		UIElement* element = ScriptApi_UICanvasGetElement(*canvas, kMagneticEnergyBarName);
		m_magneticEnergyBar = element ? dynamic_cast<UIBar*>(element) : nullptr;
		if (m_magneticEnergyBar)
			m_magneticEnergyBar->SetMaxValue((std::max)(0.01f, magnetEnergyMax));
	}

	if (!m_hudEnergyBarSprite)
		m_hudEnergyBarSprite = ScriptApi_UICanvasGetSprite(*canvas, kHudEnergyBarSpriteName);

	if (m_magneticEnergyBar)
		m_magneticEnergyBar->SetVisible(true);
	if (m_hudEnergyBarSprite)
		m_hudEnergyBarSprite->SetVisible(true);

	SyncMagneticEnergyUI();
}

void PlayerScript::EnterMagneticOverheat() {
	m_magneticOverheated = true;
	m_magneticEnergy = 0.0f;
	if (m_isOrbiting && g_playerBody) {
		MagnetFieldSystem::Instance().ForceReleaseOrbit(*g_playerBody);
		(void)MagnetFieldSystem::Instance().ConsumeReleasedOrbitDuration();
		m_isOrbiting = false;
		m_orbitPoleIndex = -1;
	}
	m_postOrbitShieldTimer = 0.0f;
	if (MagnetFieldSystem::Instance().IsOrbiting())
		CancelOrbit();
}

bool PlayerScript::CanUseMagnetic() const {
	return !m_magneticOverheated;
}

MagnetFieldConfig PlayerScript::BuildMagnetFieldConfig() const {
	MagnetFieldConfig config;
	config.maxRange = magnetMaxRange;
	config.attractAcceleration = magnetAttractStrength;
	config.repulseAcceleration = magnetRepulseStrength;
	config.attractMaxSpeed = magnetAttractMaxSpeed;
	config.repulseMaxSpeed = magnetRepulseMaxSpeed;
	config.attractFalloffDistance = magnetAttractFalloffDistance;
	config.orbitMinRadius = magnetOrbitMinRadius;
	config.orbitMaxRadius = magnetOrbitMaxRadius;
	config.orbitStartSpeed = magnetOrbitStartSpeed;
	config.orbitAcceleration = magnetOrbitAcceleration;
	config.orbitRadiusAdjustSpeed = magnetOrbitRadiusAdjustSpeed;
	config.orbitRadialSpring = magnetOrbitRadialSpring;
	config.orbitRadialDamping = magnetOrbitRadialDamping;
	config.orbitTangentialDamping = magnetOrbitTangentialDamping;
	return config;
}

MagnetHelixPath::MagnetHelixSettings PlayerScript::BuildMagnetHelixSettings() const {
	MagnetHelixPath::MagnetHelixSettings settings;
	settings.ampMin = helixAmpMin;
	settings.ampMax = helixAmpMax;
	settings.turnsMin = helixTurnsMin;
	settings.turnsMax = helixTurnsMax;
	settings.rotateSpeed = helixRotateSpeed;
	settings.travelTime = helixTravelTime;
	settings.particleSize = helixParticleSize;
	return settings;
}

bool PlayerScript::IsMagneticPullInputActive() const {
	return Mouse_IsLeftDown() || Controller_GetRightTrigger() > 0.5f;
}

bool PlayerScript::IsMagneticPushInputActive() const {
	return Mouse_IsRightDown() || Controller_IsPressed(ControllerButton::RB);
}

bool PlayerScript::IsMagneticOrbitHeld() const {
	if (!m_isOrbiting)
		return false;
	const Keyboard_State* kb = Keyboard_GetState();
	return kb->E || Mouse_IsMiddleDown()
		|| Controller_GetLeftTrigger() > kOrbitLTActivationThresh;
}

bool PlayerScript::IsMagneticEnergyDraining() const {
	if (!CanUseMagnetic())
		return false;
	if (m_controlState == PlayerControlState::Knockback)
		return false;
	if (IsMagneticOrbitHeld())
		return true;
	if (!CenterPole_HasAimed() || IsAimedTargetLocked())
		return false;
	if (IsMagneticPullInputActive() || IsMagneticPushInputActive())
		return true;
	return false;
}

void PlayerScript::TickMagneticEnergy(float dt) {
	if (dt <= 0.0f)
		return;

	const float maxEnergy = (std::max)(0.01f, magnetEnergyMax);
	const float drainPerSecond = (std::max)(0.0f, magnetEnergyDrainPerSecond);
	const float regenPerSecond = (std::max)(0.0f, magnetEnergyRegenPerSecond);
	const float unlockRatio = std::clamp(magnetOverheatUnlockRatio, 0.0f, 1.0f);
	const float unlockEnergy = maxEnergy * unlockRatio;

	if (m_magneticOverheated) {
		m_magneticEnergy += regenPerSecond * dt;
		if (m_magneticEnergy >= unlockEnergy)
			m_magneticOverheated = false;
		m_magneticEnergy = (std::min)(maxEnergy, m_magneticEnergy);
		return;
	}

	if (IsMagneticEnergyDraining()) {
		m_magneticEnergy -= drainPerSecond * dt;
		if (m_magneticEnergy <= 0.0f)
			EnterMagneticOverheat();
		else
			m_magneticEnergy = (std::max)(0.0f, m_magneticEnergy);
		return;
	}

	m_magneticEnergy += regenPerSecond * dt;
	m_magneticEnergy = (std::min)(maxEnergy, m_magneticEnergy);
}

void PlayerScript::SyncMagneticEnergyUI() {
	if (!m_magneticEnergyBar && !m_hudEnergyBarSprite)
		BindMagneticEnergyUI();

	const float maxEnergy = (std::max)(0.01f, magnetEnergyMax);
	const float fill = (std::max)(0.0f, (std::min)(1.0f, m_magneticEnergy / maxEnergy));

	if (m_magneticEnergyBar) {
		m_magneticEnergyBar->SetValue(m_magneticEnergy, maxEnergy);
		m_magneticEnergyBar->SetVisible(true);
	}
	if (m_hudEnergyBarSprite) {
		m_hudEnergyBarSprite->SetVisible(true);
		ScriptApi_UISpriteSetRadial360Fill(*m_hudEnergyBarSprite, fill, false);
	}
}

void PlayerScript::MarkChargeImpactApplied(GameObject* body) {
	if (body)
		g_chargeImpactBodiesThisFrame.insert(body);
}

bool PlayerScript::MeetsChargeApproachSpeed(const engine::GameObject& obj,
	const engine::GameObject& other,
	const XMFLOAT3& dirToOther) const {
	if (kChargeMinApproachSpeed <= 0.0f)
		return true;

	XMFLOAT3 vel = obj.GetRigidbody().GetVelocity();
	vel.y = 0.0f;
	const float approach = vel.x * dirToOther.x + vel.z * dirToOther.z;
	return approach >= kChargeMinApproachSpeed;
}

void PlayerScript::CancelOrbit() {
	MagnetFieldSystem::Instance().CancelOrbit();
	m_isOrbiting = false;
	m_orbitPoleIndex = -1;
	StopOrbitTrail();
}

engine::ParticleEmitterConfig PlayerScript::BuildOrbitTrailEmitterConfig() const {
	engine::ParticleEmitterConfig config;
	config.emissionRate = 72.0f;
	config.burstCount = 0;
	config.shape = engine::ParticleShape::Sphere;
	config.radius = 0.5f;
	config.startLifetimeMin = 0.3f;
	config.startLifetimeMax = 0.5f;
	config.startSpeedMin = 0.0f;
	config.startSpeedMax = 0.0f;
	config.startSizeMin = 0.5f;
	config.startSizeMax = 1.0f;
	const XMFLOAT4 glow = PlayerEmissionGlow();
	config.startColorMin = { glow.x * 0.75f, glow.y * 0.75f, glow.z * 0.75f, 0.65f };
	config.startColorMax = { glow.x, glow.y, glow.z, 0.95f };
	config.gravityModifier = 0.0f;
	config.colorOverLifetimeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
	config.colorOverLifetimeEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
	config.sizeOverLifetimeStart = 1.0f;
	config.sizeOverLifetimeEnd = 0.0f;
	config.simulationSpace = engine::ParticleSimulationSpace::World;
	config.maxParticles = 256;
	config.looping = true;
	config.texturePath = "rom/texture/particle.png";
	config.billboardBlendMode = engine::ParticleBillboardBlendMode::Additive;
	config.textureSheetFrames = 1;
	config.textureSheetCycles = 1.0f;
	return config;
}

void PlayerScript::EnsureOrbitTrailEmitter(engine::GameObject& obj) {
	const XMFLOAT3 position = obj.GetPosition();
	if (m_orbitTrailEmitterId == 0) {
		m_orbitTrailEmitterId = ScriptApi_ParticleCreateEmitter(BuildOrbitTrailEmitterConfig(), position);
	}
	else {
		ScriptApi_ParticleSetEmitterPosition(m_orbitTrailEmitterId, position);
		ScriptApi_ParticleSetEmitterEmitting(m_orbitTrailEmitterId, true);
	}
}

void PlayerScript::StopOrbitTrail(bool destroyImmediately) {
	if (m_orbitTrailEmitterId == 0)
		return;

	if (destroyImmediately)
		ScriptApi_ParticleDestroyEmitter(m_orbitTrailEmitterId);
	else
		ScriptApi_ParticleStopEmitter(m_orbitTrailEmitterId, true);
	m_orbitTrailEmitterId = 0;
}

void PlayerScript::EndStunVisual(engine::GameObject& self) {
	m_stunImmobilize = false;
	m_stunSpinAngle = 0.0f;
	self.m_rotation.y = m_facingYaw;
	SyncTransformFromPhysics(self);
	SyncPlayerVisualAppearance(self, false);
}

void PlayerScript::BeginKnockback(float durationSeconds, bool immobilize) {
	const float duration = (durationSeconds > 0.0f)
		? durationSeconds
		: (std::max)(0.05f, kKnockbackDuration);

	if (!immobilize && g_playerBody)
		EndStunVisual(*g_playerBody);

	m_controlState = PlayerControlState::Knockback;
	m_knockbackTimer = duration;
	m_stunImmobilize = immobilize;
	m_postOrbitShieldTimer = 0.0f;
	if (immobilize)
		m_stunSpinAngle = 0.0f;
	CancelOrbit();
	if (immobilize) {
		m_knockbackGroundStick = false;
		if (g_playerBody) {
			const XMFLOAT3 zero = { 0.0f, 0.0f, 0.0f };
			g_playerBody->GetRigidbody().SetVelocity(zero);
		}
	}
}

void PlayerScript::BeginStun(float durationSeconds) {
	BeginKnockback(durationSeconds, true);
}

void PlayerScript::SetMoveSpeedScale(float scale) {
	m_moveSpeedScale = (std::max)(0.05f, (std::min)(1.0f, scale));
}

void PlayerScript::ScaleKnockbackVelocity(float scale) {
	if (m_controlState != PlayerControlState::Knockback || !g_playerBody) return;
	const float clamped = (std::max)(0.0f, (std::min)(1.0f, scale));
	if (clamped >= 0.999f) return;

	Rigidbody& rb = g_playerBody->GetRigidbody();
	XMFLOAT3 vel = rb.GetVelocity();
	vel.x *= clamped;
	vel.z *= clamped;
	if (vel.y > 0.0f)
		vel.y *= clamped;
	rb.SetVelocity(vel);
}

void PlayerScript::SetKnockbackGroundStick(bool enabled) {
	m_knockbackGroundStick = enabled;
}

void PlayerScript::SyncStunVisualRotation(engine::GameObject& self) {
	const float spinY = m_facingYaw + m_stunSpinAngle;
	self.m_rotation.y = spinY;
	if (engine::Transform* t = ScriptApi_FindTransform(self)) {
		t->localPosition = self.GetPosition();
		t->localRotation.y = spinY;
	}
}

void PlayerScript::TickKnockbackState(engine::GameObject& self, float dt) {
	if (m_stunImmobilize) {
		self.GetRigidbody().SetVelocity({ 0.0f, 0.0f, 0.0f });
		const float spinSpeed = (std::max)(0.0f, kStunSpinSpeed);
		m_stunSpinAngle += spinSpeed * dt;
		if (m_stunSpinAngle >= 360.0f)
			m_stunSpinAngle = fmodf(m_stunSpinAngle, 360.0f);

		const XMFLOAT3 pos = self.GetPosition();
		ScriptApi_BillboardAdd(
			{ pos.x, pos.y + 1.1f, pos.z },
			{ 0.45f, 0.45f },
			{ 1.0f, 0.92f, 0.15f, 0.85f },
			nullptr,
			m_stunSpinAngle);
		SyncStunVisualRotation(self);
	}
	else if (m_knockbackGroundStick) {
		CheckGroundStatus(self);
		Rigidbody& rb = self.GetRigidbody();
		XMFLOAT3 vel = rb.GetVelocity();
		if (vel.y > 0.0f)
			vel.y *= 0.35f;
		if (m_isGrounded)
			vel.y = (std::min)(vel.y, 0.0f);
		rb.SetVelocity(vel);
		rb.AddForce({ 0.0f, -35.0f, 0.0f }, ForceMode::Acceleration);
		SyncFacingFromVelocity(self);
		UpdateFacingYaw(dt);
		self.m_rotation.y = m_facingYaw;
		SyncTransformFromPhysics(self);
	}
	else {
		SyncFacingFromVelocity(self);
		UpdateFacingYaw(dt);
		self.m_rotation.y = m_facingYaw;
		SyncTransformFromPhysics(self);
	}

	m_knockbackTimer -= dt;
	if (m_knockbackTimer <= 0.0f) {
		m_controlState = PlayerControlState::Normal;
		m_knockbackTimer = 0.0f;
		m_knockbackGroundStick = false;
		m_stunImmobilize = false;
		m_stunSpinAngle = 0.0f;
		self.m_rotation.y = m_facingYaw;
		SyncTransformFromPhysics(self);
	}
}

DirectX::XMFLOAT3 PlayerScript::GetForward(const engine::GameObject& /*obj*/) const {
	float yawRad = XMConvertToRadians(m_yaw);
	return { sinf(yawRad), 0.0f, cosf(yawRad) };
}

DirectX::XMFLOAT3 PlayerScript::GetRight(const engine::GameObject& /*obj*/) const {
	float yawRad = XMConvertToRadians(m_yaw);
	return { cosf(yawRad), 0.0f, -sinf(yawRad) };
}

void PlayerScript::SyncTransformFromPhysics(engine::GameObject& self) {
	if (engine::Transform* t = ScriptApi_FindTransform(self)) {
		t->localPosition = self.GetPosition();
		t->localRotation.y = m_facingYaw;
	}
}

void PlayerScript::OnCreate(engine::GameObject& self) {
	MagnetFieldSystem::Instance().Reset();
	m_yaw = 0.0f;
	m_facingYaw = 0.0f;
	m_targetFacingYaw = 0.0f;
	m_facingFromAccelerationThisFrame = false;
	m_isGrounded = false;
	m_isOrbiting = false;
	m_magnetAimFrozen = false;
	m_orbitPoleIndex = -1;
	m_orbitTrailEmitterId = 0;
	m_postOrbitShieldTimer = 0.0f;
	m_isRespawning = false;
	m_controlState = PlayerControlState::Normal;
	m_knockbackTimer = 0.0f;
	m_moveSpeedScale = 1.0f;
	m_knockbackGroundStick = false;
	m_stunImmobilize = false;
	m_stunSpinAngle = 0.0f;
	m_enemyMagnetCooldown.clear();
	m_enemyContactSet.clear();
	m_otherPlayerContactSet.clear();
	m_chargeKnockbackCooldownRemaining = 0.0f;
	m_magneticEnergy = (std::max)(0.01f, magnetEnergyMax);
	m_magneticOverheated = false;
	m_postOrbitShieldTimer = 0.0f;
	m_poleRegistryIndex = -1;
	g_chargeImpactBodiesThisFrame.clear();

	if (engine::Transform* t = ScriptApi_FindTransform(self))
		self.SetPosition(t->localPosition);
	else
		self.SetPosition({ 0.0f, 1.0f, 0.0f });

	self.SetScale({ 1.0f, 1.0f, 1.0f });

	// Collider is authored on the GameObject via ColliderComponent (editor GUI).
	if (auto* cc = ScriptApi_FindColliderComponent(self)) {
		ScriptApi_ApplyColliderComponent(*cc);
		self.m_debugCollider = cc->showDebugCollider;
	}

	self.GetRigidbody().SetUseGravity(true);
	self.GetRigidbody().SetDrag(1.0f);
	self.GetRigidbody().SetMass(1.0f);
	self.GetRigidbody().SetIsKinematic(false);

	if (SceneBase* scene = ScriptApi_GetCurrentScene()) {
		const auto& objs = scene->GetObjects();
		if (std::find(objs.begin(), objs.end(), &self) == objs.end())
			scene->AddObject(&self);
	}

	{
		XMFLOAT3 spawnPos = self.GetPosition();
		if (!GameMode_IsTraining())
			RoundManager_EnsureMinSpawnRadius(spawnPos);
		self.SetPosition(spawnPos);
		m_spawnPosition = spawnPos;
	}

	ThirdPersonCamera_Initialize();
	ThirdPersonCamera_SetTarget(&self.m_position, &m_yaw);
	ThirdPersonCamera_SetMouseSensitivity(cameraSensitivity);
	ThirdPersonCamera_SetControllerSensitivity(cameraSensitivity * 600.0f);
	ThirdPersonCamera_SetPitchLimits(cameraMinPitch, cameraMaxPitch);

	m_heldItem = nullptr;
	m_heldItemKind = ItemWorldKind::Unknown;

	m_danceTimer = 0.0f;
	SetupVisualAnimations(self);

	g_playerForMagnetGlow = this;
	g_playerBody = &self;
	CenterPole_AddDynamicTarget(self.GetPosition(), &self, &m_poleRegistryIndex);
	CenterPole_SetShouldSuppressAimGlow(&HostShouldSuppressAimGlow);
	BindMagneticEnergyUI();
	BindChargeKnockbackCooldownUI();
}

void PlayerScript::SetupVisualAnimations(engine::GameObject& self) {
	VisualComponent* vc = ScriptApi_FindVisualComponent(self);
	if (!vc) return;

	vc->playAnimation = true;
	if (vc->animationPaths.find(runClip) == vc->animationPaths.end())
		vc->animationPaths[runClip] = "rom/model/player/run.fbx";
	if (vc->animationPaths.find(idleClip) == vc->animationPaths.end())
		vc->animationPaths[idleClip] = "rom/model/player/idle.fbx";
	vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
	if (vc->animationPaths.find(danceClip) == vc->animationPaths.end())
		vc->animationPaths[danceClip] = "rom/model/player/dance1.fbx";
	if (auto* mat = self.GetComponent<MaterialComponent>())
		mat->color = PlayerTintRgb();
}

bool PlayerScript::IsLocomoting(const engine::GameObject& obj) const {
	if (m_isRespawning || m_stunImmobilize)
		return false;

	XMFLOAT3 vel = obj.GetRigidbody().GetVelocity();
	const float speedSq = vel.x * vel.x + vel.z * vel.z;
	if (speedSq > kLocomotionSpeedSqThreshold)
		return true;

	const Keyboard_State* kb = Keyboard_GetState();
	if (kb->W || kb->S || kb->A || kb->D)
		return true;

	XMFLOAT2 leftStick = Controller_GetLeftStick();
	if (fabsf(leftStick.x) > 0.1f || fabsf(leftStick.y) > 0.1f)
		return true;

	return false;
}

void PlayerScript::SyncPlayerAnimation(engine::GameObject& self, float dt) {
	VisualComponent* vc = ScriptApi_FindVisualComponent(self);
	if (!vc || !vc->playAnimation) return;

	if (m_isRespawning) {
		ScriptApi_Visual_PlayClip(vc, idleClip.c_str());
		m_danceTimer = 0.0f;
		return;
	}

	if (KeyLogger_IsTrigger(KK_D1))
		m_danceTimer = kDanceDuration;

	std::string targetClip;
	if (m_danceTimer > 0.0f) {
		m_danceTimer -= dt;
		if (m_danceTimer < 0.0f)
			m_danceTimer = 0.0f;
		targetClip = danceClip;
	} else if (IsLocomoting(self)) {
		targetClip = runClip;
	} else {
		targetClip = idleClip;
	}

	ScriptApi_Visual_PlayClip(vc, targetClip.c_str());
}

void PlayerScript::SetFacingFromDirection(const XMFLOAT3& dir) {
	const float speedSq = dir.x * dir.x + dir.z * dir.z;
	if (speedSq <= kLocomotionSpeedSqThreshold)
		return;

	const float invLen = 1.0f / sqrtf(speedSq);
	m_targetFacingYaw = atan2f(dir.x * invLen, dir.z * invLen) * (180.0f / 3.14159265f);
	m_facingFromAccelerationThisFrame = true;
}

void PlayerScript::SyncFacingFromVelocity(const engine::GameObject& obj) {
	if (m_facingFromAccelerationThisFrame)
		return;
	SetFacingFromDirection(obj.GetRigidbody().GetVelocity());
}

void PlayerScript::UpdateFacingYaw(float dt) {
	if (dt <= 0.0f) {
		m_facingYaw = m_targetFacingYaw;
		return;
	}

	float delta = fmodf(m_targetFacingYaw - m_facingYaw + 540.0f, 360.0f) - 180.0f;
	const float t = (std::min)(1.0f, kFacingLerpSpeed * dt);
	m_facingYaw += delta * t;
	if (m_facingYaw >= 360.0f)
		m_facingYaw -= 360.0f;
	else if (m_facingYaw < 0.0f)
		m_facingYaw += 360.0f;
}

void PlayerScript::SetHeldItem(GameObject* item, ItemWorldKind kind) {
	m_heldItem = item;
	m_heldItemKind = kind;
}

void PlayerScript::ClearHeldItem() {
	m_heldItem = nullptr;
	m_heldItemKind = ItemWorldKind::Unknown;
}

void PlayerScript::OnUpdate(engine::GameObject& self, float dt) {
	SyncTransformFromPhysics(self);
	PlayerNametag_Draw(0, self.GetPosition());
	m_facingFromAccelerationThisFrame = false;
	MagnetFieldSystem::Instance().SetConfig(BuildMagnetFieldConfig());
	MagnetLinkVfx::Instance().SetSettings(BuildMagnetHelixSettings());
	m_isOrbiting = MagnetFieldSystem::Instance().IsOrbiting();
	if (m_postOrbitShieldTimer > 0.0f) {
		m_postOrbitShieldTimer -= dt;
		if (m_postOrbitShieldTimer < 0.0f)
			m_postOrbitShieldTimer = 0.0f;
	}
	TickChargeKnockbackCooldown(dt);
	SyncChargeKnockbackCooldownUI();
	TickMagneticEnergy(dt);
	SyncMagneticEnergyUI();

	const bool gameplayActive = RoundManager_IsGameplayActive();

	if (m_crosshair) {
		bool inPlayMode = EngineGUI_IsPlaying() && !ThirdPersonCamera_IsCursorVisible();
		bool controlsEnabled = !m_isRespawning && gameplayActive;
		m_crosshair->SetVisible(inPlayMode && controlsEnabled);
	}

	if (EngineGUI_IsPlaying() && !m_isRespawning && gameplayActive) {
		ItemWorld_TickHolder(self, dt, m_facingYaw);
	}

	if (m_isRespawning || !gameplayActive) {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Normal);
		SyncPlayerVisualAppearance(self, false);
		SyncPlayerAnimation(self, dt);
		SyncLockOnUI();
		return;
	}

	g_chargeImpactBodiesThisFrame.clear();

	if (m_controlState == PlayerControlState::Knockback && m_stunImmobilize)
		HandleEnemyImpact(self, dt);

	if (m_controlState == PlayerControlState::Knockback) {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Normal);
		TickKnockbackState(self, dt);
		SyncPlayerVisualAppearance(self, m_stunImmobilize);
		SyncPlayerAnimation(self, dt);
		SyncLockOnUI();
		return;
	}

	CheckGroundStatus(self);
	HandleEnemyImpact(self, dt);
	HandleAimedTargetChargeImpact(self, dt);

	if (m_controlState == PlayerControlState::Knockback) {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Normal);
		TickKnockbackState(self, dt);
		SyncPlayerVisualAppearance(self, m_stunImmobilize);
		SyncPlayerAnimation(self, dt);
		SyncLockOnUI();
		return;
	}

	HandleAiming(self, magnetMaxRange);

	const bool wasOrbiting = m_isOrbiting;
	HandlePoleOrbit(self, dt);
	const bool justReleasedOrbit = wasOrbiting && !m_isOrbiting;

	if (m_controlState != PlayerControlState::Knockback && !m_isOrbiting) {
		if (!justReleasedOrbit)
			HandleMovement(self, static_cast<double>(dt));
		HandleJump(self);
		HandleMagneticAttraction(self, dt);
		HandleMagneticRepulsion(self, dt);
	}

	// Spawn magnet helix VFX using the link state set by ShowMagnetLink() this frame.
	MagnetFieldSystem::Instance().Tick(dt);
	MagnetFieldSystem::Instance().HideMagnetLink();

	// Drive follow-camera magnet feel from active push/pull/orbit.
	if (m_isOrbiting) {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Orbit);
		if (engine::GameObject* orbitTarget =
				CenterPole_GetRegistryNode(MagnetFieldSystem::Instance().GetOrbitTargetIndex())) {
			ThirdPersonCamera_SetMagnetTarget(orbitTarget->GetPosition());
		}
	} else if (CanUseMagnetic() && !IsAimedTargetLocked() && CenterPole_HasAimed()
		&& (Mouse_IsLeftDown() || Controller_GetRightTrigger() > 0.5f)) {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Pull);
		ThirdPersonCamera_SetMagnetTarget(CenterPole_GetAimedPosition());
	} else if (CanUseMagnetic() && !IsAimedTargetLocked() && CenterPole_HasAimed()
		&& (Mouse_IsRightDown() || Controller_IsPressed(ControllerButton::RB))) {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Push);
		ThirdPersonCamera_SetMagnetTarget(CenterPole_GetAimedPosition());
	} else {
		ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode::Normal);
	}

	SyncFacingFromVelocity(self);
	UpdateFacingYaw(dt);
	self.m_rotation.y = m_facingYaw;
	SyncTransformFromPhysics(self);
	if (m_poleRegistryIndex >= 0)
		CenterPole_UpdateDynamicPosition(m_poleRegistryIndex, self.GetPosition());
	SyncPlayerVisualAppearance(self, false);
	SyncPlayerAnimation(self, dt);
	SyncLockOnUI();
}

void PlayerScript::OnDestroy(engine::GameObject& self) {
	StopOrbitTrail(true);
	MagnetFieldSystem::Instance().Reset();
	if (g_playerForMagnetGlow == this) {
		g_playerForMagnetGlow = nullptr;
		CenterPole_SetShouldSuppressAimGlow(nullptr);
		LockOnUI::SetTarget(nullptr, LockOnTargetState::None);
	}
	if (g_playerBody == &self)
		g_playerBody = nullptr;
	if (m_poleRegistryIndex >= 0) {
		CenterPole_RemoveDynamicTarget(m_poleRegistryIndex);
		m_poleRegistryIndex = -1;
	}
	ClearHeldItem();
	ThirdPersonCamera_ShowCursor(true);
	ThirdPersonCamera_SetTarget(nullptr, nullptr);

	if (m_crosshair) {
		if (UICanvas* canvas = SceneCanvas_GetCanvas())
			ScriptApi_UICanvasRemoveElement(*canvas, "PlayerCrosshair");
		m_crosshair = nullptr;
	}

	if (SceneBase* scene = ScriptApi_GetCurrentScene())
		scene->RemoveObject(&self);
}

void PlayerScript::CheckGroundStatus(engine::GameObject& obj) {
	m_isGrounded = false;
	if (!obj.GetCollider()) return;

	SceneBase* scene = ScriptApi_GetCurrentScene();
	if (!scene) return;

	const auto& objs = scene->GetObjects();
	XMFLOAT3 myPos = obj.GetPosition();
	float myHalfHeight = 0.5f;
	if (obj.GetCollider()->GetType() == ColliderType::BOX) {
		myHalfHeight = ScriptApi_GetBoxColliderSize(*static_cast<BoxCollider*>(obj.GetCollider())).y * 0.5f;
	}
	else if (obj.GetCollider()->GetType() == ColliderType::SPHERE) {
		myHalfHeight = ScriptApi_GetSphereColliderRadius(*static_cast<SphereCollider*>(obj.GetCollider()));
	}

	for (auto* other : objs) {
		if (!other || other == &obj || !other->IsActive()) continue;

		Collider* otherCol = other->GetCollider();
		if (!otherCol) continue;

		if (obj.GetCollider()->CheckCollision(otherCol)) {
			XMFLOAT3 otherPos = ScriptApi_GetColliderWorldCenter(*otherCol);
			float deltaY = myPos.y - otherPos.y;
			float verticalBias = 0.1f;

			if (deltaY > (myHalfHeight - verticalBias)) {
				float deltaX = fabsf(myPos.x - otherPos.x);
				float deltaZ = fabsf(myPos.z - otherPos.z);

				if (otherCol->GetType() == ColliderType::BOX) {
					BoxCollider* otherBox = static_cast<BoxCollider*>(otherCol);
					XMFLOAT3 otherSize = ScriptApi_GetBoxColliderSize(*otherBox);
					float otherHalfW = otherSize.x * 0.5f;
					float otherHalfD = otherSize.z * 0.5f;
					if (deltaX < (otherHalfW + 0.2f) && deltaZ < (otherHalfD + 0.2f)) {
						m_isGrounded = true;
						break;
					}
				}
				else {
					m_isGrounded = true;
					break;
				}
			}
		}
	}
}

void PlayerScript::ApplyImpactToBody(engine::GameObject& obj, engine::GameObject& other,
	const XMFLOAT3& dirFromPlayerToOther, float dt,
	bool chargeHit, bool aimedAtTarget) {
	const bool useCharge = chargeHit && aimedAtTarget;

	{
		const XMFLOAT3 a = obj.GetPosition();
		const XMFLOAT3 b = other.GetPosition();
		const XMFLOAT3 sparkPos = {
			(a.x + b.x) * 0.5f,
			(a.y + b.y) * 0.5f + 0.45f,
			(a.z + b.z) * 0.5f
		};
		ImpactSparkVfx::Instance().Spawn(sparkPos, dirFromPlayerToOther, useCharge);
	}

	float enemyHoriz = useCharge ? kChargeImpactForce : kImpactForce;
	float enemyLift = useCharge ? kChargeImpactLift : kImpactLiftForce;
	float playerRecoil = useCharge
		? (kPlayerKnockbackImpulse * kChargePlayerRecoilScale)
		: kPlayerKnockbackImpulse;
	const float playerKnockDuration = useCharge ? kChargeKnockbackDuration : -1.0f;

	if (ChaseTestEnemyScript* chase = ChaseTestEnemy_FindByPhysicsProxy(&other)) {
		if (useCharge) {
			ChaseTestEnemy_ApplyKnockbackToProxy(
				&other,
				{ dirFromPlayerToOther.x * enemyHoriz, enemyLift, dirFromPlayerToOther.z * enemyHoriz },
				kChargeKnockbackDuration,
				dt);
			MarkChargeImpactApplied(&other);
		}
		else {
			other.GetRigidbody().AddForce(
				{ dirFromPlayerToOther.x * enemyHoriz, enemyLift, dirFromPlayerToOther.z * enemyHoriz },
				ForceMode::Impulse);
		}
	}
	else if (EnemyAi_FindByPhysicsProxy(&other)) {
		if (useCharge) {
			EnemyAi_ApplyDirectionalImpulseToProxy(
				&other,
				dirFromPlayerToOther,
				enemyHoriz,
				enemyLift,
				kChargeKnockbackDuration);
			MarkChargeImpactApplied(&other);
		}
		else {
			other.GetRigidbody().AddForce(
				{ dirFromPlayerToOther.x * enemyHoriz, enemyLift, dirFromPlayerToOther.z * enemyHoriz },
				ForceMode::Impulse);
		}
	}
	else {
		other.GetRigidbody().AddForce(
			{ dirFromPlayerToOther.x * enemyHoriz, enemyLift, dirFromPlayerToOther.z * enemyHoriz },
			ForceMode::Impulse);
		if (useCharge)
			MarkChargeImpactApplied(&other);
	}

	if (useCharge)
		BeginChargeKnockbackCooldown();

	if (playerRecoil > 0.001f) {
		obj.GetRigidbody().AddForce(
			{ -dirFromPlayerToOther.x * playerRecoil,
			 0.35f * playerRecoil,
			 -dirFromPlayerToOther.z * playerRecoil },
			ForceMode::Impulse);
	}
	BeginKnockback(playerKnockDuration, false);
}

void PlayerScript::HandleEnemyImpact(engine::GameObject& obj, float dt) {
	for (auto it = m_enemyMagnetCooldown.begin(); it != m_enemyMagnetCooldown.end(); ) {
		it->second -= dt;
		if (it->second <= 0.0f)
			it = m_enemyMagnetCooldown.erase(it);
		else
			++it;
	}

	if (m_controlState == PlayerControlState::Knockback && !m_stunImmobilize) {
		m_enemyContactSet.clear();
		return;
	}

	Collider* myCollider = obj.GetCollider();
	SceneBase* scene = ScriptApi_GetCurrentScene();
	if (!myCollider || !scene) {
		m_enemyContactSet.clear();
		return;
	}

	const bool chargeActive = IsMagneticChargeActive();
	GameObject* aimedNode = chargeActive ? CenterPole_GetAimedNode() : nullptr;

	std::unordered_set<engine::GameObject*> currentContacts;
	const auto& objs = scene->GetObjects();
	for (auto* other : objs) {
		if (!other || other == &obj || !other->IsActive()) continue;
		if (other->GetTag() != Tag::TAG_ENEMY) continue;

		Collider* otherCol = other->GetCollider();
		if (!otherCol) continue;
		if (!myCollider->CheckCollision(otherCol)) continue;

		currentContacts.insert(other);
		if (m_enemyContactSet.find(other) != m_enemyContactSet.end()) continue;

		XMFLOAT3 fromPlayerToEnemy = {
			other->GetPosition().x - obj.GetPosition().x,
			0.0f,
			other->GetPosition().z - obj.GetPosition().z
		};
		float distanceXZSq = fromPlayerToEnemy.x * fromPlayerToEnemy.x +
			fromPlayerToEnemy.z * fromPlayerToEnemy.z;
		XMFLOAT3 dir = GetForward(obj);
		if (distanceXZSq > (0.001f * 0.001f)) {
			float invDistance = 1.0f / sqrtf(distanceXZSq);
			dir = { fromPlayerToEnemy.x * invDistance, 0.0f, fromPlayerToEnemy.z * invDistance };
		}

		const bool aimedAtTarget = (aimedNode == other);
		const bool chargeHit = chargeActive && aimedAtTarget
			&& MeetsChargeApproachSpeed(obj, *other, dir);

		ApplyImpactToBody(obj, *other, dir, dt, chargeHit, aimedAtTarget);

		m_enemyMagnetCooldown[other] = kImpactCooldown;

		const int hitTargetIndex = CenterPole_FindIndexByNode(other);
		if (m_isOrbiting && hitTargetIndex >= 0 && m_orbitPoleIndex == hitTargetIndex)
			CancelOrbit();
	}

	m_enemyContactSet.swap(currentContacts);
}

void PlayerScript::HandleAimedTargetChargeImpact(engine::GameObject& obj, float dt) {
	if (!IsMagneticChargeActive())
		return;

	GameObject* aimed = CenterPole_GetAimedNode();
	if (!aimed || aimed == &obj || !aimed->IsActive())
		return;
	if (aimed->GetTag() == Tag::TAG_ENEMY)
		return;

	Collider* myCollider = obj.GetCollider();
	Collider* otherCol = aimed->GetCollider();
	if (!myCollider || !otherCol) {
		m_otherPlayerContactSet.clear();
		return;
	}

	std::unordered_set<engine::GameObject*> currentContacts;
	if (!myCollider->CheckCollision(otherCol)) {
		m_otherPlayerContactSet.clear();
		return;
	}

	currentContacts.insert(aimed);
	if (m_otherPlayerContactSet.find(aimed) != m_otherPlayerContactSet.end())
		return;

	XMFLOAT3 fromPlayerToOther = {
		aimed->GetPosition().x - obj.GetPosition().x,
		0.0f,
		aimed->GetPosition().z - obj.GetPosition().z
	};
	float distanceXZSq = fromPlayerToOther.x * fromPlayerToOther.x +
		fromPlayerToOther.z * fromPlayerToOther.z;
	XMFLOAT3 dir = GetForward(obj);
	if (distanceXZSq > (0.001f * 0.001f)) {
		float invDistance = 1.0f / sqrtf(distanceXZSq);
		dir = { fromPlayerToOther.x * invDistance, 0.0f, fromPlayerToOther.z * invDistance };
	}

	if (!MeetsChargeApproachSpeed(obj, *aimed, dir))
		return;

	ApplyImpactToBody(obj, *aimed, dir, dt, true, true);
	m_enemyMagnetCooldown[aimed] = kImpactCooldown;
	m_otherPlayerContactSet.swap(currentContacts);
}

void PlayerScript::HandleMovement(engine::GameObject& obj, double /*elapsed_time*/) {
	if (m_controlState == PlayerControlState::Knockback) return;

	const Keyboard_State* kb = Keyboard_GetState();
	XMFLOAT2 leftStick = Controller_GetLeftStick();

	XMFLOAT3 moveDir = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 forward = GetForward(obj);
	XMFLOAT3 right = GetRight(obj);

	if (kb->W) { moveDir.x += forward.x; moveDir.z += forward.z; }
	if (kb->S) { moveDir.x -= forward.x; moveDir.z -= forward.z; }
	if (kb->A) { moveDir.x -= right.x;   moveDir.z -= right.z; }
	if (kb->D) { moveDir.x += right.x;   moveDir.z += right.z; }

	moveDir.x += forward.x * leftStick.y + right.x * leftStick.x;
	moveDir.z += forward.z * leftStick.y + right.z * leftStick.x;

	float length = sqrtf(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
	if (length > 0.01f) {
		moveDir.x /= length;
		moveDir.z /= length;
		const float effectiveSpeed = moveSpeed * m_moveSpeedScale;
		obj.GetRigidbody().AddForce(
			{ moveDir.x * effectiveSpeed, 0.0f, moveDir.z * effectiveSpeed },
			ForceMode::Acceleration);
		SetFacingFromDirection(moveDir);
	}
}

void PlayerScript::HandleJump(engine::GameObject& obj) {
	if (m_controlState == PlayerControlState::Knockback) return;

	if (KeyLogger_IsTrigger(KK_SPACE) && m_isGrounded) {
		XMFLOAT3 velocity = obj.GetRigidbody().GetVelocity();
		velocity.y = 0.0f;
		obj.GetRigidbody().SetVelocity(velocity);
		obj.GetRigidbody().AddForce({ 0.0f, kJumpForce, 0.0f }, ForceMode::Impulse);
		m_isGrounded = false;
	}
}

void PlayerScript::HandleAiming(engine::GameObject& obj, float magnetMaxDist) {
	const bool holdingPullOrPush =
		IsMagneticPullInputActive() || IsMagneticPushInputActive();

	// Camera can turn during pull/push; keep the press-frame target until release.
	if (holdingPullOrPush) {
		if (!m_magnetAimFrozen) {
			CenterPole_UpdateAiming(obj.GetPosition(), ScriptApi_GetCameraForward(), magnetMaxDist);
			m_magnetAimFrozen = true;
		}
		return;
	}

	m_magnetAimFrozen = false;
	CenterPole_UpdateAiming(obj.GetPosition(), ScriptApi_GetCameraForward(), magnetMaxDist);
}

void PlayerScript::SyncLockOnUI() {
	GameObject* target = nullptr;
	LockOnTargetState state = LockOnTargetState::None;

	if (const int orbitIdx = CenterPole_GetOrbitIndex(); orbitIdx >= 0) {
		target = CenterPole_GetRegistryNode(orbitIdx);
		if (target) {
			state = IsMagnetTargetLocked(target)
				? LockOnTargetState::OnCooldown
				: LockOnTargetState::Available;
		}
	}
	else if (CenterPole_HasAimed()) {
		target = CenterPole_GetAimedNode();
		state = IsMagnetTargetLocked(target)
			? LockOnTargetState::OnCooldown
			: LockOnTargetState::Available;
	}
	else if (CenterPole_HasPreviewAimed()) {
		target = CenterPole_GetPreviewAimedNode();
		state = LockOnTargetState::TooFar;
	}

	LockOnUI::SetTarget(target, state);
}

bool PlayerScript::IsEnemyMagnetCooldownActive(const engine::GameObject* enemyBody) const {
	if (!enemyBody) return false;
	auto it = m_enemyMagnetCooldown.find(const_cast<engine::GameObject*>(enemyBody));
	return it != m_enemyMagnetCooldown.end() && it->second > 0.0f;
}

bool PlayerScript::IsMagnetTargetLocked(const engine::GameObject* targetBody) const {
	return IsEnemyMagnetCooldownActive(targetBody) || EnemyAi_IsBodyRespawning(targetBody);
}

bool PlayerScript::IsAimedTargetLocked() const {
	engine::GameObject* aimedNode = CenterPole_GetAimedNode();
	return aimedNode && IsMagnetTargetLocked(aimedNode);
}

void PlayerScript::HandleMagneticAttraction(engine::GameObject& obj, float dt) {
	if (m_controlState == PlayerControlState::Knockback) return;
	if (!CanUseMagnetic()) return;
	if (IsAimedTargetLocked()) return;
	if (!Mouse_IsLeftDown() && Controller_GetRightTrigger() <= 0.5f) return;
	if (!CenterPole_HasAimed()) return;
	const XMFLOAT3 targetPos = CenterPole_GetAimedPosition();
	SetFacingFromDirection({
		targetPos.x - obj.GetPosition().x,
		0.0f,
		targetPos.z - obj.GetPosition().z
	});
	if (engine::GameObject* target = CenterPole_GetAimedNode()) {
		MagnetFieldSystem::Instance().ShowMagnetLink(obj, *target, 1);
		MagnetFieldSystem::Instance().Attract(obj, target, dt);
	}
}

void PlayerScript::HandleMagneticRepulsion(engine::GameObject& obj, float dt) {
	if (m_controlState == PlayerControlState::Knockback) return;
	if (!CanUseMagnetic()) return;
	if (IsAimedTargetLocked()) return;
	if (!Mouse_IsRightDown() && !Controller_IsPressed(ControllerButton::RB)) return;
	if (!CenterPole_HasAimed()) return;
	const XMFLOAT3 targetPos = CenterPole_GetAimedPosition();
	SetFacingFromDirection({
		obj.GetPosition().x - targetPos.x,
		0.0f,
		obj.GetPosition().z - targetPos.z
	});
	if (engine::GameObject* target = CenterPole_GetAimedNode()) {
		MagnetFieldSystem::Instance().ShowMagnetLink(obj, *target, 2);
		MagnetFieldSystem::Instance().Repulse(obj, target, dt);
	}
}

void PlayerScript::HandlePoleOrbit(engine::GameObject& obj, float dt) {
	if (m_controlState == PlayerControlState::Knockback) {
		CancelOrbit();
		return;
	}

	if (!CanUseMagnetic()) {
		CancelOrbit();
	}

	static bool s_ltWasHeld = false;
	static bool s_eWasHeld = false;
	const bool ltHeld = Controller_GetLeftTrigger() > kOrbitLTActivationThresh;
	const bool ltJustPressed = ltHeld && !s_ltWasHeld;
	const Keyboard_State* kb = Keyboard_GetState();
	const bool eHeld = kb->E;
	const bool eJustPressed = eHeld && !s_eWasHeld;

	s_ltWasHeld = ltHeld;
	s_eWasHeld = eHeld;

	MagnetOrbitInputState inputState;
	inputState.held = eHeld || Mouse_IsMiddleDown() || ltHeld;
	inputState.startRequested = !m_isOrbiting && (eJustPressed || Mouse_IsMiddleDown() || ltJustPressed);
	XMFLOAT2 leftStick = Controller_GetLeftStick();
	const float wheelSteps = static_cast<float>(Mouse_GetWheelDelta()) / static_cast<float>(WHEEL_DELTA);
	// Tangential accel is automatic from orbit spin direction; stick X / A/D unused.
	// Radius: pull = closer, push = farther (also wheel / stick Y / D-pad).
	inputState.radiusInput = -wheelSteps - leftStick.y;
	if (Controller_IsPressed(ControllerButton::DPadUp)) inputState.radiusInput -= 1.0f;
	if (Controller_IsPressed(ControllerButton::DPadDown)) inputState.radiusInput += 1.0f;
	if (IsMagneticPullInputActive()) inputState.radiusInput -= 1.0f;
	if (IsMagneticPushInputActive()) inputState.radiusInput += 1.0f;

	GameObject* target = m_isOrbiting
		? CenterPole_GetRegistryNode(MagnetFieldSystem::Instance().GetOrbitTargetIndex())
		: CenterPole_GetAimedNode();
	if (inputState.startRequested && (!CanUseMagnetic() || IsAimedTargetLocked()))
		inputState.startRequested = false;

	const bool wasOrbiting = MagnetFieldSystem::Instance().IsOrbiting();
	MagnetFieldSystem::Instance().Orbit(obj, target, inputState, dt);
	SetFacingFromDirection(MagnetFieldSystem::Instance().GetOrbitFacingDirectionXZ());
	const float releasedOrbitDuration = MagnetFieldSystem::Instance().ConsumeReleasedOrbitDuration();
	if (releasedOrbitDuration > 0.0f)
		m_postOrbitShieldTimer = (std::max)(m_postOrbitShieldTimer, releasedOrbitDuration);
	m_isOrbiting = MagnetFieldSystem::Instance().IsOrbiting();
	m_orbitPoleIndex = MagnetFieldSystem::Instance().GetOrbitTargetIndex();
	if (m_isOrbiting)
		EnsureOrbitTrailEmitter(obj);
	else if (wasOrbiting)
		StopOrbitTrail();

	if (m_isOrbiting && target)
		MagnetFieldSystem::Instance().ShowMagnetLink(obj, *target, 3);
}
