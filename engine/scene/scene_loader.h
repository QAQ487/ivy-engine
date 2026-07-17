#pragma once

#include "libraries/nlohmann/json.hpp"
#include "engine/ecs/game_object.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

extern const std::string SCENE_DIR;
extern const std::string DEFAULT_SCENE_CONFIG;

std::vector<std::unique_ptr<GameObject>>& SceneLoader_GetSceneNodes();

void SceneLoader_StopScriptWatcher();

void SceneLoader_Initialize(bool enableScriptWatcher);
void SceneLoader_Finalize();

void SceneLoader_RegisterComponentTypes();
void SceneLoader_LoadScriptDlls();
void SceneLoader_UnloadScriptDlls();
void SceneLoader_ScanScriptDllsForChanges(bool forceReload);
void SceneLoader_DrainScriptReloadQueue(
    const std::function<void(const std::string& name)>& onReloaded);

const std::string& SceneLoader_GetDefaultScenePath();
const std::string& SceneLoader_GetCurrentScenePath();
void SceneLoader_LoadDefaultSceneSetting();
void SceneLoader_RefreshSceneAssets();
void SceneLoader_SetDefaultSceneFileName(const std::string& fileName, bool persist);
void SceneLoader_CopySceneFileName(const std::string& fileName);

bool SceneLoader_LoadFromFile(const char* filename);
bool SceneLoader_SaveToFile(const char* filename);
void SceneLoader_ClearSceneNodes();

void SyncSceneRuntime();
void ClearSceneRuntime(bool destroyColliders = false);
void ApplyEditorColliders();
void RestoreEditorDebugColliderFlags();

void TeardownAllScripts();
void TeardownScenePlaySession(bool resetTransforms);

void DrawSceneModels();
bool SceneLoader_IsSceneGraphObject(GameObject* obj);

std::unique_ptr<GameObject> GameObjectFromLegacyBlock(const nlohmann::json& blockJson);

void SnapshotSceneStartTransforms();

// Editor scene-browser helpers
bool SceneLoader_TryGetSceneAsset(int index, std::string* outPath, std::string* outDisplayName,
                                 std::string* outFileName);
int SceneLoader_GetSceneAssetCount();
bool SceneLoader_ApplySceneAssetByIndex(int index);
std::string SceneLoader_GetDefaultSceneDisplayName();
const std::string& SceneLoader_GetDefaultSceneFileName();
bool SceneLoader_IsSceneAssetsScanned();
const char* SceneLoader_GetSaveFileName();
char* SceneLoader_GetSaveFileNameBuffer();

bool SceneLoader_DebugCameraEnabledOnPlay();
bool SceneLoader_DebugCameraFlyOnPlay();
