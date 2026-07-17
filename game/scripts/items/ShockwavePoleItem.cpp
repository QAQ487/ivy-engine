#include "ShockwavePoleItem.h"

#include "CenterPoleScript.h"
#include "EnemyAiScript.h"
#include "ChaseTestEnemyScript.h"
#include "ItemWorldApi.h"
#include "PlayerScript.h"
#include "engine/physics/collider/collider.h"
#include "engine/ecs/collider_component.h"
#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

using namespace DirectX;

REGISTER_SCRIPT(ShockwavePoleItemScript)

namespace {
	ShockwavePoleItemScript* g_poleItemScript = nullptr;

	std::mt19937& ItemRng() {
		static std::mt19937 rng(std::random_device{}());
		return rng;
	}
}

void ShockwavePoleItem_ResetForWorldPickup(engine::GameObject& item) {
	ScriptComponent* sc = item.GetComponent<ScriptComponent>();
	if (!sc) return;
	if (auto* script = dynamic_cast<ShockwavePoleItemScript*>(sc->GetScript()))
		script->ResetStateForWorldPickup(item);
}

void ShockwavePoleItem_ReleaseFromField(engine::GameObject& item) {
	ScriptComponent* sc = item.GetComponent<ScriptComponent>();
	if (!sc) return;
	if (auto* script = dynamic_cast<ShockwavePoleItemScript*>(sc->GetScript()))
		script->ReleaseFromField(item);
}

void ShockwavePoleItemScript::ApplyPoleAppearance(engine::GameObject& self) {
	if (auto* vc = ScriptApi_FindVisualComponent(self)) {
		vc->emissionGlow = { 0.0f, 0.0f, 0.0f, 0.0f };
	}
	if (ColliderComponent* cc = ScriptApi_FindColliderComponent(self))
		ScriptApi_ApplyColliderComponent(*cc);
}

PoleHeldItemType ShockwavePoleItemScript::RollRandomHeldItem() const {
	std::uniform_int_distribution<int> dist(0, static_cast<int>(PoleHeldItemType::Count) - 1);
	return static_cast<PoleHeldItemType>(dist(ItemRng()));
}

XMFLOAT3 ShockwavePoleItemScript::GetShockwaveDrawCenter() const {
	return {
		m_shockwaveCenter.x,
		m_shockwaveCenter.y + 0.75f,
		m_shockwaveCenter.z
	};
}

void ShockwavePoleItemScript::DrawShockwaveRing(const XMFLOAT3& center, float radius, float alpha) const {
	(void)center;
	(void)radius;
	(void)alpha;
}

void ShockwavePoleItemScript::ResetStateForWorldPickup(engine::GameObject& self) {
	m_state = State::MysteryBoxWorld;
	m_heldItem = PoleHeldItemType::Pole;
	m_shockwaveTimer = 0.0f;
	m_shockwaveVisualTimer = 0.0f;
	m_placedLifetimeTimer = 0.0f;
	m_shockwaveCenter = { 0.0f, 0.0f, 0.0f };
	m_placedByPlayer = nullptr;
	SyncMagneticPoleRegistration(self);
}

void ShockwavePoleItemScript::ReleaseFromField(engine::GameObject& self) {
	UnregisterMagneticPole();
	m_state = State::MysteryBoxWorld;
	m_heldItem = PoleHeldItemType::Pole;
	m_shockwaveTimer = 0.0f;
	m_shockwaveVisualTimer = 0.0f;
	m_placedLifetimeTimer = 0.0f;
	m_shockwaveCenter = { 0.0f, 0.0f, 0.0f };
	m_placedByPlayer = nullptr;
	ItemWorld_DestroyRuntimeItem(self);
}

bool ShockwavePoleItemScript::IsWorldPickupState() const {
	return m_state == State::MysteryBoxWorld;
}

bool ShockwavePoleItemScript::IsHeldByBody(const GameObject& body) const {
	(void)body;
	return m_state == State::HeldByPlayer;
}

bool ShockwavePoleItemScript::IsBodyInPickupRange(GameObject& self,
	GameObject& body) const {
	const float pickupRadius = (std::max)(0.1f, itemPickupRadius);
	if (Collider* bodyCol = body.GetCollider()) {
		if (Collider* itemCol = self.GetCollider()) {
			if (bodyCol->CheckCollision(itemCol))
				return true;
		}
	}

	const XMFLOAT3 itemPos = self.GetPosition();
	const XMFLOAT3 bodyPos = body.GetPosition();
	const float dx = bodyPos.x - itemPos.x;
	const float dz = bodyPos.z - itemPos.z;
	return (dx * dx + dz * dz) <= (pickupRadius * pickupRadius);
}

