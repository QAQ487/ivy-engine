#pragma once

#include "engine/ecs/game_object.h"

#include <DirectXMath.h>
#include <vector>

class SceneBase;

constexpr const char* PREFAB_RUNTIME_SPAWN_ROOT_NAME = "__RuntimeSpawned";

using PrefabRuntimeBeforeDestroyFn = void (*)(GameObject* obj);

void PrefabRuntime_SetBeforeDestroyCallback(PrefabRuntimeBeforeDestroyFn callback);

GameObject* PrefabRuntime_EnsureSpawnRoot();
bool PrefabRuntime_IsSpawnRoot(const GameObject* obj);
bool PrefabRuntime_IsRuntimeSpawned(const GameObject* obj);

GameObject* PrefabRuntime_Instantiate(const char* prefabPath,
                                      const DirectX::XMFLOAT3& worldPos,
                                      const DirectX::XMFLOAT3& worldRot,
                                      GameObject* parent,
                                      bool startActive);

void PrefabRuntime_Destroy(GameObject* obj);
void PrefabRuntime_DestroyAllSpawnedChildren();
void PrefabRuntime_FlushSpawned();
void PrefabRuntime_FlushDestroyed();
void PrefabRuntime_ResetSessionState();
