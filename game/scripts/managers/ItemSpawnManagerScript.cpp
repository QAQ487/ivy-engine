#include "ItemSpawnManagerScript.h"
#include "ItemWorldApi.h"
#include "RoundManagerScript.h"
#include "ShockwavePoleItem.h"
#include "SpeedReducePoleItem.h"
#include "ArrowStickerItem.h"
#include "MineItem.h"
#include "MysteryBoxItem.h"
#include "PlayerScript.h"

#include "engine/physics/collider/collider.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace DirectX;

REGISTER_SCRIPT(ItemSpawnManagerScript)

namespace {
    ItemSpawnManagerScript* g_itemSpawnManager = nullptr;

    std::mt19937& ItemRng() {
        static std::mt19937 rng(std::random_device{}());
        return rng;
    }

    float Rand01() {
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(ItemRng());
    }

    XMFLOAT2 RandomOffsetInDisk(float maxRadius) {
        if (maxRadius <= 0.0f)
            return {0.0f, 0.0f};

        const float radius = maxRadius * sqrtf(Rand01());
        const float angle  = Rand01() * 6.2831853f;
        return { radius * cosf(angle), radius * sinf(angle) };
    }

    bool IsItemScriptClass(const std::string& scriptClass) {
        return scriptClass == "MysteryBoxItemScript"
            || scriptClass == "ShockwavePoleItemScript"
            || scriptClass == "SpeedReducePoleItemScript"
            || scriptClass == "MineItemScript"
            || scriptClass == "ArrowStickerItemScript";
    }
}

bool ItemSpawnManager_RequestRespawn(GameObject* itemNode) {
    if (!g_itemSpawnManager) return false;
    return g_itemSpawnManager->OnRequestRespawn(itemNode);
}

void ItemSpawnManager_ResetSession() {
    g_itemSpawnManager = nullptr;
}

void ItemSpawnManagerScript::OnCreate(engine::GameObject& self) {
    g_itemSpawnManager = this;
    ScriptApi_PrefabSetBeforeDestroyCallback(ItemWorld_DisconnectObjectPtr);
    m_itemPool.clear();
    CollectItemPool(&self);

    for (GameObject* item : m_itemPool) {
        if (item)
            item->SetActive(false);
    }
    m_spawnTimer = (std::max)(0.0f, spawnInitialDelay);
}

void ItemSpawnManagerScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    if (g_itemSpawnManager == this)
        g_itemSpawnManager = nullptr;
    m_itemPool.clear();
}

bool ItemSpawnManagerScript::FindLargestGroundBox(XMFLOAT3& outCenter, XMFLOAT3& outSize) const {
    outCenter = {0.0f, 0.0f, 0.0f};
    outSize   = {1.0f, 1.0f, 1.0f};

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return false;

    float bestArea = -1.0f;
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
            outCenter = center;
            outSize = size;
        }
    }

    return bestArea > 0.0f;
}

void ItemSpawnManagerScript::CollectItemPool(engine::GameObject* excludeSelf) {
    m_itemPool.clear();

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    if (!itemNodeName.empty()) {
        for (GameObject* obj : scene->GetObjects()) {
            if (!obj || obj == excludeSelf) continue;
            if (obj->GetName() == itemNodeName)
                m_itemPool.push_back(obj);
        }
        if (!m_itemPool.empty()) return;
    }

    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || obj == excludeSelf) continue;
        ScriptComponent* sc = obj->GetComponent<ScriptComponent>();
        if (!sc) continue;
        if (sc->GetScriptClass() != "MysteryBoxItemScript") continue;
        if (std::find(m_itemPool.begin(), m_itemPool.end(), obj) != m_itemPool.end())
            continue;
        m_itemPool.push_back(obj);
    }
    if (!m_itemPool.empty()) return;

    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || obj == excludeSelf) continue;
        ScriptComponent* sc = obj->GetComponent<ScriptComponent>();
        if (!sc) continue;
        if (!IsItemScriptClass(sc->GetScriptClass())) continue;
        if (std::find(m_itemPool.begin(), m_itemPool.end(), obj) != m_itemPool.end())
            continue;
        m_itemPool.push_back(obj);
    }
}

bool ItemSpawnManagerScript::IsItemSpawnable(const GameObject& item) const {
    if (item.IsActive())
        return false;
    if (ItemWorld_IsMysteryInPlay(item))
        return false;
    return true;
}

bool ItemSpawnManagerScript::IsPositionReachable(const XMFLOAT3& pos) const {
    XMFLOAT3 floorCenter = {};
    XMFLOAT3 floorSize = {};
    if (!FindLargestGroundBox(floorCenter, floorSize))
        return true;

    const float floorRadius = floorSize.x * 0.5f;
    const float clampedPadding = (std::max)(0.0f, itemSpawnPadding);
    const float maxReachRadius = (std::max)(0.0f, floorRadius - clampedPadding);
    if (maxReachRadius <= 0.0f)
        return false;

    const float dx = pos.x - floorCenter.x;
    const float dz = pos.z - floorCenter.z;
    return (dx * dx + dz * dz) <= maxReachRadius * maxReachRadius;
}

