/*****************************************************
 * File: game/scripts/managers/TrainingItemSlotScript.cpp
 *****************************************************/
#include "TrainingItemSlotScript.h"
#include "ItemWorldApi.h"
#include "ShockwavePoleItem.h"
#include "SpeedReducePoleItem.h"
#include "ArrowStickerItem.h"
#include "MineItem.h"
#include "RoundManagerScript.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/physics/collider/collider.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

REGISTER_SCRIPT(TrainingItemSlotScript)

void TrainingItemSlotScript::ResetItemForWorldPickup(GameObject& item) const {
    ScriptComponent* scriptComponent = item.GetComponent<ScriptComponent>();
    if (!scriptComponent) return;

    const std::string& scriptClass = scriptComponent->GetScriptClass();
    if (scriptClass == "SpeedReducePoleItemScript") {
        SpeedReducePoleItem_ResetForWorldPickup(item);
        return;
    }
    if (scriptClass == "MineItemScript") {
        MineItem_ResetForWorldPickup(item);
        return;
    }
    if (scriptClass == "ArrowStickerItemScript") {
        ArrowStickerItem_ResetForWorldPickup(item);
        return;
    }
    ShockwavePoleItem_ResetForWorldPickup(item);
}

void TrainingItemSlotScript::CaptureHome(engine::GameObject& self) {
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        m_homePosition = t->localPosition;
    } else {
        m_homePosition = self.GetPosition();
    }
    m_homePosition.y += (std::max)(0.0f, spawnHeightOffset);
}

bool TrainingItemSlotScript::HasActiveWorldPickup() const {
    return m_instance && m_instance->IsActive() && ItemWorld_IsWorldPickup(*m_instance);
}

void TrainingItemSlotScript::ApplyTrainingPickupAppearance(GameObject& item) {
    // Keep the prefab's item mesh (tower/mine/etc). Only shrink the visual.
    const float factor = (std::max)(0.01f, pickupScaleFactor);
    XMFLOAT3 visualScale = {
        m_baseScale.x * factor,
        m_baseScale.y * factor,
        m_baseScale.z * factor
    };
    item.SetScale(visualScale);
    if (engine::Transform* t = ScriptApi_FindTransform(item))
        t->localScale = visualScale;

    // Match original mystery-box pickup volume (absolute size, not tied to mini visual).
    const float hx = (std::max)(0.01f, pickupColliderHalfX);
    const float hy = (std::max)(0.01f, pickupColliderHalfY);
    const float hz = (std::max)(0.01f, pickupColliderHalfZ);
    const XMFLOAT3 boxSize = {hx * 2.0f, hy * 2.0f, hz * 2.0f};

    if (auto* box = dynamic_cast<BoxCollider*>(item.GetCollider())) {
        ScriptApi_SetBoxColliderSize(*box, boxSize);
        box->SetIsTrigger(true);
    } else {
        ScriptApi_DestroyCollider(item);
        if (BoxCollider* created = ScriptApi_CreateBoxCollider(item, boxSize, {0.0f, 0.0f, 0.0f}))
            created->SetIsTrigger(true);
    }
}

void TrainingItemSlotScript::RestoreBaseScale(GameObject& item) const {
    item.SetScale(m_baseScale);
    if (engine::Transform* t = ScriptApi_FindTransform(item))
        t->localScale = m_baseScale;
}

void TrainingItemSlotScript::ReleaseInstanceOwnership() {
    if (m_instance && m_instance->IsActive())
        RestoreBaseScale(*m_instance);
    m_instance = nullptr;
}

void TrainingItemSlotScript::TickWorldSpin(float dt) {
    if (!m_instance || !HasActiveWorldPickup())
        return;

    const float spinSpeed = (std::max)(0.0f, worldSpinSpeed);
    if (spinSpeed <= 0.0f)
        return;

    m_spinAngle += spinSpeed * dt;
    if (m_spinAngle >= 360.0f)
        m_spinAngle = fmodf(m_spinAngle, 360.0f);

    XMFLOAT3 rot = m_instance->GetRotation();
    rot.y = m_spinAngle;
    m_instance->SetRotation(rot);
    if (engine::Transform* t = ScriptApi_FindTransform(*m_instance))
        t->localRotation.y = m_spinAngle;
}

void TrainingItemSlotScript::SpawnAtHome() {
    if (prefabPath.empty())
        return;

    GameObject* spawned = ScriptApi_PrefabInstantiate(
        prefabPath.c_str(),
        m_homePosition,
        {0.0f, 0.0f, 0.0f},
        nullptr,
        true);
    if (!spawned)
        return;

    ResetItemForWorldPickup(*spawned);

    // Capture the item's normal scale before mini-visual shrink so place/hold
    // can restore full-size models (same as non-training).
    if (engine::Transform* t = ScriptApi_FindTransform(*spawned))
        m_baseScale = t->localScale;
    else
        m_baseScale = spawned->GetScale();

    ApplyTrainingPickupAppearance(*spawned);
    spawned->SetActive(true);
    spawned->SetPosition(m_homePosition);
    if (engine::Transform* t = ScriptApi_FindTransform(*spawned))
        t->localPosition = m_homePosition;

    m_instance = spawned;
    m_spinAngle = 0.0f;
    m_respawnTimer = -1.0f;
}

void TrainingItemSlotScript::OnCreate(engine::GameObject& self) {
    CaptureHome(self);
    m_instance = nullptr;
    m_respawnTimer = -1.0f;
    m_spinAngle = 0.0f;
    m_spawnedInitial = false;
    m_baseScale = {1.0f, 1.0f, 1.0f};
    self.SetActive(true);
}

void TrainingItemSlotScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    m_instance = nullptr;
}

void TrainingItemSlotScript::OnUpdate(engine::GameObject& self, float dt) {
    (void)self;
    if (!EngineGUI_IsPlaying()) return;
    if (!RoundManager_IsGameplayActive()) return;
    if (prefabPath.empty()) return;

    if (!m_spawnedInitial) {
        SpawnAtHome();
        m_spawnedInitial = true;
        return;
    }

    if (HasActiveWorldPickup()) {
        m_respawnTimer = -1.0f;
        TickWorldSpin(dt);
        return;
    }

    // Picked up / consumed: undo mini scale so held/placed items use normal size.
    ReleaseInstanceOwnership();

    if (m_respawnTimer < 0.0f)
        m_respawnTimer = (std::max)(0.0f, respawnDelaySeconds);

    m_respawnTimer -= dt;
    if (m_respawnTimer <= 0.0f)
        SpawnAtHome();
}
