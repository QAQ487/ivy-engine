#include "engine/physics/2d/physics2d.h"
#include "engine/ecs/physics2d_components.h"
#include "engine/ecs/sprite_animator_component.h"
#include "scene_execution_policy.h"
#include "engine/core/engine_gui.h"
#include "engine/scene/scene_camera.h"
#include "camera.h"
#include "engine/ecs/sprite_renderer_component.h"
#include "scene_loader.h"
#include "prepared_scene.h"
#include "scene_file_monitor.h"
#include "scene_json_validation.h"

#include "engine/scene/render_settings.h"
#include "engine/ecs/component_factory.h"
#include "engine/ecs/collider_component_access.h"
#include "engine/ecs/effect_component.h"
#include "engine/audio/audio_manager.h"
#include "engine/ecs/audio_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/ecs/tag_registry.h"
#include "engine/scene/scene_runtime.h"
#include "engine/ecs/prefab_runtime.h"
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
#include <future>
#include <chrono>
#include <locale>
#include <unordered_map>

namespace fs = std::filesystem;

using json = nlohmann::json;

const std::string SCENE_DIR = "rom/scene/";
const std::string DEFAULT_SCENE_CONFIG = "rom/scene/default_scene.txt";

static std::vector<std::unique_ptr<GameObject>> g_sceneNodes;
static ScriptWatcher g_scriptWatcher;
static std::string g_scriptBinDir = "game/scripts/bin/";
static bool g_wantScriptWatcher = false;
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
static std::string g_currentScenePath, g_currentSceneContent;
static unsigned long long g_contentRevision = 0;
const std::string& SceneLoader_GetCurrentSceneContent() { return g_currentSceneContent; }
unsigned long long SceneLoader_GetContentRevision() { return g_contentRevision; }
static char g_saveFileName[128] = "scene01.json";
static bool g_scene2D=false, g_physics3D=true, g_physics2D=false;
bool SceneLoader_Is2D(){return g_scene2D;}
bool SceneLoader_Uses2DPhysics(){return g_physics2D;}
bool SceneLoader_Uses3DPhysics(){return g_physics3D;}
void SceneLoader_SetExecutionMode(bool scene2D, bool physics3D, bool physics2D) {
    Physics2D_Reset(); g_physics2D=scene2D && physics2D;
    g_scene2D=scene2D; g_physics3D=!scene2D && physics3D;
    PhysicsSystem_ClearTriggerPairs(); PhysicsSystem_ClearDebugDrawQueue();
}
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
    SceneExecutionPolicy{g_scene2D,g_physics3D,g_physics2D}.WriteSettings(settings);
    settings["render"] = RenderSettings_ToJson();
    const auto mode=EngineGUI_GetCameraMode();
    settings["camera"]={{"mode",mode==CameraMode::ThirdPerson?"thirdPerson":mode==CameraMode::FirstPerson?"firstPerson":"scene"},
        {"node",EngineGUI_GetActiveCameraNodeName()}};
    return settings;
}

static void PrepareSceneSettings(const json& root, PreparedScene& prepared) {
    const auto settings = root.value("settings", json::object());
    if (!settings.is_object()) throw std::runtime_error("settings must be an object");
    prepared.policy = SceneExecutionPolicy::FromSettings(settings);
    const auto camera = settings.value("camera", json::object());
    if (!camera.is_object()) throw std::runtime_error("camera must be an object");
    prepared.preferredCamera = camera.value("node", std::string{});
    const auto mode = camera.value("mode", std::string("scene"));
    if (mode == "thirdPerson") prepared.cameraMode = CameraMode::ThirdPerson;
    if (mode == "firstPerson") prepared.cameraMode = CameraMode::FirstPerson;
    if (settings.contains("render")) prepared.render = RenderSettings_ParseJson(settings["render"]);
    const auto dc = settings.value("debugCamera", json::object());
    if (!dc.is_object()) throw std::runtime_error("debugCamera must be an object");
    prepared.debugFly = ToLowerString(dc.value("mode", std::string("orbit"))) == "fly";
    prepared.debugOnPlay = dc.value("enabledOnPlay", false);
    prepared.yaw = dc.value("yawDeg", 0.0f); prepared.pitch = dc.value("pitchDeg", -15.0f);
    if (dc.contains("position")) {
        const auto& pos = dc["position"];
        if (!pos.is_object()) throw std::runtime_error("debugCamera.position must be an object");
        prepared.debugPosition = true;
        prepared.position = {pos.value("x", 0.0f), pos.value("y", 3.0f), pos.value("z", -6.0f)};
    }
}

