#include "scene_loader.h"

#include "engine/scene/render_settings.h"
#include "engine/ecs/component_factory.h"
#include "engine/ecs/collider_component.h"
#include "engine/ecs/effect_component.h"
#include "engine/audio/audio_manager.h"
#include "engine/ecs/audio_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/ecs/tag_registry.h"
#include "engine/scene/scene_runtime.h"
#include "engine/ecs/prefab_runtime.h"
#include <Windows.h>
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/ecs/material_component.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/ecs/point_light_component.h"
#include "engine/ecs/spot_light_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_factory.h"
#include "engine/scripting/script_watcher.h"
#include "engine/ui/scene_canvas.h"
#include "scene_manager.h"
#include "debug_camera.h"
#include "engine/physics/core/physics_system.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <locale>
#include <unordered_map>

namespace fs = std::filesystem;

using json = nlohmann::json;

const std::string SCENE_DIR = "rom/scene/";
const std::string DEFAULT_SCENE_CONFIG = "rom/scene/default_scene.txt";

static std::vector<std::unique_ptr<GameObject>> g_sceneNodes;
static ScriptWatcher g_scriptWatcher;
static std::string g_scriptBinDir = "game/scripts/bin/";
static std::unordered_map<std::string, fs::file_time_type> g_scriptDllWriteTimes;

struct SceneAssetEntry {
    std::string path;
    std::string displayName;
    std::string fileName;
    std::string sortName;
};

static std::vector<SceneAssetEntry> g_sceneAssets;
static bool g_sceneAssetsScanned = false;
static std::string g_defaultSceneFileName = "scene01.json";
static std::string g_defaultScenePath = SCENE_DIR + g_defaultSceneFileName;
static std::string g_currentScenePath;
static char g_saveFileName[128] = "scene01.json";
static bool g_debugCameraEnabledOnPlay = false;
static bool g_debugCameraFlyOnPlay = false;

bool SceneLoader_DebugCameraEnabledOnPlay() {
    return g_debugCameraEnabledOnPlay;
}

bool SceneLoader_DebugCameraFlyOnPlay() {
    return g_debugCameraFlyOnPlay;
}

static std::string TrimString(const std::string& value) {
    const char* whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) return "";
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

static std::string ToLowerString(const std::string& value) {
    std::string result = value;
    const std::locale loc = std::locale::classic();
    std::transform(result.begin(), result.end(), result.begin(),
        [loc](unsigned char c) { return static_cast<char>(std::tolower(c, loc)); });
    return result;
}

static json BuildSceneSettingsJson() {
    json settings;
    settings["render"] = RenderSettings_ToJson();
    return settings;
}

static void ApplySceneSettings(const json& root) {
    g_debugCameraEnabledOnPlay = false;
    g_debugCameraFlyOnPlay = false;
    DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
    RenderSettings_ResetToDefaults();

    if (!root.contains("settings") || !root["settings"].is_object()) {
        RenderSettings_ReloadSkybox();
        RenderSettings_ApplyShadowQuality();
        return;
    }

    const json& settings = root["settings"];
    if (settings.contains("render") && settings["render"].is_object())
        RenderSettings_ApplyFromJson(settings["render"]);
    else {
        RenderSettings_ReloadSkybox();
        RenderSettings_ApplyShadowQuality();
    }

    if (!settings.contains("debugCamera") || !settings["debugCamera"].is_object()) return;

    const json& dc = settings["debugCamera"];
    const std::string mode = ToLowerString(dc.value("mode", "orbit"));
    g_debugCameraFlyOnPlay = (mode == "fly");
    g_debugCameraEnabledOnPlay = dc.value("enabledOnPlay", false);

    // Editor keeps orbit camera enabled. Fly mode applies only when play starts.
    if (!g_debugCameraEnabledOnPlay && g_debugCameraFlyOnPlay)
        DebugCamera_SetControlMode(DebugCameraControlMode::Fly);

    if (dc.contains("position") && dc["position"].is_object()) {
        const json& pos = dc["position"];
        const float x = pos.value("x", 0.0f);
        const float y = pos.value("y", 3.0f);
        const float z = pos.value("z", -6.0f);
        const float yawDeg = dc.value("yawDeg", 0.0f);
        const float pitchDeg = dc.value("pitchDeg", -15.0f);

        if (g_debugCameraFlyOnPlay)
            DebugCamera_SetFlyView({ x, y, z }, yawDeg, pitchDeg);
        else
            DebugCamera_SetFocalPoint({ x, y, z });
    }
}

