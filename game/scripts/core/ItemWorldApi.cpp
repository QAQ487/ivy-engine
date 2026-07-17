#include "ItemWorldApi.h"

#include "ItemSpawnManagerScript.h"
#include "CenterPoleScript.h"
#include "ArrowStickerItem.h"
#include "MineItem.h"
#include "MysteryBoxItem.h"
#include "ShockwavePoleItem.h"
#include "SpeedReducePoleItem.h"
#include "engine/physics/collider/collider.h"
#include "engine/core/game_object.h"
#include "engine/ecs/visual_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ui/ui_sprite.h"
#include "engine/input/key_logger.h"
#include "scene_manager.h"
#include "PlayerScript.h"
#include "EnemyAiScript.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include <Windows.h>
using namespace DirectX;

namespace {

std::unordered_map<const GameObject*, GameObject*> g_itemHolders;

bool g_pickupDebugEnabled = true;
float g_pickupDebugThrottle = 0.0f;

float DistSqXZ(const XMFLOAT3& a, const XMFLOAT3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

ShockwavePoleItemScript* GetShockwaveScript(GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<ShockwavePoleItemScript*>(sc->GetScript());
}

SpeedReducePoleItemScript* GetSpeedReduceScript(GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<SpeedReducePoleItemScript*>(sc->GetScript());
}

MineItemScript* GetMineScript(GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<MineItemScript*>(sc->GetScript());
}

ArrowStickerItemScript* GetArrowStickerScript(GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<ArrowStickerItemScript*>(sc->GetScript());
}

MysteryBoxItemScript* GetMysteryBoxScript(GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<MysteryBoxItemScript*>(sc->GetScript());
}

void ExplainNearbyMysteryPickup(const GameObject& holder, float scanRadius) {
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    const XMFLOAT3 from = holder.GetPosition();
    const float scanDistSq = scanRadius * scanRadius;
    const float explainRadiusSq = (scanRadius + 1.5f) * (scanRadius + 1.5f);

    GameObject* nearest = nullptr;
    float nearestDistSq = explainRadiusSq;

    for (GameObject* obj : scene->GetObjects()) {
        if (!obj) continue;
        if (!GetMysteryBoxScript(*obj)) continue;

        const float distSq = DistSqXZ(from, obj->GetPosition());
        if (distSq >= nearestDistSq)
            continue;

        nearest = obj;
        nearestDistSq = distSq;
    }

    if (!nearest)
        return;

    char buf[384];
    const float dist = std::sqrt(nearestDistSq);
    const char* name = nearest->GetName().c_str();

    if (!nearest->IsActive()) {
        snprintf(buf, sizeof(buf), "Pickup: %s inactive (dist %.1f)", name, dist);
    } else if (MysteryBoxItemScript* mystery = GetMysteryBoxScript(*nearest)) {
        if (mystery->IsHandedOffState()) {
            snprintf(buf, sizeof(buf), "Pickup: %s already handed off (dist %.1f)", name, dist);
        } else if (!mystery->IsWorldPickupState()) {
            snprintf(buf, sizeof(buf), "Pickup: %s not world state (dist %.1f)", name, dist);
        } else if (nearestDistSq > scanDistSq) {
            snprintf(buf, sizeof(buf),
                "Pickup: %s at %.1fm but scan %.1fm (walk closer)",
                name, dist, scanRadius);
        } else {
            snprintf(buf, sizeof(buf),
                "Pickup: %s selected but TryPickup failed (see mystery msg)",
                name);
        }
    } else {
        snprintf(buf, sizeof(buf), "Pickup: %s blocked (dist %.1f)", name, dist);
    }

    ItemWorld_ShowPickupDebug(buf, 3.0f);
}

PlayerScript* GetPlayerScript(GameObject& body) {
    ScriptComponent* sc = body.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<PlayerScript*>(sc->GetScript());
}

const PlayerScript* GetPlayerScript(const GameObject& body) {
    return GetPlayerScript(const_cast<GameObject&>(body));
}

EnemyAiScript* GetEnemyAiScript(GameObject& body) {
    if (EnemyAiScript* ai = EnemyAi_FindByPhysicsProxy(&body))
        return ai;
    ScriptComponent* sc = body.GetComponent<ScriptComponent>();
    if (!sc) return nullptr;
    return dynamic_cast<EnemyAiScript*>(sc->GetScript());
}

const EnemyAiScript* GetEnemyAiScript(const GameObject& body) {
    return GetEnemyAiScript(const_cast<GameObject&>(body));
}

ItemWorldKind ResolveHeldKind(GameObject* item) {
    if (!item) return ItemWorldKind::Unknown;

    GameObject* resolved = MysteryBox_ResolveHeldTarget(*item);
    ItemWorldKind kind = ItemWorld_GetKind(*resolved);
    if (kind != ItemWorldKind::Unknown)
        return kind;

    return ItemWorld_GetKind(*item);
}

const wchar_t* kItemIconAtlasPath = L"rom/texture/UI/items/item_icon.PNG";

// 2x2 atlas: TL mine, TR shockwave, BL speed reduce, BR arrow
bool SourceRectForKind(ItemWorldKind kind, int atlasW, int atlasH, int& outX, int& outY, int& outW, int& outH) {
    if (atlasW <= 0 || atlasH <= 0)
        return false;

    const int cellW = atlasW / 2;
    const int cellH = atlasH / 2;
    outW = cellW;
    outH = cellH;

    switch (kind) {
        case ItemWorldKind::Mine:
            outX = 0;
            outY = 0;
            return true;
        case ItemWorldKind::ShockwavePole:
            outX = cellW;
            outY = 0;
            return true;
        case ItemWorldKind::SpeedReducePole:
            outX = 0;
            outY = cellH;
            return true;
        case ItemWorldKind::ArrowSticker:
            outX = cellW;
            outY = cellH;
            return true;
        default:
            return false;
    }
}

} // namespace

ItemWorldKind ItemWorld_GetKind(const GameObject& item) {
    const ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return ItemWorldKind::Unknown;

    const std::string& cls = sc->GetScriptClass();
    if (cls == "ShockwavePoleItemScript") return ItemWorldKind::ShockwavePole;
    if (cls == "SpeedReducePoleItemScript") return ItemWorldKind::SpeedReducePole;
    if (cls == "MineItemScript") return ItemWorldKind::Mine;
    if (cls == "ArrowStickerItemScript") return ItemWorldKind::ArrowSticker;
    if (cls == "MysteryBoxItemScript") return ItemWorldKind::Unknown;
    return ItemWorldKind::Unknown;
}

bool ItemWorld_IsWorldPickup(const GameObject& item) {
    if (!item.IsActive()) return false;
    if (const auto* mystery = GetMysteryBoxScript(const_cast<GameObject&>(item)))
        return mystery->IsWorldPickupState();
    if (const auto* shock = GetShockwaveScript(const_cast<GameObject&>(item)))
        return shock->IsWorldPickupState();
    if (const auto* slow = GetSpeedReduceScript(const_cast<GameObject&>(item)))
        return slow->IsWorldPickupState();
    if (const auto* mine = GetMineScript(const_cast<GameObject&>(item)))
        return mine->IsWorldPickupState();
    if (const auto* sticker = GetArrowStickerScript(const_cast<GameObject&>(item)))
        return sticker->IsWorldPickupState();
    return false;
}

bool ItemWorld_IsMysteryInPlay(const GameObject& mystery) {
    return MysteryBox_IsMysteryInPlay(mystery);
}

bool ItemWorld_IsBackingReserved(const GameObject& backing) {
    return MysteryBox_IsSpawnedInstanceReserved(backing);
}

void ItemWorld_DisconnectObjectPtr(GameObject* obj) {
    if (obj)
        ItemWorld_DisconnectObject(*obj);
}

void ItemWorld_DisconnectObject(GameObject& obj) {
    if (GameObject* holder = ItemWorld_GetHolder(obj))
        ItemWorld_ClearHeldItem(*holder);
    g_itemHolders.erase(&obj);
    MysteryBox_OnSpawnedInstanceDestroying(&obj);
}

void ItemWorld_ResetGameplaySession() {
    PlayerScript_ResetSessionState();
    EnemyAi_ResetSessionState();
    MysteryBox_ClearSessionState();
    g_itemHolders.clear();
    ItemSpawnManager_ResetSession();
    CenterPole_ResetSessionState();
}

extern "C" __declspec(dllexport) void GameScripts_ResetGameplaySession() {
    ItemWorld_ResetGameplaySession();
}

void ItemWorld_DestroyRuntimeItem(GameObject& item) {
    if (!MysteryBox_OwnsSpawnedInstance(item)) {
        item.SetActive(false);
        return;
    }
    ItemWorld_DisconnectObject(item);
    ScriptApi_PrefabDestroy(&item);
}

GameObject* ItemWorld_GetHeldItem(const GameObject& holder) {
    if (const PlayerScript* player = GetPlayerScript(holder))
        return player->GetHeldItemObject();
    if (const EnemyAiScript* ai = GetEnemyAiScript(holder))
        return ai->GetHeldItemObject();
    return nullptr;
}

ItemWorldKind ItemWorld_GetHeldItemKind(const GameObject& holder) {
    if (const PlayerScript* player = GetPlayerScript(holder))
        return player->GetHeldItemKind();
    if (const EnemyAiScript* ai = GetEnemyAiScript(holder))
        return ai->GetHeldItemKind();
    return ItemWorldKind::Unknown;
}

bool ItemWorld_HolderHasItem(const GameObject& holder) {
    return ItemWorld_GetHeldItem(holder) != nullptr;
}

void ItemWorld_SetHeldItem(GameObject& holder, GameObject* item) {
    const ItemWorldKind kind = ResolveHeldKind(item);
    if (PlayerScript* player = GetPlayerScript(holder)) {
        player->SetHeldItem(item, kind);
        return;
    }
    if (EnemyAiScript* ai = GetEnemyAiScript(holder))
        ai->SetHeldItem(item, kind);
}

void ItemWorld_ClearHeldItem(GameObject& holder) {
    if (GameObject* held = ItemWorld_GetHeldItem(holder))
        g_itemHolders.erase(held);
    ItemWorld_SetHeldItem(holder, nullptr);
}

GameObject* ItemWorld_GetHolder(const GameObject& item) {
    auto it = g_itemHolders.find(&item);
    if (it == g_itemHolders.end())
        return nullptr;
    return it->second;
}

bool ItemWorld_IsPlayerHoldingItem(const GameObject& item) {
    GameObject* player = PlayerScript_GetBody();
    if (!player) return false;
    return ItemWorld_GetHeldItem(*player) == &item;
}

bool ItemWorld_IsHeldByPlayer(const GameObject& item, const GameObject& body) {
    GameObject* player = PlayerScript_GetBody();
    if (!player || player != &body) return false;
    return ItemWorld_IsPlayerHoldingItem(item);
}

bool ItemWorld_Attach(GameObject& item, GameObject& holder, ItemWorldKind kind) {
    if (ItemWorld_HolderHasItem(holder))
        return false;

    if (GameObject* previousHolder = ItemWorld_GetHolder(item)) {
        ItemWorld_Detach(*previousHolder);
    }

    g_itemHolders[&item] = &holder;
    ItemWorld_SetHeldItem(holder, &item);
    (void)kind;
    return true;
}

void ItemWorld_Detach(GameObject& holder) {
    GameObject* held = ItemWorld_GetHeldItem(holder);
    if (held)
        g_itemHolders.erase(held);
    ItemWorld_ClearHeldItem(holder);
}

void ItemWorld_SyncHolderInventory(GameObject& holder) {
    (void)holder;
}

bool ItemWorld_IsHeldBy(const GameObject& item, const GameObject& holderBody) {
    return ItemWorld_GetHolder(item) == &holderBody;
}

bool ItemWorld_TryPickup(GameObject& item, GameObject& body) {
    if (ItemWorld_HolderHasItem(body)) {
        if (ItemWorld_IsPickupDebugEnabled() && GetMysteryBoxScript(item)) {
            ItemWorld_ShowPickupDebug("Pickup: already holding an item", 2.5f);
        }
        return false;
    }

    if (auto* mystery = GetMysteryBoxScript(item))
        return mystery->TryPickupForBody(item, body);

    bool picked = false;
    if (auto* shock = GetShockwaveScript(item))
        picked = shock->TryPickupForBody(item, body);
    else if (auto* slow = GetSpeedReduceScript(item))
        picked = slow->TryPickupForBody(item, body);
    else if (auto* mine = GetMineScript(item))
        picked = mine->TryPickupForBody(item, body);
    else if (auto* sticker = GetArrowStickerScript(item))
        picked = sticker->TryPickupForBody(item, body);

    if (!picked)
        return false;

    GameObject* held = MysteryBox_ResolveHeldTarget(item);
    const ItemWorldKind kind = ResolveHeldKind(held);
    return held && ItemWorld_Attach(*held, body, kind);
}

bool ItemWorld_TryUseHeldItem(GameObject& holder, float yawDegrees) {
    GameObject* held = ItemWorld_GetHeldItem(holder);
    if (!held) return false;
    return ItemWorld_TryPlace(*held, holder, yawDegrees);
}

bool ItemWorld_TryPlace(GameObject& item, GameObject& body, float yawDegrees) {
    GameObject* held = ItemWorld_GetHeldItem(body);
    if (!held)
        return false;

    GameObject* resolved = MysteryBox_ResolveHeldTarget(item);
    if (held != &item && held != resolved)
        return false;

    bool placed = false;
    if (GetMysteryBoxScript(*held))
        placed = false;
    else if (auto* shock = GetShockwaveScript(*held))
        placed = shock->TryPlaceForBody(*held, body, yawDegrees);
    else if (auto* slow = GetSpeedReduceScript(*held))
        placed = slow->TryPlaceForBody(*held, body, yawDegrees);
    else if (auto* mine = GetMineScript(*held))
        placed = mine->TryPlaceForBody(*held, body, yawDegrees);
    else if (auto* sticker = GetArrowStickerScript(*held))
        placed = sticker->TryPlaceForBody(*held, body, yawDegrees);

    if (!placed)
        return false;

    ItemWorld_Detach(body);
    return true;
}

void ItemWorld_SetPickupDebugEnabled(bool enabled) {
    g_pickupDebugEnabled = enabled;
}

bool ItemWorld_IsPickupDebugEnabled() {
    return g_pickupDebugEnabled;
}

void ItemWorld_ShowPickupDebug(const char* message, float displaySeconds) {
    if (!g_pickupDebugEnabled || !message || !message[0])
        return;

    OutputDebugStringA("[ItemPickup] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    ScriptApi_PickupDebugSetMessage(message, displaySeconds);
}

void ItemWorld_TickHolder(GameObject& holder, float dt, float yawDegrees) {
    if (!GetPlayerScript(holder))
        return;

    if (g_pickupDebugThrottle > 0.0f)
        g_pickupDebugThrottle -= (std::max)(0.0f, dt);

    if (!ItemWorld_HolderHasItem(holder)) {
        constexpr float kPickupScanRadius = 2.5f;
        GameObject* pickup = ItemWorld_FindNearestWorldPickup(
            holder.GetPosition(), kPickupScanRadius, nullptr);
        if (pickup) {
            if (!ItemWorld_TryPickup(*pickup, holder) && g_pickupDebugThrottle <= 0.0f) {
                g_pickupDebugThrottle = 0.75f;
            }
        } else if (ItemWorld_IsPickupDebugEnabled() && g_pickupDebugThrottle <= 0.0f) {
            ExplainNearbyMysteryPickup(holder, kPickupScanRadius);
            g_pickupDebugThrottle = 0.75f;
        }
    } else if (KeyLogger_IsTrigger(KK_Q)) {
        ItemWorld_TryUseHeldItem(holder, yawDegrees);
    }

    ItemWorld_RefreshHoldHud();
}

GameObject* ItemWorld_FindNearestWorldPickup(const XMFLOAT3& from,
                                             float maxDistance,
                                             const GameObject* skipItem) {
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return nullptr;

    const float maxDistSq = maxDistance * maxDistance;
    GameObject* best = nullptr;
    float bestDistSq = maxDistSq;

    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive() || obj == skipItem) continue;
        if (!ItemWorld_IsWorldPickup(*obj)) continue;

        const float distSq = DistSqXZ(from, obj->GetPosition());
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = obj;
        }
    }

    return best;
}