bool ShockwavePoleItemScript::TryPickupForBody(engine::GameObject& self, GameObject& body) {
	if (m_state != State::MysteryBoxWorld || !body.IsActive())
		return false;
	if (!IsBodyInPickupRange(self, body))
		return false;

	m_heldItem = PoleHeldItemType::Pole;
	ApplyPoleAppearance(self);
	MoveToHeldHidePosition(self);
	m_state = State::HeldByPlayer;
	m_placedByPlayer = nullptr;
	return true;
}

bool ShockwavePoleItemScript::TryPlaceForBody(engine::GameObject& self,
	GameObject& body,
	float yawDegrees) {
	if (!ItemWorld_IsHeldBy(self, body))
		return false;
	if (m_state != State::HeldByPlayer)
		m_state = State::HeldByPlayer;
	if (m_heldItem != PoleHeldItemType::Pole)
		return false;

	XMFLOAT3 placePos = body.GetPosition();
	const XMFLOAT3 forward = GetForwardFromYaw(yawDegrees);
	placePos.x += forward.x * polePlaceDistance;
	placePos.z += forward.z * polePlaceDistance;
	placePos.y = 0.5f;

	SyncTransform(self, placePos);
	m_placedByPlayer = &body;
	m_shockwaveCenter = placePos;
	m_shockwaveTimer = (std::max)(0.01f, poleShockwaveDelay);
	m_shockwaveVisualTimer = 0.0f;
	m_placedLifetimeTimer = 0.0f;
	m_state = State::PlacedPendingShockwave;
	ApplyPoleAppearance(self);
	SyncMagneticPoleRegistration(self);
	return true;
}

bool ShockwavePoleItemScript::IsMagnetActiveOnField() const {
	return m_state == State::PlacedPendingShockwave
		|| m_state == State::PlacedActive;
}

void ShockwavePoleItemScript::UnregisterMagneticPole() {
	if (m_magnetRegistryIndex >= 0) {
		if (engine::GameObject* node = CenterPole_GetRegistryNode(m_magnetRegistryIndex))
			CenterPole_ClearOrbitIfNode(node);
		CenterPole_UnregisterExternalPole(m_magnetRegistryIndex);
		m_magnetRegistryIndex = -1;
	}
}

void ShockwavePoleItemScript::SyncMagneticPoleRegistration(engine::GameObject& self) {
	if (!IsMagnetActiveOnField()) {
		UnregisterMagneticPole();
		return;
	}

	const XMFLOAT3 pos = self.GetPosition();
	const XMFLOAT3 scale = self.GetScale();
	if (m_magnetRegistryIndex < 0) {
		CenterPole_RegisterExternalPole(self, scale, &m_magnetRegistryIndex);
	}
	else {
		CenterPole_UpdateExternalPole(m_magnetRegistryIndex, pos, scale);
	}
}

void ShockwavePoleItemScript::OnCreate(engine::GameObject& self) {
	g_poleItemScript = this;
	self.SetActive(true);
	m_magnetRegistryIndex = -1;
	m_placedByPlayer = nullptr;
	// OnCreate runs on first Update (see ScriptComponent); handoff may already hold this item.
	if (!ItemWorld_GetHolder(self))
		ResetStateForWorldPickup(self);
}

void ShockwavePoleItemScript::OnDestroy(engine::GameObject& self) {
	(void)self;
	UnregisterMagneticPole();
	if (g_poleItemScript == this)
		g_poleItemScript = nullptr;
}

bool ShockwavePoleItemScript::FindLargestGroundBox(XMFLOAT3& outCenter, XMFLOAT3& outSize) const {
	outCenter = { 0.0f, 0.0f, 0.0f };
	outSize = { 1.0f, 1.0f, 1.0f };

	SceneBase* scene = ScriptApi_GetCurrentScene();
	if (!scene) return false;

	float bestArea = -1.0f;
	for (GameObject* obj : scene->GetObjects()) {
		if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_GROUND) continue;
		auto* box = dynamic_cast<BoxCollider*>(obj->GetCollider());
		if (!box) continue;

		const XMFLOAT3 size = ScriptApi_GetBoxColliderSize(*box);
		const float area = size.x * size.z;
		if (area > bestArea) {
			bestArea = area;
			outCenter = ScriptApi_GetColliderWorldCenter(*box);
			outSize = size;
		}
	}

	return bestArea > 0.0f;
}

