#pragma once

#include <DirectXMath.h>

class GameObject;

enum class ItemWorldKind {
    Unknown,
    ShockwavePole,
    SpeedReducePole,
    Mine,
    ArrowSticker
};

ItemWorldKind ItemWorld_GetKind(const GameObject& item);
bool ItemWorld_IsWorldPickup(const GameObject& item);
bool ItemWorld_IsMysteryInPlay(const GameObject& mystery);
bool ItemWorld_IsBackingReserved(const GameObject& backing);
void ItemWorld_DisconnectObject(GameObject& obj);
void ItemWorld_DisconnectObjectPtr(GameObject* obj);
void ItemWorld_DestroyRuntimeItem(GameObject& item);
void ItemWorld_ResetGameplaySession();
bool ItemWorld_IsHeldBy(const GameObject& item, const GameObject& holderBody);
bool ItemWorld_IsHeldByPlayer(const GameObject& item, const GameObject& body);
bool ItemWorld_IsPlayerHoldingItem(const GameObject& item);
GameObject* ItemWorld_GetHolder(const GameObject& item);
bool ItemWorld_Attach(GameObject& item, GameObject& holder, ItemWorldKind kind);
void ItemWorld_Detach(GameObject& holder);

// Authoritative held-item slot on the player body.
GameObject* ItemWorld_GetHeldItem(const GameObject& holder);
ItemWorldKind ItemWorld_GetHeldItemKind(const GameObject& holder);
bool ItemWorld_HolderHasItem(const GameObject& holder);
void ItemWorld_SetHeldItem(GameObject& holder, GameObject* item);
void ItemWorld_ClearHeldItem(GameObject& holder);
void ItemWorld_SyncHolderInventory(GameObject& holder);

bool ItemWorld_TryPickup(GameObject& item, GameObject& body);
bool ItemWorld_TryPlace(GameObject& item, GameObject& body, float yawDegrees);
bool ItemWorld_TryUseHeldItem(GameObject& holder, float yawDegrees);
void ItemWorld_TickHolder(GameObject& holder, float dt, float yawDegrees);
bool ItemWorld_RequestRespawn(GameObject& item);
void ItemWorld_ReleaseFieldItem(GameObject& item);
GameObject* ItemWorld_FindNearestWorldPickup(const DirectX::XMFLOAT3& from,
                                             float maxDistance,
                                             const GameObject* skipItem = nullptr);

void ItemWorld_RefreshHoldHud();

// On-screen pickup failure overlay (bottom-left, ~4s). Enabled by default in play.
void ItemWorld_SetPickupDebugEnabled(bool enabled);
bool ItemWorld_IsPickupDebugEnabled();
void ItemWorld_ShowPickupDebug(const char* message, float displaySeconds = 4.0f);

void ItemWorld_ApplyMysteryBoxAppearance(
    GameObject& item,
    const char* modelPath,
    const char* proxyModelPath,
    float uniformScale,
    float colliderHalfX,
    float colliderHalfY,
    float colliderHalfZ);
