#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>

class GameObject;

enum class PoleHeldItemType {
    Pole = 0,
    Count
};

void ShockwavePoleItem_ResetForWorldPickup(engine::GameObject& item);
void ShockwavePoleItem_ReleaseFromField(engine::GameObject& item);

class ShockwavePoleItemScript : public Script {
#define SHOCKWAVE_POLE_ITEM_SCRIPT_FIELDS(X) \
    X(float, itemPickupRadius,              1.25f) \
    X(float, polePlaceDistance,             2.0f) \
    X(float, polePlaceHeight,               0.0f) \
    X(float, poleShockwaveDelay,            0.8f) \
    X(float, poleShockwaveRadius,          15.0f) \
    X(float, poleShockwaveImpulse,          100.0f) \
    X(float, poleShockwaveLift,              50.0f) \
    X(float, poleShockwaveKnockbackDuration, 0.45f) \
    X(float, poleShockwavePlacerImpulseScale, 0.3f) \
    X(float, poleShockwaveVisualDuration,    0.85f) \
    X(float, polePlacedLifetime,             20.0f) \
    X(float, heldHideY,                   -1000.0f)

public:
    SHOCKWAVE_POLE_ITEM_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    void ResetStateForWorldPickup(engine::GameObject& self);
    void ReleaseFromField(engine::GameObject& self);

    bool IsWorldPickupState() const;
    bool IsHeldByBody(const GameObject& body) const;
    bool TryPickupForBody(engine::GameObject& self, GameObject& body);
    bool TryPlaceForBody(engine::GameObject& self, GameObject& body, float yawDegrees);

    SCRIPT_SERIALIZE_METHODS(SHOCKWAVE_POLE_ITEM_SCRIPT_FIELDS)

private:
    enum class State {
        MysteryBoxWorld,
        HeldByPlayer,
        PlacedPendingShockwave,
        PlacedActive
    };

    bool FindLargestGroundBox(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;
    void SyncTransform(engine::GameObject& self, const DirectX::XMFLOAT3& worldPos);
    void MoveToHeldHidePosition(engine::GameObject& self);
    DirectX::XMFLOAT3 GetForwardFromYaw(float yawDegrees) const;
    void ApplyUniformShockwaveKnockback(const DirectX::XMFLOAT3& center, float integrationDt);
    bool IsMagnetActiveOnField() const;
    void SyncMagneticPoleRegistration(engine::GameObject& self);
    void UnregisterMagneticPole();
    void ApplyPoleAppearance(engine::GameObject& self);
    PoleHeldItemType RollRandomHeldItem() const;
    DirectX::XMFLOAT3 GetShockwaveDrawCenter() const;
    void DrawShockwaveRing(const DirectX::XMFLOAT3& center, float radius, float alpha) const;
    bool IsBodyInPickupRange(GameObject& self, GameObject& body) const;

    State m_state = State::MysteryBoxWorld;
    PoleHeldItemType m_heldItem = PoleHeldItemType::Pole;
    float m_shockwaveTimer = 0.0f;
    float m_shockwaveVisualTimer = 0.0f;
    float m_placedLifetimeTimer = 0.0f;
    DirectX::XMFLOAT3 m_shockwaveCenter = {0.0f, 0.0f, 0.0f};
    GameObject* m_placedByPlayer = nullptr;
    int m_magnetRegistryIndex = -1;
};
