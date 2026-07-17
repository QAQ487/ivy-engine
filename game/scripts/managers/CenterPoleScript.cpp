/*****************************************************
 * File: game/scripts/CenterPoleScript.cpp
 * Project: d3d11_engine
 * Description: CenterPoleScript implementation ? multiple-pole registry.
 *              Every CenterPoleScript instance registers itself in g_poles.
 *              CenterPole_UpdateAiming() is called by PlayerScript each frame
 *              to pick the pole best aligned with the camera forward direction.
 *              Each pole's visual emission is set to a golden glow when aimed.
 *****************************************************/
#include "CenterPoleScript.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ecs/transform.h"
#include "engine/core/game_object.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/collider_component.h"
#include "engine/physics/collider/collider.h"
#include "scene_manager.h"
#include <vector>
#include <cmath>
#include <algorithm>

using namespace DirectX;

REGISTER_SCRIPT(CenterPoleScript)

// ==== Minimal kinematic GameObject used purely for collision ========================================
class CenterPolePhysicsProxy : public GameObject {
public:
    ~CenterPolePhysicsProxy() override {
        ScriptApi_DestroyCollider(*this);
    }

    void LogicUpdate(double /*elapsed_time*/) override {}
};

// ==== Per-pole registry entry ======================================================================================================
struct PoleEntry {
    XMFLOAT3                position   = {0.0f, 0.0f, 0.0f};
    CenterPolePhysicsProxy* proxy      = nullptr;
    engine::GameObject*     node       = nullptr; // scene node (for VisualComponent)
    CenterPoleScript*       script     = nullptr; // set only for CenterPoleScript instances; nullptr for dynamic targets
    int*                    ownerIndex = nullptr; // generic pointer to owner's index field
};

namespace {
    std::vector<PoleEntry> g_poles;
    int                    g_aimedPoleIndex  = -1;
    int                    g_previewPoleIndex = -1;
    int                    g_orbitPoleIndex  = -1; // pole currently being orbited (-1 = none)

    CenterPoleShouldSuppressAimGlowFn g_shouldSuppressAimGlow = nullptr;

    static const XMFLOAT4 kGlowOff = {0.0f, 0.0f, 0.0f, 0.0f};

    void ApplyHostColliderToProxy(engine::GameObject& host, GameObject* proxy) {
        if (!proxy) return;

        if (ColliderComponent* cc = ScriptApi_FindColliderComponent(host)) {
            GameObject* savedOwner = cc->owner;
            cc->owner = proxy;
            ScriptApi_ApplyColliderComponent(*cc);
            cc->owner = savedOwner;
            proxy->m_colliderOwnedByHost = true;
            proxy->m_debugCollider = cc->showDebugCollider;
            return;
        }

        if (Collider* hostCol = host.GetCollider()) {
            if (hostCol->GetType() == ColliderType::BOX) {
                auto* hostBox = static_cast<BoxCollider*>(hostCol);
                const XMFLOAT3 size = ScriptApi_GetBoxColliderSize(*hostBox);
                ScriptApi_CreateBoxCollider(*proxy, size, hostCol->GetCenter());
                proxy->m_debugCollider = host.m_debugCollider;
                return;
            }
        }

        XMFLOAT3 size = {1.0f, 4.0f, 1.0f};
        if (const engine::Transform* t = ScriptApi_FindTransform(host))
            size = t->localScale;
        ScriptApi_CreateBoxCollider(*proxy, size, {0.0f, 0.0f, 0.0f});
        proxy->m_debugCollider = true;
    }
}

// ==== Helper: set the real-model emission for a registry entry ====================================
static void SetPoleGlow(PoleEntry& e, bool /*aimed*/, bool /*orbiting*/) {
    if (!e.node) return;
    if (auto* vc = ScriptApi_FindVisualComponent(*e.node))
        vc->emissionGlow = kGlowOff;
}

static void CenterPole_RefreshAllGlows() {
    for (int i = 0; i < (int)g_poles.size(); ++i) {
        const bool isOrbiting = (g_orbitPoleIndex == i);
        const bool isAimed    = !isOrbiting && (g_aimedPoleIndex == i);
        SetPoleGlow(g_poles[i], isAimed, isOrbiting);
    }
}