static void ApplySceneSettings(const PreparedScene& prepared, bool preserveEditorView) {
    g_scene2D = prepared.policy.scene2D; g_physics3D = prepared.policy.physics3D; g_physics2D = prepared.policy.physics2D;
    EngineGUI_SetCameraMode(prepared.cameraMode);
    EngineGUI_SetActiveCameraNodeName(SceneCameraResolveDefault(g_sceneNodes, prepared.preferredCamera));
    g_debugCameraEnabledOnPlay = prepared.debugOnPlay;
    g_debugCameraFlyOnPlay = prepared.debugFly;
    if (!preserveEditorView) {
        SetCamera(Camera{});
        DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
#ifndef GAME_RUNTIME
        DebugCamera_ResetEditorView(g_scene2D);
#endif
        if (!prepared.debugOnPlay && prepared.debugFly) DebugCamera_SetControlMode(DebugCameraControlMode::Fly);
        if (prepared.debugPosition) {
            if (prepared.debugFly) DebugCamera_SetFlyView(prepared.position, prepared.yaw, prepared.pitch);
            else DebugCamera_SetFocalPoint(prepared.position);
        }
    }
    RenderSettings_Get() = prepared.render;
    RenderSettings_ReloadSkybox();
    RenderSettings_ApplyShadowQuality();
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
    REGISTER_COMPONENT(BoxColliderComponent);
    REGISTER_COMPONENT(SphereColliderComponent);
    REGISTER_COMPONENT(CapsuleColliderComponent);
    REGISTER_COMPONENT(CylinderColliderComponent);
    REGISTER_COMPONENT(MeshColliderComponent);
    ComponentFactory::Instance().Register("Rigidbody2D", []() -> std::unique_ptr<Component> {return std::make_unique<Rigidbody2DComponent>();});
    ComponentFactory::Instance().Register("Collider2D", []() -> std::unique_ptr<Component> {return std::make_unique<Collider2DComponent>();});
    ComponentFactory::Instance().Register("SpriteAnimator", []() -> std::unique_ptr<Component> {return std::make_unique<SpriteAnimatorComponent>();});
    ComponentFactory::Instance().Register("SpriteRenderer", []() -> std::unique_ptr<Component> { return std::make_unique<SpriteRendererComponent>(); });
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
        if (auto* cc = FindColliderComponent(node))
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

static void SceneLoader_StartScriptWatcherIfWanted() {
    if (!g_wantScriptWatcher)
        return;
    g_scriptWatcher.Stop();
    g_scriptWatcher.Start(g_scriptBinDir,
        [](const std::string& name, const std::string& path) {
            ScriptFactory::Instance().RequestReload(name, path);
        });
}

void SceneLoader_LoadScriptDlls() {
    g_scriptBinDir = ResolveRuntimeScriptBinDir();
    std::error_code ec;
    if (!fs::exists(g_scriptBinDir, ec)) {
        SceneLoader_StartScriptWatcherIfWanted();
        return;
    }

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
    SceneLoader_StartScriptWatcherIfWanted();
}

void SceneLoader_UnloadScriptDlls() {
    g_scriptWatcher.Stop();
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
    g_wantScriptWatcher = enableScriptWatcher;

#if defined(GAME_RUNTIME)
    // Runtime always loads content scripts at boot.
    SceneLoader_LoadScriptDlls();
#else
    // Editor: defer until a project is open (ApplyProjectSessionState).
    // Loading leftover GameScripts.dll from the exe out-dir on the start
    // screen pulls in content hooks (e.g. SceneTransition) that expect
    // project-local textures.
#endif
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

    if (auto* cc = FindColliderComponent(node)) {
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
    TeardownScenePlaySession(false);
    PrefabRuntime_ResetSessionState();
}

namespace {
struct PreparationEntry { json node, overrides; size_t parent = SIZE_MAX; bool legacy = false; };
struct PreparationInput { json root; std::string content; std::vector<PreparationEntry> entries; };
PreparationInput ReadPreparation(const std::string& path, std::string content, bool supplied) {
    if (!supplied && !SceneFileMonitor::Read(path, content)) throw std::runtime_error("Cannot read " + path);
    PreparationInput input;
    input.content = std::move(content);
    input.root = json::parse(input.content);
    const auto& root = input.root;
    if (!root.is_object()) throw std::runtime_error("scene root must be an object");
    for (const auto* key : {"nodes", "blocks"})
        if (root.contains(key) && !root[key].is_array()) throw std::runtime_error(std::string(key) + " must be an array");
    if (root.contains("blocks")) for (const auto& block : root["blocks"]) {
        if (!block.is_object()) throw std::runtime_error("block must be an object");
        input.entries.push_back({block, json(), SIZE_MAX, true});
    }
    std::function<void(const json&, size_t, const json&)> flatten = [&](const json& source, size_t parent, const json& overrides) {
        if (!source.is_object()) throw std::runtime_error("scene node must be an object");
        if (source.contains("children") && !source["children"].is_array()) throw std::runtime_error("children must be an array");
        json shallow = json::object();
        for (auto it = source.begin(); it != source.end(); ++it)
            if (it.key() != "children") shallow[it.key()] = it.value();
        const size_t index = input.entries.size();
        input.entries.push_back({std::move(shallow), overrides, parent, false});
        if (source.contains("children")) for (const auto& child : source["children"]) flatten(child, index, json());
    };
    if (root.contains("nodes")) for (const auto& node : root["nodes"]) {
        if (!node.is_object()) throw std::runtime_error("node must be an object");
        if (node.contains("components") && !node["components"].is_object()) throw std::runtime_error("components must be an object");
        if (node.contains("prefabAssetPath") && node["prefabAssetPath"].is_string()) {
            const auto prefabPath = node["prefabAssetPath"].get<std::string>();
            std::string data;
            if (!SceneFileMonitor::Read(prefabPath, data)) throw std::runtime_error("Cannot read prefab: " + prefabPath);
            auto prefab = json::parse(data);
            const json* resolved = &prefab;
            if (prefab.contains("nodes") && prefab["nodes"].is_array()) {
                resolved = nullptr;
                for (const auto& candidate : prefab["nodes"]) if (candidate.is_object()) { resolved = &candidate; break; }
            } else if (prefab.contains("prefab") && prefab["prefab"].is_object()) resolved = &prefab["prefab"];
            if (!resolved) throw std::runtime_error("No prefab node found");
            flatten(*resolved, SIZE_MAX, node);
        } else flatten(node, SIZE_MAX, json());
    }
    return input;
}
}
struct ScenePreparationJob {
    std::future<PreparationInput> reader;
    PreparationInput input;
    std::shared_ptr<PreparedScene> prepared;
    std::vector<GameObject*> objects;
    std::string path, error;
    size_t next = 0;
    bool reading = true, done = false;
};
std::shared_ptr<ScenePreparationJob> SceneLoader_BeginPreparation(const std::string& path, const std::string* content, bool background) {
    auto job = std::make_shared<ScenePreparationJob>();
    job->path = path;
    job->reader = std::async(background ? std::launch::async : std::launch::deferred,
        ReadPreparation, path, content ? *content : std::string(), content != nullptr);
    return job;
}
bool SceneLoader_TickPreparation(ScenePreparationJob& job, double budgetMs) {
    if (job.done) return true;
    const auto start = std::chrono::steady_clock::now();
    try {
        if (job.reading) {
            if (job.reader.wait_for(std::chrono::seconds(0)) == std::future_status::timeout) return false;
            job.input = job.reader.get(); job.reading = false;
            job.prepared = std::make_shared<PreparedScene>();
            job.prepared->path = job.path; job.prepared->content = std::move(job.input.content);
            PrepareSceneSettings(job.input.root, *job.prepared);
            job.objects.reserve(job.input.entries.size());
            return false;
        }
        while (job.next < job.input.entries.size()) {
            const auto& entry = job.input.entries[job.next];
            auto node = entry.legacy ? GameObjectFromLegacyBlock(entry.node) : PrefabSerializer::DeserializeSceneNode(entry.node);
            if (!entry.overrides.is_null()) {
                const auto& jn = entry.overrides;
                const std::string prefabPath = jn["prefabAssetPath"].get<std::string>();
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
            auto* raw = node.get();
            if (entry.parent == SIZE_MAX) job.prepared->nodes.push_back(std::move(node));
            else job.objects.at(entry.parent)->AddChild(std::move(node));
            job.objects.push_back(raw); ++job.next;
            if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= budgetMs) return false;
        }
        job.prepared->canvasJson = job.input.root.value("canvas", json::object());
        job.prepared->canvas = SceneCanvas_Prepare(job.prepared->canvasJson);
        job.done = true;
    } catch (const std::exception& e) { job.error = e.what(); job.prepared.reset(); job.done = true; }
    catch (...) { job.error = "Scene deserialization failed"; job.prepared.reset(); job.done = true; }
    return job.done;
}
bool SceneLoader_PreparationReading(const ScenePreparationJob& job) { return job.reading; }
size_t SceneLoader_PreparationCompleted(const ScenePreparationJob& job) { return job.next; }
size_t SceneLoader_PreparationTotal(const ScenePreparationJob& job) { return job.input.entries.size(); }
std::shared_ptr<PreparedScene> SceneLoader_PreparationResult(const ScenePreparationJob& job) { return job.done ? job.prepared : nullptr; }
std::string SceneLoader_PreparationError(const ScenePreparationJob& job) { return job.error; }
std::shared_ptr<PreparedScene> SceneLoader_PrepareFromContent(const std::string& path, const std::string& content, std::string& error) {
    auto job = SceneLoader_BeginPreparation(path, &content, false);
    while (!SceneLoader_TickPreparation(*job, 4.0)) {}
    error = SceneLoader_PreparationError(*job);
    return SceneLoader_PreparationResult(*job);
}

void SceneLoader_ApplyPrepared(std::shared_ptr<PreparedScene> prepared, bool preserveEditorView) {
    if (!prepared) return;
    SceneLoader_PrepareForSceneReload();
    g_sceneNodes = std::move(prepared->nodes);
    for (auto& node : g_sceneNodes) node->AttachPreparedComponents();
    SceneCanvas_ApplyPrepared(std::move(prepared->canvas), prepared->canvasJson);
    ApplySceneSettings(*prepared, preserveEditorView);
    std::error_code ec;
    const fs::path scenePath(prepared->path);
    const auto relativePath = fs::relative(scenePath, SCENE_DIR, ec);
    SceneLoader_CopySceneFileName(!ec ? relativePath.generic_string() : scenePath.filename().generic_string());
    g_currentScenePath = prepared->path;
    g_currentSceneContent = std::move(prepared->content);
    ++g_contentRevision;
    SyncSceneRuntime();
}

bool SceneLoader_LoadFromFile(const char* filename) {
    if (!filename || !filename[0]) return false;
    std::string content, error;
    if (!SceneFileMonitor::Read(filename, content)) return false;
    auto prepared = SceneLoader_PrepareFromContent(filename, content, error);
    if (!prepared) return false;
    SceneLoader_ApplyPrepared(std::move(prepared));
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

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    const auto content = root.dump(4);
    file << content;
    file.close();
    if (!file) return false;
    std::error_code ec;
    if (!g_currentScenePath.empty() && fs::equivalent(filename, g_currentScenePath, ec) && !ec) {
        g_currentSceneContent = content;
        ++g_contentRevision;
    }
    return true;
}

void SceneLoader_ClearSceneNodes() {
    g_currentScenePath.clear(); g_currentSceneContent.clear(); ++g_contentRevision;
    g_scene2D=false; g_physics3D=true; g_physics2D=false;
    EngineGUI_SetCameraMode(CameraMode::SceneCamera);
    EngineGUI_SetActiveCameraNodeName("");
    SetCamera(Camera{});
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
    Physics2D_Reset();
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
    if(auto* sprite=node->GetComponent<SpriteRendererComponent>()) sprite->SnapshotStartState();
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