static std::string ResolveRuntimeScriptBinDir() {
    char exePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        fs::path scriptDir = fs::path(exePath).parent_path() / "game" / "scripts" / "bin";
        return scriptDir.string();
    }
    return "game/scripts/bin/";
}

std::vector<std::unique_ptr<GameObject>>& SceneLoader_GetSceneNodes() {
    return g_sceneNodes;
}

void SceneLoader_RegisterComponentTypes() {
    static bool s_registered = false;
    if (s_registered) return;
    s_registered = true;

    ComponentFactory::Instance().Register("SceneTransform",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<engine::Transform>();
        });
    ComponentFactory::Instance().Register("visual",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<VisualComponent>();
        });
    ComponentFactory::Instance().Register("Billboard",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<BillboardComponent>();
        });
    ComponentFactory::Instance().Register("Material",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<MaterialComponent>();
        });
    ComponentFactory::Instance().Register("Rigidbody",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<RigidbodyComponent>();
        });
    ComponentFactory::Instance().Register("Script",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<ScriptComponent>();
        });
    ComponentFactory::Instance().Register("Collider",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<ColliderComponent>();
        });
    ComponentFactory::Instance().Register("Effect",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<EffectComponent>();
        });
    ComponentFactory::Instance().Register("DirectionalLight",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<DirectionalLightComponent>();
        });
    ComponentFactory::Instance().Register("PointLight",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<PointLightComponent>();
        });
    ComponentFactory::Instance().Register("SpotLight",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<SpotLightComponent>();
        });
    ComponentFactory::Instance().Register("Camera",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<CameraComponent>();
        });
    ComponentFactory::Instance().Register("Audio",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<AudioComponent>();
        });
    ComponentFactory::Instance().Register("AudioListener",
        []() -> std::unique_ptr<Component> {
            return std::make_unique<AudioListenerComponent>();
        });
}

void SyncSceneRuntime() {
    SceneRuntime::Instance().SyncFromEditorRoots(
        g_sceneNodes, SceneManager::Instance().GetCurrentScene());
}

void ClearSceneRuntime(bool destroyColliders) {
    SceneRuntime::Instance().Clear(
        SceneManager::Instance().GetCurrentScene(), destroyColliders);
}

void ApplyEditorColliders() {
    SceneRuntime::Instance().ApplyCollidersFromEditorRoots(g_sceneNodes);
}

void RestoreEditorDebugColliderFlags() {
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node) return;
        if (auto* cc = node->GetComponent<ColliderComponent>())
            node->m_debugCollider = cc->showDebugCollider;
        for (const auto& child : node->GetChildren())
            visit(child.get());
    };
    for (const auto& root : g_sceneNodes)
        if (root) visit(root.get());
}

static void UpdateDefaultScenePath() {
    g_defaultScenePath = SCENE_DIR + g_defaultSceneFileName;
}

static void SaveDefaultSceneSetting() {
    if (!fs::exists(SCENE_DIR)) fs::create_directories(SCENE_DIR);

    std::ofstream file(DEFAULT_SCENE_CONFIG);
    if (!file.is_open()) return;
    file << g_defaultSceneFileName << "\n";
}

void SceneLoader_SetDefaultSceneFileName(const std::string& fileName, bool persist) {
    if (fileName.empty()) return;
    g_defaultSceneFileName = fileName;
    UpdateDefaultScenePath();
    if (persist) SaveDefaultSceneSetting();
}

void SceneLoader_CopySceneFileName(const std::string& fileName) {
    strncpy_s(g_saveFileName, sizeof(g_saveFileName), fileName.c_str(), _TRUNCATE);
}

const std::string& SceneLoader_GetDefaultScenePath() {
    return g_defaultScenePath;
}

