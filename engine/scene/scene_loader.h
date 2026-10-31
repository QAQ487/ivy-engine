// scene_loader.h
// Declares PreparedScene.

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

struct PreparedScene;
std::shared_ptr<PreparedScene> SceneLoader_PrepareFromContent(const std::string& path,
    const std::string& content, std::string& error);
void SceneLoader_ApplyPrepared(std::shared_ptr<PreparedScene> prepared, bool preserveEditorView = false);
const std::string& SceneLoader_GetCurrentSceneContent();
unsigned long long SceneLoader_GetContentRevision();
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

bool SceneLoader_Is2D();
bool SceneLoader_Uses3DPhysics();
bool SceneLoader_Uses2DPhysics();

void SceneLoader_SetExecutionMode(bool scene2D, bool physics3D, bool physics2D=false);

struct ScenePreparationJob;
std::shared_ptr<ScenePreparationJob> SceneLoader_BeginPreparation(const std::string& path,
    const std::string* content = nullptr, bool background = true);
bool SceneLoader_TickPreparation(ScenePreparationJob& job, double budgetMs = 4.0);
bool SceneLoader_PreparationReading(const ScenePreparationJob& job);
size_t SceneLoader_PreparationCompleted(const ScenePreparationJob& job);
size_t SceneLoader_PreparationTotal(const ScenePreparationJob& job);
std::shared_ptr<PreparedScene> SceneLoader_PreparationResult(const ScenePreparationJob& job);
std::string SceneLoader_PreparationError(const ScenePreparationJob& job);
