#include "MysteryBoxItem.h"

#include "ItemSpawnManagerScript.h"
#include "ArrowStickerItem.h"
#include "MineItem.h"
#include "PlayerScript.h"
#include "ItemWorldApi.h"
#include "ShockwavePoleItem.h"
#include "SpeedReducePoleItem.h"
#include "engine/physics/collider/collider.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <unordered_map>

using namespace DirectX;

REGISTER_SCRIPT(MysteryBoxItemScript)

namespace {
    std::unordered_map<GameObject*, GameObject*> g_spawnedToMysteryOwner;

    std::mt19937& ItemRng() {
        static std::mt19937 rng(std::random_device{}());
        return rng;
    }

    MysteryBoxItemScript* GetMysteryScript(GameObject& item) {
        ScriptComponent* sc = item.GetComponent<ScriptComponent>();
        if (!sc) return nullptr;
        return dynamic_cast<MysteryBoxItemScript*>(sc->GetScript());
    }

    bool IsInstanceReferencedInInventory(const GameObject& instance) {
        if (GameObject* player = PlayerScript_GetBody()) {
            if (ItemWorld_GetHeldItem(*player) == &instance)
                return true;
        }
        return false;
    }

    const char* KindLabel(ItemWorldKind kind) {
        switch (kind) {
            case ItemWorldKind::ShockwavePole:   return "Shockwave";
            case ItemWorldKind::SpeedReducePole: return "SpeedReduce";
            case ItemWorldKind::Mine:            return "Mine";
            case ItemWorldKind::ArrowSticker:    return "ArrowSticker";
            default:                             return "Unknown";
        }
    }

    void LogMysteryPickupFail(const GameObject& self, const char* reason) {
        char buf[384];
        snprintf(buf, sizeof(buf), "Mystery [%s]: %s", self.GetName().c_str(), reason);
        ItemWorld_ShowPickupDebug(buf);
    }

    void ApplyMysteryBoxTriggerCollider(GameObject& item) {
        if (Collider* col = item.GetCollider())
            col->SetIsTrigger(true);
    }

    void ResetSpawnedForWorldPickup(GameObject& spawned, ItemWorldKind kind) {
        switch (kind) {
            case ItemWorldKind::ShockwavePole:
                ShockwavePoleItem_ResetForWorldPickup(spawned);
                break;
            case ItemWorldKind::SpeedReducePole:
                SpeedReducePoleItem_ResetForWorldPickup(spawned);
                break;
            case ItemWorldKind::Mine:
                MineItem_ResetForWorldPickup(spawned);
                break;
            case ItemWorldKind::ArrowSticker:
                ArrowStickerItem_ResetForWorldPickup(spawned);
                break;
            default:
                break;
        }
    }
}

bool MysteryBox_IsMysteryInPlay(const GameObject& mystery) {
    const ScriptComponent* sc = mystery.GetComponent<ScriptComponent>();
    if (!sc) return false;
    const auto* script = dynamic_cast<const MysteryBoxItemScript*>(sc->GetScript());
    if (!script) return false;

    if (script->IsWorldPickupState() && mystery.IsActive())
        return true;

    for (const auto& entry : g_spawnedToMysteryOwner) {
        if (entry.second == &mystery)
            return true;
    }
    return false;
}

bool MysteryBox_IsSpawnedInstanceReserved(const GameObject& instance) {
    if (g_spawnedToMysteryOwner.find(const_cast<GameObject*>(&instance)) !=
        g_spawnedToMysteryOwner.end()) {
        return true;
    }
    return IsInstanceReferencedInInventory(instance);
}

void MysteryBox_ClearSessionState() {
    g_spawnedToMysteryOwner.clear();
}

bool MysteryBox_OwnsSpawnedInstance(const GameObject& instance) {
    return g_spawnedToMysteryOwner.find(const_cast<GameObject*>(&instance)) !=
           g_spawnedToMysteryOwner.end();
}

void MysteryBox_OnSpawnedInstanceDestroying(GameObject* instance) {
    if (!instance) return;

    const auto it = g_spawnedToMysteryOwner.find(instance);
    if (it != g_spawnedToMysteryOwner.end()) {
        if (GameObject* mystery = it->second) {
            if (MysteryBoxItemScript* script = GetMysteryScript(*mystery)) {
                if (script->GetSpawnedInstance() == instance)
                    script->ResetStateForWorldPickup(*mystery);
            }
        }
        g_spawnedToMysteryOwner.erase(it);
    }
}

void MysteryBox_PrepareForRespawn(engine::GameObject& mystery) {
    for (auto it = g_spawnedToMysteryOwner.begin(); it != g_spawnedToMysteryOwner.end(); ) {
        if (it->second == &mystery) {
            if (GameObject* spawned = it->first) {
                ItemWorld_DisconnectObject(*spawned);
                ScriptApi_PrefabDestroy(spawned);
            }
            it = g_spawnedToMysteryOwner.erase(it);
        } else {
            ++it;
        }
    }

    ScriptComponent* sc = mystery.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<MysteryBoxItemScript*>(sc->GetScript()))
        script->ResetStateForWorldPickup(mystery);
}