void ShockwavePoleItemScript::SyncTransform(engine::GameObject& self, const XMFLOAT3& worldPos) {
	self.SetPosition(worldPos);
	if (engine::Transform* t = ScriptApi_FindTransform(self))
		t->localPosition = worldPos;
}

void ShockwavePoleItemScript::MoveToHeldHidePosition(engine::GameObject& self) {
	UnregisterMagneticPole();
	XMFLOAT3 hiddenPos = self.GetPosition();
	hiddenPos.y = heldHideY;
	SyncTransform(self, hiddenPos);
}

XMFLOAT3 ShockwavePoleItemScript::GetForwardFromYaw(float yawDegrees) const {
	const float yawRad = XMConvertToRadians(yawDegrees);
	return { sinf(yawRad), 0.0f, cosf(yawRad) };
}

namespace {
	bool IsBodyInShockwaveRadius(const GameObject& body,
		const XMFLOAT3& center,
		float radius) {
		if (!body.IsActive() || !body.GetCollider()) return false;

		const float radiusSq = radius * radius;
		const XMFLOAT3 delta = {
			body.GetPosition().x - center.x,
			0.0f,
			body.GetPosition().z - center.z
		};
		return (delta.x * delta.x + delta.z * delta.z) <= radiusSq;
	}

	bool ApplyShockwaveKnockbackToBody(GameObject& body,
		const XMFLOAT3& center,
		float radius,
		float baseImpulse,
		float lift,
		float impulseScale) {
		if (!body.IsActive() || !body.GetCollider()) return false;

		Rigidbody& rb = body.GetRigidbody();
		if (rb.IsKinematic() || rb.GetMass() <= 0.0f) return false;

		const float radiusSq = radius * radius;
		XMFLOAT3 delta = {
			body.GetPosition().x - center.x,
			0.0f,
			body.GetPosition().z - center.z
		};
		const float distSq = delta.x * delta.x + delta.z * delta.z;
		if (distSq > radiusSq) return false;

		float dist = sqrtf(distSq);
		XMFLOAT3 dir = { 1.0f, 0.0f, 0.0f };
		if (dist > 0.001f) {
			const float invDist = 1.0f / dist;
			dir = { delta.x * invDist, 0.0f, delta.z * invDist };
		}

		float falloff = 1.0f - (dist / radius);
		falloff = (std::max)(0.15f, falloff);
		const float scaledImpulse = baseImpulse * falloff * impulseScale;
		const float scaledLift = lift * falloff * impulseScale;
		rb.AddForce(
			{ dir.x * scaledImpulse, scaledLift, dir.z * scaledImpulse },
			ForceMode::VelocityChange);
		return true;
	}

	float ShockwaveImpulseScaleForBody(const GameObject& body,
		const GameObject* placerBody,
		float placerImpulseScale) {
		if (placerBody && &body == placerBody)
			return (std::max)(0.0f, placerImpulseScale);
		return 1.0f;
	}

	void ApplyShockwaveKnockbackToPlayerBody(GameObject& body,
		const XMFLOAT3& center,
		float radius,
		float baseImpulse,
		float lift,
		float knockbackDuration,
		const GameObject* placerBody,
		float placerImpulseScale) {
		if (!IsBodyInShockwaveRadius(body, center, radius))
			return;

		const float impulseScale = ShockwaveImpulseScaleForBody(
			body, placerBody, placerImpulseScale);
		PlayerScript_BeginKnockbackForBody(&body, knockbackDuration);
		ApplyShockwaveKnockbackToBody(
			body, center, radius, baseImpulse, lift, impulseScale);
	}
}

