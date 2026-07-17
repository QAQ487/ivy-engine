/*****************************************************
 * File: game/scripts/managers/TrainingExitZoneScript.cpp
 *****************************************************/
#include "TrainingExitZoneScript.h"
#include "GameMode.h"
#include "PlayerScript.h"
#include "RoundManagerScript.h"
#include "ItemWorldApi.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/core/game_object.h"
#include "engine/physics/collider/collider.h"
#include "scene_manager.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

REGISTER_SCRIPT(TrainingExitZoneScript)

namespace {
constexpr float kBillboardSurfaceOffset = 0.05f;
constexpr float kPointScoreMinHeightAboveSurface = -1.0f;
constexpr float kPointScoreMaxHeightAboveSurface = 2.5f;
}

void TrainingExitZoneScript::HideMeshVisual(engine::GameObject& self) {
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        vc->modelPath.clear();
        vc->proxyModelPath.clear();
        vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

void TrainingExitZoneScript::UpdateFloorReference() {
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    float bestArea = -1.0f;
    m_floorObject = nullptr;
    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_GROUND) continue;
        Collider* collider = obj->GetCollider();
        if (!collider) continue;

        XMFLOAT3 center = {};
        XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
        if (!ScriptApi_TryGetColliderFootprint(*collider, center, size))
            continue;

        const float area = size.x * size.z;
        if (area > bestArea) {
            bestArea = area;
            m_floorObject = obj;
            m_floorCenter = center;
            m_floorSize = size;
        }
    }
}

float TrainingExitZoneScript::GetAreaSurfaceY() const {
    if (m_floorObject)
        return m_floorCenter.y + m_floorSize.y * 0.5f;
    return m_areaCenter.y;
}

bool TrainingExitZoneScript::IsStandingOnAreaSurface(const XMFLOAT3& pos) const {
    const float surfaceY = GetAreaSurfaceY();
    const float dy = pos.y - surfaceY;
    return dy >= kPointScoreMinHeightAboveSurface && dy <= kPointScoreMaxHeightAboveSurface;
}

bool TrainingExitZoneScript::IsPlayerInside(const XMFLOAT3& pos) const {
    const float dx = pos.x - m_areaCenter.x;
    const float dz = pos.z - m_areaCenter.z;
    const float radius = (std::max)(0.1f, m_effectiveAreaRadius);
    if ((dx * dx + dz * dz) > radius * radius)
        return false;
    return IsStandingOnAreaSurface(pos);
}

void TrainingExitZoneScript::SyncBillboardVisual(engine::GameObject& self) {
    BillboardComponent* billboard = self.GetComponent<BillboardComponent>();
    if (!billboard) return;

    const float diameter = (std::max)(0.2f, m_effectiveAreaRadius * 2.0f);
    billboard->sizeX = diameter;
    billboard->sizeY = diameter;
    billboard->groundAligned = true;
    billboard->enabled = true;

    if (m_transform) {
        m_transform->localPosition = {
            m_areaCenter.x,
            GetAreaSurfaceY() + kBillboardSurfaceOffset,
            m_areaCenter.z
        };
    }
}

void TrainingExitZoneScript::OnCreate(engine::GameObject& self) {
    GameMode_Set(GameMode::Training);

    m_transform = ScriptApi_FindTransform(self);
    m_effectiveAreaRadius = (std::max)(0.1f, areaRadius);
    m_stayTimer = 0.0f;
    m_exitRequested = false;

    if (m_transform)
        m_areaCenter = m_transform->localPosition;

    UpdateFloorReference();
    HideMeshVisual(self);
    SyncBillboardVisual(self);
}

void TrainingExitZoneScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    m_transform = nullptr;
    m_floorObject = nullptr;
}

void TrainingExitZoneScript::OnUpdate(engine::GameObject& self, float dt) {
    if (m_exitRequested) return;
    if (!RoundManager_IsGameplayActive()) return;

    UpdateFloorReference();
    if (m_transform)
        m_areaCenter = { m_transform->localPosition.x, m_areaCenter.y, m_transform->localPosition.z };
    m_effectiveAreaRadius = (std::max)(0.1f, areaRadius);
    SyncBillboardVisual(self);

    GameObject* player = PlayerScript_GetBody();
    if (!player || !player->IsActive() || PlayerScript_IsRespawning()) {
        m_stayTimer = 0.0f;
        return;
    }

    if (IsPlayerInside(player->GetPosition())) {
        m_stayTimer += (std::max)(0.0f, dt);
        if (m_stayTimer >= (std::max)(0.1f, requiredStaySeconds)) {
            m_exitRequested = true;
            ItemWorld_ResetGameplaySession();
            ScriptApi_SceneCanvasRequestSceneFile("scene_mode_select", true);
        }
    } else {
        m_stayTimer = 0.0f;
    }
}