bool ItemWorld_RequestRespawn(GameObject& item) {
    if (MysteryBox_TryRespawnOwner(&item))
        return true;
    return ItemSpawnManager_RequestRespawn(&item);
}

void ItemWorld_RefreshHoldHud() {
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas) return;

    UIElement* element = ScriptApi_UICanvasGetElement(*canvas, "HoldItemIcon");
    if (!element)
        element = ScriptApi_UICanvasGetElement(*canvas, "HoldItemText");
    auto* holdIcon = dynamic_cast<UISprite*>(element);
    if (!holdIcon) return;

    GameObject* player = PlayerScript_GetBody();
    if (!player || !player->IsActive()) {
        ScriptApi_UIElementSetVisible(*holdIcon, false);
        return;
    }

    if (!ItemWorld_HolderHasItem(*player)) {
        ScriptApi_UIElementSetVisible(*holdIcon, false);
        return;
    }

    static ItemWorldKind lastHudKind = ItemWorldKind::Unknown;
    const ItemWorldKind kind = ItemWorld_GetHeldItemKind(*player);
    if (kind == ItemWorldKind::Unknown) {
        ScriptApi_UIElementSetVisible(*holdIcon, false);
        lastHudKind = ItemWorldKind::Unknown;
        return;
    }

    const wchar_t* texturePath = kItemIconAtlasPath;
    int srcX = 0, srcY = 0, srcW = 0, srcH = 0;
    const int atlasTex = ScriptApi_TextureLoad(texturePath, false);
    const unsigned int atlasW = ScriptApi_TextureGetWidth(atlasTex);
    const unsigned int atlasH = ScriptApi_TextureGetHeight(atlasTex);
    if (!SourceRectForKind(kind, (int)atlasW, (int)atlasH, srcX, srcY, srcW, srcH)) {
        ScriptApi_UIElementSetVisible(*holdIcon, false);
        lastHudKind = ItemWorldKind::Unknown;
        return;
    }

    if (kind != lastHudKind) {
        ScriptApi_UISpriteSetTexture(*holdIcon, texturePath);
        ScriptApi_UISpriteSetSourceRect(*holdIcon, srcX, srcY, srcW, srcH);
        lastHudKind = kind;
    }
    ScriptApi_UIElementSetVisible(*holdIcon, true);
}