void MysteryBoxItem_ResetForWorldPickup(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    const auto* script = dynamic_cast<const MysteryBoxItemScript*>(sc->GetScript());
    if (!script) return;

    if (script->IsWorldPickupState() && item.IsActive())
        return;

    MysteryBox_PrepareForRespawn(item);
}

bool MysteryBox_TryRespawnOwner(GameObject* spawnedInstance) {
    if (!spawnedInstance) return false;

    const auto it = g_spawnedToMysteryOwner.find(spawnedInstance);
    if (it == g_spawnedToMysteryOwner.end())
        return false;

    GameObject* mystery = it->second;
    g_spawnedToMysteryOwner.erase(it);

    ItemWorld_DisconnectObject(*spawnedInstance);
    ScriptApi_PrefabDestroy(spawnedInstance);

    if (!mystery) return true;

    MysteryBox_PrepareForRespawn(*mystery);
    return ItemSpawnManager_RequestRespawn(mystery);
}

GameObject* MysteryBox_ResolveHeldTarget(GameObject& item) {
    if (MysteryBoxItemScript* mystery = GetMysteryScript(item)) {
        if (GameObject* spawned = mystery->GetSpawnedInstance())
            return spawned;
    }
    return &item;
}

void MysteryBoxItemScript::ResetStateForWorldPickup(engine::GameObject& self) {
    m_state = State::MysteryWorld;
    m_resolvedKind = ItemWorldKind::Unknown;
    m_spawnedInstance = nullptr;
    m_spinAngle = 0.0f;
    ApplyMysteryBoxTriggerCollider(self);
}

bool MysteryBoxItemScript::IsWorldPickupState() const {
    return m_state == State::MysteryWorld;
}

bool MysteryBoxItemScript::IsHandedOffState() const {
    return m_state == State::HandedOff;
}

bool MysteryBoxItemScript::IsHeldByBody(const GameObject& body) const {
    if (!m_spawnedInstance) return false;
    return ItemWorld_IsHeldByPlayer(*m_spawnedInstance, body);
}

const std::string& MysteryBoxItemScript::PrefabPathForKind(ItemWorldKind kind) const {
    switch (kind) {
        case ItemWorldKind::ShockwavePole:   return shockwavePrefabPath;
        case ItemWorldKind::SpeedReducePole: return speedReducePrefabPath;
        case ItemWorldKind::Mine:            return minePrefabPath;
        case ItemWorldKind::ArrowSticker:    return arrowStickerPrefabPath;
        default:                             return shockwavePrefabPath;
    }
}

ItemWorldKind MysteryBoxItemScript::RollResolvedKind() const {
    const float wShock = (std::max)(0.0f, rollWeightShockwave);
    const float wSlow  = (std::max)(0.0f, rollWeightSpeedReduce);
    const float wMine  = (std::max)(0.0f, rollWeightMine);
    const float wSticker = (std::max)(0.0f, rollWeightArrowSticker);
    const float total = wShock + wSlow + wMine + wSticker;
    if (total <= 0.0f)
        return ItemWorldKind::ShockwavePole;

    std::uniform_real_distribution<float> dist(0.0f, total);
    const float roll = dist(ItemRng());
    if (roll < wShock) return ItemWorldKind::ShockwavePole;
    if (roll < wShock + wSlow) return ItemWorldKind::SpeedReducePole;
    if (roll < wShock + wSlow + wMine) return ItemWorldKind::Mine;
    return ItemWorldKind::ArrowSticker;
}

