#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>

class GameObject;

void SpeedReducePoleItem_ResetForWorldPickup(engine::GameObject& item);
void SpeedReducePoleItem_ReleaseFromField(engine::GameObject& item);

class SpeedReducePoleItemScript : public Script {
#define SPEED_REDUCE_POLE_ITEM_SCRIPT_FIELDS(X) \
    X(float, itemPickupRadius,       1.25f) \
    X(float, polePlaceDistance,      2.0f) \
    X(float, polePlaceHeight,          0.0f) \
    X(float, polePlacedLifetime,      20.0f) \
    X(float, slowZoneRadius,           8.0f) \
    X(float, slowZoneMoveSpeedScale,   0.35f) \
    X(float, zoneKnockbackReduction,   0.8f) \
    X(float, testKnockbackImpulse,    45.0f) \
    X(float, testKnockbackLift,       12.0f) \
    X(float, testKnockbackDuration,    2.5f) \
    X(float, heldHideY,            -1000.0f)

public:
    SPEED_REDUCE_POLE_ITEM_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    void ResetStateForWorldPickup(engine::GameObject& self);
    void ReleaseFromField(engine::GameObject& self);

    bool IsWorldPickupState() const;
    bool IsHeldByBody(const GameObject& body) const;
    bool TryPickupForBody(engine::GameObject& self, GameObject& body);
    bool TryPlaceForBody(engine::GameObject& self, GameObject& body, float yawDegrees);

    SCRIPT_SERIALIZE_METHODS(SPEED_REDUCE_POLE_ITEM_SCRIPT_FIELDS)

private:
    enum class State {
        MysteryBoxWorld,
        HeldByPlayer,
        PlacedActive
    };

    bool FindLargestGroundBox(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;
    void SyncTransform(engine::GameObject& self, const DirectX::XMFLOAT3& worldPos);
    void MoveToHeldHidePosition(engine::GameObject& self);
    DirectX::XMFLOAT3 GetForwardFromYaw(float yawDegrees) const;
    bool IsMagnetActiveOnField() const;
    void SyncMagneticPoleRegistration(engine::GameObject& self);
    void UnregisterMagneticPole();
    void ApplyPoleAppearance(engine::GameObject& self);
    void ClearPlacerZoneEffects();
    bool IsPlacerInsideZone(const DirectX::XMFLOAT3& zoneCenter) const;
    float RetainedKnockbackScale() const;
    void TickPlacedZoneEffects(const DirectX::XMFLOAT3& zoneCenter);
    void DrawSlowZone(const DirectX::XMFLOAT3& center, float alpha) const;
    void RunKnockbackTest(GameObject& player);
    bool IsBodyInPickupRange(GameObject& self, GameObject& body) const;
    void TickZoneEffectsForOpponents(const DirectX::XMFLOAT3& zoneCenter);

    State m_state = State::MysteryBoxWorld;
    float m_placedLifetimeTimer = 0.0f;
    bool m_wasPlacerInZone = false;
    DirectX::XMFLOAT3 m_zoneCenter = {0.0f, 0.0f, 0.0f};
    GameObject* m_placedByPlayer = nullptr;
    int m_magnetRegistryIndex = -1;
};
