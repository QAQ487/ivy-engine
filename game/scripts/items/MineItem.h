#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "libraries/nlohmann/json.hpp"
#include <DirectXMath.h>

class GameObject;

void MineItem_ResetForWorldPickup(engine::GameObject& item);
void MineItem_ReleaseFromField(engine::GameObject& item);

class MineItemScript : public Script {
#define MINE_ITEM_SCRIPT_FIELDS(X) \
    X(float, itemPickupRadius,           1.25f) \
    X(float, minePlaceDistance,          2.0f) \
    X(float, minePlaceHeight,            0.15f) \
    X(float, mineArmDelay,               0.5f) \
    X(float, mineTriggerRadius,          1.25f) \
    X(float, mineStunDuration,           0.5f) \
    X(float, mineDetonateVisualDuration, 0.35f) \
    X(float, minePlacedLifetime,         60.0f) \
    X(float, heldHideY,                  -1000.0f)

public:
    MINE_ITEM_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    void ResetStateForWorldPickup(engine::GameObject& self);
    void ReleaseFromField(engine::GameObject& self);

    bool IsWorldPickupState() const;
    bool IsHeldByBody(const GameObject& body) const;
    bool TryPickupForBody(engine::GameObject& self, GameObject& body);
    bool TryPlaceForBody(engine::GameObject& self, GameObject& body, float yawDegrees);

    SCRIPT_SERIALIZE_METHODS(MINE_ITEM_SCRIPT_FIELDS)

private:
    enum class State {
        MysteryBoxWorld,
        HeldByPlayer,
        PlacedArming,
        PlacedArmed,
        Detonated
    };

    bool FindLargestGroundFootprint(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;
    float GetPlacedMineY(const DirectX::XMFLOAT3& floorCenter, const DirectX::XMFLOAT3& floorSize) const;
    void SyncTransform(engine::GameObject& self, const DirectX::XMFLOAT3& worldPos);
    void MoveToHeldHidePosition(engine::GameObject& self);
    DirectX::XMFLOAT3 GetForwardFromYaw(float yawDegrees) const;
    void ApplyMineAppearance(engine::GameObject& self, bool armed);
    void ApplyArmingAppearance(engine::GameObject& self, float armProgress);
    bool IsWithinTriggerRadius(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& targetPos) const;
    bool CheckForTrigger(const DirectX::XMFLOAT3& center) const;
    void Detonate(const DirectX::XMFLOAT3& center);
    void DrawArmingIndicator(const DirectX::XMFLOAT3& center, float progress) const;
    void DrawArmedIndicator(const DirectX::XMFLOAT3& center) const;
    void DrawDetonationBurst(const DirectX::XMFLOAT3& center, float alpha) const;
    bool IsBodyInPickupRange(GameObject& self, GameObject& body) const;

    State m_state = State::MysteryBoxWorld;
    float m_armTimer = 0.0f;
    float m_placedLifetimeTimer = 0.0f;
    float m_detonateVisualTimer = 0.0f;
    DirectX::XMFLOAT3 m_mineCenter = {0.0f, 0.0f, 0.0f};
    GameObject* m_placedByPlayer = nullptr;
};
