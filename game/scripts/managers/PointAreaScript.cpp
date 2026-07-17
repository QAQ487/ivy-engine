/*****************************************************
 * File: game/scripts/PointAreaScript.cpp
 * Project: d3d11_engine
 * Description: PointAreaScript implementation.
 *              See PointAreaScript.h for the full behaviour description.
 *****************************************************/
#include "PointAreaScript.h"
#include "EnemyAiScript.h"
#include "PlayerScript.h"
#include "RoundManagerScript.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "game_score.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/core/game_object.h"
#include "engine/physics/collider/collider.h"
#include "scene_manager.h"
#include "engine/core/engine_gui.h"
#include "engine/rendering/particle/particle_system.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

using namespace DirectX;

REGISTER_SCRIPT(PointAreaScript)

namespace {
XMFLOAT3 g_pointAreaCenter = {};
float    g_pointAreaRadius = 2.5f;
bool     g_pointAreaValid  = false;

float FloorRadiusFromSize(const XMFLOAT3& floorSize) {
    return floorSize.x * 0.5f;
}

XMFLOAT2 RandomOffsetInDisk(float maxRadius) {
    if (maxRadius <= 0.0f)
        return {0.0f, 0.0f};

    const float radius = maxRadius * sqrtf(static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
    const float angle  = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 6.2831853f;
    return { radius * cosf(angle), radius * sinf(angle) };
}

engine::ParticleEmitterConfig BuildPointAreaRiseEmitterConfig(float spawnRadius) {
    engine::ParticleEmitterConfig cfg;
    cfg.emissionRate = 50.0f;
    cfg.burstCount = 0;
    cfg.shape = engine::ParticleShape::Cone;
    cfg.radius = (std::max)(0.1f, spawnRadius * 0.85f);
    cfg.angle = 70.0f;
    cfg.startLifetimeMin = 1.0f;
    cfg.startLifetimeMax = 1.7f;
    cfg.startSpeedMin = 2.2f;
    cfg.startSpeedMax = 3.8f;
    cfg.startSizeMin = 0.08f;
    cfg.startSizeMax = 0.24f;
    cfg.startColorMin = { 1.0f, 0.98f, 0.82f, 0.75f };
    cfg.startColorMax = { 1.0f, 1.0f, 0.95f, 1.0f };
    cfg.gravityModifier = -0.04f;
    cfg.colorOverLifetimeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
    cfg.colorOverLifetimeEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
    cfg.sizeOverLifetimeStart = 1.0f;
    cfg.sizeOverLifetimeEnd = 0.2f;
    cfg.simulationSpace = engine::ParticleSimulationSpace::World;
    cfg.maxParticles = 500;
    cfg.looping = true;
    cfg.billboardBlendMode = engine::ParticleBillboardBlendMode::Additive;
    cfg.texturePath = "rom/texture/particle.png";
    cfg.textureSheetFrames = 1;
    cfg.textureSheetCycles = 1.0f;
    return cfg;
}
} // namespace

bool PointArea_GetCenter(XMFLOAT3& outCenter) {
    if (!g_pointAreaValid) return false;
    outCenter = g_pointAreaCenter;
    return true;
}

bool PointArea_IsInside(const XMFLOAT3& pos) {
    if (!g_pointAreaValid) return false;
    const float dx = pos.x - g_pointAreaCenter.x;
    const float dz = pos.z - g_pointAreaCenter.z;
    const float radiusSq = g_pointAreaRadius * g_pointAreaRadius;
    return (dx * dx + dz * dz) <= radiusSq;
}

static constexpr float kFallbackSpawnRange = 20.0f;
static constexpr float kDefaultAreaHalfHeight = 0.025f;
static constexpr float kBillboardSurfaceOffset = 0.05f;
static constexpr float kFloorReferenceRefreshInterval = 0.25f;
static constexpr float kPointScoreMinHeightAboveSurface = -1.0f;
static constexpr float kPointScoreMaxHeightAboveSurface = 2.5f;

float PointAreaScript::GetFloorRadius() const {
    return FloorRadiusFromSize(m_floorSize);
}

float PointAreaScript::GetEffectiveAreaRadius() const {
    const float baseRadius = (std::max)(0.1f, m_baseAreaRadius);
    if (m_initialFloorRadius <= 0.0001f)
        return baseRadius;

    const float scale = GetFloorRadius() / m_initialFloorRadius;
    return (std::max)(0.1f, baseRadius * scale);
}

float PointAreaScript::GetMaxSpawnRadius() const {
    const float marginRadius = GetEffectiveAreaRadius();
    if (m_floorObject) {
        return (std::max)(0.0f, GetFloorRadius() - marginRadius);
    }
    return (std::max)(0.0f, kFallbackSpawnRange - marginRadius);
}

float PointAreaScript::GetAreaSurfaceY() const {
    const float floorTopY = m_floorCenter.y + (m_floorSize.y * 0.5f);
    return m_floorObject ? (floorTopY + kBillboardSurfaceOffset) : kDefaultAreaHalfHeight;
}

bool PointAreaScript::IsStandingOnAreaSurface(const XMFLOAT3& pos) const {
    const float dy = pos.y - GetAreaSurfaceY();
    return dy >= kPointScoreMinHeightAboveSurface
        && dy <= kPointScoreMaxHeightAboveSurface;
}

void PointAreaScript::HideMeshVisual(engine::GameObject& self) {
    (void)self;
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        vc->modelPath.clear();
        vc->proxyModelPath.clear();
        vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

void PointAreaScript::SyncBillboardVisual(engine::GameObject& self) {
    if (auto* bc = self.GetComponent<BillboardComponent>()) {
        const float diameter = (std::max)(0.1f, m_effectiveAreaRadius * 2.0f);
        bc->sizeX = diameter;
        bc->sizeY = diameter;
        bc->enabled = true;
        bc->groundAligned = true;
    }
}

void PointAreaScript::EnsureRiseParticleEmitter() {
    const float spawnRadius = m_effectiveAreaRadius;
    if (m_riseParticleEmitterId != 0) {
        if (fabsf(spawnRadius - m_particleSpawnRadius) <= 0.01f)
            return;
        DestroyRiseParticleEmitter();
    }

    m_riseParticleEmitterId = ScriptApi_ParticleCreateEmitter(
        BuildPointAreaRiseEmitterConfig(spawnRadius), m_areaCenter);
    m_particleSpawnRadius = spawnRadius;
}

void PointAreaScript::SyncRiseParticleEmitter() {
    if (m_riseParticleEmitterId == 0)
        return;

    ScriptApi_ParticleSetEmitterPosition(m_riseParticleEmitterId, m_areaCenter);
    ScriptApi_ParticleSetEmitterEmitting(m_riseParticleEmitterId, true);
}

void PointAreaScript::DestroyRiseParticleEmitter() {
    if (m_riseParticleEmitterId == 0)
        return;

    ScriptApi_ParticleDestroyEmitter(m_riseParticleEmitterId);
    m_riseParticleEmitterId = 0;
    m_particleSpawnRadius = -1.0f;
}

void PointAreaScript::RandomizePosition()
{
    const XMFLOAT2 offset = RandomOffsetInDisk(GetMaxSpawnRadius());
    m_areaCenter.x = m_floorCenter.x + offset.x;
    m_areaCenter.z = m_floorCenter.z + offset.y;
    m_areaCenter.y = GetAreaSurfaceY();
    m_repositionTimer = repositionInterval;
}

void PointAreaScript::UpdateFloorReference()
{
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    float bestArea = -1.0f;
    GameObject* bestFloor = nullptr;
    XMFLOAT3 bestSize = m_floorSize;
    XMFLOAT3 bestCenter = m_floorCenter;

    for (auto* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive()) continue;
        if (obj->GetTag() != Tag::TAG_GROUND) continue;

        Collider* collider = obj->GetCollider();
        if (!collider) continue;

        XMFLOAT3 center = {};
        XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
        if (!ScriptApi_TryGetColliderFootprint(*collider, center, size))
            continue;

        const float area = size.x * size.z;
        if (area > bestArea) {
            bestArea = area;
            bestFloor = obj;
            bestSize = size;
            bestCenter = center;
        }
    }

    if (!bestFloor) return;
    m_floorObject = bestFloor;
    m_floorSize = bestSize;
    m_floorCenter = bestCenter;

    if (m_initialFloorRadius <= 0.0001f) {
        m_initialFloorRadius = FloorRadiusFromSize(m_floorSize);
        if (m_initialFloorRadius <= 0.0001f)
            m_initialFloorRadius = kFallbackSpawnRange;
    }
}

void PointAreaScript::SyncFloorFootprint()
{
    if (!m_floorObject || !m_floorObject->IsActive())
        return;

    Collider* collider = m_floorObject->GetCollider();
    if (!collider)
        return;

    XMFLOAT3 center = {};
    XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
    if (!ScriptApi_TryGetColliderFootprint(*collider, center, size))
        return;

    m_floorSize = size;
    m_floorCenter = center;
}

void PointAreaScript::TickFloorReference(float dt)
{
    m_floorRefreshTimer -= dt;
    if (!m_floorObject || m_floorRefreshTimer <= 0.0f) {
        UpdateFloorReference();
        m_floorRefreshTimer = kFloorReferenceRefreshInterval;
    } else {
        SyncFloorFootprint();
    }
}

void PointAreaScript::ClampAreaToFloor()
{
    if (!m_floorObject) return;

    const float maxDist = GetMaxSpawnRadius();
    float dx = m_areaCenter.x - m_floorCenter.x;
    float dz = m_areaCenter.z - m_floorCenter.z;
    const float distSq = dx * dx + dz * dz;

    if (distSq > maxDist * maxDist && distSq > 0.0001f) {
        const float invDist = maxDist / sqrtf(distSq);
        m_areaCenter.x = m_floorCenter.x + dx * invDist;
        m_areaCenter.z = m_floorCenter.z + dz * invDist;
    }

    m_areaCenter.y = GetAreaSurfaceY();
}

void PointAreaScript::OnCreate(engine::GameObject& self)
{
    m_transform = ScriptApi_FindTransform(self);
    m_baseAreaRadius = (std::max)(0.1f, areaRadius);
    m_effectiveAreaRadius = m_baseAreaRadius;
    m_initialFloorRadius = 0.0f;

    HideMeshVisual(self);
    SyncBillboardVisual(self);

    m_floorRefreshTimer = 0.0f;
    UpdateFloorReference();
    m_effectiveAreaRadius = GetEffectiveAreaRadius();
    RandomizePosition();

    if (m_transform)
        m_transform->localPosition = m_areaCenter;

    g_pointAreaCenter = m_areaCenter;
    g_pointAreaRadius = m_effectiveAreaRadius;
    g_pointAreaValid  = true;

    EnsureRiseParticleEmitter();
    SyncRiseParticleEmitter();
}

void PointAreaScript::OnUpdate(engine::GameObject& self, float dt)
{
    if (!EngineGUI_IsPlaying()) return;

    TickFloorReference(dt);
    m_effectiveAreaRadius = GetEffectiveAreaRadius();
    ClampAreaToFloor();

    m_repositionTimer -= dt;
    if (m_repositionTimer <= 0.0f)
        RandomizePosition();

    if (m_transform)
        m_transform->localPosition = m_areaCenter;

    g_pointAreaCenter = m_areaCenter;
    g_pointAreaRadius = m_effectiveAreaRadius;
    g_pointAreaValid  = true;

    SyncBillboardVisual(self);
    EnsureRiseParticleEmitter();
    SyncRiseParticleEmitter();

    if (!RoundManager_IsGameplayActive())
        return;

    const float radiusSq = m_effectiveAreaRadius * m_effectiveAreaRadius;
    bool playersInArea[kGameScoreMaxPlayers] = {};

    if (GameObject* playerBody = PlayerScript_GetBody()) {
        if (playerBody->IsActive()
            && RoundManager_IsPlayerEligibleForScoring(0)
            && !PlayerScript_IsRespawning()) {
            const XMFLOAT3 pos = playerBody->GetPosition();
            const float dx = pos.x - m_areaCenter.x;
            const float dz = pos.z - m_areaCenter.z;
            if ((dx * dx + dz * dz) <= radiusSq && IsStandingOnAreaSurface(pos))
                playersInArea[0] = true;
        }
    }

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || EnemyAi_IsRespawning(ai)) continue;
        GameObject* proxy = EnemyAi_GetPhysicsProxy(ai);
        if (!proxy || !proxy->IsActive()) continue;