void ItemWorld_ReleaseFieldItem(GameObject& item) {
    switch (ItemWorld_GetKind(item)) {
        case ItemWorldKind::ShockwavePole:
            ShockwavePoleItem_ReleaseFromField(item);
            break;
        case ItemWorldKind::SpeedReducePole:
            SpeedReducePoleItem_ReleaseFromField(item);
            break;
        case ItemWorldKind::Mine:
            MineItem_ReleaseFromField(item);
            break;
        case ItemWorldKind::ArrowSticker:
            ArrowStickerItem_ReleaseFromField(item);
            break;
        default:
            ItemWorld_DestroyRuntimeItem(item);
            break;
    }
}

void ItemWorld_ApplyMysteryBoxAppearance(
    GameObject& item,
    const char* modelPath,
    const char* proxyModelPath,
    float uniformScale,
    float colliderHalfX,
    float colliderHalfY,
    float colliderHalfZ) {
    const char* proxyPath = (proxyModelPath && proxyModelPath[0] != '\0')
        ? proxyModelPath
        : modelPath;

    if (auto* vc = ScriptApi_FindVisualComponent(item)) {
        if (modelPath && modelPath[0] != '\0')
            vc->modelPath = modelPath;
        if (proxyPath && proxyPath[0] != '\0')
            vc->proxyModelPath = proxyPath;
        vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
    }

    const float scale = (std::max)(0.01f, uniformScale);
    const XMFLOAT3 uniform = {scale, scale, scale};
    item.SetScale(uniform);
    if (engine::Transform* t = ScriptApi_FindTransform(item))
        t->localScale = uniform;

    if (Collider* col = item.GetCollider()) {
        col->SetIsTrigger(true);
        if (auto* box = dynamic_cast<BoxCollider*>(col)) {
            const float hx = (std::max)(0.01f, colliderHalfX);
            const float hy = (std::max)(0.01f, colliderHalfY);
            const float hz = (std::max)(0.01f, colliderHalfZ);
            ScriptApi_SetBoxColliderSize(*box, {hx * 2.0f, hy * 2.0f, hz * 2.0f});
        }
    }
}