void CenterPole_SyncAllProxies() {
    for (auto& e : g_poles) {
        if (!e.proxy || !e.node) continue;
        if (const engine::Transform* t = ScriptApi_FindTransform(*e.node)) {
            e.position = t->localPosition;
            e.proxy->SetPosition(t->localPosition);
            e.proxy->SetScale(t->localScale);
            e.proxy->SetRotation(t->localRotation);
            ApplyHostColliderToProxy(*e.node, e.proxy);
        }
    }
}

void CenterPole_DrawDebugColliders() {
    for (const auto& e : g_poles) {
        if (e.proxy && e.proxy->m_debugCollider && e.proxy->m_collider)
            e.proxy->m_collider->DebugDraw();
    }
}

// ==== Lifecycle ==================================================================================================================================

void CenterPoleScript::OnCreate(engine::GameObject& self) {
    XMFLOAT3 pos   = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};

    if (const engine::Transform* t = ScriptApi_FindTransform(self)) {
        pos   = t->localPosition;
        scale = t->localScale;
    }

    // Spawn a static physics proxy so the player can't walk through the pole.
    auto* proxy = new CenterPolePhysicsProxy();
    proxy->m_position = pos;
    proxy->m_scale    = scale;
    ApplyHostColliderToProxy(self, proxy);
    proxy->m_rigidbody.SetIsKinematic(true);
    proxy->m_rigidbody.SetUseGravity(false);
    ScriptApi_DestroyCollider(self);
    self.m_debugCollider = false;
    if (SceneBase* scene = ScriptApi_GetCurrentScene())
        scene->AddObject(proxy);

    // Register in the global pole list.
    PoleEntry entry;
    entry.position   = pos;
    entry.proxy      = proxy;
    entry.node       = &self;
    entry.script     = this;
    entry.ownerIndex = &m_registryIndex;
    m_registryIndex = (int)g_poles.size();
    g_poles.push_back(entry);
}

void CenterPoleScript::OnUpdate(engine::GameObject& self, float /*dt*/) {
    if (m_registryIndex < 0 || m_registryIndex >= (int)g_poles.size()) return;

    // Keep the registered position in sync if the node is moved at runtime.
    if (const engine::Transform* t = ScriptApi_FindTransform(self)) {
        g_poles[m_registryIndex].position = t->localPosition;
        if (g_poles[m_registryIndex].proxy) {
            g_poles[m_registryIndex].proxy->m_position = t->localPosition;
        }
    }

}

void CenterPoleScript::OnDestroy(engine::GameObject& self) {
    (void)self;

    if (m_registryIndex >= 0 && m_registryIndex < (int)g_poles.size()) {
        SetPoleGlow(g_poles[m_registryIndex], false, false);
        // Clean up physics proxy.
        CenterPolePhysicsProxy* proxy = g_poles[m_registryIndex].proxy;
        if (proxy) {
            if (SceneBase* scene = ScriptApi_GetCurrentScene())
                scene->RemoveObject(proxy);
            delete proxy;
        }
        // Clear aimed/orbit indices if they pointed at this entry.
        if (g_aimedPoleIndex == m_registryIndex) g_aimedPoleIndex = -1;
        if (g_orbitPoleIndex == m_registryIndex) g_orbitPoleIndex = -1;

        g_poles.erase(g_poles.begin() + m_registryIndex);

        // Adjust aimed/orbit indices that were above the removed slot.
        if (g_aimedPoleIndex > m_registryIndex) g_aimedPoleIndex--;
        if (g_orbitPoleIndex > m_registryIndex) g_orbitPoleIndex--;

        // Fix up all remaining owner index fields.
        for (auto& e : g_poles) {
            if (e.ownerIndex && *e.ownerIndex > m_registryIndex) {
                (*e.ownerIndex)--;
            }
        }
    }

    m_registryIndex = -1;
}

// ==== Global accessors ====================================================================================================================

void CenterPole_ResetSessionState() {
    if (SceneBase* scene = ScriptApi_GetCurrentScene()) {
        for (const PoleEntry& entry : g_poles) {
            if (entry.proxy) {
                scene->RemoveObject(entry.proxy);
                delete entry.proxy;
            }
        }
    } else {
        for (const PoleEntry& entry : g_poles) {
            delete entry.proxy;
        }
    }

    g_poles.clear();
    g_aimedPoleIndex = -1;
    g_previewPoleIndex = -1;
    g_orbitPoleIndex = -1;
}

bool CenterPole_IsActive() {
    return !g_poles.empty();
}