void ShockwavePoleItemScript::ApplyUniformShockwaveKnockback(const XMFLOAT3& center, float integrationDt) {
	const float radius = (std::max)(0.1f, poleShockwaveRadius);
	const float baseImpulse = (std::max)(0.0f, poleShockwaveImpulse);
	const float lift = (std::max)(0.0f, poleShockwaveLift);
	const float placerScale = (std::max)(0.0f, poleShockwavePlacerImpulseScale);
	const float knockbackDuration = (std::max)(0.05f, poleShockwaveKnockbackDuration);
	GameObject* const placerBody = m_placedByPlayer;

	EnemyAi_ApplyShockwaveImpulse(
		center, radius, baseImpulse, lift, knockbackDuration, placerBody, placerScale);
	ChaseTestEnemy_ApplyKnockbackInRadius(
		center, radius, baseImpulse, lift, knockbackDuration, integrationDt, placerBody, placerScale);

	if (GameObject* localPlayer = PlayerScript_GetBody()) {
		if (localPlayer->IsActive()) {
			ApplyShockwaveKnockbackToPlayerBody(
				*localPlayer, center, radius, baseImpulse, lift, knockbackDuration,
				placerBody, placerScale);
		}
	}

	SceneBase* scene = ScriptApi_GetCurrentScene();
	if (!scene) return;

	for (GameObject* obj : scene->GetObjects()) {
		if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_PLAYER) continue;
		if (obj == placerBody || obj == PlayerScript_GetBody()) continue;

		ApplyShockwaveKnockbackToPlayerBody(
			*obj, center, radius, baseImpulse, lift, knockbackDuration,
			placerBody, placerScale);
	}
}

void ShockwavePoleItemScript::OnUpdate(engine::GameObject& self, float dt) {
	if (!EngineGUI_IsPlaying()) return;

	GameObject* player = PlayerScript_GetBody();
	if (!player || !player->IsActive()) return;

	if (KeyLogger_IsTrigger(KK_K) && m_state != State::HeldByPlayer
		&& !ItemWorld_HolderHasItem(*player)) {
		m_heldItem = RollRandomHeldItem();
		ItemWorld_TryPickup(self, *player);
		return;
	}

	if (m_state == State::PlacedPendingShockwave) {
		const float maxRadius = (std::max)(0.1f, poleShockwaveRadius);
		const float totalDelay = (std::max)(0.01f, poleShockwaveDelay);
		const float progress = 1.0f - ((std::max)(0.0f, m_shockwaveTimer) / totalDelay);
		const float wireRadius = (std::max)(0.4f, maxRadius * progress);
		const XMFLOAT3 drawCenter = GetShockwaveDrawCenter();

		if (progress > 0.1f) {
			DrawShockwaveRing(drawCenter, wireRadius, 0.35f + 0.55f * progress);
			ScriptApi_DebugDrawSphereWire(
				drawCenter,
				{ 0.0f, 0.0f, 0.0f },
				wireRadius,
				{ 0.0f, 0.0f, 0.0f },
				{ 0.1f, 1.0f, 1.0f, 1.0f }
			);
		}

		m_shockwaveTimer -= dt;
		if (m_shockwaveTimer <= 0.0f) {
			ApplyUniformShockwaveKnockback(m_shockwaveCenter, dt);
			SyncTransform(self, m_shockwaveCenter);
			m_shockwaveVisualTimer = (std::max)(0.0f, poleShockwaveVisualDuration);
			m_placedLifetimeTimer = (std::max)(0.0f, polePlacedLifetime);
			m_state = State::PlacedActive;
		}
	}
	else if (m_state == State::PlacedActive) {
		if (m_shockwaveVisualTimer > 0.0f) {
			const float totalVisual = (std::max)(0.01f, poleShockwaveVisualDuration);
			const float glowFrac = m_shockwaveVisualTimer / totalVisual;
			const XMFLOAT3 drawCenter = GetShockwaveDrawCenter();
			const float ringRadius = (std::max)(0.1f, poleShockwaveRadius) * (0.35f + 0.65f * glowFrac);

			DrawShockwaveRing(drawCenter, ringRadius, glowFrac);

			ScriptApi_DebugDrawSphereWire(
				drawCenter,
				{ 0.0f, 0.0f, 0.0f },
				ringRadius,
				{ 0.0f, 0.0f, 0.0f },
				{ 0.1f, 1.0f, 1.0f, glowFrac }
			);
			m_shockwaveVisualTimer -= dt;
		}

		m_placedLifetimeTimer -= dt;
		if (m_placedLifetimeTimer <= 0.0f) {
			UnregisterMagneticPole();
			if (!ItemWorld_RequestRespawn(self))
				ResetStateForWorldPickup(self);
		}
	}

	SyncMagneticPoleRegistration(self);
}
