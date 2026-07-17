/*****************************************************
 * File: game/scripts/managers/TrainingItemSlotScript.h
 * Description: Fixed training pickup slot. Spawns typed item prefabs at authored
 *              positions with mystery-box-sized pickup volume and a mini item
 *              visual (not the mystery box mesh). Respawns after pickup.
 *****************************************************/
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>
#include <string>

class GameObject;

class TrainingItemSlotScript : public Script {
#define TRAINING_ITEM_SLOT_SCRIPT_FIELDS(X) \
    X(std::string, prefabPath,           "") \
    X(float,       respawnDelaySeconds,  2.0f) \
    X(float,       spawnHeightOffset,    0.0f) \
    X(float,       pickupScaleFactor,    0.333333f) \
    X(float,       pickupColliderHalfX,  0.35f) \
    X(float,       pickupColliderHalfY,  0.35f) \
    X(float,       pickupColliderHalfZ,  0.35f) \
    X(float,       worldSpinSpeed,       90.0f)

public:
    TRAINING_ITEM_SLOT_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    SCRIPT_SERIALIZE_METHODS(TRAINING_ITEM_SLOT_SCRIPT_FIELDS)

private:
    DirectX::XMFLOAT3 m_homePosition = {0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 m_baseScale = {1.0f, 1.0f, 1.0f};
    GameObject*       m_instance = nullptr;
    float             m_respawnTimer = -1.0f;
    float             m_spinAngle = 0.0f;
    bool              m_spawnedInitial = false;

    void CaptureHome(engine::GameObject& self);
    bool HasActiveWorldPickup() const;
    void SpawnAtHome();
    void ApplyTrainingPickupAppearance(GameObject& item);
    void RestoreBaseScale(GameObject& item) const;
    void ReleaseInstanceOwnership();
    void TickWorldSpin(float dt);
    void ResetItemForWorldPickup(GameObject& item) const;
};