void ItemSpawnManagerScript::RespawnUnreachableMysteryBoxes() {
    for (GameObject* item : m_itemPool) {
        if (!item || !item->IsActive())
            continue;

        ScriptComponent* sc = item->GetComponent<ScriptComponent>();
        if (!sc || sc->GetScriptClass() != "MysteryBoxItemScript")
            continue;

        if (IsPositionReachable(item->GetPosition()))
            continue;

        item->SetActive(false);
        SpawnPickupAtRandomPosition(*item);
    }
}

void ItemSpawnManagerScript::SyncItemNodeTransform(GameObject& item,
                                                   const XMFLOAT3& worldPos) const {
    item.SetPosition(worldPos);
    if (engine::Transform* t = ScriptApi_FindTransform(item))
        t->localPosition = worldPos;
}

void ItemSpawnManagerScript::ApplyPickupAppearance(engine::GameObject& item) const {
    const std::string proxyPath = pickupProxyModelPath.empty()
        ? pickupModelPath
        : pickupProxyModelPath;

    ItemWorld_ApplyMysteryBoxAppearance(
        item,
        pickupModelPath.c_str(),
        proxyPath.c_str(),
        pickupScale,
        pickupColliderHalfX,
        pickupColliderHalfY,
        pickupColliderHalfZ);
}

void ItemSpawnManagerScript::ResetItemForWorldPickup(engine::GameObject& item) const {
    if (ScriptComponent* scriptComponent = item.GetComponent<ScriptComponent>()) {
        const std::string& scriptClass = scriptComponent->GetScriptClass();
        if (scriptClass == "MysteryBoxItemScript") {
            MysteryBoxItem_ResetForWorldPickup(item);
            return;
        }
        if (scriptClass == "SpeedReducePoleItemScript") {
            SpeedReducePoleItem_ResetForWorldPickup(item);
            return;
        }
        if (scriptClass == "MineItemScript") {
            MineItem_ResetForWorldPickup(item);
            return;
        }
        if (scriptClass == "ArrowStickerItemScript") {
            ArrowStickerItem_ResetForWorldPickup(item);
            return;
        }
    }
    ShockwavePoleItem_ResetForWorldPickup(item);
}

bool ItemSpawnManagerScript::SpawnPickupAtRandomPosition(GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (sc && sc->GetScriptClass() == "MysteryBoxItemScript")
        MysteryBox_PrepareForRespawn(item);

    if (ItemWorld_IsMysteryInPlay(item))
        return false;

    XMFLOAT3 floorCenter = {};
    XMFLOAT3 floorSize = {};
    if (!FindLargestGroundBox(floorCenter, floorSize))
        return false;

    const float floorRadius = floorSize.x * 0.5f;
    const float clampedPadding = (std::max)(0.0f, itemSpawnPadding);
    const float maxSpawnRadius = (std::max)(0.0f, floorRadius - clampedPadding);
    const XMFLOAT2 offset = RandomOffsetInDisk(maxSpawnRadius);
    const float spawnX = floorCenter.x + offset.x;
    const float spawnZ = floorCenter.z + offset.y;
    const float floorTop = floorCenter.y + floorSize.y * 0.5f;
    const float spawnY = floorTop + (std::max)(0.1f, itemSpawnHeightOffset);

    SyncItemNodeTransform(item, {spawnX, spawnY, spawnZ});
    item.SetActive(true);
    ResetItemForWorldPickup(item);
    ApplyPickupAppearance(item);
    return true;
}

void ItemSpawnManagerScript::SpawnNextMystery() {
    if (m_itemPool.empty())
        return;

    std::vector<GameObject*> candidates;
    candidates.reserve(m_itemPool.size());
    for (GameObject* item : m_itemPool) {
        if (item && IsItemSpawnable(*item))
            candidates.push_back(item);
    }

    if (candidates.empty())
        return;

    std::shuffle(candidates.begin(), candidates.end(), ItemRng());

    GameObject* item = candidates.front();
    if (item)
        SpawnPickupAtRandomPosition(*item);
}

void ItemSpawnManagerScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!EngineGUI_IsPlaying()) return;
    if (!RoundManager_IsGameplayActive()) return;

    if (m_itemPool.empty())
        CollectItemPool(&self);

    RespawnUnreachableMysteryBoxes();

    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.0f) {
        SpawnNextMystery();
        m_spawnTimer = (std::max)(1.0f, spawnInterval);
    }
}

bool ItemSpawnManagerScript::OnRequestRespawn(GameObject* itemNode) {
    if (itemNode)
        return SpawnPickupAtRandomPosition(*itemNode);

    if (m_itemPool.empty())
        return false;

    for (GameObject* item : m_itemPool) {
        if (item && IsItemSpawnable(*item))
            return SpawnPickupAtRandomPosition(*item);
    }

    return false;
}
