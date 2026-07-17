#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "ItemWorldApi.h"
#include <string>

class GameObject;

void MysteryBoxItem_ResetForWorldPickup(engine::GameObject& item);
void MysteryBox_PrepareForRespawn(engine::GameObject& mystery);
bool MysteryBox_TryRespawnOwner(GameObject* spawnedInstance);
GameObject* MysteryBox_ResolveHeldTarget(GameObject& item);
bool MysteryBox_IsMysteryInPlay(const GameObject& mystery);
bool MysteryBox_IsSpawnedInstanceReserved(const GameObject& instance);
bool MysteryBox_OwnsSpawnedInstance(const GameObject& instance);
void MysteryBox_OnSpawnedInstanceDestroying(GameObject* instance);
void MysteryBox_ClearSessionState();

class MysteryBoxItemScript : public Script {
#define MYSTERY_BOX_ITEM_SCRIPT_FIELDS(X) \
    X(float,       itemPickupRadius,        1.25f) \
    X(float,       heldHideY,               -1000.0f) \
    X(std::string, shockwavePrefabPath,     "rom/prefabs/shockwave_pole_item.json") \
    X(std::string, speedReducePrefabPath,   "rom/prefabs/speed_reduce_pole_item.json") \
    X(std::string, minePrefabPath,          "rom/prefabs/mine_item.json") \
    X(std::string, arrowStickerPrefabPath,  "rom/prefabs/arrow_sticker_item.json") \
    X(float,       rollWeightShockwave,     1.0f) \
    X(float,       rollWeightSpeedReduce,   1.0f) \
    X(float,       rollWeightMine,          1.0f) \
    X(float,       rollWeightArrowSticker,  1.0f) \
    X(float,       worldSpinSpeed,          90.0f)

public:
    MYSTERY_BOX_ITEM_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    void ResetStateForWorldPickup(engine::GameObject& self);

    bool IsWorldPickupState() const;
    bool IsHandedOffState() const;
    bool IsHeldByBody(const GameObject& body) const;
    bool TryPickupForBody(engine::GameObject& self, GameObject& body);
    bool TryPlaceForBody(engine::GameObject& self, GameObject& body, float yawDegrees);

    GameObject* GetSpawnedInstance() const { return m_spawnedInstance; }
    GameObject* GetLastHandedBacking() const { return m_spawnedInstance; }
    ItemWorldKind GetResolvedKind() const { return m_resolvedKind; }

    SCRIPT_SERIALIZE_METHODS(MYSTERY_BOX_ITEM_SCRIPT_FIELDS)

private:
    enum class State {
        MysteryWorld,
        HandedOff
    };

    ItemWorldKind RollResolvedKind() const;
    const std::string& PrefabPathForKind(ItemWorldKind kind) const;
    bool SpawnResolvedItem(engine::GameObject& self, GameObject& body, ItemWorldKind kind);
    bool IsBodyInPickupRange(GameObject& self, GameObject& body) const;
    void DrawWorldMarker(const DirectX::XMFLOAT3& itemPos,
                         engine::GameObject& self) const;

    State m_state = State::MysteryWorld;
    ItemWorldKind m_resolvedKind = ItemWorldKind::Unknown;
    GameObject* m_spawnedInstance = nullptr;
    float m_spinAngle = 0.0f;
};
