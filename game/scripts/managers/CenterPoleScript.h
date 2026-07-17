/*****************************************************
 * File: game/scripts/CenterPoleScript.h
 * Project: d3d11_engine
 * Description: CenterPoleScript – marks a prefab node as a magnetic pole.
 *              Supports multiple poles in one scene.  Each instance registers
 *              itself in a global registry.  PlayerScript calls
 *              CenterPole_UpdateAiming() every frame to mark the pole that
 *              is closest to the camera forward direction as the "aimed" pole.
 *              The aimed pole shows a lock-on UI reticle; only it responds to LMB/RMB.
 *
 * Prefab JSON:
 *   "Collider": { ... }  — box halfExtents define the pole collision volume
 *   "Script": { "scriptClass": "CenterPoleScript", "properties": {} }
 *****************************************************/
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scene/scene_node.h"
#include "libraries/nlohmann/json.hpp"
#include <DirectXMath.h>

class CenterPoleScript : public Script {
public:
    // ── Lifecycle ─────────────────────────────────────────────────────────
    void OnCreate (engine::GameObject& self) override;
    void OnUpdate (engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    // ── Serialization (no configurable properties) ────────────────────────
    nlohmann::json Serialize()  const override { return {}; }
    void Deserialize(const nlohmann::json& j)  override { (void)j; }

private:
    int m_registryIndex = -1; // index into the global pole registry
};

// ── Global accessors (used by PlayerScript) ───────────────────────────────────

// Clears pole registry and physics proxies (call on scene unload).
void CenterPole_ResetSessionState();

// Returns true while at least one CenterPoleScript instance is alive.
bool CenterPole_IsActive();

// Returns true when the player is currently aiming at some pole.
bool CenterPole_HasAimed();

// Returns the world-space position of the currently aimed pole, or {0,0,0}.
DirectX::XMFLOAT3 CenterPole_GetAimedPosition();

// Backward-compat alias → CenterPole_GetAimedPosition().
DirectX::XMFLOAT3 CenterPole_GetPosition();

// Called every frame by PlayerScript to update which pole is aimed at.
// Picks the pole whose direction from playerPos best matches cameraForward
// (horizontal XZ dot-product) and is within maxDist.
// No-op (clears aimed index) while an orbit pole is set.
void CenterPole_UpdateAiming(DirectX::XMFLOAT3 playerPos,
                              DirectX::XMFLOAT3 cameraForward,
                              float maxDist);

// Read-only cone pick (does not change global aimed index). forwardXZ is horizontal.
int CenterPole_FindBestTargetInCone(DirectX::XMFLOAT3 origin,
                                    DirectX::XMFLOAT3 forwardXZ,
                                    float maxDist);

// Returns the registry index of the currently aimed pole (-1 if none).
int CenterPole_GetAimedIndex();

// Scene node for the aimed registry entry (nullptr if none). Used for per-target cooldowns.
engine::GameObject* CenterPole_GetAimedNode();

// Preview aim: in cone but beyond magnet range (UI-only).
bool CenterPole_HasPreviewAimed();
engine::GameObject* CenterPole_GetPreviewAimedNode();

// Registry node at index (-1 / out of range → nullptr). Used for lock-on UI while orbiting.
engine::GameObject* CenterPole_GetRegistryNode(int index);

// Optional: while set, lock-on UI is hidden for registry nodes where this returns true
// (e.g. player magnet cooldown on that enemy body).
using CenterPoleShouldSuppressAimGlowFn = bool (*)(const engine::GameObject* registryNode);
void CenterPole_SetShouldSuppressAimGlow(CenterPoleShouldSuppressAimGlowFn fn);
bool CenterPole_IsAimGlowSuppressedForNode(const engine::GameObject* registryNode);

// Sets the registry index of the pole the player is orbiting (-1 to clear).
// While set, aim highlighting is suppressed on all other poles and the orbit
// pole is the active lock-on target for orbit mechanics.
void CenterPole_SetOrbitIndex(int idx);
void CenterPole_ClearOrbitIfNode(const engine::GameObject* node);

// Returns the registry index of the target the player is currently orbiting (-1 if none).
int  CenterPole_GetOrbitIndex();
int  CenterPole_FindIndexByNode(const engine::GameObject* node);

// ── Dynamic (non-pole) target registration ───────────────────────────────────
// Registers an external magnetic target (e.g. a dummy player) in the same
// aiming registry as CenterPole instances.  The PlayerScript's push/pull/orbit
// mechanics will then work on the target just like on a regular pole.
//
// pos         – initial world-space position.
// node        – owning scene node (used to drive aim / orbit glow via VisualComponent).
// callerIndex – pointer to the caller's own index field; the registry keeps this
//               up to date when earlier entries are removed so the caller always
//               holds a valid index.
// Returns the assigned registry index (also written to *callerIndex).
int  CenterPole_AddDynamicTarget(DirectX::XMFLOAT3 pos,
                                  engine::GameObject* node,
                                  int* callerIndex);

// Update the world-space position of a previously registered dynamic target.
// Call this every frame so aiming and force calculations use the live position.
void CenterPole_UpdateDynamicPosition(int index, DirectX::XMFLOAT3 pos);

// Remove a previously registered dynamic target and re-index all remaining owners.
// Also clears *callerIndex (set in CenterPole_AddDynamicTarget) to -1.
void CenterPole_RemoveDynamicTarget(int index);

// Sync physics-proxy transforms from scene nodes (editor + paused mode).
void CenterPole_SyncAllProxies();
void CenterPole_DrawDebugColliders();

// Register a scene object as a magnetic pole with a blocking kinematic proxy
// (same behaviour as CenterPoleScript). Returns registry index via callerIndex.
int CenterPole_RegisterExternalPole(engine::GameObject& node,
                                    const DirectX::XMFLOAT3& scale,
                                    int* callerIndex);
void CenterPole_UpdateExternalPole(int index,
                                   const DirectX::XMFLOAT3& position,
                                   const DirectX::XMFLOAT3& scale);
void CenterPole_UnregisterExternalPole(int index);