void SceneLoader_RefreshSceneAssets() {
    g_sceneAssets.clear();
    g_sceneAssetsScanned = true;

    std::error_code ec;
    if (!fs::exists(SCENE_DIR, ec)) return;

    for (const auto& entry : fs::recursive_directory_iterator(SCENE_DIR, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        fs::path path = entry.path();
        if (ToLowerString(path.extension().string()) != ".json") continue;

        SceneAssetEntry asset;
        asset.path = path.string();

        fs::path relativePath = fs::relative(path, SCENE_DIR, ec);
        if (ec) {
            ec.clear();
            relativePath = path.filename();
        }

        asset.fileName = relativePath.generic_string();
        relativePath.replace_extension();
        asset.displayName = relativePath.generic_string();
        asset.sortName = ToLowerString(asset.displayName);
        g_sceneAssets.push_back(asset);
    }

    std::sort(g_sceneAssets.begin(), g_sceneAssets.end(),
        [](const SceneAssetEntry& a, const SceneAssetEntry& b) {
            return a.sortName != b.sortName ? a.sortName < b.sortName
                                            : a.displayName < b.displayName;
        });

    const auto defaultIt = std::find_if(g_sceneAssets.begin(), g_sceneAssets.end(),
        [](const SceneAssetEntry& asset) { return asset.fileName == g_defaultSceneFileName; });
    if (defaultIt == g_sceneAssets.end() && !g_sceneAssets.empty())
        SceneLoader_SetDefaultSceneFileName(g_sceneAssets.front().fileName, false);
}

void SceneLoader_LoadDefaultSceneSetting() {
    std::ifstream file(DEFAULT_SCENE_CONFIG);
    if (file.is_open()) {
        std::string configuredScene;
        std::getline(file, configuredScene);
        configuredScene = TrimString(configuredScene);
        if (!configuredScene.empty()) {
            const bool configuredExists = g_sceneAssets.empty() ||
                std::any_of(g_sceneAssets.begin(), g_sceneAssets.end(),
                    [&](const SceneAssetEntry& asset) { return asset.fileName == configuredScene; });
            if (configuredExists) {
                SceneLoader_SetDefaultSceneFileName(configuredScene, false);
                return;
            }
        }
    }

    if (!g_sceneAssets.empty())
        SceneLoader_SetDefaultSceneFileName(g_sceneAssets.front().fileName, false);
}

void SceneLoader_LoadScriptDlls() {
    g_scriptBinDir = ResolveRuntimeScriptBinDir();
    std::error_code ec;
    if (!fs::exists(g_scriptBinDir, ec)) return;

    for (const auto& entry : fs::directory_iterator(g_scriptBinDir, ec)) {
        const std::string fileName = entry.path().filename().string();
        if (entry.path().extension() == ".dll" && fileName.find(".hot_") == std::string::npos) {
            const std::string name = entry.path().stem().string();
            ScriptFactory::Instance().LoadScriptDll(name, entry.path().string());
            std::error_code timeEc;
            g_scriptDllWriteTimes[entry.path().string()] =
                fs::last_write_time(entry.path(), timeEc);
        }
    }
}

void SceneLoader_UnloadScriptDlls() {
    ScriptFactory::Instance().UnloadAllDlls();
}

void SceneLoader_ScanScriptDllsForChanges(bool forceReload) {
    std::error_code ec;
    if (!fs::exists(g_scriptBinDir, ec)) return;

    for (const auto& entry : fs::directory_iterator(g_scriptBinDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".dll")
            continue;

        const std::string fileName = entry.path().filename().string();
        if (fileName.find(".hot_") != std::string::npos)
            continue;

        const std::string path = entry.path().string();
        const auto writeTime = fs::last_write_time(entry.path(), ec);
        if (ec) continue;

        auto it = g_scriptDllWriteTimes.find(path);
        const bool changed = (it == g_scriptDllWriteTimes.end() || it->second != writeTime);
        g_scriptDllWriteTimes[path] = writeTime;

        if (forceReload || changed) {
            const std::string name = entry.path().stem().string();
            ScriptFactory::Instance().RequestReload(name, path);
        }
    }
}

void SceneLoader_DrainScriptReloadQueue(
    const std::function<void(const std::string& name)>& onReloaded)
{
    ScriptFactory::Instance().DrainReloadQueue(onReloaded);
}

void SceneLoader_Initialize(bool enableScriptWatcher) {
    TagRegistry::Instance().LoadFromFile();
    SceneLoader_RegisterComponentTypes();
    SceneLoader_RefreshSceneAssets();
    SceneLoader_LoadDefaultSceneSetting();
    SceneLoader_LoadScriptDlls();

    if (enableScriptWatcher) {
        g_scriptWatcher.Start(g_scriptBinDir,
            [](const std::string& name, const std::string& path) {
                ScriptFactory::Instance().RequestReload(name, path);
            });
    }
}

void SceneLoader_StopScriptWatcher() {
    g_scriptWatcher.Stop();
}

void SceneLoader_Finalize() {
    g_scriptWatcher.Stop();
    TeardownScenePlaySession(false);
    g_sceneNodes.clear();
    SceneLoader_UnloadScriptDlls();
}

static std::unique_ptr<GameObject> CreateModelGameObject(
    const std::string& modelName, const std::string& displayName)
{
    auto node = std::make_unique<GameObject>(displayName.empty() ? modelName : displayName);
    node->AddComponent<engine::Transform>();
    auto* vc = node->AddComponent<VisualComponent>();
    vc->modelPath = modelName;
    node->AddComponent<ColliderComponent>();
    return node;
}

std::unique_ptr<GameObject> GameObjectFromLegacyBlock(const json& jb) {
    std::string modelName = jb.value("modelName", "");
    std::string name = jb.value("name", modelName);
    auto node = CreateModelGameObject(modelName, name);

    if (engine::Transform* t = node->GetTransform()) {
        t->localPosition.x = jb["position"].value("x", 0.0f);
        t->localPosition.y = jb["position"].value("y", 0.0f);
        t->localPosition.z = jb["position"].value("z", 0.0f);
        t->localRotation.x = jb["rotation"].value("x", 0.0f);
        t->localRotation.y = jb["rotation"].value("y", 0.0f);
        t->localRotation.z = jb["rotation"].value("z", 0.0f);
        t->localScale.x = jb["scale"].value("x", 1.0f);
        t->localScale.y = jb["scale"].value("y", 1.0f);
        t->localScale.z = jb["scale"].value("z", 1.0f);
        t->SnapshotStartState();
    }

    const int tagValue = jb.value("tag", 0);
    if (TagRegistry::Instance().IsValid(tagValue))
        node->SetTag(tagValue);
    else
        node->SetTag(Tag::TAG_NONE);

    if (auto* cc = node->GetComponent<ColliderComponent>()) {
        cc->colliderType = static_cast<ColliderType>(jb.value("colliderType", 0));
        if (jb.contains("colliderCenter")) {
            cc->colliderCenter.x = jb["colliderCenter"].value("x", 0.0f);
            cc->colliderCenter.y = jb["colliderCenter"].value("y", 0.0f);
            cc->colliderCenter.z = jb["colliderCenter"].value("z", 0.0f);
        }
        if (jb.contains("halfExtents")) {
            cc->halfExtents.x = jb["halfExtents"].value("x", 0.5f);
            cc->halfExtents.y = jb["halfExtents"].value("y", 0.5f);
            cc->halfExtents.z = jb["halfExtents"].value("z", 0.5f);
        }
        cc->radius = jb.value("radius", 0.5f);
        cc->height = jb.value("height", 2.0f);
        cc->meshSizePadding = jb.value("meshSizePadding", 0.0f);
        cc->showDebugCollider = jb.value("showDebugCollider", false);
    }

    return node;
}

static void InvokeGameplaySessionReset() {
    ScriptFactory::Instance().InvokeExportOnLoadedDlls("GameScripts_ResetGameplaySession");
}

static void SceneLoader_PrepareForSceneReload() {
    PrefabRuntime_FlushDestroyed();
    TeardownAllScripts();
    PrefabRuntime_DestroyAllSpawnedChildren();

    if (SceneBase* scene = SceneManager::Instance().GetCurrentScene()) {
        SceneRuntime::Instance().Clear(scene, true);
        scene->ClearObjects();
    }

    PrefabRuntime_ResetSessionState();
    InvokeGameplaySessionReset();
}

bool SceneLoader_LoadFromFile(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    SceneLoader_PrepareForSceneReload();

    json root;
    try { file >> root; }
    catch (...) {
        OutputDebugStringA("[SceneLoader] Failed to parse scene JSON.\n");
        return false;
    }

    g_sceneNodes.clear();

    if (root.contains("blocks") && root["blocks"].is_array()) {
        for (const auto& jb : root["blocks"]) {
            if (auto node = GameObjectFromLegacyBlock(jb))
                g_sceneNodes.push_back(std::move(node));
        }
    }

    if (root.contains("nodes") && root["nodes"].is_array()) {
        for (const auto& jn : root["nodes"]) {
            std::unique_ptr<GameObject> node;

            if (jn.contains("prefabAssetPath") && jn["prefabAssetPath"].is_string()) {
                const std::string prefabPath = jn["prefabAssetPath"].get<std::string>();
                node = PrefabSerializer::LoadFromFile(prefabPath);
                if (node) {
                    if (jn.contains("name") && jn["name"].is_string())
                        node->SetName(jn["name"].get<std::string>());
                    if (jn.contains("tag") && jn["tag"].is_number_integer()) {
                        const int tagValue = jn["tag"].get<int>();
                        if (TagRegistry::Instance().IsValid(tagValue))
                            node->SetTag(tagValue);
                    }
                    if (jn.contains("components") && jn["components"].is_object() &&
                        jn["components"].contains("SceneTransform")) {
                        if (engine::Transform* t = node->GetTransform()) {
                            const auto& st = jn["components"]["SceneTransform"];
                            t->localPosition.x = st.value("posX", t->localPosition.x);
                            t->localPosition.y = st.value("posY", t->localPosition.y);
                            t->localPosition.z = st.value("posZ", t->localPosition.z);
                            t->localRotation.x = st.value("rotX", t->localRotation.x);
                            t->localRotation.y = st.value("rotY", t->localRotation.y);
                            t->localRotation.z = st.value("rotZ", t->localRotation.z);
                            t->localScale.x = st.value("scaleX", t->localScale.x);
                            t->localScale.y = st.value("scaleY", t->localScale.y);
                            t->localScale.z = st.value("scaleZ", t->localScale.z);
                            t->SnapshotStartState();
                            node->SyncLegacyTransformFromComponents();
                        }
                    }
                    node->SetPrefabAssetPath(prefabPath);
                }
            } else {
                node = PrefabSerializer::Deserialize(jn.dump());
            }

            if (node) g_sceneNodes.push_back(std::move(node));
        }
    }

    if (root.contains("canvas") && root["canvas"].is_object())
        SceneCanvas_LoadSceneCanvas(root["canvas"]);
    else
        SceneCanvas_ClearSceneCanvas();

    ApplySceneSettings(root);

    std::error_code ec;
    fs::path scenePath(filename);
    fs::path relativePath = fs::relative(scenePath, SCENE_DIR, ec);
    if (!ec)
        SceneLoader_CopySceneFileName(relativePath.generic_string());
    else
        SceneLoader_CopySceneFileName(scenePath.filename().generic_string());

    g_currentScenePath = filename;
    SyncSceneRuntime();
    return true;
}

const std::string& SceneLoader_GetCurrentScenePath() {
    return g_currentScenePath;
}

bool SceneLoader_SaveToFile(const char* filename) {
    json root;
    root["version"] = 4;
    root["settings"] = BuildSceneSettingsJson();

    json nodes = json::array();
    for (const auto& node : g_sceneNodes) {
        if (!node) continue;
        if (PrefabRuntime_IsSpawnRoot(node.get())) continue;
        try { nodes.push_back(json::parse(PrefabSerializer::Serialize(*node))); }
        catch (...) {}
    }
    root["nodes"] = nodes;
    root["canvas"] = SceneCanvas_SaveSceneCanvas();

    std::ofstream file(filename);
    if (!file.is_open()) return false;
    file << root.dump(4);
    return true;
}

void SceneLoader_ClearSceneNodes() {
    ClearSceneRuntime();
    g_sceneNodes.clear();
    SceneCanvas_ClearSceneCanvas();
    SyncSceneRuntime();
}

void TeardownAllScripts() {
    std::function<void(GameObject*)> visit;
    visit = [&visit](GameObject* node) {
        if (!node) return;
        if (auto* sc = node->GetComponent<ScriptComponent>())
            sc->Teardown();
        for (const auto& child : node->GetChildren())
            visit(child.get());
    };
    for (const auto& root : g_sceneNodes)
        visit(root.get());
}

void TeardownScenePlaySession(bool resetTransforms) {
    // Drop deferred Collider* wire draws before any object teardown (F11 viz).
    PhysicsSystem_ClearDebugDrawQueue();
    PrefabRuntime_FlushDestroyed();
    TeardownAllScripts();
    PrefabRuntime_DestroyAllSpawnedChildren();
    InvokeGameplaySessionReset();
    AudioManager::Instance().StopSceneInstances();

    if (resetTransforms) {
        for (auto& node : g_sceneNodes)
            if (node) node->Reset();
    }

    ClearSceneRuntime(true);

    if (SceneBase* scene = SceneManager::Instance().GetCurrentScene())
        scene->ClearObjects();
}

static bool IsSceneGraphObjectRecursive(GameObject* obj, GameObject* root) {
    if (!obj || !root) return false;
    if (obj == root) return true;
    for (const auto& child : root->GetChildren())
        if (IsSceneGraphObjectRecursive(obj, child.get())) return true;
    return false;
}

bool SceneLoader_IsSceneGraphObject(GameObject* obj) {
    if (!obj) return false;
    for (const auto& root : g_sceneNodes) {
        if (root && IsSceneGraphObjectRecursive(obj, root.get())) return true;
    }
    return false;
}

void DrawSceneModels() {
    for (const auto& root : g_sceneNodes)
        if (root) root->DrawVisual(true);
}

static void SnapshotStartTransformsNode(GameObject* node) {
    if (!node) return;
    if (engine::Transform* t = node->GetComponent<engine::Transform>())
        t->SnapshotStartState();
    for (const auto& child : node->GetChildren())
        SnapshotStartTransformsNode(child.get());
}

void SnapshotSceneStartTransforms() {
    for (const auto& node : g_sceneNodes)
        SnapshotStartTransformsNode(node.get());
}

// Editor-only helpers exposed for engine_gui scene browser
bool SceneLoader_TryGetSceneAsset(int index, std::string* outPath, std::string* outDisplayName,
                                 std::string* outFileName);
int SceneLoader_GetSceneAssetCount();
bool SceneLoader_ApplySceneAssetByIndex(int index);
std::string SceneLoader_GetDefaultSceneDisplayName();
const std::string& SceneLoader_GetDefaultSceneFileName();
bool SceneLoader_IsSceneAssetsScanned();

bool SceneLoader_TryGetSceneAsset(int index, std::string* outPath, std::string* outDisplayName,
                                 std::string* outFileName)
{
    if (index < 0 || index >= static_cast<int>(g_sceneAssets.size())) return false;
    const SceneAssetEntry& asset = g_sceneAssets[static_cast<size_t>(index)];
    if (outPath) *outPath = asset.path;
    if (outDisplayName) *outDisplayName = asset.displayName;
    if (outFileName) *outFileName = asset.fileName;
    return true;
}

int SceneLoader_GetSceneAssetCount() {
    return static_cast<int>(g_sceneAssets.size());
}

bool SceneLoader_ApplySceneAssetByIndex(int index) {
    std::string path, fileName;
    if (!SceneLoader_TryGetSceneAsset(index, &path, nullptr, &fileName)) return false;
    if (!SceneLoader_LoadFromFile(path.c_str())) return false;
    SceneLoader_CopySceneFileName(fileName);
    return true;
}

std::string SceneLoader_GetDefaultSceneDisplayName() {
    for (const SceneAssetEntry& asset : g_sceneAssets) {
        if (asset.fileName == g_defaultSceneFileName)
            return asset.displayName;
    }
    return fs::path(g_defaultSceneFileName).stem().generic_string();
}

const std::string& SceneLoader_GetDefaultSceneFileName() {
    return g_defaultSceneFileName;
}

bool SceneLoader_IsSceneAssetsScanned() {
    return g_sceneAssetsScanned;
}

const char* SceneLoader_GetSaveFileName() {
    return g_saveFileName;
}

char* SceneLoader_GetSaveFileNameBuffer() {
    return g_saveFileName;
}
