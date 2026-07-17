/*****************************************************
 * File: game/scripts/managers/TrainingExitZoneScript.h
 * Description: Fixed stay-zone; after requiredStaySeconds inside, return to mode select.
 *****************************************************/
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>

namespace engine { class Transform; }
class GameObject;
class BillboardComponent;

class TrainingExitZoneScript : public Script {
#define TRAINING_EXIT_ZONE_SCRIPT_FIELDS(X) \
    X(float, areaRadius,           4.0f) \
    X(float, requiredStaySeconds,  3.0f)

public:
    TRAINING_EXIT_ZONE_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    SCRIPT_SERIALIZE_METHODS(TRAINING_EXIT_ZONE_SCRIPT_FIELDS)

private:
    DirectX::XMFLOAT3  m_areaCenter = {0.0f, 0.0f, 0.0f};
    float              m_stayTimer = 0.0f;
    bool               m_exitRequested = false;
    engine::Transform* m_transform = nullptr;
    GameObject*        m_floorObject = nullptr;
    DirectX::XMFLOAT3  m_floorCenter = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3  m_floorSize = {50.0f, 1.0f, 50.0f};
    float              m_effectiveAreaRadius = 4.0f;

    void UpdateFloorReference();
    void SyncBillboardVisual(engine::GameObject& self);
    void HideMeshVisual(engine::GameObject& self);
    float GetAreaSurfaceY() const;
    bool IsStandingOnAreaSurface(const DirectX::XMFLOAT3& pos) const;
    bool IsPlayerInside(const DirectX::XMFLOAT3& pos) const;
};
