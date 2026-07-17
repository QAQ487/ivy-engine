#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

class GameObject;

bool ItemSpawnManager_RequestRespawn(GameObject* itemNode = nullptr);
void ItemSpawnManager_ResetSession();

class ItemSpawnManagerScript : public Script {
#define ITEM_SPAWN_MANAGER_SCRIPT_FIELDS(X) \
    X(float,       itemSpawnPadding,      1.5f) \
    X(float,       itemSpawnHeightOffset, 0.75f) \
    X(float,       spawnInterval,         12.0f) \
    X(float,       spawnInitialDelay,     2.0f) \
    X(int,         spawnCountMin,         1) \
    X(int,         spawnCountMax,         1) \
    X(std::string, itemNodeName,          "") \
    X(std::string, pickupModelPath,       "rom/model/map objects/mystery_box.fbx") \
    X(std::string, pickupProxyModelPath,  "rom/model/map objects/mystery_box.fbx") \
    X(float,       pickupScale,           0.7f) \
    X(float,       pickupColliderHalfX,   0.35f) \
    X(float,       pickupColliderHalfY,   0.35f) \
    X(float,       pickupColliderHalfZ,   0.35f)

public:
    ITEM_SPAWN_MANAGER_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;
    bool OnRequestRespawn(GameObject* itemNode);

    SCRIPT_SERIALIZE_METHODS(ITEM_SPAWN_MANAGER_SCRIPT_FIELDS)

private:
    bool FindLargestGroundBox(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;
    void CollectItemPool(engine::GameObject* excludeSelf);
    bool IsItemSpawnable(const GameObject& item) const;
    bool IsPositionReachable(const DirectX::XMFLOAT3& pos) const;
    bool SpawnPickupAtRandomPosition(GameObject& item);
    void RespawnUnreachableMysteryBoxes();
    void SpawnNextMystery();
    void ApplyPickupAppearance(engine::GameObject& item) const;
    void ResetItemForWorldPickup(engine::GameObject& item) const;
    void SyncItemNodeTransform(GameObject& item, const DirectX::XMFLOAT3& worldPos) const;

    std::vector<GameObject*> m_itemPool;
    float m_spawnTimer = 0.0f;
};