        const int scoreIndex = ai->GetPlayerIndex();
        if (scoreIndex < 0 || scoreIndex >= kGameScoreMaxPlayers)
            continue;
        if (!RoundManager_IsPlayerEligibleForScoring(scoreIndex))
            continue;

        const XMFLOAT3 pos = proxy->GetPosition();
        const float dx = pos.x - m_areaCenter.x;
        const float dz = pos.z - m_areaCenter.z;
        if ((dx * dx + dz * dz) <= radiusSq && IsStandingOnAreaSurface(pos))
            playersInArea[scoreIndex] = true;
    }

    for (int playerIndex = 0; playerIndex < kGameScoreMaxPlayers; ++playerIndex) {
        if (!playersInArea[playerIndex]) {
            m_pointAccumulators[playerIndex] = 0.0f;
            continue;
        }

        m_pointAccumulators[playerIndex] += pointsPerSecond * dt;
        const int earned = static_cast<int>(m_pointAccumulators[playerIndex]);
        if (earned > 0) {
            GameScore_Add(playerIndex, earned);
            m_pointAccumulators[playerIndex] -= static_cast<float>(earned);
        }
    }
}

void PointAreaScript::OnDestroy(engine::GameObject& self)
{
    (void)self;
    DestroyRiseParticleEmitter();
    g_pointAreaValid = false;
}
