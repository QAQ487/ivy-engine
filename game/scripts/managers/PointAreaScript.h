/*****************************************************
 * File: game/scripts/PointAreaScript.h
 * Project: d3d11_engine
 * Description: PointAreaScript – a circular scoring zone rendered as a flat
 *              ground-aligned billboard disc. Visual diameter matches areaRadius.
 *
 *              Behaviour:
 *                - Randomly spawns on the platform floor (circular bounds).
 *                - Relocates to a new random position every repositionInterval
 *                  seconds.
 *                - Awards pointsPerSecond points per second to the player
 *                  while the player stands inside the circular area.
 *                - Tracks the ground collider each frame so it stays on the
 *                  floor surface even when RoundManagerScript shrinks the arena.
 *
 *              Prefab JSON example:
 *                "Billboard": {
 *                  "texture": "rom/texture/point_area_circle.png",
 *                  "sizeX": 5.0, "sizeY": 5.0,
 *                  "groundAligned": true
 *                },
 *                "Script": {
 *                  "scriptClass": "PointAreaScript",
 *                  "properties": {
 *                    "repositionInterval": 8.0,
 *                    "pointsPerSecond":    1.0,
 *                    "areaRadius":         2.5
 *                  }
 *                }
 *****************************************************/
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "engine/scene/scene_node.h"
#include "game_score.h"
#include <DirectXMath.h>

namespace engine { class Transform; }
class GameObject;
class BillboardComponent;

class PointAreaScript : public Script {
#define POINT_AREA_SCRIPT_FIELDS(X) \
    X(float, repositionInterval, 8.0f) \
    X(float, pointsPerSecond,    1.0f) \
    X(float, areaRadius,         2.5f)

public:
    POINT_AREA_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate (engine::GameObject& self) override;
    void OnUpdate (engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    SCRIPT_SERIALIZE_METHODS(POINT_AREA_SCRIPT_FIELDS)

private:
    DirectX::XMFLOAT3  m_areaCenter       = {0.0f, 0.0f, 0.0f};
    float              m_repositionTimer  = 0.0f;
    float              m_pointAccumulators[kGameScoreMaxPlayers] = {};
    engine::Transform* m_transform        = nullptr;
    GameObject*        m_floorObject      = nullptr;
    DirectX::XMFLOAT3  m_floorCenter      = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3  m_floorSize        = {50.0f, 1.0f, 50.0f};
    float              m_floorRefreshTimer = 0.0f;
    float              m_baseAreaRadius   = 2.5f;
    float              m_initialFloorRadius = 0.0f;
    float              m_effectiveAreaRadius = 2.5f;
    float              m_particleSpawnRadius = -1.0f;

    float GetFloorRadius() const;
    float GetEffectiveAreaRadius() const;
    float GetMaxSpawnRadius() const;
    float GetAreaSurfaceY() const;
    void  RandomizePosition();
    void  UpdateFloorReference();
    void  SyncFloorFootprint();
    void  TickFloorReference(float dt);
    void  ClampAreaToFloor();
    void  HideMeshVisual(engine::GameObject& self);
    void  SyncBillboardVisual(engine::GameObject& self);
    void  EnsureRiseParticleEmitter();
    void  SyncRiseParticleEmitter();
    void  DestroyRiseParticleEmitter();
    bool  IsStandingOnAreaSurface(const DirectX::XMFLOAT3& pos) const;

    std::uint32_t m_riseParticleEmitterId = 0;
};

bool PointArea_GetCenter(DirectX::XMFLOAT3& outCenter);
bool PointArea_IsInside(const DirectX::XMFLOAT3& pos);