bool MysteryBoxItemScript::SpawnResolvedItem(engine::GameObject& self,
                                           GameObject& body,
                                           ItemWorldKind kind) {
    if (ItemWorld_HolderHasItem(body)) {
        LogMysteryPickupFail(self, "player already holds an item");
        return false;
    }
    if (kind == ItemWorldKind::Unknown) {
        LogMysteryPickupFail(self, "rolled Unknown item kind");
        return false;
    }

    const std::string& prefabPath = PrefabPathForKind(kind);
    if (prefabPath.empty()) {
        LogMysteryPickupFail(self, "missing prefab path for rolled item");
        return false;
    }

    const XMFLOAT3 pos = self.GetPosition();
    const XMFLOAT3 rot = self.GetRotation();
    GameObject* spawned = ScriptApi_PrefabInstantiate(
        prefabPath.c_str(), pos, rot, nullptr, false);
    if (!spawned) {
        char buf[192];
        snprintf(buf, sizeof(buf), "failed to instantiate %s prefab", KindLabel(kind));
        LogMysteryPickupFail(self, buf);
        return false;
    }

    ResetSpawnedForWorldPickup(*spawned, kind);
    spawned->SetActive(true);

    bool picked = false;
    if (ScriptComponent* sc = spawned->GetComponent<ScriptComponent>()) {
        if (auto* shock = dynamic_cast<ShockwavePoleItemScript*>(sc->GetScript())) {
            picked = shock->TryPickupForBody(*spawned, body);
        } else if (auto* slow = dynamic_cast<SpeedReducePoleItemScript*>(sc->GetScript())) {
            picked = slow->TryPickupForBody(*spawned, body);
        } else if (auto* mine = dynamic_cast<MineItemScript*>(sc->GetScript())) {
            picked = mine->TryPickupForBody(*spawned, body);
        } else if (auto* sticker = dynamic_cast<ArrowStickerItemScript*>(sc->GetScript())) {
            picked = sticker->TryPickupForBody(*spawned, body);
        }
    }

    if (!picked) {
        char buf[192];
        snprintf(buf, sizeof(buf), "spawned %s TryPickup failed (range/state)",
            spawned->GetName().c_str());
        LogMysteryPickupFail(self, buf);
        ItemWorld_ReleaseFieldItem(*spawned);
        ItemWorld_DisconnectObject(*spawned);
        ScriptApi_PrefabDestroy(spawned);
        return false;
    }
    if (!ItemWorld_Attach(*spawned, body, kind)) {
        LogMysteryPickupFail(self, "ItemWorld_Attach failed");
        ItemWorld_ReleaseFieldItem(*spawned);
        ItemWorld_DisconnectObject(*spawned);
        ScriptApi_PrefabDestroy(spawned);
        return false;
    }

    g_spawnedToMysteryOwner[spawned] = &self;
    m_spawnedInstance = spawned;
    m_resolvedKind = kind;
    m_state = State::HandedOff;
    self.SetActive(false);
    return true;
}

bool MysteryBoxItemScript::IsBodyInPickupRange(GameObject& self, GameObject& body) const {
    const float pickupRadius = (std::max)(0.1f, itemPickupRadius);
    if (Collider* bodyCol = body.GetCollider()) {
        if (Collider* itemCol = self.GetCollider()) {
            if (bodyCol->CheckCollision(itemCol))
                return true;
        }
    }

    const XMFLOAT3 itemPos = self.GetPosition();
    const XMFLOAT3 bodyPos = body.GetPosition();
    const float dx = bodyPos.x - itemPos.x;
    const float dz = bodyPos.z - itemPos.z;
    return (dx * dx + dz * dz) <= (pickupRadius * pickupRadius);
}

void MysteryBoxItemScript::DrawWorldMarker(const XMFLOAT3& itemPos,
                                           engine::GameObject& self) const {
    (void)itemPos;
    (void)self;
}

bool MysteryBoxItemScript::TryPickupForBody(engine::GameObject& self, GameObject& body) {
    if (m_state != State::MysteryWorld) {
        LogMysteryPickupFail(self,
            m_state == State::HandedOff ? "already handed off" : "wrong state");
        return false;
    }
    if (!body.IsActive()) {
        LogMysteryPickupFail(self, "player body inactive");
        return false;
    }
    if (!IsBodyInPickupRange(self, body)) {
        LogMysteryPickupFail(self, "out of pickup range");
        return false;
    }
    return SpawnResolvedItem(self, body, RollResolvedKind());
}

bool MysteryBoxItemScript::TryPlaceForBody(engine::GameObject& self,
                                          GameObject& body,
                                          float yawDegrees) {
    (void)self;
    (void)body;
    (void)yawDegrees;
    return false;
}

void MysteryBoxItemScript::OnCreate(engine::GameObject& self) {
    ResetStateForWorldPickup(self);
}

void MysteryBoxItemScript::OnDestroy(engine::GameObject& self) {
    if (m_spawnedInstance) {
        g_spawnedToMysteryOwner.erase(m_spawnedInstance);
        m_spawnedInstance = nullptr;
    }
    (void)self;
}

void MysteryBoxItemScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!EngineGUI_IsPlaying()) return;
    if (m_state != State::MysteryWorld || !self.IsActive()) return;

    const float spinSpeed = (std::max)(0.0f, worldSpinSpeed);
    if (spinSpeed > 0.0f) {
        m_spinAngle += spinSpeed * dt;
        if (m_spinAngle >= 360.0f)
            m_spinAngle = fmodf(m_spinAngle, 360.0f);

        XMFLOAT3 rot = self.GetRotation();
        rot.y = m_spinAngle;
        self.SetRotation(rot);
        if (engine::Transform* t = ScriptApi_FindTransform(self))
            t->localRotation.y = m_spinAngle;
    }

    DrawWorldMarker(self.GetPosition(), self);
}
