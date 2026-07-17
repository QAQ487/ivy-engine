#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>
#include <string>
#include <unordered_map>

class GameObject;

void ArrowStickerItem_ResetForWorldPickup(engine::GameObject& item);
void ArrowStickerItem_ReleaseFromField(engine::GameObject& item);

class ArrowStickerItemScript : public Script {
#define ARROW_STICKER_ITEM_SCRIPT_FIELDS(X) \
    X(float, itemPickupRadius,         1.25f) \
    X(float, stickerPlaceDistance,     2.0f) \
    X(float, stickerPlaceHeight,       0.02f) \
    X(float, stickerTriggerRadius,     1.1f) \
    X(float, stickerBoostImpulse,    110.0f) \
    X(float, stickerBoostLift,        12.0f) \
    X(float, stickerKnockbackDuration, 0.4f) \
    X(float, stickerLifetime,         15.0f) \
    X(float, stickerPerBodyCooldown,   5.0f) \
    X(float, heldHideY,             -1000.0f)

public:
    ARROW_STICKER_ITEM_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    void ResetStateForWorldPickup(engine::GameObject& self);
    void ReleaseFromField(engine::GameObject& self);

    bool IsWorldPickupState() const;
    bool IsHeldByBody(const GameObject& body) const;
    bool TryPickupForBody(engine::GameObject& self, GameObject& body);
    bool TryPlaceForBody(engine::GameObject& self, GameObject& body, float yawDegrees);

    SCRIPT_SERIALIZE_METHODS(ARROW_STICKER_ITEM_SCRIPT_FIELDS)

private:
    enum class State {
        MysteryBoxWorld,
        HeldByPlayer,
        PlacedActive
    };

    bool FindLargestGroundFootprint(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;
    float GetPlacedStickerY(const DirectX::XMFLOAT3& floorCenter, const DirectX::XMFLOAT3& floorSize) const;
    void SyncTransform(engine::GameObject& self, const DirectX::XMFLOAT3& worldPos);
    void MoveToHeldHidePosition(engine::GameObject& self);
    DirectX::XMFLOAT3 GetForwardFromYaw(float yawDegrees) const;
    void CachePrefabVisualPaths(GameObject& self);
    void ApplyWorldPickupAppearance(engine::GameObject& self);
    void ApplyPlacedCollider(engine::GameObject& self);
    bool IsWithinTriggerRadius(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& targetPos) const;
    bool IsBodyInPickupRange(GameObject& self, GameObject& body) const;
    void SyncBillboardVisual(engine::GameObject& self, float yawDegrees, float alpha, bool visible,
                             bool groundAligned = true);
    void TickTriggers(float dt);
    void TryTriggerForBody(GameObject* body);
    void ApplyDirectionalBoost(GameObject& body);
    void TickBodyCooldowns(float dt);

    State m_state = State::MysteryBoxWorld;
    float m_lifetimeTimer = 0.0f;
    float m_triggerFlashTimer = 0.0f;
    float m_placedYawDegrees = 0.0f;
    DirectX::XMFLOAT3 m_stickerCenter = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_boostDirection = {0.0f, 0.0f, 1.0f};
    GameObject* m_placedByPlayer = nullptr;
    std::string m_prefabModelPath;
    std::string m_prefabProxyModelPath;
    std::unordered_map<const GameObject*, float> m_bodyCooldownTimers;
};