bool CenterPole_HasAimed() {
    return g_aimedPoleIndex >= 0 && g_aimedPoleIndex < (int)g_poles.size();
}

XMFLOAT3 CenterPole_GetAimedPosition() {
    if (!CenterPole_HasAimed()) return {0.0f, 0.0f, 0.0f};
    return g_poles[g_aimedPoleIndex].position;
}

XMFLOAT3 CenterPole_GetPosition() {
    return CenterPole_GetAimedPosition();
}

static float HorizontalDistance(XMFLOAT3 origin, const PoleEntry& entry) {
    const float dx = entry.position.x - origin.x;
    const float dz = entry.position.z - origin.z;
    return sqrtf(dx * dx + dz * dz);
}

static int FindBestTargetInConeImpl(XMFLOAT3 origin, XMFLOAT3 forwardXZ, float maxDist) {
    if (g_poles.empty()) return -1;

    float fLen = sqrtf(forwardXZ.x * forwardXZ.x + forwardXZ.z * forwardXZ.z);
    if (fLen < 1e-4f) return -1;
    float fwX = forwardXZ.x / fLen;
    float fwZ = forwardXZ.z / fLen;

    int   bestIdx = -1;
    float bestDot = 0.5f;
    const bool unlimitedRange = maxDist < 0.0f;

    for (int i = 0; i < (int)g_poles.size(); ++i) {
        if (!g_poles[i].node || !g_poles[i].node->IsActive()) continue;

        float dx   = g_poles[i].position.x - origin.x;
        float dz   = g_poles[i].position.z - origin.z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist < 0.1f) continue;
        if (!unlimitedRange && dist > maxDist) continue;

        float nx  = dx / dist;
        float nz  = dz / dist;
        float dot = nx * fwX + nz * fwZ;
        if (dot > bestDot) {
            bestDot = dot;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int CenterPole_FindBestTargetInCone(XMFLOAT3 origin, XMFLOAT3 forwardXZ, float maxDist) {
    return FindBestTargetInConeImpl(origin, forwardXZ, maxDist);
}

void CenterPole_UpdateAiming(XMFLOAT3 playerPos, XMFLOAT3 cameraForward, float maxDist) {
    if (g_orbitPoleIndex >= 0) {
        g_aimedPoleIndex = -1;
        g_previewPoleIndex = -1;
        CenterPole_RefreshAllGlows();
        return;
    }

    g_previewPoleIndex = FindBestTargetInConeImpl(playerPos, cameraForward, -1.0f);
    if (g_previewPoleIndex >= 0) {
        const float dist = HorizontalDistance(playerPos, g_poles[g_previewPoleIndex]);
        g_aimedPoleIndex = (dist <= maxDist) ? g_previewPoleIndex : -1;
    } else {
        g_aimedPoleIndex = -1;
    }
    CenterPole_RefreshAllGlows();
}

int  CenterPole_GetAimedIndex() {
    return g_aimedPoleIndex;
}

engine::GameObject* CenterPole_GetAimedNode() {
    if (g_aimedPoleIndex < 0 || g_aimedPoleIndex >= (int)g_poles.size())
        return nullptr;
    return g_poles[g_aimedPoleIndex].node;
}

bool CenterPole_HasPreviewAimed() {
    return g_previewPoleIndex >= 0
        && g_previewPoleIndex < (int)g_poles.size()
        && g_aimedPoleIndex < 0;
}

engine::GameObject* CenterPole_GetPreviewAimedNode() {
    if (!CenterPole_HasPreviewAimed())
        return nullptr;
    return g_poles[g_previewPoleIndex].node;
}

engine::GameObject* CenterPole_GetRegistryNode(int index) {
    if (index < 0 || index >= (int)g_poles.size())
        return nullptr;
    return g_poles[index].node;
}

void CenterPole_SetShouldSuppressAimGlow(CenterPoleShouldSuppressAimGlowFn fn) {
    g_shouldSuppressAimGlow = fn;
}

bool CenterPole_IsAimGlowSuppressedForNode(const engine::GameObject* registryNode) {
    return g_shouldSuppressAimGlow && registryNode && g_shouldSuppressAimGlow(registryNode);
}

void CenterPole_SetOrbitIndex(int idx) {
    g_orbitPoleIndex = idx;
    if (idx >= 0) {
        g_aimedPoleIndex = -1;
        g_previewPoleIndex = -1;
    }
    CenterPole_RefreshAllGlows();
}

void CenterPole_ClearOrbitIfNode(const engine::GameObject* node) {
    if (!node || g_orbitPoleIndex < 0 || g_orbitPoleIndex >= (int)g_poles.size()) return;
    if (g_poles[g_orbitPoleIndex].node == node)
        CenterPole_SetOrbitIndex(-1);
}

int CenterPole_GetOrbitIndex() {
    return g_orbitPoleIndex;
}

int CenterPole_FindIndexByNode(const engine::GameObject* node) {
    if (!node) return -1;
    for (int i = 0; i < (int)g_poles.size(); ++i) {
        if (g_poles[i].node == node)
            return i;
    }
    return -1;
}

// ── Dynamic target management ─────────────────────────────────────────────────

int CenterPole_AddDynamicTarget(XMFLOAT3 pos, engine::GameObject* node, int* callerIndex) {
    PoleEntry entry;
    entry.position   = pos;
    entry.proxy      = nullptr;  // no static physics proxy for dynamic targets
    entry.node       = node;
    entry.script     = nullptr;
    entry.ownerIndex = callerIndex;
    int idx = (int)g_poles.size();
    g_poles.push_back(entry);
    if (callerIndex) *callerIndex = idx;
    return idx;
}

void CenterPole_UpdateDynamicPosition(int index, XMFLOAT3 pos) {
    if (index < 0 || index >= (int)g_poles.size()) return;
    g_poles[index].position = pos;
}

static void CenterPole_RemoveRegistryEntry(int index, bool destroyProxy) {
    if (index < 0 || index >= (int)g_poles.size()) return;

    SetPoleGlow(g_poles[index], false, false);

    if (destroyProxy) {
        CenterPolePhysicsProxy* proxy = g_poles[index].proxy;
        if (proxy) {
            if (SceneBase* scene = ScriptApi_GetCurrentScene())
                scene->RemoveObject(proxy);
            delete proxy;
        }
    }

    if (g_poles[index].ownerIndex) *g_poles[index].ownerIndex = -1;

    if (g_aimedPoleIndex == index) g_aimedPoleIndex = -1;
    if (g_orbitPoleIndex == index) g_orbitPoleIndex = -1;

    g_poles.erase(g_poles.begin() + index);

    if (g_aimedPoleIndex > index) g_aimedPoleIndex--;
    if (g_orbitPoleIndex > index) g_orbitPoleIndex--;

    for (auto& e : g_poles) {
        if (e.ownerIndex && *e.ownerIndex > index)
            (*e.ownerIndex)--;
    }
}

void CenterPole_RemoveDynamicTarget(int index) {
    CenterPole_RemoveRegistryEntry(index, false);
}

int CenterPole_RegisterExternalPole(engine::GameObject& node, const XMFLOAT3& scale, int* callerIndex) {
    XMFLOAT3 pos = node.GetPosition();
    if (const engine::Transform* t = ScriptApi_FindTransform(node))
        pos = t->localPosition;

    auto* proxy = new CenterPolePhysicsProxy();
    proxy->m_position = pos;
    proxy->m_scale    = scale;
    ApplyHostColliderToProxy(node, proxy);
    proxy->m_rigidbody.SetIsKinematic(true);
    proxy->m_rigidbody.SetUseGravity(false);
    if (SceneBase* scene = ScriptApi_GetCurrentScene())
        scene->AddObject(proxy);

    PoleEntry entry;
    entry.position   = pos;
    entry.proxy      = proxy;
    entry.node       = &node;
    entry.script     = nullptr;
    entry.ownerIndex = callerIndex;
    const int idx = (int)g_poles.size();
    g_poles.push_back(entry);
    if (callerIndex) *callerIndex = idx;
    return idx;
}

void CenterPole_UpdateExternalPole(int index, const XMFLOAT3& position, const XMFLOAT3& scale) {
    if (index < 0 || index >= (int)g_poles.size()) return;

    g_poles[index].position = position;
    if (CenterPolePhysicsProxy* proxy = g_poles[index].proxy) {
        proxy->SetPosition(position);
        proxy->SetScale(scale);
        if (engine::GameObject* node = g_poles[index].node)
            ApplyHostColliderToProxy(*node, proxy);
    }
}

void CenterPole_UnregisterExternalPole(int index) {
    CenterPole_RemoveRegistryEntry(index, true);
    CenterPole_RefreshAllGlows();
}
