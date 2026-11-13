#include "engine/editor/pivot_presets.h"
#include <tuple>
#include <atomic>
#include "engine/scene/scene_camera.h"
/******************************************************
 * File: engine_gui.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-12
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   - engine::GameObject ?? ::GameObject throughout (alias, zero logic change).
 *   - Removed PrefabProxyLoader dependency.  DrawEditorSceneProxies() now
 *     reads the model path exclusively from VisualComponent (already in memory
 *     from deserialization), with Box_Blue.fbx as the absolute fallback.
 *   - GetTransform() helper calls GetComponent<engine::Transform>() because
 *     the standalone ::GameObject does not have a GetTransform() shortcut.
 *****************************************************/
#include "engine_gui.h"
#include "play_fps_overlay.h"
#include "engine_gui_internal.h"
#include "engine/scene/scene_loader.h"
#include "engine/scene/json_float.h"
#include "engine/scene/scene_asset_preload.h"
#include "engine/scene/scene_file_monitor.h"
#include "engine/scene/render_settings.h"
#include "engine/rendering/culling/frustum_cull.h"
#include "engine/rendering/culling/shadow_cascade_cull.h"
#include "engine/rendering/culling/sprite_cull.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/d3d11/post_process.h"
#include "engine/core/job_system.h"
#include "engine/core/debug_system.h"
#include "engine/core/utf8_path.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/icons_font_awesome.h"

#include <cstdio>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <locale>
#include <system_error>
#include <Windows.h>
#include <filesystem>

#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/rendering/d3d11/renderer.h"
#include "WICTextureLoader11.h"
#include "engine/physics/collider/collider.h"
#include "engine/physics/core/physics_system.h"
#include "engine/ecs/game_object.h"          // unified ::GameObject
#include "engine/ecs/tag_registry.h"
#include "debug_camera.h"
#include "engine/input/mouse.h"
#include "engine/debug_draw/debug_drawer.h"
#include "camera.h"
#include "grid.h"

 // Scene-graph / component-system headers
#include "engine/ecs/transform.h"
#include "engine/rendering/model/model.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/ecs/component_factory.h"
#include "engine/editor/editor_project_browser.h"
#include "engine/editor/editor_ui_panel.h"
#include "engine/editor/project_asset_index.h"
#include "engine/core/content_path.h"
#include "engine/rendering/environment/panorama_sky.h"
#include "engine/rendering/environment/skybox_asset_path.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/editor/project_manager.h"
#include "engine/editor/playable_package_builder.h"
#include "engine/audio/audio_manager.h"
#include "engine/editor/transform_gizmo.h"
#include "engine/editor/ui_gizmo.h"
#include "engine/editor/camera_orientation_gizmo.h"
#include "engine/editor/editor_undo.h"
#include "engine/editor/content_file_monitor.h"
#include "engine/editor/content_removal_policy.h"
#include "engine/ui/ui_font.h"
#include "engine/rendering/effect/effect_system.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/ecs/sprite_renderer_component.h"
#include "engine/ecs/sprite_animator_component.h"
#include "engine/ecs/material_component.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/ecs/physics2d_components.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/ecs/point_light_component.h"
#include "engine/ecs/spot_light_component.h"
#include "engine/ecs/camera_component.h"
#include "engine/editor/light_gizmo.h"
#include "engine/editor/camera_gizmo.h"
#include "engine/ecs/collider_component_access.h"
#include "engine/ecs/effect_component.h"
#include "engine/ecs/audio_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/model_asset_cache.h"
#include "engine/rendering/model/model_util.h"
#include "engine/scene/scene_runtime.h"
#include "engine/ecs/prefab_runtime.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/Script.h"
#include "third_person_camera.h"
#include "first_person_camera.h"
#include "scene_manager.h"
#include "engine/ui/scene_canvas.h"
#include "engine/ui/ui_canvas.h"
#include "engine/ui/ui_element.h"
// --- DLL hot-reload ---
#include "engine/scripting/script_factory.h"
#include "engine/scripting/script_watcher.h"


// nlohmann/json for JSON scene format
#include "libraries/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// --- State ---
static bool        g_enabled = true;
bool              g_isPlaying = false;
static std::atomic<bool> g_exitGameRequested{false};
static PlayMode    g_playMode = PlayMode::None;
static PlayMode    g_pendingPlayMode = PlayMode::Viewport;
static bool        g_holdPlayVisuals = false;
static bool        g_isOnStart = true;
static bool        g_imguiInitialized = false;

// When the editor skips ImGui::NewFrame (play / F2-hide), stop queuing Win32
// inputs into ImGui and flush any backlog so exit does not trickle-consume them.
static void SyncImGuiInputAcceptance(bool willProcessImGuiFrame) {
    if (!g_imguiInitialized)
        return;
    ImGuiIO& io = ImGui::GetIO();
    if (willProcessImGuiFrame) {
        if (!io.AppAcceptingEvents) {
            io.ClearEventsQueue();
            io.ClearInputKeys();
            io.ClearInputMouse();
            io.SetAppAcceptingEvents(true);
        }
    }
    else if (io.AppAcceptingEvents) {
        io.SetAppAcceptingEvents(false);
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
}

static SceneAssetPreloader g_sceneAssetPreloader;
static bool        g_playWhenPreloadDone = false;
bool               g_sceneLoadPending = false;
static bool IsSceneLoadBusy();
static std::string g_pendingSceneLoadPath;
static std::shared_ptr<ScenePreparationJob> g_scenePreparation;
static bool g_loadingPresented = false, g_loadingDrawn = false, g_sceneReadyToApply = false;
static bool g_preparationHotLoad = false;
static std::string g_sceneLoadError;
static const char* g_loadingStage = "Reading scene...";
static std::chrono::steady_clock::time_point g_loadStarted, g_stageStarted;
static double g_longestPreparationTick = 0.0;
static void LogSceneLoading(const char* event) {
    static std::ofstream log("editor_scene_loading.log", std::ios::trunc);
    static unsigned events = 0;
    if (++events > 512) return;
    const auto now = std::chrono::steady_clock::now();
    log << event << " elapsed_ms=" << std::chrono::duration<double, std::milli>(now-g_loadStarted).count()
        << " stage_ms=" << std::chrono::duration<double, std::milli>(now-g_stageStarted).count()
        << " nodes=" << (g_scenePreparation ? SceneLoader_PreparationCompleted(*g_scenePreparation) : 0)
        << " max_tick_ms=" << g_longestPreparationTick << std::endl;
}
void EngineGUI_NotifyFramePresented() {
    // Play transitions retain their existing scene presentation instead of an editor modal.
    if (!g_loadingDrawn && !g_holdPlayVisuals && !(g_isPlaying && g_sceneLoadPending)) return;
    g_loadingDrawn = false;
    if (g_sceneLoadPending && !g_loadingPresented) {
        g_loadingPresented = true;
        LogSceneLoading(g_sceneReadyToApply ? "apply_screen_presented" : "loading_screen_presented");
    }
}

static CameraMode  g_cameraMode = CameraMode::SceneCamera;
static std::string g_activeCameraNode;
static float g_scrollSensitivity = 1.0f;
static char  g_newSceneName[128] = "new_scene";
static char  g_newTagName[64] = "";
static bool  g_showRenderingSettings = false;
static bool  g_showPostFxShowcase = false;
static bool  g_showShortcutHints = false;
static bool  g_showNewScenePopup = false;
static bool  g_showAddTagPopup = false;
static bool  g_showDeleteTagPopup = false;
static int   g_pendingDeleteTagId = -1;
static bool  g_showDefaultScenePopup = false;
static bool  g_showGravityPopup = false;
static bool  g_showAudioPopup = false;
static bool  g_showUiOverlay = true;
static bool g_newProject2D=false;
static bool  g_showNewProjectPopup = false;
static char  g_newProjectName[128] = "NewProject";
static std::string g_newProjectParentDir;
static std::string g_projectStatusMessage;
enum class PendingFolderPickAction { None = 0, NewProject, OpenProject, BuildOutput };
static PendingFolderPickAction g_pendingFolderPick = PendingFolderPickAction::None;
static bool g_showBuildUnsavedPopup = false;
static bool g_showBuildPreflight = false;
static bool g_showBuildLog = false;
static char g_buildDest[1024] = {};
static int g_buildConfigIndex = 0;
static EditorEditMode g_editMode = EditorEditMode::Scene3D;

static constexpr float kToolbarHeight = 36.0f;
static constexpr float kLeftPanelWidth = 300.0f;
static constexpr float kRightPanelWidth = 350.0f;
static constexpr float kBottomPanelMinHeight = 340.0f;
static constexpr float kBottomPanelScreenFraction = 0.34f;
static constexpr float kEditorViewportMinHeight = 120.0f;
// Preserve preview aspect from original 1920x1080 layout (250px bottom, 58px top chrome).
static constexpr float kEditorViewportAspect =
    (1920.0f - kLeftPanelWidth - kRightPanelWidth) /
    (1080.0f - 58.0f - 250.0f);

// Editor UI zoom (100 / 125 / 150 / 175 / 200). Default 100%; persisted to rom/editor/ui_prefs.json.
static constexpr const char* kUiPrefsPath = PlayFpsOverlay::kPrefsPath;
static constexpr int kUiZoomPercents[] = { 100, 125, 150, 175, 200 };
static int g_uiZoomPercent = 100;
static float g_uiZoom = 1.0f;
static ImGuiStyle g_baseStyle{};
static bool g_baseStyleCaptured = false;

static float Ui(float px) { return px * g_uiZoom; }

static bool IsValidUiZoomPercent(int percent) {
    for (int p : kUiZoomPercents) {
        if (p == percent) return true;
    }
    return false;
}

static void SaveUiPrefs() {
    try {
        const fs::path dest = content_path::WritableFileCurrent(kUiPrefsPath);
        if (dest.empty())
            return;
        std::error_code ec;
        fs::create_directories(dest.parent_path(), ec);
        json j;
        // Preserve unknown keys (and Asset Browser prefs) when rewriting zoom/FPS.
        try {
            std::ifstream existing(dest);
            if (existing.is_open())
                existing >> j;
        }
        catch (...) {
            j = json::object();
        }
        if (!j.is_object())
            j = json::object();
        j["uiZoomPercent"] = g_uiZoomPercent;
        PlayFpsOverlay::WritePrefs(j);
        if (ProjectManager::Instance().IsOpen()) {
            j["assetBrowserListView"] = ProjectManager::Instance().GetAssetBrowserListView();
            j["assetBrowserTileSize"] = ProjectManager::Instance().GetAssetBrowserTileSize();
        }
        std::ofstream file(dest);
        if (file.is_open())
            file << j.dump(2);
    }
    catch (...) {
    }
}

static void LoadUiPrefs() {
    PlayFpsOverlay::SetEnabled(false);
    g_uiZoomPercent = 100;
    g_uiZoom = 1.0f;
    try {
        std::ifstream file(content_path::ReadableFileCurrent(kUiPrefsPath));
        if (!file.is_open()) return;
        json j;
        file >> j;
        PlayFpsOverlay::LoadPrefs(j);
        const int percent = j.value("uiZoomPercent", 100);
        if (IsValidUiZoomPercent(percent)) {
            g_uiZoomPercent = percent;
            g_uiZoom = static_cast<float>(percent) / 100.0f;
        }
        // Asset Browser prefs are applied when a project is open / panel syncs.
        if (ProjectManager::Instance().IsOpen()) {
            ProjectManager::Instance().SetAssetBrowserListView(
                j.value("assetBrowserListView", false));
            ProjectManager::Instance().SetAssetBrowserTileSize(
                j.value("assetBrowserTileSize", 100));
        }
    }
    catch (...) {
    }
}

static void ApplyUiZoom(int percent) {
    if (!IsValidUiZoomPercent(percent))
        percent = 100;
    g_uiZoomPercent = percent;
    g_uiZoom = static_cast<float>(percent) / 100.0f;

    if (!g_imguiInitialized) return;

    ImGuiStyle& style = ImGui::GetStyle();
    if (!g_baseStyleCaptured) {
        g_baseStyle = style;
        g_baseStyleCaptured = true;
    }
    style = g_baseStyle;
    style.ScaleAllSizes(g_uiZoom);
    style.FontScaleMain = g_uiZoom;
}

static std::string ShaderVariantPreviewLabel(Shader3DVariant v) {
    const char* label = Shader3DVariantLabel(v);
    if (Shader3DVariantIsUserVisible(v))
        return label;
    return std::string(label) + " (legacy)";
}

struct EditorLayoutMetrics {
    float topChrome = 0.0f;
    float leftWidth = 0.0f;
    float rightWidth = 0.0f;
    float bottomHeight = 0.0f;
    float sidePanelH = 0.0f;
    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportW = 0.0f;
    float viewportH = 0.0f;
};

GameObject* g_selNode = nullptr;
UIElement* g_selUiElement = nullptr;

// Editor scene dirty-state (unsaved changes).
static bool g_sceneDirty = false;

bool EngineGUI_IsSceneDirty() { return g_sceneDirty; }
void EngineGUI_MarkSceneDirty() { g_sceneDirty = true; }

bool EngineGUI_ApplySkyboxCubemapPath(const std::string& assetPath) {
    const std::string romPath = skybox_asset::RomPathFromAssetPath(assetPath);
    if (romPath.empty())
        return false;
    RenderSettingsData& rs = RenderSettings_Get();
    if (rs.skyboxCubemapPath == romPath) {
        RenderSettings_ReloadSkybox();
        return true;
    }
    rs.skyboxCubemapPath = romPath;
    RenderSettings_ReloadSkybox();
    EngineGUI_MarkSceneDirty();
    return true;
}
void EngineGUI_ClearSceneDirty() { g_sceneDirty = false; }

// Unsaved-changes popup state. Used to gate scene switches and exit.
enum class PendingEditorAction {
    None = 0,
    LoadScene,
    NewScene,
    OpenProject,
    CreateProject,
    CloseProject,
    Exit,
};

static PendingEditorAction g_pendingEditorAction = PendingEditorAction::None;
static std::string g_pendingEditorSceneFile;
static std::string g_pendingProjectPath;
static std::string g_pendingCreateProjectParent;
static std::string g_pendingCreateProjectName;
static bool g_showUnsavedChangesPopup = false;

void EngineGUI_QueueUnsavedLoad(const char* filename) {
    if (!filename || !filename[0]) return;
    g_pendingEditorAction = PendingEditorAction::LoadScene;
    g_pendingEditorSceneFile = filename;
    g_showUnsavedChangesPopup = true;
}

void EngineGUI_QueueUnsavedExit() {
    g_pendingEditorAction = PendingEditorAction::Exit;
    g_pendingEditorSceneFile.clear();
    g_showUnsavedChangesPopup = true;
}

const std::string PREFAB_DIR = "rom/prefabs/";

enum class ReloadLightState { Paused, Watching, Pending, Failed };
struct ReloadLight {
    bool failed = false;
    double brightUntil = 0;
    void Success() { failed = false; brightUntil = ImGui::GetTime() + 1.2; }
    void Failure() { failed = true; brightUntil = 0; }
};
static ReloadLight g_sceneReloadLight, g_scriptReloadLight;
static unsigned long long g_scriptFeedbackRevision = 0;
static ReloadLightState GetReloadLightState(bool active, bool paused, bool pending, const ReloadLight& light) {
    if (!active || paused) return ReloadLightState::Paused;
    if (pending) return ReloadLightState::Pending;
    return light.failed ? ReloadLightState::Failed : ReloadLightState::Watching;
}
static void LogContentReload(const std::string& message) {
    std::error_code ec;
    const auto size = std::filesystem::file_size("content_hot_reload.log", ec);
    std::ofstream log("content_hot_reload.log", !ec && size > 1024 * 1024 ? std::ios::trunc : std::ios::app);
    log << message << std::endl;
}
static bool ContentPathCurrentlyLoaded(const std::string& path) {
    const auto widePath = PathFromUtf8(path).wstring();
    if (TextureIsLoaded(widePath.c_str()) || CubemapIsLoaded(widePath.c_str())) return true;
    if (ModelUtil_IsTextureCached(widePath.c_str())) return true;
    if (ModelAssetCache::Instance().IsModelLoaded(path)) return true;
    if (AudioManager::Instance().IsContentLoaded(path)) return true;
    return false;
}
static std::string g_contentProjectRoot;
static ContentFileMonitor g_contentMonitor;
void EngineGUI_RequestContentRefresh() { g_contentMonitor.RefreshSoon(); }
static std::vector<ContentFileMonitor::Change> g_contentChanges;
static ReloadLight g_contentReloadLight;
static std::string g_contentReloadStatus = "Watching project content";
static bool g_contentSceneReload = false;
static PlayMode g_contentPlayMode = PlayMode::None;
static SceneFileMonitor g_sceneFileMonitor;
static unsigned long long g_monitoredSceneRevision = 0;
static std::optional<SceneFileMonitor::Version> g_sceneReloadConflict;
static std::optional<SceneFileMonitor::Version> g_pendingSceneHotLoad;
static std::string g_sceneReloadStatus = "Watching current scene JSON";
static void ResetSceneHotLoad() {
    g_sceneFileMonitor.Reset();
    g_sceneReloadLight = {};
    g_sceneReloadConflict.reset();
    g_pendingSceneHotLoad.reset();
    g_monitoredSceneRevision = 0;
    g_sceneReloadStatus = "Watching current scene JSON";
}
static void QueueSceneHotLoad(SceneFileMonitor::Version version) {
    // Validate referenced images before legacy deserializers can show a modal or
    // replace the live scene with a missing/partially written resource.
    std::vector<std::string> models;
    std::vector<std::string> prefabs;
    try {
        const auto root = nlohmann::json::parse(version.content);
        std::function<void(const nlohmann::json&, const std::string&)> validate;
        validate = [&](const nlohmann::json& value, const std::string& key) {
            if (value.is_object()) { for (auto it = value.begin(); it != value.end(); ++it) validate(it.value(), it.key()); return; }
            if (value.is_array()) { for (const auto& child : value) validate(child, key); return; }
            if (!value.is_string()) return;
            if (key == "prefabAssetPath" && value.get<std::string>().size()) {
                const auto prefab = value.get<std::string>();
                if (std::find(prefabs.begin(), prefabs.end(), prefab) != prefabs.end()) return;
                prefabs.push_back(prefab);
                std::string content;
                if (!SceneFileMonitor::Read(prefab, content)) throw std::runtime_error("Cannot read prefab: " + prefab);
                validate(nlohmann::json::parse(content), "");
                return;
            }
            if ((key == "model" || key == "proxy_model") && value.get<std::string>().size()) models.push_back(value.get<std::string>());
            if (key != "texture" && key != "texturePath" && key != "normalMap" && key != "rampTexture" && key != "surfaceFxTexture" && key != "font") return;
            const auto path = value.get<std::string>();
            if (path.empty()) return;
            const auto file = PathFromUtf8(path);
            auto ext = PathToUtf8(file.extension());
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga")
                if (!TextureValidateFile(file.c_str())) throw std::runtime_error("Cannot read texture: " + path);
        };
        validate(root, "");
    } catch (const std::exception& error) {
        g_sceneReloadLight.Failure(); g_sceneReloadStatus = "Scene reload failed: " + std::string(error.what());
        LogContentReload(g_sceneReloadStatus);
        return;
    }
    ModelUtil_ReleaseTextureCache();
    if (!ModelAssetCache::Instance().BeginContentReload()) {
        g_sceneReloadLight.Failure();
        g_sceneReloadStatus = "Model reload failed; current scene retained";
        return;
    }
    for (const auto& model : models) {
        if (!ModelAssetCache::Instance().PrepareContentModel(model)) {
            ModelAssetCache::Instance().EndContentReload(false);
            g_sceneReloadLight.Failure(); g_sceneReloadStatus = "Model reload failed; current scene retained: " + model;
            LogContentReload(g_sceneReloadStatus); return;
        }
    }
    EngineGUI_QueueSceneLoad(version.path.c_str());
    g_pendingSceneHotLoad = std::move(version);
}
static void ResolveSceneReloadConflict(bool loadDisk) {
    if (!g_sceneReloadConflict) return;
    if (loadDisk) QueueSceneHotLoad(std::move(*g_sceneReloadConflict));
    else {
        g_sceneReloadStatus = "External scene version ignored; Editor changes kept";
        g_sceneReloadLight = {};
    }
    g_sceneReloadConflict.reset();
}
#ifdef _DEBUG
static void SceneHotLoadCheck_Tick(double elapsed);
#endif
static std::string g_scriptReloadStatus = "Script DLL reload: not initialized";

static void SaveScene();
static void SaveSceneToFile(const std::string& sceneFile);
static void NewScene();
static void SaveProject();
static void ApplyProjectSessionState();
static bool SwitchToOpenedProject();
static void RequestOpenProject(const std::string& rootPath);
static void RequestCloseProject();
static void CloseOpenedProject();
static void BeginNewProjectFlow();
static void BeginOpenProjectFlow();
static void OpenBuildPreflight();
static void RequestBuildCurrentProject();
static void ConsumePendingFolderPick();
static void DrawEngineStartScreen();
static void DrawEditorPopups();

static void RemapNodeTagsAfterDelete(GameObject* node, int deletedId) {
    if (!node)
        return;

    const int tagId = node->GetTagId();
    if (tagId == deletedId)
        node->SetTag(Tag::TAG_NONE);
    else if (tagId > deletedId)
        node->SetTag(tagId - 1);

    auto& ignore = node->m_ignoreCollisionTags;
    for (size_t i = 0; i < ignore.size();) {
        const int ignoreId = static_cast<int>(ignore[i]);
        if (ignoreId == deletedId) {
            ignore.erase(ignore.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (ignoreId > deletedId)
            ignore[i] = static_cast<Tag>(ignoreId - 1);
        ++i;
    }

    for (auto& child : node->GetChildren())
        RemapNodeTagsAfterDelete(child.get(), deletedId);
}

static void RemapSceneTagsAfterDelete(int deletedId) {
    for (auto& root : SceneLoader_GetSceneNodes())
        RemapNodeTagsAfterDelete(root.get(), deletedId);
}

static bool ToolbarIconButton(const char* icon, const char* tooltip)
{
    const bool pressed = ImGui::Button(icon, ImVec2(Ui(28.0f), 0.0f));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

static std::string TrimString(const std::string& value) {
    const char* whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) return "";
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

static std::string NormalizeSceneFileName(const std::string& sceneName) {
    std::string trimmed = TrimString(sceneName);
    if (trimmed.empty()) return "";

    for (char& c : trimmed) {
        if (c == '\\') c = '/';
    }

    fs::path path(trimmed);
    if (path.extension().empty())
        path += ".json";

    return path.generic_string();
}

bool EngineGUI_IsSceneGraphObject(GameObject* obj) {
    return SceneLoader_IsSceneGraphObject(obj);
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

namespace {
    struct PrimitiveCreateSpec {
        const char* menuLabel;
        const char* displayName;
        const char* assetPath;
        ColliderType colliderType;
        float height;
    };

    const PrimitiveCreateSpec kDefaultPrimitives[] = {
        { "Cube",     "Cube",     "rom/model/primitives/Cube.fbx",     ColliderType::BOX,      1.0f },
        { "Sphere",   "Sphere",   "rom/model/primitives/Sphere.fbx",   ColliderType::SPHERE,   1.0f },
        { "Cylinder", "Cylinder", "rom/model/primitives/Cylinder.fbx", ColliderType::CYLINDER, 1.0f },
        { "Cone",     "Cone",     "rom/model/primitives/Cone.fbx",     ColliderType::MESH,     1.0f },
        // CapsuleCollider height = cylinder segment (sphere centers), not total
        // extent. Mesh is total H=2 with r=0.5 → segment = 2 - 2*r = 1.
        { "Capsule",  "Capsule",  "rom/model/primitives/Capsule.fbx",  ColliderType::CAPSULE,  1.0f },
        { "Donut",    "Donut",    "rom/model/primitives/Donut.fbx",    ColliderType::MESH,     1.0f },
    };
}

static std::unique_ptr<GameObject> CreatePrimitiveGameObject(const PrimitiveCreateSpec& spec)
{
    auto node = CreateModelGameObject(spec.assetPath, spec.displayName);
    if (auto* col = FindColliderComponent(node)) {
        col->colliderType = spec.colliderType;
        col->radius = 0.5f;
        col->halfExtents = { 0.5f, 0.5f, 0.5f };
        col->height = spec.height;
    }
    return node;
}

static float EditorMenuBarHeight() {
    return g_imguiInitialized ? ImGui::GetFrameHeight() : 22.0f;
}

static float EditorTopChrome() {
    return EditorMenuBarHeight() + Ui(kToolbarHeight);
}

static EditorLayoutMetrics ComputeEditorLayout(float screenW, float screenH) {
    EditorLayoutMetrics layout;
    layout.topChrome = EditorTopChrome();
    layout.leftWidth = Ui(kLeftPanelWidth);
    layout.rightWidth = Ui(kRightPanelWidth);

    layout.bottomHeight = (std::max)(Ui(kBottomPanelMinHeight), screenH * kBottomPanelScreenFraction);
    const float maxBottomHeight = screenH - layout.topChrome - Ui(kEditorViewportMinHeight);
    layout.bottomHeight = (std::min)(layout.bottomHeight, maxBottomHeight);

    const float previewAvailX = layout.leftWidth;
    const float previewAvailY = layout.topChrome;
    const float previewAvailW = (std::max)(1.0f, screenW - layout.leftWidth - layout.rightWidth);
    const float previewAvailH = (std::max)(
        1.0f, screenH - layout.topChrome - layout.bottomHeight);
    layout.sidePanelH = previewAvailH;

    layout.viewportW = previewAvailW;
    layout.viewportH = layout.viewportW / kEditorViewportAspect;
    if (layout.viewportH > previewAvailH) {
        layout.viewportH = previewAvailH;
        layout.viewportW = layout.viewportH * kEditorViewportAspect;
    }
    layout.viewportX = previewAvailX + (previewAvailW - layout.viewportW) * 0.5f;
    layout.viewportY = previewAvailY + (previewAvailH - layout.viewportH) * 0.5f;

    // Side panels extend to viewport edges so letterbox gutters are filled.
    layout.leftWidth = layout.viewportX;
    layout.rightWidth = screenW - (layout.viewportX + layout.viewportW);
    return layout;
}

static std::unique_ptr<GameObject> CreateEmptyGameObject() {
    auto node = std::make_unique<GameObject>("GameObject");
    node->AddComponent<engine::Transform>();
    node->AddComponent<VisualComponent>();
    return node;
}

static std::unique_ptr<GameObject> CreateDirectionalLightObject() {
    auto node = std::make_unique<GameObject>("Directional Light");
    auto* t = node->AddComponent<engine::Transform>();
    t->localRotation = { 50.0f, -30.0f, 0.0f };
    node->AddComponent<DirectionalLightComponent>();
    node->AddComponent<VisualComponent>();
    return node;
}

static std::unique_ptr<GameObject> CreatePointLightObject() {
    auto node = std::make_unique<GameObject>("Point Light");
    auto* t = node->AddComponent<engine::Transform>();
    t->localPosition = { 0.0f, 8.0f, 0.0f };
    node->AddComponent<PointLightComponent>();
    node->AddComponent<VisualComponent>();
    return node;
}

static std::unique_ptr<GameObject> CreateSpotLightObject() {
    auto node = std::make_unique<GameObject>("Spot Light");
    auto* t = node->AddComponent<engine::Transform>();
    t->localRotation = { 45.0f, 0.0f, 0.0f };
    node->AddComponent<SpotLightComponent>();
    node->AddComponent<VisualComponent>();
    return node;
}

static std::unique_ptr<GameObject> CreateCameraObject(bool debugFly = false) {
    const std::string base = debugFly ? "Debug Camera" : "Camera";
    std::vector<std::string> names;
    std::function<void(GameObject*)> collect = [&](GameObject* n) {
        if(!n) return;
        names.push_back(n->GetName());
        for(const auto& child:n->GetChildren()) collect(child.get());
    };
    for(const auto& root:g_sceneNodes) collect(root.get());
    std::string name=base;
    for(int suffix=2;std::find(names.begin(),names.end(),name)!=names.end();++suffix)
        name=base+" "+std::to_string(suffix);
    auto node = std::make_unique<GameObject>(name);
    auto* t = node->AddComponent<engine::Transform>();
    t->localPosition = { 0.0f, 20.0f, -20.0f };
    t->localRotation = { 35.0f, 0.0f, 0.0f };
    node->AddComponent<CameraComponent>()->debugFly = debugFly;
    node->AddComponent<AudioListenerComponent>();
    return node;
}

static void SpawnRootGameObject(std::unique_ptr<GameObject> node) {
    if (!node) return;
    GameObject* raw = node.get();
    g_sceneNodes.push_back(std::move(node));
    g_selNode = raw;
    g_selUiElement = nullptr;
    EditorUndo_PushCreate(raw, nullptr, static_cast<int>(g_sceneNodes.size()) - 1);
    EngineGUI_MarkSceneDirty();
    SyncSceneRuntime();
}

static void SpawnChildGameObject(GameObject* parent) {
    if (!parent) return;
    auto node = CreateEmptyGameObject();
    GameObject* raw = node.get();
    parent->AddChild(std::move(node));
    g_selNode = raw;
    g_selUiElement = nullptr;
    const int index = static_cast<int>(parent->GetChildren().size()) - 1;
    EditorUndo_PushCreate(raw, parent, index);
    EngineGUI_MarkSceneDirty();
    SyncSceneRuntime();
}

static std::unique_ptr<GameObject> DuplicateGameObject(const GameObject& src) {
    return PrefabSerializer::Deserialize(PrefabSerializer::Serialize(src));
}

static void SpawnDuplicate(GameObject* source) {
    if (!source) return;
    auto dup = DuplicateGameObject(*source);
    if (!dup) return;
    dup->SetName(source->GetName() + " (Copy)");
    GameObject* raw = dup.get();
    GameObject* parent = source->GetParent();
    int index = 0;
    if (parent) {
        parent->AddChild(std::move(dup));
        index = static_cast<int>(parent->GetChildren().size()) - 1;
    } else {
        g_sceneNodes.push_back(std::move(dup));
        index = static_cast<int>(g_sceneNodes.size()) - 1;
    }
    g_selNode = raw;
    g_selUiElement = nullptr;
    EditorUndo_PushCreate(raw, parent, index);
    EngineGUI_MarkSceneDirty();
    SyncSceneRuntime();
}

static void DeleteGameObject(GameObject* toDelete, bool recordUndo = true) {
    if (!toDelete) return;
    if (recordUndo)
        EditorUndo_PushDelete(toDelete);
    ClearSceneRuntime();
    if (g_selNode == toDelete)
        g_selNode = nullptr;
    if (!toDelete->GetParent()) {
        auto it = std::find_if(g_sceneNodes.begin(), g_sceneNodes.end(),
            [toDelete](const std::unique_ptr<GameObject>& n) { return n.get() == toDelete; });
        if (it != g_sceneNodes.end())
            g_sceneNodes.erase(it);
    }
    else {
        GameObject* parent = toDelete->GetParent();
        auto& ch = parent->GetChildren();
        auto it = std::find_if(ch.begin(), ch.end(),
            [toDelete](const std::unique_ptr<GameObject>& n) { return n.get() == toDelete; });
        if (it != ch.end())
            ch.erase(it);
    }
    EngineGUI_MarkSceneDirty();
    SyncSceneRuntime();
}

static void ClearScene() {
    ClearSceneRuntime();
    g_sceneNodes.clear();
    g_selNode = nullptr;
    g_selUiElement = nullptr;
    SceneCanvas_ClearSceneCanvas();
    EditorUndo_Clear();
    EngineGUI_MarkSceneDirty();
    SyncSceneRuntime();
}

// --- Helpers ---
static bool ScrollValue(float* value, float baseSpeed = 1.0f) {
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            *value += wheel * baseSpeed * g_scrollSensitivity;
            return true;
        }
    }
    return false;
}

static std::string LowerAssetExtension(const char* path) {
    if (!path) return "";
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return ext;
}

static bool AssetPathHasExtension(const char* path, const char* const* extensions, int count) {
    const std::string ext = LowerAssetExtension(path);
    for (int i = 0; i < count; ++i) {
        if (ext == extensions[i])
            return true;
    }
    return false;
}

static bool AcceptAssetPathDrop(const char* const* extensions, int count, std::string& outPath) {
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path && AssetPathHasExtension(path, extensions, count)) {
                outPath = path;
                accepted = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}

static bool DrawScriptPropertyField(GameObject* editNode,
                                    const std::string& key,
                                    nlohmann::json& val) {
    std::string wid = "##SC_" + key;
    if (val.is_number_float()) {
        float f = val.get<float>();
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        if (ImGui::DragFloat(wid.c_str(), &f, 0.1f)) { val = f; return true; }
        if (ImGui::IsItemHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) { f += wheel * 0.1f * g_scrollSensitivity; val = f; return true; }
        }
    }
    else if (val.is_number_integer() || val.is_number_unsigned()) {
        int iv = val.get<int>();
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt(wid.c_str(), &iv)) { val = iv; return true; }
    }
    else if (val.is_boolean()) {
        bool bv = val.get<bool>();
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        if (ImGui::Checkbox(wid.c_str(), &bv)) { val = bv; return true; }
    }
    else if (val.is_string()) {
        static GameObject* s_editNode = nullptr;
        static std::string s_editKey;
        static char s_strBuf[256] = {};
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (s_editNode != editNode || s_editKey != key) {
            s_editNode = editNode; s_editKey = key;
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
        }
        if (ImGui::InputText(wid.c_str(), s_strBuf, sizeof(s_strBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            val = std::string(s_strBuf);
            return true;
        }
        if (!ImGui::IsItemActive())
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
    }
    return false;
}

static void DrawPostFxShowcaseWindow(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("PostFX Showcase", open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    static PostFXParams s_fx;
    static bool s_synced = false;
    if (!s_synced) {
        PostProcess_GetEffects(s_fx);
        s_synced = true;
    }

    auto applyFx = [&]() { PostProcess_SetEffects(s_fx); };

    ImGui::TextWrapped(
        "Live stackable post-process over the game viewport. "
        "Strength 0 = off for each effect.");
    ImGui::Separator();

    if (ImGui::SliderFloat("RGB Shift", &s_fx.rgbShift, 0.0f, 0.04f, "%.4f"))
        applyFx();
    if (ImGui::SliderFloat("Mosaic Blocks", &s_fx.mosaicBlocks, 0.0f, 80.0f, "%.0f"))
        applyFx();
    if (ImGui::SliderFloat("Posterize", &s_fx.posterize, 0.0f, 32.0f, "%.0f"))
        applyFx();
    if (ImGui::SliderFloat("Noise", &s_fx.noise, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Grayscale", &s_fx.grayscale, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Contrast (1.0 = neutral)", &s_fx.contrast, 0.0f, 2.0f, "%.2f"))
        applyFx();
    if (ImGui::SliderFloat("Vignette", &s_fx.vignette, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Scanline", &s_fx.scanline, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Flicker", &s_fx.flicker, 0.0f, 1.0f, "%.3f"))
        applyFx();

    ImGui::Separator();
    ImGui::Text("Bloom (Bright Glow)");
    if (ImGui::SliderFloat("Bloom Intensity", &s_fx.bloomIntensity, 0.0f, 2.5f, "%.2f"))
        applyFx();
    if (ImGui::SliderFloat("Bloom Threshold", &s_fx.bloomThreshold, 0.0f, 1.5f, "%.2f"))
        applyFx();
    if (ImGui::SliderFloat("Bloom Spread", &s_fx.bloomSpread, 0.1f, 4.0f, "%.2f"))
        applyFx();

    ImGui::Spacing();
    if (ImGui::Button("Off")) {
        s_fx = PostFXParams{};
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Bloom Only")) {
        s_fx = PostFXParams{};
        s_fx.bloomIntensity = 0.8f;
        s_fx.bloomThreshold = 0.7f;
        s_fx.bloomSpread = 1.2f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Dreamy Bloom")) {
        s_fx = PostFXParams{};
        s_fx.bloomIntensity = 1.5f;
        s_fx.bloomThreshold = 0.5f;
        s_fx.bloomSpread = 2.2f;
        s_fx.contrast = 1.1f;
        applyFx();
    }

    ImGui::Spacing();
    if (ImGui::Button("Backrooms VHS")) {
        s_fx = PostFXParams{};
        s_fx.noise = 0.12f;
        s_fx.vignette = 0.55f;
        s_fx.scanline = 0.25f;
        s_fx.contrast = 1.15f;
        s_fx.rgbShift = 0.0015f;
        s_fx.flicker = 0.08f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Emo")) {
        s_fx = PostFXParams{};
        s_fx.grayscale = 1.0f;
        s_fx.contrast = 1.4f;
        s_fx.noise = 0.15f;
        s_fx.posterize = 8.0f;
        applyFx();
    }

    ImGui::Spacing();
    if (ImGui::Button("Preset: Mild RGB")) {
        s_fx = PostFXParams{};
        s_fx.rgbShift = 0.008f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset: Mosaic")) {
        s_fx = PostFXParams{};
        s_fx.mosaicBlocks = 40.0f;
        applyFx();
    }
    if (ImGui::Button("Preset: Combined")) {
        s_fx = PostFXParams{};
        s_fx.rgbShift = 0.01f;
        s_fx.mosaicBlocks = 32.0f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset: Neon Glow")) {
        s_fx = PostFXParams{};
        s_fx.bloomIntensity = 1.2f;
        s_fx.bloomThreshold = 0.6f;
        s_fx.bloomSpread = 1.5f;
        s_fx.rgbShift = 0.003f;
        s_fx.vignette = 0.35f;
        applyFx();
    }

    ImGui::Separator();
    ImGui::Text("Bursts (temporary RGB punch)");
    if (ImGui::Button("GOAL Burst (0.012 / 0.45s)"))
        PostProcess_TriggerBurst(0.012f, 0.45f);
    ImGui::SameLine();
    if (ImGui::Button("Strong Burst (0.025 / 0.6s)"))
        PostProcess_TriggerBurst(0.025f, 0.6f);

    ImGui::Spacing();
    ImGui::TextDisabled("Hotkeys: F9 = GOAL burst, F10 = toggle mosaic");

    ImGui::End();
}

static void DrawShortcutHintsWindow(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shortcut Hints", open)) {
        ImGui::End();
        return;
    }

    auto row = [](const char* keys, const char* desc) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(keys);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", desc);
    };

    if (ImGui::BeginTable("##ShortcutHints", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Editor");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("");

        row("F2", "Play in viewport / stop");
        row("F3", "Play fullscreen / stop");
        row("F6", "Toggle third-person cursor");
        row("F8", "Toggle job-system debug window");
        row("F", "Focus selected object in scene view");
        row("Ctrl+S", "Save scene");
        row("Ctrl+Shift+S", "Save project");
        row("Ctrl+Z / Ctrl+Y", "Undo / redo");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Play / Runtime");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("");

        row("F7", "Cycle time scale (1.0 ?? 0.5 ?? 0.25 ?? freeze)");
        row("F11", "Toggle collision visualizer");
        row("F12", "Step one frame while frozen");
        row("F9", "PostFX GOAL RGB burst");
        row("F10", "Toggle PostFX mosaic showcase");

        ImGui::EndTable();
    }

    ImGui::End();
}

static void DrawRenderingSettingsWindow(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Rendering Settings", open, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::BeginDisabled(EngineGUI_IsPlaying());
    bool scene2D=SceneLoader_Is2D();
    if(ImGui::Checkbox("2D Scene",&scene2D)) {
        SceneLoader_SetExecutionMode(scene2D,!scene2D);
        EngineGUI_MarkSceneDirty();
    }
    if(!scene2D) {
        bool physics3D=SceneLoader_Uses3DPhysics();
        if(ImGui::Checkbox("3D Physics",&physics3D)) {
            SceneLoader_SetExecutionMode(false,physics3D); EngineGUI_MarkSceneDirty();
        }
    } else {
        bool physics2D=SceneLoader_Uses2DPhysics();
        if(ImGui::Checkbox("2D Physics (Box2D)",&physics2D)) {
            SceneLoader_SetExecutionMode(true,false,physics2D); EngineGUI_MarkSceneDirty();
        }
        ImGui::TextDisabled("Sprite rendering; 3D physics and effects disabled.");
    }
    ImGui::EndDisabled();
    RenderSettingsData& rs = RenderSettings_Get();
    static char cubemapBuf[256] = {};
    static std::string syncedCubemapPath;

    if (rs.skyboxCubemapPath != syncedCubemapPath) {
        syncedCubemapPath = rs.skyboxCubemapPath;
        strncpy_s(cubemapBuf, rs.skyboxCubemapPath.c_str(), sizeof(cubemapBuf) - 1);
    }

    ImGui::Text("Environment");
    ImGui::Separator();
    if (ImGui::Checkbox("Skybox", &rs.skyboxEnabled))
        RenderSettings_ReloadSkybox();

    ImGui::Text("Cubemap / panorama PNG");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##SkyboxCubemap", cubemapBuf, sizeof(cubemapBuf),
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        rs.skyboxCubemapPath = cubemapBuf;
        syncedCubemapPath = rs.skyboxCubemapPath;
        RenderSettings_ReloadSkybox();
        EngineGUI_MarkSceneDirty();
    }
    {
        const char* skyExts[] = { ".dds", ".png" };
        std::string droppedPath;
        if (AcceptAssetPathDrop(skyExts, IM_ARRAYSIZE(skyExts), droppedPath) &&
            EngineGUI_ApplySkyboxCubemapPath(droppedPath)) {
            syncedCubemapPath = rs.skyboxCubemapPath;
            strncpy_s(cubemapBuf, rs.skyboxCubemapPath.c_str(), sizeof(cubemapBuf) - 1);
        }
    }
    if (panorama::IsEquirectSkyPath(rs.skyboxCubemapPath)) {
        ImGui::DragFloat("Vertical FOV", &rs.skyboxVerticalFov, 0.5f, 0.0f, 180.0f, "%.1f deg (0=auto)");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (!(rs.skyboxVerticalFov > 0.0f))
                rs.skyboxVerticalFov = 0.0f;
            else if (rs.skyboxVerticalFov > 180.0f)
                rs.skyboxVerticalFov = 180.0f;
            RenderSettings_ReloadSkybox();
            EngineGUI_MarkSceneDirty();
        }
        ImGui::TextDisabled("0 = 360*height/width, centered on the horizon. Width is always 360 deg.");
        const panorama::Prep& prep = panorama::CurrentPrep();
        if (!prep.isPanorama || !panorama::SameRomPath(prep.sourceRom, rs.skyboxCubemapPath)) {
            ImGui::TextDisabled("Equirectangular PNG cooks to a DDS cubemap on reload.");
        } else if (!prep.ok) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "Panorama failed");
            ImGui::TextWrapped("%s", prep.error.c_str());
        } else if (!prep.iblReady) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "IBL not matching sky");
            if (!prep.error.empty())
                ImGui::TextWrapped("%s", prep.error.c_str());
        } else {
            ImGui::TextDisabled("IBL baked from this panorama. BRDF LUT is unchanged.");
        }
    }

    ImGui::ColorEdit4("Tint", &rs.skyboxTint.x);
    ImGui::DragFloat("Exposure", &rs.skyboxExposure, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("Rotation Y", &rs.skyboxRotationYDeg, 1.0f, -180.0f, 180.0f);
    if (ImGui::Checkbox("IBL", &rs.iblEnabled))
        RenderSettings_ReloadSkybox();
    ImGui::DragFloat("IBL Intensity", &rs.iblIntensity, 0.05f, 0.0f, 5.0f);
    ImGui::ColorEdit4("Clear Color", &rs.clearColor.x);

    ImGui::Text("Default Shader");
    {
        const int variantCount = Shader3DVariantCount();
        const Shader3DVariant currentVariant = Shader3DVariantFromId(rs.defaultShader);
        const std::string preview = ShaderVariantPreviewLabel(currentVariant);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##DefaultShader", preview.c_str())) {
            for (int i = 0; i < variantCount; ++i) {
                const Shader3DVariant v = Shader3DVariantByIndex(i);
                if (!Shader3DVariantIsUserVisible(v))
                    continue;
                const bool selected = (v == currentVariant);
                if (ImGui::Selectable(Shader3DVariantLabel(v), selected))
                    rs.defaultShader = Shader3DVariantId(v);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled(
            "PBR (Cook-Torrance): assign via this Default Shader or a VisualComponent Shader override. "
            "Tune Material Roughness (0.1 vs 0.8) and Metallic (0 vs 1); compare side-by-side with Toon. "
            "Open rom/scene/scene_pbr_showcase.json for a ready grid.");
    }

    ImGui::Spacing();
    ImGui::Text("Shadows");
    ImGui::Separator();
    {
        const char* preview = RenderSettings_ShadowQualityLabel(rs.shadowQuality);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##ShadowQuality", preview)) {
            const ShadowQuality options[] = {
                ShadowQuality::Low,
                ShadowQuality::Medium,
                ShadowQuality::High
            };
            for (ShadowQuality q : options) {
                const bool selected = (q == rs.shadowQuality);
                if (ImGui::Selectable(RenderSettings_ShadowQualityLabel(q), selected)) {
                    if (rs.shadowQuality != q) {
                        rs.shadowQuality = q;
                        RenderSettings_ApplyShadowQuality();
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        const ShadowQualityPreset preset = RenderSettings_GetShadowPreset();
        ImGui::TextDisabled("%u px  |  %d cascades  |  range %.0f",
            preset.mapSize, preset.cascadeCount, preset.maxDistance);
    }

    ImGui::Spacing();
    ImGui::Text("Culling");
    ImGui::Separator();
    ImGui::Checkbox("Frustum Culling (3D)", &rs.frustumCullingEnabled);
    {
        const FrustumCullStats cull = FrustumCull_GetLastStats();
        ImGui::TextDisabled("frustum  considered %d  |  culled %d  |  drawn %d",
            cull.considered, cull.culled, cull.drawn);
        ImGui::TextDisabled("Main camera pass only.");
    }
    ImGui::Checkbox("Shadow Cascade Culling", &rs.shadowCascadeCullingEnabled);
    {
        const ShadowCascadeCullStats sc = ShadowCascadeCull_GetLastStats();
        ImGui::TextDisabled("shadow   considered %d  |  culled %d  |  drawn %d",
            sc.considered, sc.culled, sc.drawn);
        const int cascades = ShadowCascadeCull_GetLastCascadeCount();
        for (int i = 0; i < cascades; ++i) {
            const ShadowCascadeCullStats cs = ShadowCascadeCull_GetLastCascadeStats(i);
            ImGui::TextDisabled("  cascade %d  c:%d x:%d d:%d",
                i, cs.considered, cs.culled, cs.drawn);
        }
        ImGui::TextDisabled("Per-cascade light ortho; open near toward light.");
    }
    ImGui::Checkbox("Sprite Culling (2D)", &rs.spriteCullingEnabled);
    {
        const SpriteCullStats sc = SpriteCull_GetLastStats();
        ImGui::TextDisabled("sprite   considered %d  |  culled %d  |  drawn %d",
            sc.considered, sc.culled, sc.drawn);
        ImGui::TextDisabled("Ortho view-rect; render submit only.");
    }

    ImGui::Spacing();
    ImGui::Text("Fog");
    ImGui::Separator();
    ImGui::Checkbox("Height Fog", &rs.fogEnabled);
    ImGui::ColorEdit3("Fog Color", &rs.fogColor.x);
    ImGui::DragFloat("Density", &rs.fogDensity, 0.001f, 0.0f, 1.0f, "%.4f");
    ImGui::DragFloat("Start", &rs.fogStart, 0.5f, 0.0f, 500.0f);
    ImGui::DragFloat("Height", &rs.fogHeight, 0.1f, -50.0f, 200.0f);
    ImGui::DragFloat("Height Falloff", &rs.fogHeightFalloff, 0.01f, 0.0f, 5.0f, "%.3f");

    ImGui::Spacing();
    ImGui::TextDisabled(
        "A DDS cubemap loads directly. A PNG panorama (width = 360 deg; height <= width/2) cooks into rom/skybox/cache/. Drag a .png/.dds from the Asset Browser onto the path field, or right-click Set as > Skybox.");
    ImGui::End();
}

static void DrawPropertiesEmptyState() {
    ImGui::TextDisabled("No object selected");
    ImGui::Spacing();
    ImGui::TextWrapped("Use GameObject menu to create objects or UI elements.");
    ImGui::Spacing();
    ImGui::TextWrapped("Use the Asset Browser below to browse rom/ assets.");
    ImGui::Spacing();
    ImGui::TextWrapped("Use Window > Rendering Settings for skybox, shadows, and render defaults.");
}

static void ApplyProjectSessionState() {
    auto& project = ProjectManager::Instance();
    if (!project.IsOpen())
        return;

    PlayFpsOverlay::LoadFromFile();

    const int zoom = project.GetEditorUiZoomPercent();
    if (IsValidUiZoomPercent(zoom))
        ApplyUiZoom(zoom);
    else
        ApplyUiZoom(100);

    TagRegistry::Instance().LoadFromFile();
    ModelAssetCache::Instance().RefreshAssetDirectory();
    ProjectAssetIndex::Instance().Refresh();
    SceneLoader_RefreshSceneAssets();

    if (!project.GetData().defaultScene.empty())
        SceneLoader_SetDefaultSceneFileName(project.GetData().defaultScene, false);
    else
        SceneLoader_LoadDefaultSceneSetting();

    SceneLoader_UnloadScriptDlls();
    SceneLoader_LoadScriptDlls();
    g_scriptReloadStatus = "Watching script DLLs";
    g_scriptReloadLight = {};
    g_scriptFeedbackRevision = ScriptFactory::Instance().GetLastReloadFeedback().revision;
}

static bool SwitchToOpenedProject() {
    if (!ProjectManager::Instance().IsOpen())
        return false;

    if (g_isPlaying)
        ReturnToStart();

    ResetSceneHotLoad();
    ApplyProjectSessionState();

    std::string scenePath = ProjectManager::Instance().ResolveSceneToLoad();
    if (scenePath.empty())
        scenePath = EngineGUI_GetDefaultScenePath();

    if (!scenePath.empty())
        EngineGUI_LoadFromFileCore(scenePath.c_str());

    ProjectManager::Instance().UpdateWindowTitle(GetWindowHandle());
    g_projectStatusMessage.clear();
    return true;
}

static void RequestOpenTestbed() {
    const std::string path = ProjectManager::Instance().ResolveInRepoTestbedPath();
    if (path.empty()) {
        MessageBoxA(GetWindowHandle(),
                    "The in-repo testbed folder was not found next to the engine.",
                    "Open Testbed", MB_OK | MB_ICONWARNING);
        return;
    }
    RequestOpenProject(path);
}

static void RequestOpenProject(const std::string& rootPath) {
    if (rootPath.empty())
        return;

    if (EngineGUI_IsSceneDirty()) {
        g_pendingEditorAction = PendingEditorAction::OpenProject;
        g_pendingProjectPath = rootPath;
        g_showUnsavedChangesPopup = true;
        return;
    }

    std::string error;
    if (!ProjectManager::Instance().OpenProject(rootPath, &error)) {
        g_projectStatusMessage = error.empty() ? "Failed to open project." : error;
        MessageBoxA(GetWindowHandle(), g_projectStatusMessage.c_str(), "Open Project",
                    MB_OK | MB_ICONERROR);
        return;
    }
    SwitchToOpenedProject();
}

static void CloseOpenedProject() {
    ResetSceneHotLoad();
    if (!ProjectManager::Instance().IsOpen())
        return;

    if (g_isPlaying)
        ReturnToStart();

    ClearSceneRuntime();
    g_sceneNodes.clear();
    g_selNode = nullptr;
    g_selUiElement = nullptr;
    SceneCanvas_ClearSceneCanvas();
    EditorUndo_Clear();
    EngineGUI_ClearSceneDirty();
    SyncSceneRuntime();

    SceneLoader_UnloadScriptDlls();
    ProjectManager::Instance().CloseProject();
    ProjectManager::Instance().UpdateWindowTitle(GetWindowHandle());
    g_projectStatusMessage.clear();
}

static void RequestCloseProject() {
    if (!ProjectManager::Instance().IsOpen())
        return;

    if (EngineGUI_IsSceneDirty()) {
        g_pendingEditorAction = PendingEditorAction::CloseProject;
        g_pendingEditorSceneFile.clear();
        g_pendingProjectPath.clear();
        g_showUnsavedChangesPopup = true;
        return;
    }

    CloseOpenedProject();
}

static void BeginOpenProjectFlow() {
    std::string folder;
    if (!ProjectManager::PickFolder(GetWindowHandle(), L"Open Project", &folder))
        return;
    RequestOpenProject(folder);
}

static void SetBuildDestBuffer(const std::string& path) {
    strncpy_s(g_buildDest, path.c_str(), _TRUNCATE);
}

static void OpenBuildPreflight() {
    auto& project = ProjectManager::Instance();
    if (!project.IsOpen())
        return;

    PlayablePackage::Prefs prefs;
    PlayablePackage::LoadPrefs(project.GetRootPath(), &prefs);
    SetBuildDestBuffer(prefs.destination);
    g_buildConfigIndex = (prefs.configuration == PlayablePackage::Configuration::Release) ? 1 : 0;
    g_showBuildPreflight = true;
}

static void RequestBuildCurrentProject() {
    if (!ProjectManager::Instance().IsOpen() || PlayablePackage::IsBuildRunning())
        return;

    if (EngineGUI_IsSceneDirty()) {
        g_showBuildUnsavedPopup = true;
        return;
    }

    OpenBuildPreflight();
}

static void BeginNewProjectFlow() {
    std::string folder;
    if (!ProjectManager::PickFolder(GetWindowHandle(), L"Select Parent Folder for New Project",
                                    &folder)) {
        return;
    }
    g_newProjectParentDir = folder;
    snprintf(g_newProjectName, sizeof(g_newProjectName), "NewProject");
    g_showNewProjectPopup = true;
}

static void SaveProject() {
    auto& project = ProjectManager::Instance();
    if (!project.IsOpen()) {
        MessageBoxA(GetWindowHandle(), "No project is open.", "Save Project",
                    MB_OK | MB_ICONWARNING);
        return;
    }

    SaveScene();

    const std::string sceneFile = NormalizeSceneFileName(SceneLoader_GetSaveFileName());
    if (!sceneFile.empty())
        project.SetLastOpenedScene(sceneFile);

    project.SetDefaultScene(SceneLoader_GetDefaultSceneFileName());
    project.SetEditorUiZoomPercent(g_uiZoomPercent);
    SaveUiPrefs();

    std::string error;
    if (!project.SaveProjectFile(&error)) {
        MessageBoxA(GetWindowHandle(),
                    error.empty() ? "Failed to save project.json" : error.c_str(),
                    "Save Project", MB_OK | MB_ICONERROR);
        return;
    }

    project.UpdateWindowTitle(GetWindowHandle());
    g_projectStatusMessage = "Project saved.";
}

static void ConsumePendingFolderPick() {
    // Run Win32 folder dialogs after ImGui finishes the frame (avoids menu reentrancy).
    if (g_pendingFolderPick == PendingFolderPickAction::None)
        return;

    const PendingFolderPickAction action = g_pendingFolderPick;
    g_pendingFolderPick = PendingFolderPickAction::None;
    if (action == PendingFolderPickAction::NewProject)
        BeginNewProjectFlow();
    else if (action == PendingFolderPickAction::OpenProject)
        BeginOpenProjectFlow();
    else if (action == PendingFolderPickAction::BuildOutput) {
        std::string folder;
        if (ProjectManager::PickFolder(GetWindowHandle(), L"Playable Package Folder",
                                       &folder, g_buildDest))
            SetBuildDestBuffer(folder);
    }
}

static ID3D11ShaderResourceView* GetIvyLogoSrv() {
    static ID3D11ShaderResourceView* s_srv = nullptr;
    static bool s_attempted = false;
    if (!s_attempted) {
        s_attempted = true;
        ID3D11Device* device = GetDevice();
        if (device) {
            ID3D11Resource* tex = nullptr;
            const std::wstring logo = content_path::ReadableWideCurrent(L"rom/branding/ivy_logo.png");
            if (FAILED(DirectX::CreateWICTextureFromFile(
                    device, logo.empty() ? L"rom/branding/ivy_logo.png" : logo.c_str(), &tex, &s_srv))) {
                s_srv = nullptr;
            }
            if (tex)
                tex->Release();
        }
    }
    return s_srv;
}

static void DrawEngineStartScreen() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 1.0f));
    ImGui::Begin("##EngineStartScreen", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleColor();

    const ImVec2 cardSize(480.0f, 520.0f);
    const ImVec2 cardPos(
        viewport->Pos.x + (viewport->Size.x - cardSize.x) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - cardSize.y) * 0.5f);
    ImGui::SetCursorScreenPos(cardPos);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(36.0f, 30.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.93f, 0.95f, 1.0f));
    ImGui::BeginChild("##StartCard", cardSize, ImGuiChildFlags_Borders);

    if (ID3D11ShaderResourceView* logoSrv = GetIvyLogoSrv()) {
        const float logoSize = 160.0f;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (logoSize < avail)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - logoSize) * 0.5f);
        ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)logoSrv), ImVec2(logoSize, logoSize));
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
    }

    {
        const char* title = "Ivy Engine";
        const float titleW = ImGui::CalcTextSize(title).x;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (titleW < avail)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - titleW) * 0.5f);
        ImGui::TextUnformatted(title);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.64f, 1.0f));
    {
        const char* subtitle = "Custom D3D11 game engine";
        const float subW = ImGui::CalcTextSize(subtitle).x;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (subW < avail)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - subW) * 0.5f);
        ImGui::TextUnformatted(subtitle);
    }
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    const float btnW = ImGui::GetContentRegionAvail().x;
    const float btnH = 44.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.65f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.45f, 0.26f, 1.0f));
    if (ImGui::Button("Open Project...", ImVec2(btnW, btnH)))
        g_pendingFolderPick = PendingFolderPickAction::OpenProject;
    ImGui::PopStyleColor(3);
    if (!ProjectManager::Instance().ResolveInRepoTestbedPath().empty()) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (ImGui::Button("Open Testbed", ImVec2(btnW, btnH)))
            RequestOpenTestbed();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    if (ImGui::Button("New Project...", ImVec2(btnW, btnH)))
        g_pendingFolderPick = PendingFolderPickAction::NewProject;

    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    ImGui::TextUnformatted("Recent projects");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const auto& recent = ProjectManager::Instance().GetRecentProjects();
    if (recent.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.52f, 0.56f, 1.0f));
        ImGui::TextUnformatted("(No recent projects)");
        ImGui::PopStyleColor();
    }
    else {
        const size_t count = (std::min)(recent.size(), size_t{5});
        for (size_t i = 0; i < count; ++i) {
            const std::string& path = recent[i];
            const std::string leaf = fs::path(path).filename().string();
            const std::string label = leaf.empty() ? path : leaf;
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(label.c_str()))
                RequestOpenProject(path);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.c_str());
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::End();
}

static void DrawEditorMenuBar() {
    const bool canMutate = !g_isPlaying && !IsSceneLoadBusy();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project...", nullptr, false, canMutate))
            g_pendingFolderPick = PendingFolderPickAction::NewProject;
        if (ImGui::MenuItem("Open Project...", nullptr, false, canMutate))
            g_pendingFolderPick = PendingFolderPickAction::OpenProject;
        if (ImGui::MenuItem("Open Testbed", nullptr, false,
                            canMutate && !ProjectManager::Instance().ResolveInRepoTestbedPath().empty()))
            RequestOpenTestbed();
        if (ImGui::MenuItem("Close Project", nullptr, false,
                            canMutate && ProjectManager::Instance().IsOpen()))
            RequestCloseProject();
        if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S", false, canMutate && ProjectManager::Instance().IsOpen()))
            SaveProject();

        if (ImGui::BeginMenu("Recent Projects")) {
            const auto& recent = ProjectManager::Instance().GetRecentProjects();
            if (recent.empty()) {
                ImGui::MenuItem("(empty)", nullptr, false, false);
            }
            else {
                for (size_t i = 0; i < recent.size(); ++i) {
                    const std::string& path = recent[i];
                    const bool isCurrent =
                        ProjectManager::Instance().IsOpen() &&
                        path == ProjectManager::Instance().GetRootPath();
                    char label[512];
                    snprintf(label, sizeof(label), "%zu  %s", i + 1, path.c_str());
                    if (ImGui::MenuItem(label, nullptr, isCurrent, canMutate && !isCurrent))
                        RequestOpenProject(path);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        {
            const bool canBuild = canMutate && ProjectManager::Instance().IsOpen() &&
                                  !PlayablePackage::IsBuildRunning();
            if (ImGui::MenuItem("Build Current Project...", nullptr, false, canBuild))
                RequestBuildCurrentProject();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, canMutate))
            SaveScene();
        if (ImGui::MenuItem("New Scene...", nullptr, false, canMutate))
            g_showNewScenePopup = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Scene...", nullptr, false, canMutate))
            ImGui::OpenPopup("ClearSceneConfirm");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const bool canEdit = !g_isPlaying && !EngineGUI_IsSceneAssetPreloadActive();
        auto& undoStack = EditorUndoStack::Instance();
        const std::string undoDesc = undoStack.TopUndoDescribe();
        const std::string redoDesc = undoStack.TopRedoDescribe();
        char undoLabel[256];
        char redoLabel[256];
        if (undoDesc.empty())
            snprintf(undoLabel, sizeof(undoLabel), "Undo");
        else
            snprintf(undoLabel, sizeof(undoLabel), "Undo %s", undoDesc.c_str());
        if (redoDesc.empty())
            snprintf(redoLabel, sizeof(redoLabel), "Redo");
        else
            snprintf(redoLabel, sizeof(redoLabel), "Redo %s", redoDesc.c_str());

        if (ImGui::MenuItem(undoLabel, "Ctrl+Z", false, canEdit && undoStack.CanUndo())) {
            if (EditorUndo_Undo()) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        if (ImGui::MenuItem(redoLabel, "Ctrl+Y", false, canEdit && undoStack.CanRedo())) {
            if (EditorUndo_Redo()) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject")) {
        if (g_isPlaying)
            ImGui::BeginDisabled(true);
        if (ImGui::MenuItem("Create Empty"))
            SpawnRootGameObject(CreateEmptyGameObject());
        if (ImGui::BeginMenu("3D Object")) {
            for (const PrimitiveCreateSpec& spec : kDefaultPrimitives) {
                if (ImGui::MenuItem(spec.menuLabel))
                    SpawnRootGameObject(CreatePrimitiveGameObject(spec));
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Camera")) SpawnRootGameObject(CreateCameraObject());
            if (ImGui::MenuItem("Debug Camera")) {
                SpawnRootGameObject(CreateCameraObject(true));
                if(g_selNode) {
                    EngineGUI_SetCameraMode(CameraMode::SceneCamera);
                    EngineGUI_SetActiveCameraNodeName(g_selNode->GetName());
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional Light"))
                SpawnRootGameObject(CreateDirectionalLightObject());
            if (ImGui::MenuItem("Point Light"))
                SpawnRootGameObject(CreatePointLightObject());
            if (ImGui::MenuItem("Spot Light"))
                SpawnRootGameObject(CreateSpotLightObject());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("UI")) {
            if (ImGui::MenuItem("Button")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Button);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            if (ImGui::MenuItem("Text")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Text);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            if (ImGui::MenuItem("Sprite")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Sprite);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            if (ImGui::MenuItem("Bar")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Bar);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            ImGui::EndMenu();
        }
        if (g_isPlaying)
            ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        if (ImGui::MenuItem("Rendering Settings", nullptr, g_showRenderingSettings))
            g_showRenderingSettings = !g_showRenderingSettings;
        if (ImGui::MenuItem("PostFX Showcase", nullptr, g_showPostFxShowcase))
            g_showPostFxShowcase = !g_showPostFxShowcase;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project Settings")) {
        if (g_isPlaying)
            ImGui::BeginDisabled(true);
        if (ImGui::MenuItem("Default Scene..."))
            g_showDefaultScenePopup = true;
        if (ImGui::MenuItem("Gravity...", nullptr, false, ProjectManager::Instance().IsOpen()))
            g_showGravityPopup = true;
        if (ImGui::MenuItem("Audio...", nullptr, false, ProjectManager::Instance().IsOpen()))
            g_showAudioPopup = true;
        if (g_isPlaying)
            ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Engine Settings")) {
        if (ImGui::MenuItem("Show FPS When Playing", nullptr, PlayFpsOverlay::IsEnabled())) {
            PlayFpsOverlay::SetEnabled(!PlayFpsOverlay::IsEnabled());
            SaveUiPrefs();
        }
        if (ImGui::BeginMenu("UI Scale")) {
            for (int percent : kUiZoomPercents) {
                char label[16];
                snprintf(label, sizeof(label), "%d%%", percent);
                if (ImGui::MenuItem(label, nullptr, g_uiZoomPercent == percent)) {
                    ApplyUiZoom(percent);
                    SaveUiPrefs();
                    ProjectManager::Instance().SetEditorUiZoomPercent(percent);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Shortcut Hints...", nullptr, g_showShortcutHints))
            g_showShortcutHints = !g_showShortcutHints;
        ImGui::EndMenu();
    }
}

static void ExecutePendingEditorAction() {
    const PendingEditorAction action = g_pendingEditorAction;
    const std::string payload = g_pendingEditorSceneFile;
    const std::string projectPath = g_pendingProjectPath;
    const std::string createParent = g_pendingCreateProjectParent;
    const std::string createName = g_pendingCreateProjectName;

    g_pendingEditorAction = PendingEditorAction::None;
    g_pendingEditorSceneFile.clear();
    g_pendingProjectPath.clear();
    g_pendingCreateProjectParent.clear();
    g_pendingCreateProjectName.clear();
    g_showUnsavedChangesPopup = false;
    ImGui::CloseCurrentPopup();

    switch (action) {
    case PendingEditorAction::LoadScene:
        if (!payload.empty())
            EngineGUI_LoadFromFileCore(payload.c_str());
        break;

    case PendingEditorAction::NewScene:
        if (!payload.empty()) {
            SceneLoader_ClearSceneNodes();
            g_selNode = nullptr;
            g_selUiElement = nullptr;
            SceneLoader_CopySceneFileName(payload);
            SaveScene();
        }
        break;

    case PendingEditorAction::OpenProject:
        if (!projectPath.empty()) {
            std::string error;
            if (!ProjectManager::Instance().OpenProject(projectPath, &error)) {
                MessageBoxA(GetWindowHandle(),
                            error.empty() ? "Failed to open project." : error.c_str(),
                            "Open Project", MB_OK | MB_ICONERROR);
            }
            else {
                SwitchToOpenedProject();
            }
        }
        break;

    case PendingEditorAction::CreateProject:
        if (!createParent.empty()) {
            std::string error;
            if (!ProjectManager::Instance().CreateProject(createParent, createName, &error,g_newProject2D)) {
                MessageBoxA(GetWindowHandle(),
                            error.empty() ? "Failed to create project." : error.c_str(),
                            "New Project", MB_OK | MB_ICONERROR);
            }
            else {
                SwitchToOpenedProject();
            }
        }
        break;

    case PendingEditorAction::CloseProject:
        CloseOpenedProject();
        break;

    case PendingEditorAction::Exit:
        PostQuitMessage(0);
        break;

    case PendingEditorAction::None:
    default:
        break;
    }
}

static void DrawEditorPopups() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("ClearSceneConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Clear all objects from the scene?");
        ImGui::TextDisabled("This cannot be undone.");
        ImGui::Separator();
        if (ImGui::Button("Clear", ImVec2(120, 0))) {
            ClearScene();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (g_showNewScenePopup)
        ImGui::OpenPopup("NewScenePopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("NewScenePopup", &g_showNewScenePopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New Scene Name");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##NewSceneNameModal", g_newSceneName, IM_ARRAYSIZE(g_newSceneName));
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (EngineGUI_IsSceneDirty()) {
                const std::string sceneFile = NormalizeSceneFileName(g_newSceneName);
                if (!sceneFile.empty()) {
                    g_pendingEditorAction = PendingEditorAction::NewScene;
                    g_pendingEditorSceneFile = sceneFile;
                    g_showUnsavedChangesPopup = true;
                }
            }
            else {
                NewScene();
            }
            g_showNewScenePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_showNewScenePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showNewProjectPopup)
        ImGui::OpenPopup("NewProjectPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("NewProjectPopup", &g_showNewProjectPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("2D Project",&g_newProject2D);
        ImGui::Text("New Project Name");
        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("##NewProjectNameModal", g_newProjectName, IM_ARRAYSIZE(g_newProjectName));
        ImGui::TextDisabled("%s", g_newProjectParentDir.c_str());
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            const std::string name = TrimString(g_newProjectName);
            if (EngineGUI_IsSceneDirty()) {
                g_pendingEditorAction = PendingEditorAction::CreateProject;
                g_pendingCreateProjectParent = g_newProjectParentDir;
                g_pendingCreateProjectName = name;
                g_showUnsavedChangesPopup = true;
            }
            else {
                std::string error;
                if (!ProjectManager::Instance().CreateProject(g_newProjectParentDir, name, &error,g_newProject2D)) {
                    MessageBoxA(GetWindowHandle(),
                                error.empty() ? "Failed to create project." : error.c_str(),
                                "New Project", MB_OK | MB_ICONERROR);
                }
                else {
                    SwitchToOpenedProject();
                }
            }
            g_showNewProjectPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_showNewProjectPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showAddTagPopup)
        ImGui::OpenPopup("AddTagPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("AddTagPopup", &g_showAddTagPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        static bool s_addTagFailed = false;
        ImGui::Text("New Tag Name");
        ImGui::SetNextItemWidth(260.0f);
        const bool enterPressed = ImGui::InputText("##NewTagNameModal", g_newTagName,
            IM_ARRAYSIZE(g_newTagName), ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemEdited())
            s_addTagFailed = false;
        if (s_addTagFailed)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name is empty or already exists.");
        ImGui::Separator();

        auto tryAddTag = [&]() {
            TagRegistry& tags = TagRegistry::Instance();
            const int newId = tags.AddTag(g_newTagName);
            if (newId < 0) {
                s_addTagFailed = true;
                return false;
            }
            tags.SaveToFile();
            if (g_selNode) {
                const int oldTag = g_selNode->GetTagId();
                g_selNode->SetTag(newId);
                EditorUndo_PushTag(g_selNode, oldTag, newId);
                EngineGUI_MarkSceneDirty();
            }
            s_addTagFailed = false;
            g_showAddTagPopup = false;
            ImGui::CloseCurrentPopup();
            return true;
        };

        if (ImGui::Button("Create", ImVec2(120, 0)) || enterPressed)
            tryAddTag();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            s_addTagFailed = false;
            g_showAddTagPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showDeleteTagPopup)
        ImGui::OpenPopup("DeleteTagPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("DeleteTagPopup", &g_showDeleteTagPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        TagRegistry& tags = TagRegistry::Instance();
        const char* tagName = tags.IsValid(g_pendingDeleteTagId)
            ? tags.GetName(g_pendingDeleteTagId)
            : "(invalid)";
        ImGui::Text("Delete tag \"%s\"?", tagName);
        ImGui::TextWrapped(
            "Nodes using this tag will be set to NONE. "
            "Higher custom tag ids will be remapped.");
        ImGui::Separator();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            if (tags.CanRemove(g_pendingDeleteTagId)) {
                const int deletedId = g_pendingDeleteTagId;
                RemapSceneTagsAfterDelete(deletedId);
                tags.RemoveTag(deletedId);
                tags.SaveToFile();
                EngineGUI_MarkSceneDirty();
            }
            g_pendingDeleteTagId = -1;
            g_showDeleteTagPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_pendingDeleteTagId = -1;
            g_showDeleteTagPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showDefaultScenePopup)
        ImGui::OpenPopup("DefaultScenePopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("DefaultScenePopup", &g_showDefaultScenePopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!SceneLoader_IsSceneAssetsScanned())
            SceneLoader_RefreshSceneAssets();

        ImGui::Text("Default Scene");
        ImGui::TextDisabled("Stored in project.json (defaultScene)");
        ImGui::TextDisabled("Current: %s", SceneLoader_GetDefaultSceneDisplayName().c_str());
        ImGui::Separator();

        const int assetCount = SceneLoader_GetSceneAssetCount();
        if (assetCount == 0) {
            ImGui::TextDisabled("No scenes found");
        }
        else {
            ImGui::BeginChild("##DefaultSceneList", ImVec2(320, 240), true);
            for (int i = 0; i < assetCount; ++i) {
                std::string displayName, fileName;
                SceneLoader_TryGetSceneAsset(i, nullptr, &displayName, &fileName);
                const bool selected = (fileName == SceneLoader_GetDefaultSceneFileName());
                const std::string label = std::string(ICON_FA_MAP) + " " + displayName;
                if (ImGui::Selectable(label.c_str(), selected)) {
                    SceneLoader_SetDefaultSceneFileName(fileName, true);
                    ProjectManager::Instance().SetDefaultScene(fileName);
                    // Persist immediately (same pattern as GravityPopup); otherwise
                    // project.json keeps the old defaultScene and wins on next open.
                    if (ProjectManager::Instance().IsOpen())
                        ProjectManager::Instance().SaveProjectFile(nullptr);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndChild();
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            g_showDefaultScenePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }


    if (g_showGravityPopup)
        ImGui::OpenPopup("GravityPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const bool gravityPopupWasOpen = g_showGravityPopup;
    if (ImGui::BeginPopupModal("GravityPopup", &g_showGravityPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Project Gravity");
        ImGui::TextDisabled("Stored in project.json (physics.gravity)");
        ImGui::Separator();

        float gravity = ProjectManager::Instance().GetPhysicsGravity();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::DragFloat("m/s^2", &gravity, 0.05f, 0.1f, 100.0f, "%.2f"))
            ProjectManager::Instance().SetPhysicsGravity(gravity);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ProjectManager::Instance().SetPhysicsGravity(gravity);
            ProjectManager::Instance().SaveProjectFile(nullptr);
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ProjectManager::Instance().SetPhysicsGravity(gravity);
            ProjectManager::Instance().SaveProjectFile(nullptr);
            g_showGravityPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (gravityPopupWasOpen && !g_showGravityPopup)
        ProjectManager::Instance().SaveProjectFile(nullptr);

    if (g_showAudioPopup)
        ImGui::OpenPopup("AudioPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const bool audioPopupWasOpen = g_showAudioPopup;
    if (ImGui::BeginPopupModal("AudioPopup", &g_showAudioPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Project Audio");
        ImGui::TextDisabled("Stored in project.json (audio.*)");
        ImGui::Separator();

        auto& audio = AudioManager::Instance();
        float master = audio.GetMasterVolume();
        float bgm = audio.GetBGMVolume();
        float sfx = audio.GetSFXVolume();
        float voice = audio.GetVoiceVolume();
        auto saveBuses = [&]() {
            ProjectManager::Instance().SetAudioBuses(
                audio.GetMasterVolume(), audio.GetBGMVolume(),
                audio.GetSFXVolume(), audio.GetVoiceVolume());
            ProjectManager::Instance().SaveProjectFile(nullptr);
        };
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("Master", &master, 0.0f, 1.0f, "%.2f")) {
            audio.SetMasterVolume(master);
            ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("BGM", &bgm, 0.0f, 1.0f, "%.2f")) {
            audio.SetBGMVolume(bgm);
            ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("SFX", &sfx, 0.0f, 1.0f, "%.2f")) {
            audio.SetSFXVolume(sfx);
            ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("Voice", &voice, 0.0f, 1.0f, "%.2f")) {
            audio.SetVoiceVolume(voice);
            ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();

        ImGui::Separator();
        ImGui::Text("UI Click Sound");
        ImGui::TextDisabled("WAV played when a UI button activates");
        char clickBuf[256];
        strncpy_s(clickBuf, ProjectManager::Instance().GetAudioSettings().uiClickSound.c_str(),
            sizeof(clickBuf) - 1);
        clickBuf[sizeof(clickBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::InputText("##UiClickSound", clickBuf, sizeof(clickBuf)))
            ProjectManager::Instance().SetUiClickSound(clickBuf);
        if (ImGui::IsItemDeactivatedAfterEdit())
            ProjectManager::Instance().SaveProjectFile(nullptr);
        {
            const char* wavExts[] = { ".wav" };
            std::string droppedPath;
            if (AcceptAssetPathDrop(wavExts, IM_ARRAYSIZE(wavExts), droppedPath)) {
                ProjectManager::Instance().SetUiClickSound(droppedPath);
                ProjectManager::Instance().SaveProjectFile(nullptr);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ProjectManager::Instance().SetAudioBuses(
                audio.GetMasterVolume(), audio.GetBGMVolume(),
                audio.GetSFXVolume(), audio.GetVoiceVolume());
            ProjectManager::Instance().SaveProjectFile(nullptr);
            g_showAudioPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (audioPopupWasOpen && !g_showAudioPopup)
        ProjectManager::Instance().SaveProjectFile(nullptr);

    if (g_sceneReloadConflict && EngineGUI_IsSceneDirty() && !g_isPlaying && !IsSceneLoadBusy()
        && !ImGui::GetTopMostPopupModal() && !g_showUnsavedChangesPopup)
        ImGui::OpenPopup("SceneJsonChangedPopup");
    if (ImGui::BeginPopupModal("SceneJsonChangedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!g_sceneReloadConflict || g_sceneReloadConflict->path != SceneLoader_GetCurrentScenePath()) {
            g_sceneReloadConflict.reset();
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextUnformatted("The scene JSON changed outside the Editor.");
            ImGui::TextUnformatted("The Editor has unsaved changes.");
            if (ImGui::Button("Load disk version (discard Editor changes)")) {
                ResolveSceneReloadConflict(true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep Editor changes")) {
                ResolveSceneReloadConflict(false);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    if (g_showUnsavedChangesPopup)
        ImGui::OpenPopup("UnsavedChangesPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("UnsavedChangesPopup", &g_showUnsavedChangesPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes.");
        ImGui::TextDisabled("Save before continuing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string leavingSceneFile;
            {
                const std::string& currentScenePath = SceneLoader_GetCurrentScenePath();
                if (!currentScenePath.empty()) {
                    leavingSceneFile = NormalizeSceneFileName(
                        fs::path(currentScenePath).filename().string());
                }
            }
            if (leavingSceneFile.empty())
                leavingSceneFile = NormalizeSceneFileName(SceneLoader_GetSaveFileName());

            if (!leavingSceneFile.empty())
                SaveSceneToFile(leavingSceneFile);
            ExecutePendingEditorAction();
        }
        ImGui::SameLine();

        if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
            EngineGUI_ClearSceneDirty();
            ExecutePendingEditorAction();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_pendingEditorAction = PendingEditorAction::None;
            g_pendingEditorSceneFile.clear();
            g_pendingProjectPath.clear();
            g_pendingCreateProjectParent.clear();
            g_pendingCreateProjectName.clear();
            g_showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (g_showBuildUnsavedPopup)
        ImGui::OpenPopup("BuildUnsavedPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("BuildUnsavedPopup", &g_showBuildUnsavedPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Build %s?", ProjectManager::Instance().GetName().c_str());
        ImGui::Text("You have unsaved scene changes.");
        ImGui::TextDisabled("Save the scene before building a playable package.");
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            SaveScene();
            if (EngineGUI_IsSceneDirty()) {
                MessageBoxA(GetWindowHandle(),
                            "Could not save the scene. Save it from File first, then build.",
                            "Build Current Project", MB_OK | MB_ICONWARNING);
            }
            else {
                g_showBuildUnsavedPopup = false;
                ImGui::CloseCurrentPopup();
                OpenBuildPreflight();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_showBuildUnsavedPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showBuildPreflight)
        ImGui::OpenPopup("BuildPreflightPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("BuildPreflightPopup", &g_showBuildPreflight, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Build %s?", ProjectManager::Instance().GetName().c_str());
        ImGui::TextDisabled("%s", ProjectManager::Instance().GetRootPath().c_str());
        ImGui::Separator();

        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputText("Output folder", g_buildDest, IM_ARRAYSIZE(g_buildDest));
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
            g_pendingFolderPick = PendingFolderPickAction::BuildOutput;

        const char* configs[] = { "Debug", "Release" };
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("Configuration", &g_buildConfigIndex, configs, IM_ARRAYSIZE(configs));
        ImGui::TextDisabled("Play FPS overlay follows Engine Settings > Show FPS When Playing in both configurations.");

        const std::string destReason = PlayablePackage::DestinationRefusalReason(
            ProjectManager::Instance().GetRootPath(), g_buildDest);
        if (!destReason.empty())
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", destReason.c_str());

        ImGui::Separator();
        const bool canStart = destReason.empty() && !PlayablePackage::IsBuildRunning();
        if (!canStart)
            ImGui::BeginDisabled();
        if (ImGui::Button("Build", ImVec2(120, 0))) {
            PlayablePackage::Prefs prefs;
            prefs.destination = g_buildDest;
            prefs.configuration = (g_buildConfigIndex == 1)
                ? PlayablePackage::Configuration::Release
                : PlayablePackage::Configuration::Debug;
            PlayablePackage::SavePrefs(ProjectManager::Instance().GetRootPath(), prefs);

            PlayablePackage::Request request;
            request.contentRoot = ProjectManager::Instance().GetRootPath();
            request.destination = g_buildDest;
            request.gameName = ProjectManager::Instance().GetName();
            request.configuration = prefs.configuration;
            if (PlayablePackage::StartBuild(request)) {
                g_showBuildPreflight = false;
                ImGui::CloseCurrentPopup();
                g_showBuildLog = true;
            }
        }
        if (!canStart)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_showBuildPreflight = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showBuildLog)
        ImGui::OpenPopup("BuildLogPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    {
        const bool buildDone = PlayablePackage::IsBuildFinished();
        ImGuiWindowFlags logFlags = ImGuiWindowFlags_AlwaysAutoResize;
        bool* logOpen = buildDone ? &g_showBuildLog : nullptr;
        if (ImGui::BeginPopupModal("BuildLogPopup", logOpen, logFlags)) {
            if (!buildDone) {
                const std::string buildHeader =
                    "Building " + ProjectManager::Instance().GetName() + " ? Ivy Engine";
                ImGui::SeparatorText(buildHeader.c_str());
            }
            else if (PlayablePackage::LastBuildSucceeded())
                ImGui::Text("Build succeeded.");
            else
                ImGui::Text("Build failed.");

            std::string log;
            PlayablePackage::CopyLog(&log);
            ImGui::BeginChild("##BuildLogText", ImVec2(640, 280), true);
            ImGui::TextUnformatted(log.empty() ? "(no output yet)" : log.c_str());
            if (!buildDone)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (buildDone) {
                const std::string result = PlayablePackage::LastBuildResultMessage();
                if (!result.empty())
                    ImGui::TextWrapped("%s", result.c_str());
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    g_showBuildLog = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            else {
                ImGui::TextDisabled("This window stays open until MSBuild finishes.");
            }
            ImGui::EndPopup();
        }
    }
}

static void SaveSceneToFile(const std::string& sceneFile) {
    if (sceneFile.empty()) return;

    const std::string normalized = NormalizeSceneFileName(sceneFile);
    if (normalized.empty()) return;

    if (!fs::exists(SCENE_DIR)) fs::create_directories(SCENE_DIR);
    const fs::path scenePath = fs::path(SCENE_DIR) / normalized;
    if (scenePath.has_parent_path())
        fs::create_directories(scenePath.parent_path());

    if (EngineGUI_SaveToFile(scenePath.string().c_str())) {
        SceneLoader_CopySceneFileName(normalized);
        SceneLoader_RefreshSceneAssets();
        EngineGUI_ClearSceneDirty();
    }
}

static void SaveScene() {
    const std::string sceneFile = NormalizeSceneFileName(SceneLoader_GetSaveFileName());
    if (sceneFile.empty()) return;

    SaveSceneToFile(sceneFile);
}

static void NewScene() {
    const std::string sceneFile = NormalizeSceneFileName(g_newSceneName);
    if (sceneFile.empty()) return;

    SceneLoader_ClearSceneNodes();
    g_selNode = nullptr;
    g_selUiElement = nullptr;
    EditorUndo_Clear();

    SceneLoader_CopySceneFileName(sceneFile);
    SaveScene();
}

void TeardownPlaySession(bool resetEditorState) {
    g_isPlaying = false;
    g_playMode = PlayMode::None;
    g_isOnStart = true;
    ThirdPersonCamera_ShowCursor(true);
    ThirdPersonCamera_SetTarget(nullptr, nullptr);
    FirstPersonCamera_SetTarget(nullptr, nullptr);
    TeardownScenePlaySession(resetEditorState);
}

// Stop play simulation for an in-game scene switch without restoring editor
// chrome. Keep the current world until Flush replaces it (runtime loads
// atomically ? clearing early caused a clear-color gap under the transition).
void SoftStopPlayForSceneLoad() {
    if (!g_isPlaying && g_holdPlayVisuals)
        return;
    g_pendingPlayMode = (g_playMode != PlayMode::None) ? g_playMode : PlayMode::Viewport;
    g_isPlaying = false;
    g_playMode = g_pendingPlayMode;
    g_isOnStart = true;
    g_holdPlayVisuals = true;
    g_enabled = (g_playMode == PlayMode::Viewport);
    DebugCamera_SetEnabled(false);
    DebugSystem::ResetTimeScale();
}

void ReturnToStart() {
    g_playMode = PlayMode::None;
    g_holdPlayVisuals = false;
    TeardownPlaySession(true);
    DebugSystem::ResetTimeScale();
    g_enabled = true;
    g_editMode = EditorEditMode::Scene3D;
    DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
    DebugCamera_SetEnabled(true);
    DebugCamera_RestoreEditorView();
    ApplyEditorColliders();
    RestoreEditorDebugColliderFlags();
    SyncImGuiInputAcceptance(true);
}

static bool HasThirdPersonPlayerNode(GameObject* node) {
    if (!node) return false;
    if (node->GetTag() == Tag::TAG_PLAYER)
        return true;
    for (const auto& child : node->GetChildren()) {
        if (HasThirdPersonPlayerNode(child.get()))
            return true;
    }
    return false;
}

static bool HasCameraComponentNode(GameObject* node) {
    if (!node) return false;
    if (node->GetComponent<CameraComponent>())
        return true;
    for (const auto& child : node->GetChildren()) {
        if (HasCameraComponentNode(child.get()))
            return true;
    }
    return false;
}

static bool HasCameraComponentInScene() {
    for (const auto& node : g_sceneNodes) {
        if (HasCameraComponentNode(node.get()))
            return true;
    }
    return false;
}

static bool HasThirdPersonPlayer() {
    for (const auto& node : g_sceneNodes) {
        if (HasThirdPersonPlayerNode(node.get()))
            return true;
    }
    return false;
}

static bool ShouldUseDebugCameraDuringPlay() {
    if (SceneLoader_DebugCameraEnabledOnPlay())
        return true;
    return false; // Editor navigation must not silently become the game camera.
}

bool EngineGUI_IsDebugCameraPlayInputActive() {
    return g_isPlaying && DebugCamera_IsEnabled();
}

static void StartPlay(PlayMode mode = PlayMode::Viewport) {
    if (g_sceneReloadConflict) return;
    if (g_sceneAssetPreloader.IsActive() || g_sceneLoadPending) {
        g_playWhenPreloadDone = true;
        g_pendingPlayMode = mode;
        return;
    }

    DebugCamera_SaveEditorView();
    SnapshotSceneStartTransforms();
    SyncSceneRuntime();
    // Instant scene switches (cached preload) can StartPlay in the same frame as
    // Load. Without prior SavePositions frames, m_previousPosition stays at
    // (0,0,0) and the post-script swept resolve teleports dynamics from origin.
    PhysicsSystem_SavePositions();
    // Create runtime spawn root before the play update loop so the first
    // PrefabInstantiate does not push_back into g_sceneNodes mid-iteration.
    PrefabRuntime_EnsureSpawnRoot();
    g_isOnStart = false;
    g_isPlaying = true;
    g_playMode = mode;
    g_pendingPlayMode = mode;
    g_holdPlayVisuals = false;
    const bool isViewportPlay = (mode == PlayMode::Viewport);
    g_enabled = isViewportPlay;
    if (ShouldUseDebugCameraDuringPlay()) {
        if (SceneLoader_DebugCameraEnabledOnPlay() && SceneLoader_DebugCameraFlyOnPlay())
            DebugCamera_SetControlMode(DebugCameraControlMode::Fly);
        else
            DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
        DebugCamera_SetEnabled(true);
        ThirdPersonCamera_ShowCursor(true);
    } else {
        DebugCamera_SetEnabled(false);
        const bool hideCursor =
            ((g_cameraMode == CameraMode::ThirdPerson) || (g_cameraMode == CameraMode::FirstPerson))
            && HasThirdPersonPlayer();
        ThirdPersonCamera_ShowCursor(!hideCursor);
    }
    SceneCameraBeginPlay();
    SyncImGuiInputAcceptance(isViewportPlay);
}

void EngineGUI_QueueSceneLoad(const char* filename) {
    if (g_sceneLoadPending || g_sceneAssetPreloader.IsActive()) return;
    g_loadingPresented = false; g_loadingDrawn = false; g_sceneReadyToApply = false;
    g_scenePreparation.reset(); g_sceneLoadError.clear();
    g_loadingStage = "Reading scene...";
    g_loadStarted = g_stageStarted = std::chrono::steady_clock::now();
    g_longestPreparationTick = 0.0;
    LogSceneLoading("queued");

    // Amendment 1: each new scene switch resets deferred play so a stale F3
    // cannot auto-play into the wrong scene.
    g_playWhenPreloadDone = false;
    g_sceneLoadPending = true;
    g_pendingSceneHotLoad.reset();
    g_sceneReloadConflict.reset();
    g_pendingSceneLoadPath = filename ? filename : "";
}

void EngineGUI_BeginSceneAssetPreload() {
    const auto tasks = SceneAssetPreload_BuildTaskList(SceneLoader_GetSceneNodes());
    g_sceneAssetPreloader.Begin(tasks);
}

void EngineGUI_FlushSceneLoadPending() {
    if (!g_sceneLoadPending)
        return;

    if (g_pendingSceneLoadPath.empty()) {
        g_sceneLoadPending = false;
        return;
    }

    if (!g_loadingPresented) return;
    const std::string path = g_pendingSceneLoadPath;
    if (!g_scenePreparation) {
        g_preparationHotLoad = g_pendingSceneHotLoad.has_value();
        if (g_preparationHotLoad && path != SceneLoader_GetCurrentScenePath()) {
            ModelAssetCache::Instance().EndContentReload(false);
            g_pendingSceneHotLoad.reset(); g_sceneLoadPending = false; g_playWhenPreloadDone = false;
            return;
        }
        g_stageStarted = std::chrono::steady_clock::now();
        LogSceneLoading("read_begin");
        g_scenePreparation = SceneLoader_BeginPreparation(path,
            g_preparationHotLoad ? &g_pendingSceneHotLoad->content : nullptr);
        g_pendingSceneHotLoad.reset();
        return;
    }
    if (!g_sceneReadyToApply) {
        const auto start = std::chrono::steady_clock::now();
        const bool wasReading = SceneLoader_PreparationReading(*g_scenePreparation);
        const bool done = SceneLoader_TickPreparation(*g_scenePreparation, 4.0);
        g_longestPreparationTick = (std::max)(g_longestPreparationTick,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count());
        if (wasReading && !SceneLoader_PreparationReading(*g_scenePreparation)) {
            LogSceneLoading("read_complete"); g_stageStarted = std::chrono::steady_clock::now();
        }
        if (!done) {
            g_loadingStage = SceneLoader_PreparationReading(*g_scenePreparation) ? "Reading / parsing scene..." : "Creating objects...";
            return;
        }
        if (!SceneLoader_PreparationResult(*g_scenePreparation)) {
            const auto error = SceneLoader_PreparationError(*g_scenePreparation);
            // Hot-load reports through the existing status light without blocking later disk fixes.
            g_sceneLoadError = g_preparationHotLoad ? std::string() : error;
            g_sceneReloadLight.Failure();
            g_sceneReloadStatus = "Scene reload failed: " + error;
            LogSceneLoading("failed");
            g_scenePreparation.reset();
            ModelAssetCache::Instance().EndContentReload(false);
            g_pendingSceneLoadPath.clear();
            g_sceneLoadPending = false; g_playWhenPreloadDone = false;
            if (g_holdPlayVisuals) ReturnToStart();
            return;
        }
        LogSceneLoading("objects_complete");
        g_loadingStage = "Applying scene...";
        g_sceneReadyToApply = true; g_loadingPresented = false;
        return;
    }
    const bool hotLoad = g_preparationHotLoad;
    if (hotLoad && g_isPlaying) {
        g_contentPlayMode = g_playMode;
        SoftStopPlayForSceneLoad();
        g_playWhenPreloadDone = true;
        g_pendingPlayMode = g_contentPlayMode;
    }
    auto prepared = SceneLoader_PreparationResult(*g_scenePreparation);
    g_stageStarted = std::chrono::steady_clock::now();
    LogSceneLoading("apply_begin");
    // Preparation must succeed before any old scripts/runtime are torn down.
    SceneLoader_ApplyPrepared(std::move(prepared), hotLoad);
    ModelAssetCache::Instance().EndContentReload(true);
    LogSceneLoading("apply_complete");
    if (hotLoad) g_sceneReloadLight.Success();
    else g_sceneReloadLight = {};
    g_sceneReloadStatus = hotLoad ? "Scene JSON reloaded" : "Watching current scene JSON";

    g_selNode = nullptr;
    g_selUiElement = nullptr;
    if (!hotLoad) EngineGUI_SetEditMode(EditorEditMode::Scene3D);
    EngineGUI_ClearSceneDirty();
    EditorUndo_Clear();

    if (ProjectManager::Instance().IsOpen()) {
        const std::filesystem::path filePath(path);
        ProjectManager::Instance().SetLastOpenedScene(filePath.filename().string());
    }

    g_loadingStage = "Preloading assets...";
    g_stageStarted = std::chrono::steady_clock::now();
    EngineGUI_BeginSceneAssetPreload();
    LogSceneLoading("preload_begin");
    g_scenePreparation.reset(); g_pendingSceneLoadPath.clear();
    g_sceneLoadPending = false;
    if (!g_sceneAssetPreloader.IsActive()) LogSceneLoading("complete");

    // Empty preload never becomes Active, so Tick never resumes play.
    // Match runtime: stay in / re-enter play immediately when deferred.
    if (g_playWhenPreloadDone && !g_sceneAssetPreloader.IsActive()) {
        g_playWhenPreloadDone = false;
        StartPlay(g_pendingPlayMode);
    }
}

void EngineGUI_TickSceneAssetPreload() {
    if (!g_sceneAssetPreloader.IsActive())
        return;

    if (!g_sceneAssetPreloader.Tick())
        return;

    LogSceneLoading("complete");
    if (g_playWhenPreloadDone) {
        g_playWhenPreloadDone = false;
        StartPlay(g_pendingPlayMode);
    }
}

bool EngineGUI_IsSceneAssetPreloadActive() {
    return g_sceneAssetPreloader.IsActive() || g_sceneLoadPending;
}

static bool IsSceneLoadBusy() {
    return g_sceneAssetPreloader.IsActive() || g_sceneLoadPending;
}

// --- Public API ---

void EngineGUI_Initialize(HWND hwnd, ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    ProjectManager::Instance().BootstrapAtStartup();

    ModelAssetCache::Instance().RefreshAssetDirectory();
    SceneLoader_Initialize(true);
    if (ProjectManager::Instance().IsOpen()) {
        SceneLoader_LoadScriptDlls();
        g_scriptReloadStatus = "Watching script DLLs";
        g_scriptReloadLight = {};
        g_scriptFeedbackRevision = ScriptFactory::Instance().GetLastReloadFeedback().revision;
    }
    else {
        g_scriptReloadStatus = "No project open";
    }

    if (ProjectManager::Instance().IsOpen() &&
        !ProjectManager::Instance().GetData().defaultScene.empty()) {
        SceneLoader_SetDefaultSceneFileName(
            ProjectManager::Instance().GetData().defaultScene, false);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    {
        ImFontConfig iconsConfig;
        iconsConfig.MergeMode = true;
        iconsConfig.GlyphMinAdvanceX = 13.0f;
        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        const fs::path iconFile = content_path::ReadableFileCurrent("rom/font/fa-solid-900.ttf");
        const std::string iconPath = iconFile.empty()
            ? std::string("rom/font/fa-solid-900.ttf")
            : iconFile.string();
        if (!io.Fonts->AddFontFromFileTTF(
                iconPath.c_str(), 13.0f, &iconsConfig, iconRanges)) {
            std::fprintf(stderr,
                "Warning: failed to load icon font rom/font/fa-solid-900.ttf\n");
        }
    }

    ImGui_ImplWin32_EnableDpiAwareness();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);
    ImGui::StyleColorsDark();
    g_imguiInitialized = true;

    LoadUiPrefs();
    if (ProjectManager::Instance().IsOpen()) {
        const int zoom = ProjectManager::Instance().GetEditorUiZoomPercent();
        if (IsValidUiZoomPercent(zoom))
            ApplyUiZoom(zoom);
        else
            ApplyUiZoom(g_uiZoomPercent);
    }
    else {
        ApplyUiZoom(g_uiZoomPercent);
    }

    ProjectManager::Instance().UpdateWindowTitle(hwnd);
    DebugCamera_SetEnabled(g_enabled);
}

void EngineGUI_Finalize() {
    // Joining the JSON-only worker precedes scene/runtime teardown.
    g_scenePreparation.reset();
    g_sceneLoadPending = false;

    SceneLoader_StopScriptWatcher();

    TeardownPlaySession(false);

    g_selNode = nullptr;
    g_selUiElement = nullptr;
    g_sceneNodes.clear();
    g_activeCameraNode.clear();
    g_enabled = true;

    SceneLoader_UnloadScriptDlls();

    if (g_imguiInitialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiInitialized = false;
    }
}

bool EngineGUI_IsPlaying() { return g_isPlaying; }
PlayMode EngineGUI_GetPlayMode() { return g_playMode; }
bool EngineGUI_PreferPlayVisuals() { return g_isPlaying || g_holdPlayVisuals; }
bool EngineGUI_IsOnStart() { return g_isOnStart; }
void EngineGUI_StartPlay(PlayMode mode) {
    if (mode == PlayMode::None)
        mode = g_holdPlayVisuals ? g_pendingPlayMode : PlayMode::Viewport;
    StartPlay(mode);
}
void EngineGUI_StopPlay() { ReturnToStart(); }
void EngineGUI_RequestExitGame() { g_exitGameRequested.store(true); }
void EngineGUI_RequestBridgeHostQuit() {
    // Bridge-initiated process exit only. Do not use RequestExitGame — that
    // path ReturnToStart()s in the Editor and never destroys the HWND.
    if (g_isPlaying)
        ReturnToStart();
    if (HWND hwnd = GetWindowHandle())
        PostMessageW(hwnd, WM_APP + 48, 0, 0);
}
CameraMode EngineGUI_GetCameraMode() { return g_cameraMode; }
void EngineGUI_SetCameraMode(CameraMode m) { g_cameraMode = m; }
const std::string& EngineGUI_GetActiveCameraNodeName() { return g_activeCameraNode; }
void EngineGUI_SetActiveCameraNodeName(const std::string& n) { g_activeCameraNode = n; }
void EngineGUI_SetEnabled(bool e) { g_enabled = e; DebugCamera_SetEnabled(e); }
bool EngineGUI_IsEnabled() { return g_enabled; }
const std::string& EngineGUI_GetDefaultScenePath() { return SceneLoader_GetDefaultScenePath(); }

std::string EngineGUI_ResolveStartupScenePath() {
    return ProjectManager::Instance().ResolveSceneToLoad();
}

bool EngineGUI_IsUiOverlayVisible() { return g_showUiOverlay; }
void EngineGUI_SetUiOverlayVisible(bool visible) { g_showUiOverlay = visible; }

EditorEditMode EngineGUI_GetEditMode() { return g_editMode; }
void EngineGUI_SetEditMode(EditorEditMode mode) {
    g_editMode = mode;
    if (mode == EditorEditMode::UI)
        g_showUiOverlay = true;
}
bool EngineGUI_IsUiEditMode() { return g_editMode == EditorEditMode::UI; }

void EngineGUI_GetGameViewportRect(float& x, float& y, float& width, float& height) {
    const float screenW = GetBackBufferWidth();
    const float screenH = GetBackBufferHeight();

    if (g_playMode == PlayMode::Fullscreen || !g_enabled) {
        x = 0.0f;
        y = 0.0f;
        width = screenW;
        height = screenH;
        return;
    }

    const EditorLayoutMetrics layout = ComputeEditorLayout(screenW, screenH);
    x = layout.viewportX;
    y = layout.viewportY;
    width = layout.viewportW;
    height = layout.viewportH;
}

bool EngineGUI_MouseToViewportLocal(float& x, float& y) {
    int mx = 0;
    int my = 0;
    Mouse_GetPosition(mx, my);

    float vx = 0.0f, vy = 0.0f, vw = 0.0f, vh = 0.0f;
    EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
    x = static_cast<float>(mx) - vx;
    y = static_cast<float>(my) - vy;
    return x >= 0.0f && y >= 0.0f && x <= vw && y <= vh;
}

// --- Ray helpers ---

static float RaySphere(DirectX::XMVECTOR rayOrg, DirectX::XMVECTOR rayDir,
    const DirectX::XMFLOAT3& center, float radius)
{
    using namespace DirectX;
    XMVECTOR C = XMLoadFloat3(&center);
    XMVECTOR oc = XMVectorSubtract(rayOrg, C);
    float b = XMVectorGetX(XMVector3Dot(oc, rayDir));
    float c = XMVectorGetX(XMVector3LengthSq(oc)) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0f) return -1.0f;
    float sq = sqrtf(disc);
    float t = -b - sq;
    if (t >= 0.0f) return t;
    t = -b + sq;
    return (t >= 0.0f) ? t : -1.0f;
}

// Moller?Trumbore ray-triangle test (local/world space; t is along rayDir).
static bool RayIntersectsTriangle(const DirectX::XMFLOAT3& rayOrigin,
    const DirectX::XMFLOAT3& rayDir,
    const DirectX::XMFLOAT3& v0,
    const DirectX::XMFLOAT3& v1,
    const DirectX::XMFLOAT3& v2,
    float& t)
{
    using namespace DirectX;
    const float EPSILON = 0.0000001f;

    XMVECTOR vertex0 = XMLoadFloat3(&v0);
    XMVECTOR vertex1 = XMLoadFloat3(&v1);
    XMVECTOR vertex2 = XMLoadFloat3(&v2);
    XMVECTOR origin = XMLoadFloat3(&rayOrigin);
    XMVECTOR direction = XMLoadFloat3(&rayDir);

    XMVECTOR edge1 = vertex1 - vertex0;
    XMVECTOR edge2 = vertex2 - vertex0;
    XMVECTOR h = XMVector3Cross(direction, edge2);
    float a = XMVectorGetX(XMVector3Dot(edge1, h));

    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    XMVECTOR s = origin - vertex0;
    float u = f * XMVectorGetX(XMVector3Dot(s, h));
    if (u < 0.0f || u > 1.0f)
        return false;

    XMVECTOR q = XMVector3Cross(s, edge1);
    float v = f * XMVectorGetX(XMVector3Dot(direction, q));
    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * XMVectorGetX(XMVector3Dot(edge2, q));
    return t > EPSILON;
}

static float RayPickModelMesh(const std::vector<Model3D::VertexPNTC>& verts,
    DirectX::XMVECTOR rayOrg,
    DirectX::XMVECTOR rayDir,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& scale,
    const DirectX::XMFLOAT3& rotation)
{
    using namespace DirectX;
    if (verts.empty()) return -1.0f;

    XMMATRIX world =
        XMMatrixScaling(scale.x, scale.y, scale.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotation.x),
            XMConvertToRadians(rotation.y),
            XMConvertToRadians(rotation.z)) *
        XMMatrixTranslation(position.x, position.y, position.z);

    XMFLOAT3 rayOrigin, rayDirection;
    XMStoreFloat3(&rayOrigin, rayOrg);
    XMStoreFloat3(&rayDirection, rayDir);

    float bestT = FLT_MAX;
    bool hit = false;
    for (size_t i = 0; i + 2 < verts.size(); i += 3) {
        XMFLOAT3 wv0, wv1, wv2;
        XMStoreFloat3(&wv0, XMVector3Transform(XMLoadFloat3(&verts[i].Position), world));
        XMStoreFloat3(&wv1, XMVector3Transform(XMLoadFloat3(&verts[i + 1].Position), world));
        XMStoreFloat3(&wv2, XMVector3Transform(XMLoadFloat3(&verts[i + 2].Position), world));

        float t = 0.0f;
        if (RayIntersectsTriangle(rayOrigin, rayDirection, wv0, wv1, wv2, t) &&
            t >= 0.0f && t < bestT) {
            bestT = t;
            hit = true;
        }
    }
    return hit ? bestT : -1.0f;
}

static bool BuildPickRay(int mx, int my,
    DirectX::XMVECTOR& rayOrg, DirectX::XMVECTOR& rayDir)
{
    using namespace DirectX;
    float vx, vy, vw, vh;
    EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
    if ((float)mx < vx || (float)mx > vx + vw || (float)my < vy || (float)my > vy + vh)
        return false;

    Camera& cam = GetCamera();
    XMMATRIX vp = cam.GetViewMatrix() * cam.GetProjMatrix(vw, vh);
    XMMATRIX ivp = XMMatrixInverse(nullptr, vp);
    float clipX = 2.0f * ((float)mx - vx) / vw - 1.0f;
    float clipY = -2.0f * ((float)my - vy) / vh + 1.0f;
    XMVECTOR near4 = XMVector4Transform(XMVectorSet(clipX, clipY, 0.0f, 1.0f), ivp);
    XMVECTOR far4 = XMVector4Transform(XMVectorSet(clipX, clipY, 1.0f, 1.0f), ivp);
    near4 = XMVectorDivide(near4, XMVectorSplatW(near4));
    far4 = XMVectorDivide(far4, XMVectorSplatW(far4));
    rayOrg = near4;
    rayDir = XMVector3Normalize(XMVectorSubtract(far4, near4));
    return true;
}

// --- Wireframe highlight ---

static void DrawMeshWireframeHighlight(
    const std::vector<Model3D::VertexPNTC>& verts,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& scale,
    const DirectX::XMFLOAT3& rotation,
    DebugDrawer& drawer,
    const DirectX::XMFLOAT4& color)
{
    using namespace DirectX;
    if (verts.empty()) return;
    XMMATRIX world =
        XMMatrixScaling(scale.x, scale.y, scale.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotation.x),
            XMConvertToRadians(rotation.y),
            XMConvertToRadians(rotation.z)) *
        XMMatrixTranslation(position.x, position.y, position.z);

    std::vector<XMFLOAT3> lineVerts;
    lineVerts.reserve((verts.size() / 3) * 6);
    for (size_t i = 0; i + 2 < verts.size(); i += 3) {
        XMFLOAT3 tv0, tv1, tv2;
        XMStoreFloat3(&tv0, XMVector3Transform(XMLoadFloat3(&verts[i].Position), world));
        XMStoreFloat3(&tv1, XMVector3Transform(XMLoadFloat3(&verts[i + 1].Position), world));
        XMStoreFloat3(&tv2, XMVector3Transform(XMLoadFloat3(&verts[i + 2].Position), world));
        lineVerts.push_back(tv0); lineVerts.push_back(tv1);
        lineVerts.push_back(tv1); lineVerts.push_back(tv2);
        lineVerts.push_back(tv2); lineVerts.push_back(tv0);
    }
    drawer.DrawLinesCached(lineVerts, color);
}

static GameObject* GetSelectionPoseSource(GameObject* node) {
    if (!node) return nullptr;
    if (!g_isPlaying) return node;

    if (auto* sc = node->GetComponent<ScriptComponent>()) {
        if (Script* script = sc->GetScript()) {
            if (GameObject* proxy = script->GetRuntimePhysicsProxy(*node))
                return proxy;
        }
    }
    return node;
}

static void DrawSelectionColliderHighlight(GameObject* node, DebugDrawer& drawer) {
    using namespace DirectX;
    if (!node) return;

    auto* cc = FindColliderComponent(node);
    if (!cc || cc->colliderType == ColliderType::NONE) return;

    static const XMFLOAT4 kColliderColor = { 0.0f, 1.0f, 1.0f, 1.0f };

    GameObject* poseSource = GetSelectionPoseSource(node);
    if (!poseSource) return;

    poseSource->SyncLegacyTransformFromComponents();

    XMFLOAT3 pos, rot, scale;
    poseSource->GetWorldPose(pos, rot, scale);

    auto absAxis = [](float v) {
        if (!std::isfinite(v))
            return 1.0f;
        return v < 0.0f ? -v : v;
    };
    const float sx = absAxis(scale.x);
    const float sy = absAxis(scale.y);
    const float sz = absAxis(scale.z);
    const float sMax = (std::max)({ sx, sy, sz });
    const float sHoriz = sx > sz ? sx : sz;
    const XMFLOAT3 center = {
        cc->colliderCenter.x * scale.x,
        cc->colliderCenter.y * scale.y,
        cc->colliderCenter.z * scale.z
    };

    // Same scale rules as the runtime collider: box per axis, sphere by the
    // largest axis, capsule and cylinder radius by XZ and height by Y.
    switch (cc->colliderType) {
    case ColliderType::BOX:
        drawer.DrawBox(pos, center,
            { cc->halfExtents.x * 2.0f * sx, cc->halfExtents.y * 2.0f * sy, cc->halfExtents.z * 2.0f * sz },
            rot, kColliderColor);
        break;
    case ColliderType::SPHERE:
        drawer.DrawSphere(pos, center, cc->radius * sMax, rot, kColliderColor);
        break;
    case ColliderType::CAPSULE:
        drawer.DrawCapsule(pos, center, cc->radius * sHoriz, cc->height * sy,
            rot, kColliderColor);
        break;
    case ColliderType::CYLINDER:
        drawer.DrawCylinder(pos, center, cc->radius * sHoriz, cc->height * sy,
            rot, kColliderColor);
        break;
    case ColliderType::MESH:
        if (auto* vc = node->GetComponent<VisualComponent>()) {
            if (Model3D* model = vc->GetModel()) {
                if (!model->GetVerticesCPU().empty()) {
                    DrawMeshWireframeHighlight(model->GetVerticesCPU(),
                        pos, scale, rot, drawer, kColliderColor);
                }
            }
        }
        break;
    default:
        break;
    }

    // DebugDrawer primitives enable depth; keep selection overlay on top.
    SetDepthEnable(false);
}

static bool NodeHasLightComponent(GameObject* node) {
    return node->GetComponent<DirectionalLightComponent>()
        || node->GetComponent<PointLightComponent>()
        || node->GetComponent<SpotLightComponent>();
}

static bool NodeHasCameraComponent(GameObject* node) {
    return node->GetComponent<CameraComponent>() != nullptr;
}

static void DrawSelectionHighlight() {
    using namespace DirectX;

    static DebugDrawer drawer;
    if (!drawer.Initialize()) return;
    static const XMFLOAT4 kMeshColor = { 1.0f, 1.0f, 0.0f, 1.0f };

    SetDepthEnable(false);

    if (!EngineGUI_PreferPlayVisuals()) {
        LightGizmo::DrawSceneLights(g_sceneNodes, drawer);
        CameraGizmo::DrawSceneCameras(g_sceneNodes, drawer);
    }

    if (g_selNode && !EngineGUI_PreferPlayVisuals()) {
        if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
            if (!NodeHasLightComponent(g_selNode) && !NodeHasCameraComponent(g_selNode)) {
                Model3D* model = nullptr;
                if (auto* vc = g_selNode->GetComponent<VisualComponent>())
                    model = vc->GetModel();
                if (auto* sprite=g_selNode->GetComponent<SpriteRendererComponent>()) {
                    const auto size=sprite->GetWorldSize();
                    XMFLOAT3 corners[4];
                    const XMFLOAT3 local[4]={
                        {-sprite->pivot.x*size.x,-sprite->pivot.y*size.y,0},
                        {(1-sprite->pivot.x)*size.x,-sprite->pivot.y*size.y,0},
                        {(1-sprite->pivot.x)*size.x,(1-sprite->pivot.y)*size.y,0},
                        {-sprite->pivot.x*size.x,(1-sprite->pivot.y)*size.y,0}};
                    for(int i=0;i<4;++i)
                        XMStoreFloat3(&corners[i],XMVector3TransformCoord(XMLoadFloat3(&local[i]),t->GetWorldMatrix()));
                    for(int i=0;i<4;++i) drawer.DrawLine(corners[i],corners[(i+1)%4],kMeshColor);
                }
                else if (model && !model->GetVerticesCPU().empty()) {
                    DrawMeshWireframeHighlight(model->GetVerticesCPU(),
                        t->localPosition, t->localScale, t->localRotation, drawer, kMeshColor);
                }
                else {
                    drawer.DrawBox(t->localPosition, { 0.0f, 0.0f, 0.0f },
                        t->localScale, t->localRotation, kMeshColor);
                }
            }
        }
        DrawSelectionColliderHighlight(g_selNode, drawer);
    }

    SetDepthEnable(true);
}

void EngineGUI_DrawSelectionIndicator() {
    DrawSelectionHighlight();
}

static void FocusSelectedObjectWithCamera(const engine::Transform* t) {
    if (!t) return;

    Camera& cam = GetCamera();
    using namespace DirectX;
    if(DebugCamera_Is2DView()) {
        const auto position=t->GetWorldPosition();
        float height=2;
        if(g_selNode) if(auto* sprite=g_selNode->GetComponent<SpriteRendererComponent>()) {
            const auto size=sprite->GetWorldSize();
            XMFLOAT3 wp,wr,ws; t->GetWorldPositionRotationScale(wp,wr,ws);
            float vx,vy,vw,vh; EngineGUI_GetGameViewportRect(vx,vy,vw,vh);
            const float aspect=(std::max)(0.01f,vw/(std::max)(1.0f,vh));
            float radiusX=0,radiusY=0;
            for(int i=0;i<4;++i) {
                const auto local=XMVectorSet(((i&1)?1-sprite->pivot.x:-sprite->pivot.x)*size.x,
                    ((i&2)?1-sprite->pivot.y:-sprite->pivot.y)*size.y,0,1);
                XMFLOAT3 corner; XMStoreFloat3(&corner,XMVector3TransformCoord(local,t->GetWorldMatrix()));
                radiusX=(std::max)(radiusX,std::fabs(corner.x-position.x));
                radiusY=(std::max)(radiusY,std::fabs(corner.y-position.y));
            }
            height=(std::max)(2.0f,(std::max)(radiusY,radiusX/aspect)*3.0f);
        }
        cam.SetPosition({position.x,position.y,position.z-10});
        cam.SetTarget(position); cam.orthographicHeight=height;
        DebugCamera_SyncOrbitFromCurrentView(); return;
    }

    const XMVECTOR vPos = XMLoadFloat3(&cam.position);
    const XMVECTOR vAt = XMLoadFloat3(&cam.at);
    XMVECTOR vDir = XMVectorSubtract(vAt, vPos);

    const float scaleX = fabsf(t->localScale.x);
    const float scaleY = fabsf(t->localScale.y);
    const float scaleZ = fabsf(t->localScale.z);
    const float maxScale = (std::max)(scaleX, (std::max)(scaleY, scaleZ));
    const float approxRadius = (maxScale > 0.01f) ? (maxScale * 0.75f) : 0.75f;
    const float halfFovRad = XMConvertToRadians(cam.fov * 0.5f);
    float focusDistance = approxRadius / (tanf(halfFovRad) + 1e-4f);
    focusDistance = (std::max)(focusDistance * 1.25f, 2.0f);
    focusDistance = (std::min)(focusDistance, 60.0f);

    if (XMVectorGetX(XMVector3Length(vDir)) < 0.1f) {
        vDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    } else {
        vDir = XMVector3Normalize(vDir);
    }

    const XMVECTOR vTarget = XMLoadFloat3(&t->localPosition);
    const XMVECTOR vNewPos = XMVectorSubtract(vTarget, XMVectorScale(vDir, focusDistance));

    XMFLOAT3 newPos;
    XMStoreFloat3(&newPos, vNewPos);
    cam.SetPosition(newPos);
    cam.SetTarget(t->localPosition);
    DebugCamera_SyncOrbitFromCurrentView();
}

// --- EngineGUI_Update ---

void EngineGUI_Update(double elapsed_time) {
    if (g_exitGameRequested.exchange(false)) { ReturnToStart(); return; }
#ifdef _DEBUG
    SceneHotLoadCheck_Tick(elapsed_time);
#endif

    // Advance only after the loading stage has actually been presented.
    if (g_sceneLoadPending)
        EngineGUI_FlushSceneLoadPending();

    if (!IsSceneLoadBusy()) {
        SceneLoader_ScanScriptDllsForChanges(false);
        const bool reloadDuringPlay = g_isPlaying && ScriptFactory::Instance().PendingReloadCount() != 0;
        bool dllChanged = false;
        SceneLoader_DrainScriptReloadQueue(
            [&](const std::string& name) {
                dllChanged = true;
                if (reloadDuringPlay) return;
                int reloadedCount = 0;
                std::function<void(GameObject*)> visit = [&](GameObject* node) {
                    if (!node) return;
                    if (auto* sc = node->GetComponent<ScriptComponent>()) {
                        if (sc->GetScriptClass() == name) {
                            sc->HotReload();
                            ++reloadedCount;
                        }
                    }
                    for (const auto& child : node->GetChildren())
                        visit(child.get());
                    };
                for (const auto& root : g_sceneNodes)
                    visit(root.get());

                if (UICanvas* canvas = SceneCanvas_GetCanvas()) {
                    for (const auto& element : canvas->GetElements()) {
                        if (element && element->GetScriptClass() == name) {
                            element->HotReloadAttachedScript();
                            ++reloadedCount;
                        }
                    }
                }

                g_scriptReloadStatus = "Reloaded script: " + name
                    + " on " + std::to_string(reloadedCount) + " component(s)";
            });
        if (dllChanged && reloadDuringPlay) g_contentSceneReload = true;
        const auto& feedback = ScriptFactory::Instance().GetLastReloadFeedback();
        if (feedback.revision != g_scriptFeedbackRevision) {
            g_scriptFeedbackRevision = feedback.revision;
            if (feedback.failed) {
                g_scriptReloadLight.Failure();
                g_scriptReloadStatus = "Script reload failed: " + feedback.error;
            } else g_scriptReloadLight.Success();
        }
    }

    if (ProjectManager::Instance().IsOpen()) {
        const auto& projectRoot = ProjectManager::Instance().GetRootPath();
        if (g_contentProjectRoot != projectRoot) {
            g_contentChanges.clear(); g_contentSceneReload = false;
            ModelAssetCache::Instance().EndContentReload(false);
            g_contentProjectRoot = projectRoot;
            g_contentReloadLight = {}; g_contentReloadStatus = "Watching project content";
        }
        const auto root = PathFromUtf8(projectRoot) / "rom";
        auto changes = g_contentMonitor.Poll(root);
        g_contentChanges.insert(g_contentChanges.end(), changes.begin(), changes.end());
        if (!IsSceneLoadBusy() && !g_contentChanges.empty()) {
            try {
                const bool directoryChanged = std::any_of(g_contentChanges.begin(), g_contentChanges.end(),
                    [](const auto& change) { return change.added || change.removed; });
                if (directoryChanged) {
                    ProjectAssetIndex::Instance().Refresh();
                    SceneLoader_RefreshSceneAssets();
                }
                for (const auto& change : g_contentChanges) {
                    const std::string path = ContentFileMonitor::Utf8Path(std::filesystem::path("rom") / PathFromUtf8(change.path));
                    auto ext = PathToUtf8(PathFromUtf8(path).extension());
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    std::error_code changeEc, sceneEc;
                    const auto absChange = std::filesystem::absolute(PathFromUtf8(path), changeEc).lexically_normal();
                    const auto absScene = SceneLoader_GetCurrentScenePath().empty()
                        ? std::filesystem::path{}
                        : std::filesystem::absolute(PathFromUtf8(SceneLoader_GetCurrentScenePath()), sceneEc).lexically_normal();
                    if (!absScene.empty() && !changeEc && !sceneEc && absChange == absScene) {
                        std::string content;
                        if (change.removed) {
                            g_contentReloadLight.Failure(); g_contentReloadStatus = "Current scene removed; live scene retained: " + path;
                            LogContentReload(g_contentReloadStatus);
                        } else if (SceneFileMonitor::Read(path, content) && content != SceneLoader_GetCurrentSceneContent()) g_contentSceneReload = true;
                        continue;
                    }
                    if (ext == ".dll") continue; // ScriptFactory owns DLL validation and module lifetime.
                    if (path.rfind("rom/skybox/cache/", 0) == 0) {
                        if (!change.removed && ext == ".dds") {
                            const auto widePath = PathFromUtf8(path).wstring();
                            if (CubemapReload(widePath.c_str()) == TextureReloadResult::Reloaded)
                                RenderSettings_ReloadSkybox();
                        }
                        continue;
                    }
                    if (change.added && !content_reload::SceneJsonReferencesRomPath(SceneLoader_GetCurrentSceneContent(), path)) continue;
                    if (change.removed) {
                        const auto removal = content_reload::DecideRemovedContentAction(
                            path, SceneLoader_GetCurrentSceneContent(), ContentPathCurrentlyLoaded(path));
                        if (removal == content_reload::RemovedContentAction::KeepWatching) {
                            LogContentReload("Content removed (unreferenced, not loaded); watching: " + path);
                            continue;
                        }
                        g_contentReloadLight.Failure();
                        g_contentReloadStatus = "Content removed; existing resources retained: " + path;
                        LogContentReload(g_contentReloadStatus);
                        continue;
                    }
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga") {
                        if (ext == ".png" && panorama::IsEquirectSkyPath(path) &&
                            panorama::SameRomPath(path, RenderSettings_GetConst().skyboxCubemapPath)) {
                            const panorama::Prep prep = panorama::PrepareSky(RenderSettings_GetConst().skyboxCubemapPath);
                            if (!prep.ok) {
                                g_contentReloadLight.Failure();
                                g_contentReloadStatus = "Panorama skybox failed: " + prep.error;
                                LogContentReload(g_contentReloadStatus);
                                continue;
                            }
                            auto reloadCube = [](const std::string& rom) {
                                if (rom.empty())
                                    return;
                                const std::wstring wide = PathFromUtf8(rom).wstring();
                                CubemapReload(wide.c_str());
                            };
                            reloadCube(prep.cubemapRom);
                            reloadCube(prep.irradianceRom);
                            reloadCube(prep.prefilterRom);
                            RenderSettings_ReloadSkybox();
                            if (prep.iblReady) {
                                g_contentReloadLight.Success();
                                g_contentReloadStatus = "Panorama skybox updated: " + path;
                            } else {
                                g_contentReloadLight.Failure();
                                g_contentReloadStatus = "IBL not matching sky: " + prep.error;
                            }
                            LogContentReload(g_contentReloadStatus);
                            continue;
                        }
                        const auto widePath = PathFromUtf8(path).wstring();
                        const auto cube = CubemapReload(widePath.c_str());
                        if (cube == TextureReloadResult::Reloaded) RenderSettings_ReloadSkybox();
                        const auto result = cube != TextureReloadResult::NotLoaded ? cube : TextureReload(widePath.c_str());
                        if (result == TextureReloadResult::Failed) {
                            g_contentReloadLight.Failure(); g_contentReloadStatus = "Texture reload failed; previous version retained: " + path;
                            LogContentReload(g_contentReloadStatus);
                        } else {
                            if (path.rfind("rom/effects/", 0) == 0 && !EffectSystem::ReloadContent()) {
                                g_contentReloadLight.Failure(); g_contentReloadStatus = "Effect resource reload failed: " + path;
                                LogContentReload(g_contentReloadStatus); continue;
                            }
                            g_contentReloadLight.Success(); g_contentReloadStatus = "Texture updated: " + path;
                            // Shared model/material textures also need their owning components rebuilt.
                            if ((change.added && result == TextureReloadResult::NotLoaded && content_reload::SceneJsonReferencesRomPath(SceneLoader_GetCurrentSceneContent(), path)) || (!change.added && (result == TextureReloadResult::NotLoaded || ModelUtil_IsTextureCached(widePath.c_str()) || path.rfind("rom/model/", 0) == 0 || path.rfind("rom/effects/", 0) == 0))) g_contentSceneReload = true;
                        }
                        continue;
                    }
                    if ((ext == ".ttf" || ext == ".otf") && !UIFont_Reload(PathFromUtf8(path).c_str())) {
                        g_contentReloadLight.Failure(); g_contentReloadStatus = "Invalid font; previous version retained: " + path;
                        LogContentReload(g_contentReloadStatus); continue;
                    }
                    if (path.rfind("rom/effects/", 0) == 0 && !EffectSystem::ReloadContent()) {
                        g_contentReloadLight.Failure(); g_contentReloadStatus = "Invalid effect; previous version retained: " + path;
                        LogContentReload(g_contentReloadStatus); continue;
                    }
                    if (ext == ".wav" && !AudioManager::Instance().ReloadContent(path)) {
                        g_contentReloadLight.Failure(); g_contentReloadStatus = "Invalid audio; previous version retained: " + path;
                        LogContentReload(g_contentReloadStatus); continue;
                    }
                    if (ext == ".json" || ext == ".physmat") {
                        std::string content;
                        if (!SceneFileMonitor::Read(path, content) || nlohmann::json::parse(content, nullptr, false).is_discarded()) {
                            g_contentReloadLight.Failure(); g_contentReloadStatus = "Invalid content; previous scene retained: " + path;
                            LogContentReload(g_contentReloadStatus); continue;
                        }
                    }
                    if (!change.added || content_reload::SceneJsonReferencesRomPath(SceneLoader_GetCurrentSceneContent(), path)) g_contentSceneReload = true;
                }
            } catch (const std::system_error& error) {
                g_contentReloadLight.Failure();
                g_contentReloadStatus = std::string("Content path error: ") + error.what();
                LogContentReload(g_contentReloadStatus);
            } catch (const std::exception& error) {
                g_contentReloadLight.Failure();
                g_contentReloadStatus = std::string("Content reload error: ") + error.what();
                LogContentReload(g_contentReloadStatus);
            }
            g_contentChanges.clear();
        }
        if (g_contentSceneReload && !IsSceneLoadBusy() && !g_sceneReloadConflict) {
            const auto& path = SceneLoader_GetCurrentScenePath();
            std::string content;
            if (!path.empty() && SceneFileMonitor::Read(path, content)) {
                if (!g_isPlaying && EngineGUI_IsSceneDirty()) g_sceneReloadConflict = SceneFileMonitor::Version{path, std::move(content)};
                else QueueSceneHotLoad({path, std::move(content)});
            }
            g_contentSceneReload = false;
        }
        if (!g_contentMonitor.Error().empty()) {
            g_contentReloadLight.Failure();
            if (g_contentReloadStatus != g_contentMonitor.Error()) LogContentReload(g_contentMonitor.Error());
            g_contentReloadStatus = g_contentMonitor.Error();
        }
    } else {
        if (!g_contentProjectRoot.empty()) { g_contentMonitor.Stop(); g_contentProjectRoot.clear(); }
        g_contentChanges.clear(); g_contentSceneReload = false;
    }

    if (!ProjectManager::Instance().IsOpen()) {
        ResetSceneHotLoad();
    } else if (!IsSceneLoadBusy()) {
        const auto revision = SceneLoader_GetContentRevision();
        if (g_monitoredSceneRevision != revision) {
            g_sceneFileMonitor.SetBaseline(SceneLoader_GetCurrentScenePath(), SceneLoader_GetCurrentSceneContent());
            g_monitoredSceneRevision = revision;
            g_sceneReloadLight.failed = false;
            g_sceneReloadConflict.reset();
        }
        // An existing conflict can absorb newer writes without opening another modal.

        // Project scenes share the content batch; the legacy monitor covers scenes
        // opened outside rom/ (including standalone transactional host fixtures).
        const auto relativeScene = SceneLoader_GetCurrentScenePath().empty()
            ? std::filesystem::path{}
            : content_path::LexicalChildPath(SceneLoader_GetCurrentScenePath(), "rom");
        const bool contentOwnsScene = !relativeScene.empty();
        if (auto version = g_sceneFileMonitor.Poll(elapsed_time)) {
            if (!contentOwnsScene) g_sceneReloadConflict = std::move(*version);
        }
        if (g_sceneReloadConflict && !ImGui::GetTopMostPopupModal() && !g_showUnsavedChangesPopup) {
            if (g_isPlaying || !EngineGUI_IsSceneDirty()) {
                QueueSceneHotLoad(std::move(*g_sceneReloadConflict));
                g_sceneReloadConflict.reset();
            }
        }
    }

    // Time-sliced asset preload / scene-switch busy: gate editor interaction.
    if (IsSceneLoadBusy()) {
        if (KeyLogger_IsTrigger(KK_F2) && !g_isPlaying) {
            g_playWhenPreloadDone = true;
            g_pendingPlayMode = PlayMode::Viewport;
        }
        if (KeyLogger_IsTrigger(KK_F3) && !g_isPlaying) {
            g_playWhenPreloadDone = true;
            g_pendingPlayMode = PlayMode::Fullscreen;
        }
        if (KeyLogger_IsTrigger(KK_F8))
            JobSystem_ToggleDebugWindow();
        if (g_sceneAssetPreloader.IsActive())
            EngineGUI_TickSceneAssetPreload();
        // Still loading, or deferred StartPlay just entered play ? skip editors.
        if (IsSceneLoadBusy() || g_isPlaying)
            return;
    }

    if (KeyLogger_IsTrigger(KK_F2)) {
        if (g_isPlaying) {
            ReturnToStart();
        }
        else {
            StartPlay(PlayMode::Viewport);
        }
    }

    if (KeyLogger_IsTrigger(KK_F3)) {
        if (g_isPlaying) {
            ReturnToStart();
        }
        else {
            StartPlay(PlayMode::Fullscreen);
        }
    }

    if (g_isPlaying) {
        // Snapshot roots before updating. PrefabRuntime_EnsureSpawnRoot() may
        // push_back into g_sceneNodes during a script Update (e.g. mystery box
        // pickup), which would invalidate a live range-for over g_sceneNodes.
        std::vector<GameObject*> rootsToUpdate;
        rootsToUpdate.reserve(g_sceneNodes.size());
        for (auto& node : g_sceneNodes) {
            if (node)
                rootsToUpdate.push_back(node.get());
        }
        // TAG_PLAYER roots are updated by GameScene's player batch (after
        // FrameTransformSnapshot_Capture) so they can run on workers.
        for (GameObject* node : rootsToUpdate) {
            if (!node || node->GetTag() == Tag::TAG_PLAYER)
                continue;
            node->Update(elapsed_time);
        }
    }

    if (KeyLogger_IsTrigger(KK_F6))
        ThirdPersonCamera_ShowCursor(!ThirdPersonCamera_IsCursorVisible());

    // Editor-only: F8 toggles Job System debug overlay (also works while playing).
    if (KeyLogger_IsTrigger(KK_F8))
        JobSystem_ToggleDebugWindow();

    if (!g_enabled) return;

    if (!g_isPlaying && g_editMode == EditorEditMode::UI
        && g_selUiElement && g_showUiOverlay) {
        UiGizmo::HandleInput(g_selUiElement);
    }

    // F: focus debug camera on selected object
    if (!g_isPlaying && !ImGui::GetIO().WantCaptureKeyboard && KeyLogger_IsTrigger(KK_F) && g_selNode) {
        if (const engine::Transform* t = g_selNode->GetTransform())
            FocusSelectedObjectWithCamera(t);
    }

    // Left-click picking
    {
        static bool s_prevLDown = false;
        bool lDown = Mouse_IsLeftDown();
        bool clicked = lDown && !s_prevLDown;
        s_prevLDown = lDown;
        bool altHeld = Keyboard_IsKeyDown(KK_LEFTALT) || Keyboard_IsKeyDown(KK_RIGHTALT);

        if (clicked && !ImGui::GetIO().WantCaptureMouse
            && !TransformGizmo::IsInteracting()
            && !UiGizmo::IsInteracting()
            && !CameraOrientationGizmo::IsInteracting()
            && !altHeld) {
            if (!g_isPlaying && g_editMode == EditorEditMode::UI) {
                float localX = 0.0f;
                float localY = 0.0f;

                UIElement* picked = nullptr;
                if (EngineGUI_MouseToViewportLocal(localX, localY)) {
                    if (g_selUiElement && UiGizmo::WouldCaptureMouse(g_selUiElement, localX, localY))
                        picked = g_selUiElement;
                    else if (UICanvas* canvas = SceneCanvas_GetCanvas()) {
                        const auto& elements = canvas->GetElements();
                        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
                            if (*it && (*it)->HitTest(localX, localY)) {
                                picked = it->get();
                                break;
                            }
                        }
                    }
                }

                g_selUiElement = picked;
                g_selNode = nullptr;
            }
            else if (!g_isPlaying && g_editMode == EditorEditMode::Scene3D) {
            using namespace DirectX;
            int mx, my;
            Mouse_GetPosition(mx, my);
            XMVECTOR rayOrg, rayDir;
            if (!BuildPickRay(mx, my, rayOrg, rayDir))
                return;

            float bestT = FLT_MAX;
            GameObject* bestNode = nullptr;

            std::function<void(GameObject*)> pickNode = [&](GameObject* node) {
                if (!node || !node->IsActive()) return;
                if(SceneLoader_Is2D()) {
                    if(auto* sprite=node->GetComponent<SpriteRendererComponent>(); sprite && sprite->HitTestRay(rayOrg,rayDir)) {
                        auto* previous=bestNode?bestNode->GetComponent<SpriteRendererComponent>():nullptr;
                        if(!previous || std::tie(sprite->sortingLayer,sprite->orderInLayer)>=std::tie(previous->sortingLayer,previous->orderInLayer)) bestNode=node;
                    }
                } else if (const engine::Transform* tr = node->GetTransform()) {
                    float t = -1.0f;
                    if (auto* vc = node->GetComponent<VisualComponent>()) {
                        if (Model3D* model = vc->GetModel()) {
                            const auto& meshVerts = model->GetVerticesCPU();
                            if (!meshVerts.empty()) {
                                t = RayPickModelMesh(meshVerts, rayOrg, rayDir,
                                    tr->localPosition, tr->localScale, tr->localRotation);
                            }
                        }
                    }
                    if (t < 0.0f) {
                        const float pickR = NodeHasLightComponent(node) ? 0.5f : 0.0f;
                        XMFLOAT3 pos = tr->localPosition;
                        float sx = fabsf(tr->localScale.x);
                        float sy = fabsf(tr->localScale.y);
                        float sz = fabsf(tr->localScale.z);
                        float r = pickR > 0.0f ? pickR : (sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz)) * 0.5f;
                        if (r < 0.5f) r = 0.5f;
                        t = RaySphere(rayOrg, rayDir, pos, r);
                    }
                    if (t >= 0.0f && t < bestT) {
                        bestT = t;
                        bestNode = node;
                    }
                }
                for (const auto& child : node->GetChildren())
                    pickNode(child.get());
            };

            for (auto& root : g_sceneNodes)
                pickNode(root.get());

            g_selNode = bestNode;
            g_selUiElement = nullptr;
            }
        }
    }
}

// --- Proxy / scene drawing ---

static void DrawEditorSceneProxiesNode(GameObject* node) {
    if (!node) return;
    node->DrawVisual(false);
}

void DrawEditorSceneProxies() {
    for (const auto& root : g_sceneNodes)
        if (root) DrawEditorSceneProxiesNode(root.get());
}

// --- Scene hierarchy ImGui helpers ---

static bool MoveNodeInList(std::vector<std::unique_ptr<GameObject>>& nodes,
                           GameObject* node,
                           int offset)
{
    if (!node || offset == 0) return false;

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [node](const std::unique_ptr<GameObject>& owned) {
            return owned.get() == node;
        });
    if (it == nodes.end()) return false;

    const int index = static_cast<int>(std::distance(nodes.begin(), it));
    int target = index + offset;
    if (target < 0) target = 0;
    const int last = static_cast<int>(nodes.size()) - 1;
    if (target > last) target = last;
    if (target == index) return false;

    auto owned = std::move(*it);
    nodes.erase(it);
    nodes.insert(nodes.begin() + target, std::move(owned));
    return true;
}

static bool MoveNodeOrder(GameObject* node, int offset) {
    if (!node) return false;
    if (GameObject* parent = node->GetParent())
        return MoveNodeInList(parent->GetChildren(), node, offset);
    return MoveNodeInList(g_sceneNodes, node, offset);
}

static bool MoveNodeToBack(GameObject* node) {
    if (!node) return false;
    const int distance = static_cast<int>((node->GetParent()
        ? node->GetParent()->GetChildren().size()
        : g_sceneNodes.size()));
    return MoveNodeOrder(node, -distance);
}

static bool MoveNodeToFront(GameObject* node) {
    if (!node) return false;
    const int distance = static_cast<int>((node->GetParent()
        ? node->GetParent()->GetChildren().size()
        : g_sceneNodes.size()));
    return MoveNodeOrder(node, distance);
}

static int GetNodeSiblingIndexLocal(GameObject* node) {
    if (!node) return -1;
    if (GameObject* parent = node->GetParent()) {
        const auto& ch = parent->GetChildren();
        for (int i = 0; i < (int)ch.size(); ++i)
            if (ch[i].get() == node) return i;
        return -1;
    }
    for (int i = 0; i < (int)g_sceneNodes.size(); ++i)
        if (g_sceneNodes[i].get() == node) return i;
    return -1;
}

static bool UndoableReorderNode(GameObject* node, bool (*moveFn)(GameObject*)) {
    if (!node || !moveFn) return false;
    GameObject* parent = node->GetParent();
    const int oldIndex = GetNodeSiblingIndexLocal(node);
    if (oldIndex < 0 || !moveFn(node))
        return false;
    const int newIndex = GetNodeSiblingIndexLocal(node);
    if (oldIndex != newIndex)
        EditorUndo_PushReparent(node, parent, oldIndex, parent, newIndex);
    return true;
}

static void DrawSceneNodeHierarchy(GameObject* node, GameObject* rootOwner) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node->GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (g_selNode == node)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool nodeOpen = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(node), flags, "%s", node->GetName().c_str());

    if (!g_isPlaying && ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        g_selNode = node;
        g_selUiElement = nullptr;
    }

    if (!g_isPlaying && ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Empty Child"))
            SpawnChildGameObject(node);
        if (ImGui::MenuItem("Duplicate"))
            SpawnDuplicate(node);
        ImGui::Separator();
        if (ImGui::MenuItem("Delete"))
            DeleteGameObject(node);
        ImGui::EndPopup();
    }

    // Drag source
    if (!g_isPlaying && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        GameObject* ptr = node;
        ImGui::SetDragDropPayload("HIERARCHY_GO", &ptr, sizeof(ptr));
        ImGui::Text("Move: %s", node->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target
    if (!g_isPlaying && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("HIERARCHY_GO")) {
            GameObject* dragged = *static_cast<GameObject**>(pl->Data);
            if (dragged && dragged != node) {
                GameObject* oldParent = dragged->GetParent();
                int oldIndex = -1;
                if (oldParent) {
                    const auto& ch = oldParent->GetChildren();
                    for (int i = 0; i < (int)ch.size(); ++i)
                        if (ch[i].get() == dragged) { oldIndex = i; break; }
                } else {
                    for (int i = 0; i < (int)g_sceneNodes.size(); ++i)
                        if (g_sceneNodes[i].get() == dragged) { oldIndex = i; break; }
                }
                GameObject* dragParent = dragged->GetParent();
                if (dragParent) {
                    dragParent->ReparentChild(dragged, *node);
                }
                else {
                    auto it = std::find_if(g_sceneNodes.begin(), g_sceneNodes.end(),
                        [dragged](const std::unique_ptr<GameObject>& p) { return p.get() == dragged; });
                    if (it != g_sceneNodes.end()) {
                        std::unique_ptr<GameObject> owned = std::move(*it);
                        g_sceneNodes.erase(it);
                        owned->SetParent(node);
                        node->AddChild(std::move(owned));
                    }
                }
                const int newIndex = (int)node->GetChildren().size() - 1;
                EditorUndo_PushReparent(dragged, oldParent, oldIndex, node, newIndex);
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
                if (g_selNode == dragged) g_selNode = nullptr;
            }
        }
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {
            std::string path(static_cast<const char*>(pl->Data));
            auto prefabNode = PrefabSerializer::LoadFromFile(path);
            if (prefabNode) {
                GameObject* raw = prefabNode.get();
                node->AddChild(std::move(prefabNode));
                const int idx = (int)node->GetChildren().size() - 1;
                EditorUndo_PushCreate(raw, node, idx);
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (nodeOpen && !node->GetChildren().empty()) {
        for (auto& child : node->GetChildren())
            DrawSceneNodeHierarchy(child.get(), rootOwner);
        ImGui::TreePop();
    }
}

struct MeshLocalBounds {
    DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    float hx = 0.0f;
    float hy = 0.0f;
    float hz = 0.0f;
    float sphereRadius = 0.0f;
    float cylinderRadius = 0.0f;
};

// Stored collider size is mesh-local. Owner scale is applied when the
// collider is drawn and tested, so resizing the object resizes the
// collider without running Auto-Fit again.
static bool ComputeMeshLocalBounds(GameObject* node, MeshLocalBounds& out) {
    if (!node)
        return false;
    VisualComponent* visual = node->GetComponent<VisualComponent>();
    if (!visual)
        return false;
    Model3D* model = visual->GetModel();
    if (!model)
        return false;
    const std::vector<Model3D::VertexPNTC>& verts = model->GetVerticesCPU();
    if (verts.empty())
        return false;

    DirectX::XMFLOAT3 bmin = { FLT_MAX, FLT_MAX, FLT_MAX };
    DirectX::XMFLOAT3 bmax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const Model3D::VertexPNTC& v : verts) {
        const float x = v.Position.x;
        const float y = v.Position.y;
        const float z = v.Position.z;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        bmin.x = (std::min)(bmin.x, x);
        bmin.y = (std::min)(bmin.y, y);
        bmin.z = (std::min)(bmin.z, z);
        bmax.x = (std::max)(bmax.x, x);
        bmax.y = (std::max)(bmax.y, y);
        bmax.z = (std::max)(bmax.z, z);
    }

    out.center = {
        (bmin.x + bmax.x) * 0.5f,
        (bmin.y + bmax.y) * 0.5f,
        (bmin.z + bmax.z) * 0.5f
    };
    out.hx = (bmax.x - bmin.x) * 0.5f;
    out.hy = (bmax.y - bmin.y) * 0.5f;
    out.hz = (bmax.z - bmin.z) * 0.5f;

    // AABB corners are empty on round meshes (a cone's base is a circle).
    // Measure the farthest real vertex from the bounds center instead.
    float maxRadiusSq = 0.0f;
    float maxCylinderRadiusSq = 0.0f;
    for (const Model3D::VertexPNTC& v : verts) {
        const float dx = v.Position.x - out.center.x;
        const float dy = v.Position.y - out.center.y;
        const float dz = v.Position.z - out.center.z;
        maxRadiusSq = (std::max)(maxRadiusSq, dx * dx + dy * dy + dz * dz);
        maxCylinderRadiusSq = (std::max)(maxCylinderRadiusSq, dx * dx + dz * dz);
    }
    out.sphereRadius = std::sqrt(maxRadiusSq);
    out.cylinderRadius = std::sqrt(maxCylinderRadiusSq);
    return true;
}

static bool AutoFitColliderToMesh(GameObject* node, ColliderComponent* cc) {
    MeshLocalBounds bounds;
    if (!cc || !ComputeMeshLocalBounds(node, bounds))
        return false;

    switch (cc->colliderType) {
    case ColliderType::BOX:
        cc->colliderCenter = bounds.center;
        cc->halfExtents = { bounds.hx, bounds.hy, bounds.hz };
        return true;
    case ColliderType::SPHERE:
        cc->colliderCenter = bounds.center;
        cc->radius = bounds.sphereRadius;
        return true;
    case ColliderType::CYLINDER:
        cc->colliderCenter = bounds.center;
        cc->radius = bounds.cylinderRadius;
        cc->height = 2.0f * bounds.hy;
        return true;
    case ColliderType::CAPSULE:
        // cylinderRadius, not the AABB corner. Round meshes leave that
        // corner empty, and the engine capsule primitive is r=0.5 with a
        // segment of 1 for a mesh of total height 2. See ComputeCapsuleFit.
        cc->colliderCenter = bounds.center;
        ComputeCapsuleFit(bounds.hy, bounds.cylinderRadius, cc->radius, cc->height);
        return true;
    default:
        return false;
    }
}

// One-shot box fit in mesh-local space. Object scale resizes it later.
void EngineGUI_FitNewCollider(GameObject* node) {
    if (!node)
        return;
    ColliderComponent* cc = FindColliderComponent(node);
    if (!cc)
        cc = node->AddComponent<ColliderComponent>();
    cc->colliderType = ColliderType::BOX;
    if (!AutoFitColliderToMesh(node, cc))
        cc->colliderType = ColliderType::NONE;
}

// --- EngineGUI_Draw ---

void EngineGUI_OnBackbufferResized() {
    if (!g_imguiInitialized) return;
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();
}

static void DrawReloadLight(const char* id, const char* name, ReloadLightState state,
    const ReloadLight& light, const std::string& detail) {
    const float size = Ui(18.0f);
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool bright = state == ReloadLightState::Watching && ImGui::GetTime() < light.brightUntil;
    ImU32 color;
    const char* status;
    switch (state) {
    case ReloadLightState::Paused: color = IM_COL32(110, 115, 125, 255); status = "Paused / inactive"; break;
    case ReloadLightState::Pending: color = IM_COL32(245, 190, 55, 255); status = "Waiting / processing"; break;
    case ReloadLightState::Failed: color = IM_COL32(235, 75, 75, 255); status = "Reload failed"; break;
    default: color = bright ? IM_COL32(135, 255, 165, 255) : IM_COL32(60, 165, 95, 255);
        status = bright ? "Reload succeeded" : "Watching"; break;
    }
    const ImVec2 center(origin.x + size * 0.5f, origin.y + size * 0.5f);
    if (bright) ImGui::GetWindowDrawList()->AddCircleFilled(center, Ui(7.0f), IM_COL32(90, 255, 140, 65));
    ImGui::GetWindowDrawList()->AddCircleFilled(center, Ui(4.0f), color);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %s", name, status);
        if (state == ReloadLightState::Paused) ImGui::TextUnformatted("Hot-load requires an open project.");
        ImGui::TextUnformatted(detail.c_str());
        ImGui::EndTooltip();
    }
}

void EngineGUI_Draw() {
    const bool playVisuals=EngineGUI_PreferPlayVisuals();
    // Play mode sets g_enabled=false (hides editor chrome). Still draw the
    // Job System overlay when F8 has opened it so it survives play.
    const bool willProcessImGuiFrame =
        g_enabled || (JobSystem_IsDebugWindowVisible() && g_imguiInitialized);
    SyncImGuiInputAcceptance(willProcessImGuiFrame && !g_holdPlayVisuals);

    if (!g_enabled) {
        if (!JobSystem_IsDebugWindowVisible() || !g_imguiInitialized)
            return;

        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        Renderer_UpdateClientSize();
        io.DisplaySize = ImVec2(GetClientWidth(), GetClientHeight());
        const float bbW = GetBackBufferWidth();
        const float bbH = GetBackBufferHeight();
        if (bbW > 0.0f && bbH > 0.0f) {
            io.DisplayFramebufferScale = ImVec2(GetClientWidth() / bbW, GetClientHeight() / bbH);
        }
        else {
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        }
        ImGui::NewFrame();
        JobSystem_DebugDraw();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (ImGui::GetIO().WantCaptureMouse)
            Mouse_ClearWheel();
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    Renderer_UpdateClientSize();
    io.DisplaySize = ImVec2(GetClientWidth(), GetClientHeight());
    const float bbW = GetBackBufferWidth();
    const float bbH = GetBackBufferHeight();
    if (bbW > 0.0f && bbH > 0.0f) {
        io.DisplayFramebufferScale = ImVec2(GetClientWidth() / bbW, GetClientHeight() / bbH);
    }
    else {
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    }
    ImGui::NewFrame();

    // No project open: product start screen only (folder-pick still runs after render).
    if (!ProjectManager::Instance().IsOpen()) {
        DrawEngineStartScreen();
        // New Project name modal (and unsaved guard) live in DrawEditorPopups.
        DrawEditorPopups();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (ImGui::GetIO().WantCaptureMouse)
            Mouse_ClearWheel();
        ConsumePendingFolderPick();
        return;
    }

    TransformGizmo::BeginFrame();

    const float screenW = io.DisplaySize.x;
    const float screenH = io.DisplaySize.y;
    const float menuBarHeight = EditorMenuBarHeight();
    const EditorLayoutMetrics layout = ComputeEditorLayout(screenW, screenH);
    const float leftWidth = layout.leftWidth;
    const float rightWidth = layout.rightWidth;
    const float bottomHeight = layout.bottomHeight;
    const float centerTop = layout.topChrome;
    const float centerH = layout.sidePanelH;

    const bool sceneLoadBusy = IsSceneLoadBusy();
    const bool preloadActive = g_sceneAssetPreloader.IsActive();
    const bool showSceneLoadModal=sceneLoadBusy && !g_holdPlayVisuals;
    if (showSceneLoadModal)
        ImGui::OpenPopup("SceneAssetPreloadModal");

    if (!sceneLoadBusy) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            SaveScene();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S))
            SaveProject();
        // RouteAlways: default RouteFocused fails when focus is on the game viewport.
        if (!playVisuals && !ImGui::GetIO().WantTextInput) {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteAlways)) {
                if (EditorUndo_Undo()) {
                    EngineGUI_MarkSceneDirty();
                    SyncSceneRuntime();
                }
            }
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteAlways) ||
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteAlways)) {
                if (EditorUndo_Redo()) {
                    EngineGUI_MarkSceneDirty();
                    SyncSceneRuntime();
                }
            }
        }
    }

    if (ImGui::BeginMainMenuBar()) {
        DrawEditorMenuBar();
        ImGui::EndMainMenuBar();
    }

    DrawEditorPopups();
    DrawRenderingSettingsWindow(&g_showRenderingSettings);
    DrawPostFxShowcaseWindow(&g_showPostFxShowcase);
    DrawShortcutHintsWindow(&g_showShortcutHints);

    // --- Toolbar ---
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(screenW, Ui(kToolbarHeight)));
    ImGui::Begin("##Toolbar", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    {
        const float lightSize = Ui(18.0f);
        const float lightGap = Ui(4.0f);
        const float lightsX = ImGui::GetWindowContentRegionMax().x - lightSize * 3 - lightGap * 2;
        const float lightsY = ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - lightSize) * 0.5f;
        const auto toolbarPos = ImGui::GetWindowPos();
        const auto contentMin = ImGui::GetWindowContentRegionMin();
        ImGui::PushClipRect(ImVec2(contentMin.x + toolbarPos.x, contentMin.y + toolbarPos.y),
            ImVec2(toolbarPos.x + lightsX - Ui(4.0f), toolbarPos.y + ImGui::GetWindowContentRegionMax().y), true);
        if (playVisuals) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.15f, 0.15f, 1.0f));
            if (ToolbarIconButton(ICON_FA_STOP, "Stop (F2 / F3)")) {
                ReturnToStart();
            }
            ImGui::PopStyleColor(3);
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.55f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.30f, 0.55f, 1.0f));
            if (ToolbarIconButton(ICON_FA_PLAY, "Play in Viewport (F2)")) {
                StartPlay(PlayMode::Viewport);
            }
            ImGui::SameLine(0.0f, Ui(2.0f));
            const bool openPlayMenu = ImGui::Button(ICON_FA_CARET_DOWN "##PlayMenuBtn", ImVec2(Ui(16.0f), 0.0f));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Play Mode");
            if (openPlayMenu)
                ImGui::OpenPopup("PlayModeDropdownMenu");

            if (ImGui::BeginPopup("PlayModeDropdownMenu")) {
                if (ImGui::MenuItem("Play in Viewport", "F2")) {
                    StartPlay(PlayMode::Viewport);
                }
                if (ImGui::MenuItem("Play Fullscreen", "F3")) {
                    StartPlay(PlayMode::Fullscreen);
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        {
            const bool canSave = !playVisuals && !sceneLoadBusy;
            if (!canSave)
                ImGui::BeginDisabled();
            if (ToolbarIconButton(ICON_FA_FLOPPY_DISK, "Save Scene (Ctrl+S)"))
                SaveScene();
            if (!canSave)
                ImGui::EndDisabled();
        }

        ImGui::SameLine();
        {
            const bool canEditUndo = !playVisuals && !sceneLoadBusy;
            if (!canEditUndo || !EditorUndoStack::Instance().CanUndo())
                ImGui::BeginDisabled();
            const bool undoPressed = ToolbarIconButton(ICON_FA_ROTATE_LEFT, "Undo (Ctrl+Z)");
            if (!canEditUndo || !EditorUndoStack::Instance().CanUndo())
                ImGui::EndDisabled();
            if (undoPressed) {
                if (EditorUndo_Undo()) {
                    EngineGUI_MarkSceneDirty();
                    SyncSceneRuntime();
                }
            }
            ImGui::SameLine();
            if (!canEditUndo || !EditorUndoStack::Instance().CanRedo())
                ImGui::BeginDisabled();
            const bool redoPressed = ToolbarIconButton(ICON_FA_ROTATE_RIGHT, "Redo (Ctrl+Y)");
            if (!canEditUndo || !EditorUndoStack::Instance().CanRedo())
                ImGui::EndDisabled();
            if (redoPressed) {
                if (EditorUndo_Redo()) {
                    EngineGUI_MarkSceneDirty();
                    SyncSceneRuntime();
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROWS_ROTATE, ImVec2(Ui(28.0f), 0.0f)))
            SceneLoader_ScanScriptDllsForChanges(true);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Reload Scripts");

        if (!playVisuals) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.15f, 0.15f, 1.0f));
            if (ToolbarIconButton(ICON_FA_BACKWARD_STEP, "Return to Start")) ReturnToStart();
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            const bool highlightUiOverlay = g_showUiOverlay;
            if (highlightUiOverlay)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.55f, 1.0f));
            const bool overlayToggled = ToolbarIconButton(
                highlightUiOverlay ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                highlightUiOverlay ? "Hide UI Overlay" : "Show UI Overlay");
            if (overlayToggled)
                g_showUiOverlay = !g_showUiOverlay;
            if (highlightUiOverlay)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            const bool highlightGrid = Grid_IsVisible();
            if (highlightGrid)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.55f, 1.0f));
            const bool gridToggled = ToolbarIconButton(
                highlightGrid ? ICON_FA_BORDER_ALL : ICON_FA_BORDER_NONE,
                highlightGrid ? "Hide Grid" : "Show Grid");
            if (gridToggled)
                Grid_SetVisible(!highlightGrid);
            if (highlightGrid)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::RadioButton(ICON_FA_CUBE, g_editMode == EditorEditMode::Scene3D))
                EngineGUI_SetEditMode(EditorEditMode::Scene3D);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Scene Edit Mode");
            ImGui::SameLine();
            if (ImGui::RadioButton(ICON_FA_LAYER_GROUP, g_editMode == EditorEditMode::UI))
                EngineGUI_SetEditMode(EditorEditMode::UI);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("UI Edit Mode");

            ImGui::SameLine();
            bool view2D=DebugCamera_Is2DView();
            ImGui::BeginDisabled(playVisuals);
            if(ImGui::Checkbox("2D View",&view2D)) DebugCamera_Set2DView(view2D);
            ImGui::EndDisabled(); ImGui::SameLine();
            TransformGizmo::DrawToolbarControls();

            ImGui::SameLine();
            // Camera selector
            {
                std::vector<std::string> camNames;
                std::function<void(GameObject*)> collectCams = [&](GameObject* node) {
                    if (!node || !node->IsActive()) return;
                    if (auto* camera = node->GetComponent<CameraComponent>();
                        camera && camera->enabled && node->GetComponent<engine::Transform>())
                        camNames.push_back(node->GetName());
                    for (const auto& child : node->GetChildren())
                        collectCams(child.get());
                };
                for (const auto& node : g_sceneNodes)
                    collectCams(node.get());

                if (g_activeCameraNode.empty() && camNames.size() == 1)
                    g_activeCameraNode = camNames[0];

                const bool validSelection = g_cameraMode == CameraMode::SceneCamera
                    && std::count(camNames.begin(),camNames.end(),g_activeCameraNode)==1;
                const std::string currentLabel = validSelection ? g_activeCameraNode
                    : camNames.empty() ? "No enabled game camera" : "Select game camera";

                ImGui::Text("Camera:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::BeginCombo("##Camera", currentLabel.c_str())) {
                    for (const auto& name : camNames) {
                        bool sel = (g_cameraMode == CameraMode::SceneCamera && g_activeCameraNode == name);
                        if (ImGui::Selectable(name.c_str(), sel)) {
                            g_cameraMode = CameraMode::SceneCamera;
                            g_activeCameraNode = name;
                            EngineGUI_MarkSceneDirty();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        ImGui::PopClipRect();
        ImGui::SetCursorPos(ImVec2(lightsX, lightsY));
        const bool projectOpen = ProjectManager::Instance().IsOpen();
        DrawReloadLight("##SceneReloadLight", "Scene JSON",
            GetReloadLightState(projectOpen && !SceneLoader_GetCurrentScenePath().empty(), false,
                sceneLoadBusy || g_sceneReloadConflict.has_value() || g_sceneFileMonitor.HasPendingChange(), g_sceneReloadLight),
            g_sceneReloadLight, g_sceneReloadStatus);
        ImGui::SameLine(0.0f, lightGap);
        DrawReloadLight("##ContentReloadLight", "Content",
            GetReloadLightState(projectOpen, false, g_contentMonitor.Pending() || !g_contentChanges.empty() || g_contentSceneReload,
                g_contentReloadLight), g_contentReloadLight, g_contentReloadStatus);
        ImGui::SameLine(0.0f, lightGap);
        DrawReloadLight("##ScriptReloadLight", "Script DLL",
            GetReloadLightState(projectOpen, false, ScriptFactory::Instance().PendingReloadCount() != 0, g_scriptReloadLight),
            g_scriptReloadLight, g_scriptReloadStatus);
    }
    ImGui::End();

    // --- Scene Hierarchy (left) ---
    ImGui::SetNextWindowPos(ImVec2(0, centerTop));
    ImGui::SetNextWindowSize(ImVec2(leftWidth, centerH));
    ImGui::Begin("Scene Hierarchy", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (playVisuals)
            ImGui::BeginDisabled(true);

        if (ImGui::CollapsingHeader("Game Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& rootNode : g_sceneNodes)
                if (rootNode) DrawSceneNodeHierarchy(rootNode.get(), rootNode.get());

            // Root drop zone
            ImGui::Selectable("##RootDropZone", false,
                ImGuiSelectableFlags_AllowOverlap, ImVec2(-1, 6));
            if (!playVisuals && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("HIERARCHY_GO")) {
                    GameObject* dragged = *static_cast<GameObject**>(pl->Data);
                    if (dragged && dragged->GetParent()) {
                        GameObject* oldParent = dragged->GetParent();
                        int oldIndex = -1;
                        auto& siblings = oldParent->GetChildren();
                        for (int i = 0; i < (int)siblings.size(); ++i)
                            if (siblings[i].get() == dragged) { oldIndex = i; break; }
                        auto it = std::find_if(siblings.begin(), siblings.end(),
                            [dragged](const std::unique_ptr<GameObject>& c) { return c.get() == dragged; });
                        if (it != siblings.end()) {
                            std::unique_ptr<GameObject> owned = std::move(*it);
                            siblings.erase(it);
                            owned->SetParent(nullptr);
                            g_sceneNodes.push_back(std::move(owned));
                            const int newIndex = (int)g_sceneNodes.size() - 1;
                            EditorUndo_PushReparent(dragged, oldParent, oldIndex, nullptr, newIndex);
                            EngineGUI_MarkSceneDirty();
                            SyncSceneRuntime();
                        }
                    }
                }
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {
                    std::string path(static_cast<const char*>(pl->Data));
                    auto prefabNode = PrefabSerializer::LoadFromFile(path);
                    if (prefabNode) {
                        GameObject* raw = prefabNode.get();
                        g_sceneNodes.push_back(std::move(prefabNode));
                        EditorUndo_PushCreate(raw, nullptr, (int)g_sceneNodes.size() - 1);
                        EngineGUI_MarkSceneDirty();
                        SyncSceneRuntime();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            g_sceneNodes.erase(
                std::remove_if(g_sceneNodes.begin(), g_sceneNodes.end(),
                    [](const std::unique_ptr<GameObject>& n) { return !n; }),
                g_sceneNodes.end());
        }

        if (ImGui::CollapsingHeader("Canvas Elements", ImGuiTreeNodeFlags_DefaultOpen)) {
            EngineGUIUI::DrawCanvasElementHierarchy(g_selUiElement, g_selNode);
        }

        if (playVisuals)
            ImGui::EndDisabled();
    }
    ImGui::End();

    // --- Properties (right) ---
    ImGui::SetNextWindowPos(ImVec2(screenW - rightWidth, centerTop));
    ImGui::SetNextWindowSize(ImVec2(rightWidth, centerH));
    ImGui::Begin("Properties", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (playVisuals)
            ImGui::BeginDisabled(true);

        if (g_selNode) {
            bool runtimeDirty = false;
            static GameObject* s_lastSelNode = nullptr;
            static char nodeNameBuf[128] = {};
            if (g_selNode != s_lastSelNode) {
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = g_selNode;
                strncpy_s(nodeNameBuf, g_selNode->GetName().c_str(), sizeof(nodeNameBuf) - 1);
            }

            // Do not auto-add Visual on select/inspect — that marked scenes dirty
            // without a user edit. Add via the component combo instead.
            VisualComponent* vc = g_selNode->GetComponent<VisualComponent>();

            ImGui::Text("Node: %s", g_selNode->GetName().c_str());
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##NodeName", nodeNameBuf, sizeof(nodeNameBuf),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
                const std::string oldName = g_selNode->GetName();
                const std::string newName(nodeNameBuf);
                g_selNode->SetName(newName);
                EditorUndo_PushRename(g_selNode, oldName, newName);
                EngineGUI_MarkSceneDirty();
            }
            if (!ImGui::IsItemActive())
                strncpy_s(nodeNameBuf, g_selNode->GetName().c_str(), sizeof(nodeNameBuf) - 1);
            ImGui::Spacing();

            ImGui::Text("Hierarchy Order");
            if (ImGui::Button("Back", ImVec2(70, 0)))
            {
                if (UndoableReorderNode(g_selNode, MoveNodeToBack))
                    runtimeDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Down", ImVec2(70, 0)))
            {
                if (UndoableReorderNode(g_selNode, [](GameObject* n) {
                    return MoveNodeOrder(n, -1);
                }))
                    runtimeDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Up", ImVec2(70, 0)))
            {
                if (UndoableReorderNode(g_selNode, [](GameObject* n) {
                    return MoveNodeOrder(n, 1);
                }))
                    runtimeDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Top", ImVec2(70, 0)))
            {
                if (UndoableReorderNode(g_selNode, MoveNodeToFront))
                    runtimeDirty = true;
            }
            ImGui::TextDisabled("Later items draw later; UI later items appear on top.");
            ImGui::Spacing();

            // Transform
            if (engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                ImGui::Text("Transform");
                ImGui::Separator();
                EditorUndo_BeginComponentEdit(g_selNode, "SceneTransform");
                const float xformBefore[9] = {
                    t->localPosition.x, t->localPosition.y, t->localPosition.z,
                    t->localRotation.x, t->localRotation.y, t->localRotation.z,
                    t->localScale.x, t->localScale.y, t->localScale.z
                };
                bool xformEdited = false;
                ImGui::Text("Position");
                ImGui::Text("X:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NPX", &t->localPosition.x, 0.1f);
                xformEdited |= ScrollValue(&t->localPosition.x);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NPY", &t->localPosition.y, 0.1f);
                xformEdited |= ScrollValue(&t->localPosition.y);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NPZ", &t->localPosition.z, 0.1f);
                xformEdited |= ScrollValue(&t->localPosition.z);
                ImGui::Spacing();
                ImGui::Text("Rotation");
                ImGui::Text("X:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NRX", &t->localRotation.x, 1.0f);
                xformEdited |= ScrollValue(&t->localRotation.x, 1.0f);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NRY", &t->localRotation.y, 1.0f);
                xformEdited |= ScrollValue(&t->localRotation.y, 1.0f);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NRZ", &t->localRotation.z, 1.0f);
                xformEdited |= ScrollValue(&t->localRotation.z, 1.0f);
                ImGui::Spacing();
                ImGui::Text("Scale");
                ImGui::Text("X:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NSX", &t->localScale.x, 0.1f, 0.001f, 1000.0f);
                xformEdited |= ScrollValue(&t->localScale.x);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NSY", &t->localScale.y, 0.1f, 0.001f, 1000.0f);
                xformEdited |= ScrollValue(&t->localScale.y);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NSZ", &t->localScale.z, 0.1f, 0.001f, 1000.0f);
                xformEdited |= ScrollValue(&t->localScale.z);
                ImGui::Spacing();
                const float xformAfter[9] = {
                    t->localPosition.x, t->localPosition.y, t->localPosition.z,
                    t->localRotation.x, t->localRotation.y, t->localRotation.z,
                    t->localScale.x, t->localScale.y, t->localScale.z
                };
                if (xformEdited && JsonFloat_TransformChanged(xformBefore, xformAfter))
                    runtimeDirty = true;
                else if (xformEdited && !JsonFloat_TransformChanged(xformBefore, xformAfter)) {
                    // DragFloat/Scroll reported a change that does not survive 3dp — keep bits.
                    t->localPosition = { xformBefore[0], xformBefore[1], xformBefore[2] };
                    t->localRotation = { xformBefore[3], xformBefore[4], xformBefore[5] };
                    t->localScale    = { xformBefore[6], xformBefore[7], xformBefore[8] };
                }
                g_selNode->SyncLegacyTransformFromComponents();
                EditorUndo_EndComponentEdit(g_selNode, "SceneTransform");
            }

            ImGui::Text("Scroll Sensitivity");
            ImGui::SliderFloat("##Sens", &g_scrollSensitivity, 0.1f, 5.0f, "%.1f");
            ImGui::Separator();

            ImGui::Text("Tag");
            {
                TagRegistry& tags = TagRegistry::Instance();
                int currentTag = g_selNode->GetTagId();
                if (!tags.IsValid(currentTag))
                    currentTag = static_cast<int>(Tag::TAG_NONE);

                ImGui::SameLine(80);
                ImGui::SetNextItemWidth(-1);
                const char* preview = tags.GetName(currentTag);
                if (ImGui::BeginCombo("##Tag", preview, ImGuiComboFlags_HeightLarge)) {
                    const float removeW = ImGui::GetFrameHeight();
                    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
                    const int noneId = static_cast<int>(Tag::TAG_NONE);

                    for (int i = 0; i < tags.GetCount(); ++i) {
                        ImGui::PushID(i);
                        const bool selected = (i == currentTag);
                        const bool showRemove = (i != noneId);
                        const bool canRemove = tags.CanRemove(i);
                        const char* name = tags.GetName(i);

                        if (showRemove) {
                            const float labelW =
                                ImGui::GetContentRegionAvail().x - removeW - spacing;
                            if (ImGui::Selectable(name, selected, 0,
                                    ImVec2((labelW > 1.0f) ? labelW : 1.0f, 0.0f))) {
                                const int oldTag = g_selNode->GetTagId();
                                g_selNode->SetTag(i);
                                EditorUndo_PushTag(g_selNode, oldTag, i);
                                runtimeDirty = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();

                            ImGui::SameLine(0.0f, spacing);
                            if (!canRemove)
                                ImGui::BeginDisabled();
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                ImVec4(0.85f, 0.25f, 0.25f, 0.55f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                ImVec4(0.9f, 0.2f, 0.2f, 0.75f));
                            if (ImGui::Button(ICON_FA_XMARK "##RemoveTag",
                                    ImVec2(removeW, removeW))) {
                                g_pendingDeleteTagId = i;
                                g_showDeleteTagPopup = true;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::PopStyleColor(3);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled |
                                    ImGuiHoveredFlags_DelayShort)) {
                                ImGui::SetTooltip(canRemove
                                    ? "Delete tag"
                                    : "Built-in tags cannot be deleted");
                            }
                            if (!canRemove)
                                ImGui::EndDisabled();
                        }
                        else {
                            if (ImGui::Selectable(name, selected)) {
                                const int oldTag = g_selNode->GetTagId();
                                g_selNode->SetTag(i);
                                EditorUndo_PushTag(g_selNode, oldTag, i);
                                runtimeDirty = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    if (ImGui::Selectable(ICON_FA_PLUS "  Add New")) {
                        g_newTagName[0] = '\0';
                        g_showAddTagPopup = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::Spacing();

            if (vc) {
            ImGui::Separator();
            ImGui::Text("Visual");
            EditorUndo_BeginComponentEdit(g_selNode, "visual");

            ImGui::Text("Mesh");
            {
                const auto& defs = ModelAssetCache::Instance().GetObjectDefinitions();
                int currentIdx = -1;
                for (int i = 0; i < (int)defs.size(); ++i) {
                    if (defs[i].assetPath == vc->modelPath) {
                        currentIdx = i;
                        break;
                    }
                }
                const char* preview = vc->modelPath.empty()
                    ? "(None)"
                    : (currentIdx >= 0 ? defs[currentIdx].displayName.c_str() : vc->modelPath.c_str());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##Mesh", preview)) {
                    for (int i = 0; i < (int)defs.size(); ++i) {
                        const bool selected = (i == currentIdx);
                        if (!selected &&
                            ModelAssetCache::IsHiddenPrimitiveAlias(defs[i].assetPath))
                            continue;
                        if (ImGui::Selectable(defs[i].displayName.c_str(), selected)) {
                            if (vc->modelPath != defs[i].assetPath) {
                                vc->modelPath = defs[i].assetPath;
                                runtimeDirty = true;
                            }
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                {
                    const char* modelExts[] = { ".fbx", ".obj", ".gltf", ".glb" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(modelExts, IM_ARRAYSIZE(modelExts), droppedPath)) {
                        if (vc->modelPath != droppedPath) {
                            vc->modelPath = droppedPath;
                            runtimeDirty = true;
                        }
                    }
                }
                if (currentIdx < 0 && !vc->modelPath.empty())
                    ImGui::TextDisabled("Custom / missing asset: %s", vc->modelPath.c_str());
            }

            bool animate = vc->playAnimation;
            if (ImGui::Checkbox("Animate", &animate)) {
                vc->playAnimation = animate;
                runtimeDirty = true;
            }

            ImGui::Text("Shader");
            {
                const int variantCount = Shader3DVariantCount();
                const bool useSceneDefault = vc->shaderOverride.empty();
                const Shader3DVariant currentVariant = useSceneDefault
                    ? RenderSettings_GetDefaultShader()
                    : Shader3DVariantFromId(vc->shaderOverride);
                const std::string preview = useSceneDefault
                    ? "Use Scene Default"
                    : ShaderVariantPreviewLabel(currentVariant);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##ObjectShader", preview.c_str())) {
                    if (ImGui::Selectable("Use Scene Default", useSceneDefault)) {
                        vc->shaderOverride.clear();
                        runtimeDirty = true;
                    }
                    for (int i = 0; i < variantCount; ++i) {
                        const Shader3DVariant v = Shader3DVariantByIndex(i);
                        if (!Shader3DVariantIsUserVisible(v))
                            continue;
                        const bool selected = !useSceneDefault
                            && (Shader3DVariantId(v) == vc->shaderOverride);
                        if (ImGui::Selectable(Shader3DVariantLabel(v), selected)) {
                            vc->shaderOverride = Shader3DVariantId(v);
                            if (v == Shader3DVariant::SurfaceFX)
                                vc->hasSurfaceFx = true;
                            runtimeDirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (!useSceneDefault) {
                    ImGui::TextDisabled("Scene default: %s",
                        Shader3DVariantLabel(RenderSettings_GetDefaultShader()));
                }

                if (currentVariant == Shader3DVariant::SurfaceFX) {
                    vc->hasSurfaceFx = true;
                    ImGui::Separator();
                    ImGui::Text("Surface FX");

                    char texBuf[256];
                    strncpy_s(texBuf, vc->surfaceFxTexturePath.c_str(), sizeof(texBuf) - 1);
                    texBuf[sizeof(texBuf) - 1] = '\0';
                    ImGui::Text("Detail Texture");
                    if (ImGui::InputText("##SfxTex", texBuf, sizeof(texBuf))) {
                        vc->surfaceFxTexturePath = texBuf;
                        runtimeDirty = true;
                    }
                    {
                        const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                        std::string droppedPath;
                        if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                            vc->surfaceFxTexturePath = droppedPath;
                            runtimeDirty = true;
                        }
                    }

                    if (ImGui::ColorEdit4("Color 1", &vc->surfaceFx.Color1.x))
                        runtimeDirty = true;
                    if (ImGui::ColorEdit4("Color 2", &vc->surfaceFx.Color2.x))
                        runtimeDirty = true;
                    if (ImGui::DragFloat2("Scroll Speed 1", &vc->surfaceFx.ScrollSpeed1.x, 0.001f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat2("Scroll Speed 2", &vc->surfaceFx.ScrollSpeed2.x, 0.001f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("UV Scale", &vc->surfaceFx.UvScale, 0.001f, 0.001f, 10.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Fresnel Power", &vc->surfaceFx.FresnelPower, 0.05f, 0.1f, 16.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Emission Strength", &vc->surfaceFx.EmissionStrength, 0.01f, 0.0f, 5.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Gradient Start", &vc->surfaceFx.GradientStart, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Gradient End", &vc->surfaceFx.GradientEnd, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Edge Fade Start", &vc->surfaceFx.EdgeFadeStart, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Edge Fade End", &vc->surfaceFx.EdgeFadeEnd, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    ImGui::TextDisabled("Detail texture scrolls; not Material normal map.");
                }
            }
            EditorUndo_EndComponentEdit(g_selNode, "visual");
            }

            ImGui::Spacing();

            if (DirectionalLightComponent* dl = g_selNode->GetComponent<DirectionalLightComponent>()) {
                ImGui::Separator();
                ImGui::Text("Directional Light");
                EditorUndo_BeginComponentEdit(g_selNode, "DirectionalLight");
                if (ImGui::Checkbox("Enabled", &dl->enabled))
                    runtimeDirty = true;
                if (ImGui::ColorEdit3("Color", &dl->color.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Intensity", &dl->intensity, 0.05f, 0.0f, 10.0f))
                    runtimeDirty = true;
                if (ImGui::ColorEdit3("Specular", &dl->specColor.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Spec Power", &dl->specPower, 1.0f, 1.0f, 256.0f))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Cast Shadows", &dl->castShadows))
                    runtimeDirty = true;

                if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                    const DirectX::XMFLOAT3 dir = DirectionalLightComponent::ComputeDirectionWS(t);
                    ImGui::Text("Direction: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
                    ImGui::TextDisabled("Rotate Transform to aim light (+Z forward).");
                }
                ImGui::TextDisabled("Shader: Directional (Pixel) on Visual component.");
                EditorUndo_EndComponentEdit(g_selNode, "DirectionalLight");
                ImGui::Spacing();
            }

            if (CameraComponent* cam = g_selNode->GetComponent<CameraComponent>()) {
                ImGui::Separator();
                ImGui::Text("Camera");
                EditorUndo_BeginComponentEdit(g_selNode, "Camera");
                if (ImGui::Checkbox("Enabled", &cam->enabled))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Editor Camera Controls on Play", &cam->debugFly)) runtimeDirty = true;
                if (ImGui::Checkbox("Orthographic", &cam->orthographic))
                    runtimeDirty = true;
                if (cam->orthographic) {
                    if (ImGui::DragFloat("View Height", &cam->orthographicHeight, 0.1f, 0.01f, 100000.0f))
                        runtimeDirty = true;
                }
                else if (ImGui::DragFloat("FOV", &cam->fov, 0.5f, 1.0f, 179.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Near Clip", &cam->nearClip, 0.01f, 0.01f, 100.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Far Clip", &cam->farClip, 1.0f, 1.0f, 100000.0f))
                    runtimeDirty = true;
                if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                    const DirectX::XMFLOAT3 fwd = CameraComponent::ComputeForwardWS(t);
                    const DirectX::XMFLOAT3 up = CameraComponent::ComputeUpWS(t);
                    const DirectX::XMFLOAT3 right = CameraComponent::ComputeRightWS(t);
                    ImGui::Text("Forward: (%.2f, %.2f, %.2f)", fwd.x, fwd.y, fwd.z);
                    ImGui::Text("Up:      (%.2f, %.2f, %.2f)", up.x, up.y, up.z);
                    ImGui::Text("Right:   (%.2f, %.2f, %.2f)", right.x, right.y, right.z);
                    ImGui::TextDisabled("Wire: blue=fwd green=up red=right. Aim with Transform (+Z).");
                    ImGui::TextDisabled("View uses world-up bank (no upside-down past 90 pitch).");
                }
                EditorUndo_EndComponentEdit(g_selNode, "Camera");
                ImGui::Spacing();
            }

            if (PointLightComponent* pl = g_selNode->GetComponent<PointLightComponent>()) {
                ImGui::Separator();
                ImGui::Text("Point Light");
                EditorUndo_BeginComponentEdit(g_selNode, "PointLight");
                if (ImGui::Checkbox("Enabled", &pl->enabled)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Color", &pl->color.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Intensity", &pl->intensity, 0.05f, 0.0f, 10.0f)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Specular", &pl->specColor.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Spec Power", &pl->specPower, 1.0f, 1.0f, 256.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Range", &pl->range, 0.1f, 0.1f, 100.0f)) runtimeDirty = true;
                ImGui::Text("Attenuation (kc, kl, kq)");
                if (ImGui::DragFloat3("##PtAtten", &pl->attenuation.x, 0.01f, 0.0f, 10.0f)) runtimeDirty = true;
                ImGui::TextDisabled("Shader: Point (Pixel) on Visual component.");
                EditorUndo_EndComponentEdit(g_selNode, "PointLight");
                ImGui::Spacing();
            }

            if (SpotLightComponent* sl = g_selNode->GetComponent<SpotLightComponent>()) {
                ImGui::Separator();
                ImGui::Text("Spot Light");
                EditorUndo_BeginComponentEdit(g_selNode, "SpotLight");
                if (ImGui::Checkbox("Enabled", &sl->enabled)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Color", &sl->color.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Intensity", &sl->intensity, 0.05f, 0.0f, 10.0f)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Specular", &sl->specColor.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Spec Power", &sl->specPower, 1.0f, 1.0f, 256.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Range", &sl->range, 0.1f, 0.1f, 100.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Inner Angle", &sl->innerAngleDeg, 0.5f, 0.0f, 89.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Outer Angle", &sl->outerAngleDeg, 0.5f, 0.0f, 89.0f)) runtimeDirty = true;
                if (ImGui::Checkbox("Cast Shadows", &sl->castShadows)) runtimeDirty = true;
                ImGui::Text("Attenuation (kc, kl, kq)");
                if (ImGui::DragFloat3("##SpAtten", &sl->attenuation.x, 0.01f, 0.0f, 10.0f)) runtimeDirty = true;
                if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                    const DirectX::XMFLOAT3 shine = LightGizmo::GetShineDirectionWS(t);
                    ImGui::Text("Direction: (%.2f, %.2f, %.2f)", shine.x, shine.y, shine.z);
                    ImGui::TextDisabled("Rotate Transform to aim spot (+Z forward).");
                }
                ImGui::TextDisabled("Shader: Spot (Pixel) on Visual component.");
                EditorUndo_EndComponentEdit(g_selNode, "SpotLight");
                ImGui::Spacing();
            }

            if (ColliderComponent* cc = FindColliderComponent(g_selNode)) {
                ImGui::Separator();
                const char* colliderTypeName=cc->GetTypeName();
                const char* colliderLabels[]={"Collider (Legacy)","Box Collider","Sphere Collider","Capsule Collider","Mesh Collider","Cylinder Collider"};
                const int colliderIndex=static_cast<int>(cc->colliderType);
                ImGui::TextUnformatted(colliderIndex>=0&&colliderIndex<IM_ARRAYSIZE(colliderLabels)?colliderLabels[colliderIndex]:"Collider (Legacy)");
                EditorUndo_BeginComponentEdit(g_selNode, colliderTypeName);
                auto noteColliderEdit = [&](bool edited) {
                    if (edited)
                        runtimeDirty = true;
                };
                if (cc->colliderType != ColliderType::NONE) {
                    ImGui::Text("Center");
                    ImGui::Text("X:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##CCX", &cc->colliderCenter.x, 0.1f));
                    noteColliderEdit(ScrollValue(&cc->colliderCenter.x));
                    ImGui::Text("Y:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##CCY", &cc->colliderCenter.y, 0.1f));
                    noteColliderEdit(ScrollValue(&cc->colliderCenter.y));
                    ImGui::Text("Z:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##CCZ", &cc->colliderCenter.z, 0.1f));
                    noteColliderEdit(ScrollValue(&cc->colliderCenter.z));
                }
                if (cc->colliderType == ColliderType::BOX) {
                    ImGui::Text("Half Extents");
                    ImGui::Text("X:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##HEX", &cc->halfExtents.x, 0.1f));
                    ImGui::Text("Y:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##HEY", &cc->halfExtents.y, 0.1f));
                    ImGui::Text("Z:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##HEZ", &cc->halfExtents.z, 0.1f));
                }
                else if (cc->colliderType == ColliderType::SPHERE) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##Rad", &cc->radius, 0.1f));
                }
                else if (cc->colliderType == ColliderType::CAPSULE) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CRad", &cc->radius, 0.1f));
                    ImGui::Text("Height:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CHei", &cc->height, 0.1f));
                }
                else if (cc->colliderType == ColliderType::CYLINDER) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CyRad", &cc->radius, 0.1f));
                    ImGui::Text("Height:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CyHei", &cc->height, 0.1f));
                }
                else if (cc->colliderType == ColliderType::MESH) {
                    ImGui::Text("Size Padding:"); ImGui::SameLine(100);
                    noteColliderEdit(ImGui::DragFloat("##MSP", &cc->meshSizePadding, 0.01f, 0.0f, 10.0f));
                }
                if (cc->colliderType == ColliderType::BOX
                    || cc->colliderType == ColliderType::SPHERE
                    || cc->colliderType == ColliderType::CAPSULE
                    || cc->colliderType == ColliderType::CYLINDER) {
                    static GameObject* s_autoFitFailNode = nullptr;
                    if (ImGui::Button("Auto-Fit")) {
                        if (AutoFitColliderToMesh(g_selNode, cc)) {
                            s_autoFitFailNode = nullptr;
                            runtimeDirty = true;
                            ImGui::MarkItemEdited(ImGui::GetItemID());
                        } else {
                            s_autoFitFailNode = g_selNode;
                        }
                    }
                    if (s_autoFitFailNode == g_selNode)
                        ImGui::TextDisabled("Auto-Fit: no usable mesh vertices.");
                }
                if (ImGui::Checkbox("Show Debug Collider", &cc->showDebugCollider)) {
                    g_selNode->m_debugCollider = cc->showDebugCollider;
                    runtimeDirty = true;
                }
                if (ImGui::Checkbox("Is Trigger", &cc->isTrigger)) {
                    runtimeDirty = true;
                }
                {
                    ImGui::Text("Physics Material");
                    char matBuf[256];
                    strncpy_s(matBuf, cc->physicsMaterial.c_str(), sizeof(matBuf) - 1);
                    matBuf[sizeof(matBuf) - 1] = '\0';
                    ImGui::SameLine(140);
                    if (ImGui::InputText("##PhysMat", matBuf, sizeof(matBuf))) {
                        cc->physicsMaterial = matBuf;
                        runtimeDirty = true;
                    }
                    const char* physMatExts[] = { ".physmat" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(physMatExts, IM_ARRAYSIZE(physMatExts), droppedPath)) {
                        cc->physicsMaterial = droppedPath;
                        runtimeDirty = true;
                    }
                    if (!cc->physicsMaterial.empty()) {
                        ImGui::SameLine();
                        if (ImGui::Button("Clear##PhysMat")) {
                            cc->physicsMaterial.clear();
                            runtimeDirty = true;
                        }
                    }
                    if (Collider* col = g_selNode->GetCollider()) {
                        const ContactSurface& surface = col->GetContactSurface();
                        if (surface.assigned) {
                            ImGui::TextDisabled(
                                "Static %.3f  Dynamic %.3f  Restitution %.3f",
                                surface.staticFriction, surface.dynamicFriction, surface.restitution);
                        } else if (!cc->physicsMaterial.empty()) {
                            ImGui::TextDisabled("Not loaded; using rigidbody friction/restitution.");
                        }
                    }
                }
                EditorUndo_EndComponentEdit(g_selNode, colliderTypeName);
                if (ImGui::Button("Remove Collider")) {
                    EditorUndo_PushRemoveComponent(g_selNode, colliderTypeName);
                    RemoveColliderComponent(g_selNode);
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (RigidbodyComponent* rbc = g_selNode->GetComponent<RigidbodyComponent>()) {
                ImGui::Separator();
                ImGui::Text("Rigidbody");
                EditorUndo_BeginComponentEdit(g_selNode, "Rigidbody");
                Rigidbody& rb = rbc->Body();

                bool useGravity = rb.UseGravity();
                if (ImGui::Checkbox("Use Gravity", &useGravity)) {
                    rb.SetUseGravity(useGravity);
                    runtimeDirty = true;
                }

                bool isKinematic = rb.IsKinematic();
                if (ImGui::Checkbox("Is Kinematic", &isKinematic)) {
                    rb.SetIsKinematic(isKinematic);
                    runtimeDirty = true;
                }

                float mass = rb.GetMass();
                if (ImGui::DragFloat("Mass", &mass, 0.05f, 0.001f, 1000.0f)) {
                    rb.SetMass(mass);
                    runtimeDirty = true;
                }

                float drag = rb.GetDrag();
                if (ImGui::DragFloat("Drag", &drag, 0.01f, 0.0f, 50.0f)) {
                    rb.SetDrag(drag);
                    runtimeDirty = true;
                }

                float angularDrag = rb.GetAngularDrag();
                if (ImGui::DragFloat("Angular Drag", &angularDrag, 0.01f, 0.0f, 50.0f)) {
                    rb.SetAngularDrag(angularDrag);
                    runtimeDirty = true;
                }

                float restitution = rb.GetRestitution();
                if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 2.0f)) {
                    rb.SetRestitution(restitution);
                    runtimeDirty = true;
                }

                float friction = rb.GetFriction();
                if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 10.0f)) {
                    rb.SetFriction(friction);
                    runtimeDirty = true;
                }

                EditorUndo_EndComponentEdit(g_selNode, "Rigidbody");
                if (ImGui::Button("Remove Rigidbody")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Rigidbody");
                    g_selNode->RemoveComponent<RigidbodyComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // Material property editor
            if (MaterialComponent* mc = g_selNode->GetComponent<MaterialComponent>()) {
                ImGui::Separator();
                ImGui::Text("Material");
                EditorUndo_BeginComponentEdit(g_selNode, "Material");

                if (ImGui::ColorEdit3("Color", &mc->color.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Roughness", &mc->roughness, 0.01f, 0.0f, 1.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Metallic", &mc->metallic, 0.01f, 0.0f, 1.0f))
                    runtimeDirty = true;

                char nbuf[256];
                strncpy_s(nbuf, mc->normalMapPath.c_str(), sizeof(nbuf) - 1);
                nbuf[sizeof(nbuf) - 1] = '\0';
                ImGui::Text("Normal Map");
                if (ImGui::InputText("##MatNormal", nbuf, sizeof(nbuf))) {
                    mc->normalMapPath = nbuf;
                    runtimeDirty = true;
                }
                {
                    const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                        mc->normalMapPath = droppedPath;
                        runtimeDirty = true;
                    }
                }
                if (ImGui::DragFloat("Normal Strength", &mc->normalStrength, 0.05f, 0.0f, 4.0f))
                    runtimeDirty = true;

                // Toon fields (useful when shader is "toon"; harmless otherwise)
                {
                    VisualComponent* vc = g_selNode->GetComponent<VisualComponent>();
                    const bool isToon = vc && (
                        vc->shaderOverride == "toon"
                        || (vc->shaderOverride.empty()
                            && RenderSettings_GetDefaultShader() == Shader3DVariant::Toon));
                    if (isToon || ImGui::TreeNode("Toon / Outline")) {
                        if (isToon)
                            ImGui::TextDisabled("Toon / Outline");
                        if (ImGui::DragFloat("Shade Steps", &mc->shadeSteps, 0.1f, 1.0f, 8.0f))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Shade Smoothness", &mc->shadeSmoothness, 0.005f, 0.0f, 0.5f))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Spec Threshold", &mc->specularThreshold, 0.01f, 0.0f, 1.0f))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Rim Strength", &mc->rimStrength, 0.01f, 0.0f, 2.0f))
                            runtimeDirty = true;
                        if (ImGui::ColorEdit3("Shadow Tint", &mc->shadowTint.x))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Outline Width", &mc->outlineWidth, 0.0005f, 0.0f, 0.05f, "%.4f"))
                            runtimeDirty = true;
                        if (ImGui::ColorEdit4("Outline Color", &mc->outlineColor.x))
                            runtimeDirty = true;

                        char rbuf[256];
                        strncpy_s(rbuf, mc->rampTexturePath.c_str(), sizeof(rbuf) - 1);
                        rbuf[sizeof(rbuf) - 1] = '\0';
                        ImGui::Text("Ramp Texture (optional)");
                        if (ImGui::InputText("##MatRamp", rbuf, sizeof(rbuf))) {
                            mc->rampTexturePath = rbuf;
                            runtimeDirty = true;
                        }
                        {
                            const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                            std::string droppedPath;
                            if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                                mc->rampTexturePath = droppedPath;
                                runtimeDirty = true;
                            }
                        }
                        if (!isToon)
                            ImGui::TreePop();
                    }
                }

                EditorUndo_EndComponentEdit(g_selNode, "Material");
                if (ImGui::Button("Remove Material")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Material");
                    g_selNode->RemoveComponent<MaterialComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // Effect property editor
            if (EffectComponent* ec = g_selNode->GetComponent<EffectComponent>()) {
                ImGui::Separator();
                ImGui::Text("Effect");
                EditorUndo_BeginComponentEdit(g_selNode, "Effect");

                std::string path = ec->effectPath;
                char buf[256];
                strncpy_s(buf, path.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::Text("Path"); ImGui::SameLine(60);
                if (ImGui::InputText("##EffPath", buf, sizeof(buf))) {
                    ec->SetEffectPath(buf);
                    runtimeDirty = true;
                }
                {
                    const char* effectExts[] = { ".efk", ".efkefc" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(effectExts, IM_ARRAYSIZE(effectExts), droppedPath)) {
                        ec->SetEffectPath(droppedPath.c_str());
                        runtimeDirty = true;
                    }
                }

                if (ImGui::Checkbox("Auto Play", &ec->autoPlay)) {
                    runtimeDirty = true;
                }

                if (ImGui::Checkbox("Auto Destroy", &ec->autoDestroy)) {
                    runtimeDirty = true;
                }

                ImGui::Text("Speed"); ImGui::SameLine(60);
                if (ImGui::SliderFloat("##EffSpeed", &ec->speed, 0.0f, 5.0f)) {
                    runtimeDirty = true;
                }

                if (ImGui::Button("Play")) {
                    ec->Play();
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    ec->Stop();
                }

                EditorUndo_EndComponentEdit(g_selNode, "Effect");
                if (ImGui::Button("Remove Effect")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Effect");
                    g_selNode->RemoveComponent<EffectComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (AudioComponent* ac = g_selNode->GetComponent<AudioComponent>()) {
                ImGui::Separator();
                ImGui::Text("Audio");
                EditorUndo_BeginComponentEdit(g_selNode, "Audio");

                std::string path = ac->clipPath;
                char buf[256];
                strncpy_s(buf, path.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::Text("Clip"); ImGui::SameLine(80);
                if (ImGui::InputText("##AudioClip", buf, sizeof(buf))) {
                    ac->SetClipPath(buf);
                    runtimeDirty = true;
                }
                {
                    const char* wavExts[] = { ".wav" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(wavExts, IM_ARRAYSIZE(wavExts), droppedPath)) {
                        ac->SetClipPath(droppedPath);
                        runtimeDirty = true;
                    }
                }

                const char* busPreview = "SFX";
                if (ac->bus == AudioType::BGM) busPreview = "BGM";
                else if (ac->bus == AudioType::VOICE) busPreview = "Voice";
                ImGui::Text("Bus"); ImGui::SameLine(80);
                if (ImGui::BeginCombo("##AudioBus", busPreview)) {
                    if (ImGui::Selectable("BGM", ac->bus == AudioType::BGM)) {
                        ac->bus = AudioType::BGM;
                        runtimeDirty = true;
                    }
                    if (ImGui::Selectable("SFX", ac->bus == AudioType::SFX)) {
                        ac->bus = AudioType::SFX;
                        runtimeDirty = true;
                    }
                    if (ImGui::Selectable("Voice", ac->bus == AudioType::VOICE)) {
                        ac->bus = AudioType::VOICE;
                        runtimeDirty = true;
                    }
                    ImGui::EndCombo();
                }

                ImGui::Text("Volume"); ImGui::SameLine(80);
                if (ImGui::SliderFloat("##AudioVol", &ac->volume, 0.0f, 1.0f, "%.2f")) {
                    ac->ApplyLiveSettings();
                    runtimeDirty = true;
                }

                if (ImGui::Checkbox("Loop", &ac->loop))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Play On Awake", &ac->autoPlay))
                    runtimeDirty = true;
                if (ImGui::Checkbox("3D", &ac->spatial))
                    runtimeDirty = true;
                if (ac->spatial) {
                    ImGui::Text("Range"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##AudioRange", &ac->distanceScale, 0.5f, 0.1f, 500.0f, "%.1f")) {
                        ac->ApplyLiveSettings();
                        runtimeDirty = true;
                    }
                }

                if (ImGui::Button("Play##Audio"))
                    ac->Play();
                ImGui::SameLine();
                if (ImGui::Button("Stop##Audio"))
                    ac->Stop();

                EditorUndo_EndComponentEdit(g_selNode, "Audio");
                if (ImGui::Button("Remove Audio")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Audio");
                    g_selNode->RemoveComponent<AudioComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (AudioListenerComponent* alc = g_selNode->GetComponent<AudioListenerComponent>()) {
                (void)alc;
                ImGui::Separator();
                ImGui::Text("AudioListener");
                ImGui::TextDisabled("3D ear. Active camera's listener is used.");
                if (ImGui::Button("Remove AudioListener")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "AudioListener");
                    g_selNode->RemoveComponent<AudioListenerComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // Script property editor
            if (ScriptComponent* sc = g_selNode->GetComponent<ScriptComponent>()) {
                Script* script = sc->GetScript();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f),
                    "Script: %s", sc->GetScriptClass().c_str());
                if (script) {
                    EditorUndo_BeginComponentEdit(g_selNode, "Script");
                    nlohmann::json props = sc->GetEditableProperties();
                    if (props.is_object() && !props.empty()) {
                        bool changed = false;
                        for (auto& [key, val] : props.items()) {
                            if (DrawScriptPropertyField(g_selNode, key, val))
                                changed = true;
                        }
                        if (changed) {
                            sc->ApplyProperties(props);
                            runtimeDirty = true;
                        }
                    }
                    else {
                        ImGui::TextDisabled("(no editable properties)");
                    }
                    EditorUndo_EndComponentEdit(g_selNode, "Script");
                }
                else {
                    ImGui::TextDisabled("(script not loaded)");
                }
                if (ImGui::Button("Remove Script")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Script");
                    g_selNode->RemoveComponent<ScriptComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // 2D physics settings are authored before Play; runtime owns the bodies.
            if (auto* body = g_selNode->GetComponent<Rigidbody2DComponent>()) {
                ImGui::Separator();
                ImGui::TextUnformatted("Rigidbody 2D");
                ImGui::PushID("Rigidbody2D");
                ImGui::BeginDisabled(EngineGUI_IsPlaying());
                EditorUndo_BeginComponentEdit(g_selNode, "Rigidbody2D");
                ImGui::PushItemWidth(-1);
                ImGui::TextUnformatted("Body Type");
                int type = static_cast<int>(body->bodyType);
                if (ImGui::Combo("##Type", &type, "Static\0Dynamic\0Kinematic\0")) {
                    body->bodyType = static_cast<BodyType2D>(type);
                    runtimeDirty = true;
                }
                auto number = [&](const char* label, float* value, float minimum, float maximum) {
                    ImGui::TextUnformatted(label);
                    std::string id = std::string("##") + label;
                    if (ImGui::DragFloat(id.c_str(), value, 0.01f, minimum, maximum)) runtimeDirty = true;
                };
                number("Mass", &body->mass, 0.001f, 100000.0f);
                number("Gravity Scale", &body->gravityScale, -100.0f, 100.0f);
                number("Linear Damping", &body->linearDamping, 0.0f, 100.0f);
                ImGui::TextUnformatted("Initial Velocity (XY)");
                if (ImGui::DragFloat2("##Velocity", &body->initialVelocity.x, 0.01f)) runtimeDirty = true;
                if (ImGui::Checkbox("Fixed Rotation", &body->fixedRotation)) runtimeDirty = true;
                if (ImGui::Checkbox("Continuous Collision", &body->bullet)) runtimeDirty = true;
                ImGui::PopItemWidth();
                ImGui::TextWrapped("Dynamic and kinematic bodies must be root objects with uniform positive XY scale. Edit physics settings before Play.");
                EditorUndo_EndComponentEdit(g_selNode, "Rigidbody2D");
                if (ImGui::Button("Remove Rigidbody 2D")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Rigidbody2D");
                    g_selNode->RemoveComponent<Rigidbody2DComponent>();
                    runtimeDirty = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            if (auto* collider = g_selNode->GetComponent<Collider2DComponent>()) {
                ImGui::Separator();
                ImGui::TextUnformatted("Collider 2D");
                ImGui::PushID("Collider2D");
                EditorUndo_BeginComponentEdit(g_selNode, "Collider2D");
                if (ImGui::Checkbox("Show Debug Collider", &collider->showDebugCollider)) runtimeDirty = true;
                EditorUndo_EndComponentEdit(g_selNode, "Collider2D");
                ImGui::BeginDisabled(EngineGUI_IsPlaying());
                EditorUndo_BeginComponentEdit(g_selNode, "Collider2D");
                ImGui::PushItemWidth(-1);
                if (ImGui::Checkbox("Enabled", &collider->enabled)) runtimeDirty = true;
                ImGui::TextUnformatted("Shape");
                int shape = static_cast<int>(collider->shape);
                if (ImGui::Combo("##Shape", &shape, "Box\0Circle\0")) {
                    collider->shape = static_cast<Shape2D>(shape);
                    runtimeDirty = true;
                }
                if (collider->shape == Shape2D::Box) {
                    ImGui::TextUnformatted("Size (XY)");
                    if (ImGui::DragFloat2("##Size", &collider->size.x, 0.01f, 0.001f, 100000.0f)) runtimeDirty = true;
                } else {
                    ImGui::TextUnformatted("Radius");
                    if (ImGui::DragFloat("##Radius", &collider->radius, 0.01f, 0.001f, 100000.0f)) runtimeDirty = true;
                }
                ImGui::TextUnformatted("Offset (XY)");
                if (ImGui::DragFloat2("##Offset", &collider->offset.x, 0.01f)) runtimeDirty = true;
                if (ImGui::Checkbox("Trigger", &collider->isTrigger)) runtimeDirty = true;
                ImGui::TextUnformatted("Friction");
                if (ImGui::DragFloat("##Friction", &collider->friction, 0.01f, 0.0f, 1.0f)) runtimeDirty = true;
                ImGui::TextUnformatted("Restitution");
                if (ImGui::DragFloat("##Restitution", &collider->restitution, 0.01f, 0.0f, 1.0f)) runtimeDirty = true;
                ImGui::TextUnformatted("Category Bits (hex)");
                if (ImGui::InputScalar("##Category", ImGuiDataType_U32, &collider->categoryBits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal)) runtimeDirty = true;
                ImGui::TextUnformatted("Mask Bits (hex)");
                if (ImGui::InputScalar("##Mask", ImGuiDataType_U32, &collider->maskBits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal)) runtimeDirty = true;
                ImGui::PopItemWidth();
                ImGui::TextWrapped("Box size and circle radius use local world units. Use uniform positive XY scale; settings are locked during Play.");
                EditorUndo_EndComponentEdit(g_selNode, "Collider2D");
                if (ImGui::Button("Remove Collider 2D")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Collider2D");
                    g_selNode->RemoveComponent<Collider2DComponent>();
                    runtimeDirty = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }

            if (SpriteRendererComponent* sprite = g_selNode->GetComponent<SpriteRendererComponent>()) {
                ImGui::Separator();
                ImGui::Text("Sprite Renderer");
                ImGui::PushID("SpriteRenderer");
                EditorUndo_BeginComponentEdit(g_selNode, "SpriteRenderer");
                char textureBuf[1024];
                strncpy_s(textureBuf, sprite->texturePath.c_str(), _TRUNCATE);
                if (ImGui::InputText("Texture", textureBuf, sizeof(textureBuf))) {
                    sprite->texturePath = textureBuf;
                    runtimeDirty = true;
                }
                const char* extensions[] = { ".png", ".jpg", ".jpeg", ".bmp" };
                std::string droppedPath;
                if (AcceptAssetPathDrop(extensions, IM_ARRAYSIZE(extensions), droppedPath)) {
                    sprite->texturePath = droppedPath;
                    runtimeDirty = true;
                }
                if (ImGui::DragFloat("Pixels Per Unit", &sprite->pixelsPerUnit, 1.0f, 0.01f, 100000.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat4("Source Rect", &sprite->sourceRect.x, 1.0f, 0.0f, 100000.0f))
                    runtimeDirty = true;
                ImGui::TextDisabled("Source rect: x, y, width, height in pixels. Zero size uses full texture.");
                if(DrawPivotPreset("Pivot",sprite->pivot,sprite)) runtimeDirty=true;
                if (ImGui::ColorEdit4("Tint", &sprite->tint.x)) runtimeDirty = true;
                if (ImGui::Checkbox("Flip X", &sprite->flipX)) runtimeDirty = true;
                if (ImGui::Checkbox("Flip Y", &sprite->flipY)) runtimeDirty = true;
                if (ImGui::InputInt("Sorting Layer", &sprite->sortingLayer)) runtimeDirty = true;
                if (ImGui::InputInt("Order In Layer", &sprite->orderInLayer)) runtimeDirty = true;
                EditorUndo_EndComponentEdit(g_selNode, "SpriteRenderer");
                if (ImGui::Button("Remove Sprite Renderer")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "SpriteRenderer");
                    g_selNode->RemoveComponent<SpriteRendererComponent>();
                    runtimeDirty = true;
                }
                ImGui::PopID();
                ImGui::Spacing();
            }

            if (auto* animator = g_selNode->GetComponent<SpriteAnimatorComponent>()) {
                ImGui::Separator();
                ImGui::Text("Sprite Animator");
                ImGui::PushID("SpriteAnimator");
                EditorUndo_BeginComponentEdit(g_selNode, "SpriteAnimator");
                bool changed = false;
                char defaultName[256];
                strncpy_s(defaultName, animator->defaultClip.c_str(), _TRUNCATE);
                if (ImGui::InputText("Default Clip", defaultName, sizeof(defaultName))) {
                    animator->defaultClip = defaultName;
                    changed = true;
                }
                changed |= ImGui::Checkbox("Auto Play", &animator->autoPlay);
                static char newClipName[256] = "idle";
                ImGui::InputText("New Clip Name", newClipName, sizeof(newClipName));
                if (ImGui::Button("Add Clip") && newClipName[0]) {
                    changed |= animator->clips.emplace(newClipName, SpriteAnimationClip{}).second;
                }
                static char metadataPath[1024] = "";
                static char importName[256] = "idle";
                static bool importFailed = false;
                ImGui::InputText("Sheet Metadata JSON", metadataPath, sizeof(metadataPath));
                const char* jsonExtensions[] = { ".json" };
                std::string metadataDrop;
                if (AcceptAssetPathDrop(jsonExtensions, IM_ARRAYSIZE(jsonExtensions), metadataDrop))
                    strncpy_s(metadataPath, metadataDrop.c_str(), _TRUNCATE);
                ImGui::InputText("Import Clip Name", importName, sizeof(importName));
                if (ImGui::Button("Import Sheet")) {
                    const bool imported = animator->ImportSheet(metadataPath, importName);
                    importFailed = !imported;
                    changed |= imported;
                }
                if (importFailed) ImGui::TextDisabled("Import failed. Check metadata path and frame rectangles.");
                for (auto it = animator->clips.begin(); it != animator->clips.end();) {
                    ImGui::PushID(it->first.c_str());
                    bool removeClip = false;
                    if (ImGui::TreeNode("Clip", "%s", it->first.c_str())) {
                        auto& clip = it->second;
                        char texture[1024];
                        strncpy_s(texture, clip.texture.c_str(), _TRUNCATE);
                        if (ImGui::InputText("Texture", texture, sizeof(texture))) {
                            clip.texture = texture;
                            changed = true;
                        }
                        const char* extensions[] = { ".png", ".jpg", ".jpeg", ".bmp" };
                        std::string droppedPath;
                        if (AcceptAssetPathDrop(extensions, IM_ARRAYSIZE(extensions), droppedPath)) {
                            clip.texture = droppedPath;
                            changed = true;
                        }
                        float fps = static_cast<float>(clip.fps);
                        if (ImGui::DragFloat("FPS", &fps, 0.1f, 0.01f, 1000.0f)) {
                            clip.fps = (std::max)(0.01f, fps);
                            changed = true;
                        }
                        changed |= ImGui::Checkbox("Loop", &clip.loop);
                        for (size_t i = 0; i < clip.frames.size();) {
                            ImGui::PushID(static_cast<int>(i));
                            auto& frame = clip.frames[i];
                            float rect[] = {frame.x, frame.y, frame.w, frame.h};
                            if (ImGui::DragFloat4("Frame XYWH", rect, 1.0f, 0.0f, 100000.0f)) {
                                frame = {rect[0], rect[1], (std::max)(1.0f, rect[2]), (std::max)(1.0f, rect[3])};
                                changed = true;
                            }
                            const bool removeFrame = ImGui::Button("Remove Frame");
                            ImGui::PopID();
                            if (removeFrame) {
                                clip.frames.erase(clip.frames.begin() + i);
                                changed = true;
                            } else ++i;
                        }
                        if (ImGui::Button("Add Frame")) {
                            clip.frames.push_back(SpriteAnimationFrame{});
                            changed = true;
                        }
                        removeClip = ImGui::Button("Remove Clip");
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (removeClip) {
                        if (animator->defaultClip == it->first) animator->defaultClip.clear();
                        it = animator->clips.erase(it);
                        changed = true;
                    } else ++it;
                }
                if (changed) {
                    animator->OnReset();
                    runtimeDirty = true;
                }
                EditorUndo_EndComponentEdit(g_selNode, "SpriteAnimator");
                if (ImGui::Button("Remove Sprite Animator")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "SpriteAnimator");
                    g_selNode->RemoveComponent<SpriteAnimatorComponent>();
                    runtimeDirty = true;
                }
                ImGui::PopID();
                ImGui::Spacing();
            }

            if (BillboardComponent* bbc = g_selNode->GetComponent<BillboardComponent>()) {
                ImGui::Separator();
                ImGui::Text("Billboard");
                EditorUndo_BeginComponentEdit(g_selNode, "Billboard");

                char texBuf[256];
                strncpy_s(texBuf, bbc->texturePath.c_str(), sizeof(texBuf) - 1);
                texBuf[sizeof(texBuf) - 1] = '\0';
                ImGui::Text("Texture");
                if (ImGui::InputText("##BillboardTex", texBuf, sizeof(texBuf))) {
                    bbc->texturePath = texBuf;
                    bbc->InvalidateTextureCache();
                    runtimeDirty = true;
                }
                {
                    const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                        bbc->texturePath = droppedPath;
                        bbc->InvalidateTextureCache();
                        runtimeDirty = true;
                    }
                }
                if (ImGui::DragFloat("Size X", &bbc->sizeX, 0.05f, 0.01f, 1000.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Size Y", &bbc->sizeY, 0.05f, 0.01f, 1000.0f))
                    runtimeDirty = true;
                if (ImGui::ColorEdit4("Color##Billboard", &bbc->color.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Rotation##Billboard", &bbc->rotation, 0.5f))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Ground Aligned", &bbc->groundAligned))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Enabled##Billboard", &bbc->enabled))
                    runtimeDirty = true;

                EditorUndo_EndComponentEdit(g_selNode, "Billboard");
                if (ImGui::Button("Remove Billboard")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Billboard");
                    g_selNode->RemoveComponent<BillboardComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            ImGui::Separator();
            ImGui::Text("Components:");
            for (const auto& kv : g_selNode->GetComponents())
                ImGui::BulletText("%s", kv.second->GetTypeName());
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##AddComponent", "Add Component...", ImGuiComboFlags_HeightLarge)) {
                static ImGuiTextFilter componentFilter;
                const bool opening=ImGui::IsWindowAppearing();
                if(opening) {componentFilter.Clear();ImGui::SetKeyboardFocusHere();}
                componentFilter.Draw("##ComponentSearch", -1);
                ImGui::Separator();
                int availableCount=0;
                auto option=[&](const char* label,const char* typeName,bool available,auto create) {
                    if(!available||!componentFilter.PassFilter(label))return;
                    ++availableCount;
                    if(ImGui::Selectable(label)) {
                        create();
                        EditorUndo_PushAddComponent(g_selNode,typeName);
                        runtimeDirty=true;
                        ImGui::CloseCurrentPopup();
                    }
                };
                option("Transform","SceneTransform",!g_selNode->GetComponent<engine::Transform>(),[&] {g_selNode->AddComponent<engine::Transform>();});
                option("Visual","visual",!g_selNode->GetComponent<VisualComponent>(),[&] {g_selNode->AddComponent<VisualComponent>();});
                option("Sprite Renderer","SpriteRenderer",!g_selNode->GetComponent<SpriteRendererComponent>(),[&] {g_selNode->AddComponent<SpriteRendererComponent>();});
                option("Sprite Animator","SpriteAnimator",!g_selNode->GetComponent<SpriteAnimatorComponent>(),[&] {g_selNode->AddComponent<SpriteAnimatorComponent>();});
                option("Billboard","Billboard",!g_selNode->GetComponent<BillboardComponent>(),[&] {g_selNode->AddComponent<BillboardComponent>();});
                option("Material","Material",!g_selNode->GetComponent<MaterialComponent>(),[&] {g_selNode->AddComponent<MaterialComponent>();});
                option("Camera","Camera",!g_selNode->GetComponent<CameraComponent>(),[&] {g_selNode->AddComponent<CameraComponent>();});
                option("Directional Light","DirectionalLight",!g_selNode->GetComponent<DirectionalLightComponent>(),[&] {g_selNode->AddComponent<DirectionalLightComponent>();});
                option("Point Light","PointLight",!g_selNode->GetComponent<PointLightComponent>(),[&] {g_selNode->AddComponent<PointLightComponent>();});
                option("Spot Light","SpotLight",!g_selNode->GetComponent<SpotLightComponent>(),[&] {g_selNode->AddComponent<SpotLightComponent>();});
                option("Rigidbody 3D","Rigidbody",!g_selNode->GetComponent<RigidbodyComponent>(),[&] {g_selNode->AddComponent<RigidbodyComponent>();});
                option("Rigidbody 2D","Rigidbody2D",!g_selNode->GetComponent<Rigidbody2DComponent>(),[&] {g_selNode->AddComponent<Rigidbody2DComponent>();});
                option("Script","Script",!g_selNode->GetComponent<ScriptComponent>(),[&] {g_selNode->AddComponent<ScriptComponent>();});
                option("Effect","Effect",!g_selNode->GetComponent<EffectComponent>(),[&] {g_selNode->AddComponent<EffectComponent>();});
                option("Audio","Audio",!g_selNode->GetComponent<AudioComponent>(),[&] {g_selNode->AddComponent<AudioComponent>();});
                option("Audio Listener","AudioListener",!g_selNode->GetComponent<AudioListenerComponent>(),[&] {g_selNode->AddComponent<AudioListenerComponent>();});
                const auto* currentCollider=FindColliderComponent(g_selNode);
                const bool canAdd3D=!currentCollider||currentCollider->colliderType==ColliderType::NONE;
                auto removeEmptyCollider=[&] {if(currentCollider)RemoveColliderComponent(g_selNode);};
                option("Box Collider 3D","BoxCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<BoxColliderComponent>();});
                option("Sphere Collider 3D","SphereCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<SphereColliderComponent>();});
                option("Capsule Collider 3D","CapsuleCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<CapsuleColliderComponent>();});
                option("Cylinder Collider 3D","CylinderCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<CylinderColliderComponent>();});
                option("Mesh Collider 3D","MeshCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<MeshColliderComponent>();});
                option("Box Collider 2D","Collider2D",!g_selNode->GetComponent<Collider2DComponent>(),[&] {g_selNode->AddComponent<Collider2DComponent>()->shape=Shape2D::Box;});
                option("Circle Collider 2D","Collider2D",!g_selNode->GetComponent<Collider2DComponent>(),[&] {g_selNode->AddComponent<Collider2DComponent>()->shape=Shape2D::Circle;});
                if(!availableCount)ImGui::TextDisabled("No matching components available.");
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            if (runtimeDirty) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }

            // Save as prefab
            ImGui::Separator();
            if (ImGui::Button("Save as Prefab", ImVec2(-1, 0))) {
                if (!fs::exists(PREFAB_DIR)) fs::create_directories(PREFAB_DIR);
                std::string prefabName = g_selNode->GetName();
                for (char& c : prefabName)
                    if (c == ' ' || c == '/' || c == '\\') c = '_';
                PrefabSerializer::SaveToFile(*g_selNode, PREFAB_DIR + prefabName + ".json");
            }

            ImGui::Separator();
            if (ImGui::Button("Delete Node", ImVec2(-1, 30))) {
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = nullptr;
                DeleteGameObject(g_selNode);
            }
        }
        else if (g_selUiElement) {
            EditorUndo_BeginUiEdit(g_selUiElement);
            EngineGUIUI::DrawCanvasElementProperties(g_selUiElement);
            if (g_selUiElement)
                EditorUndo_EndUiEdit(g_selUiElement);
        }
        else {
            DrawPropertiesEmptyState();
        }

        if (playVisuals)
            ImGui::EndDisabled();
    }
    ImGui::End();

    // --- Asset Browser (bottom) ---
    ImGui::SetNextWindowPos(ImVec2(0, screenH - bottomHeight));
    ImGui::SetNextWindowSize(ImVec2(screenW, bottomHeight));
    ImGui::Begin("Asset Browser", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (sceneLoadBusy || playVisuals)
            ImGui::BeginDisabled(true);
        if (GameObject* spawned = MapEditor::ProjectBrowserPanel(g_sceneNodes)) {
            if (!sceneLoadBusy && !playVisuals) {
                g_selNode = spawned;
                g_selUiElement = nullptr;
                const int idx = [&]() {
                    for (int i = 0; i < (int)g_sceneNodes.size(); ++i)
                        if (g_sceneNodes[i].get() == spawned) return i;
                    return (int)g_sceneNodes.size() - 1;
                }();
                EditorUndo_PushCreate(spawned, nullptr, idx);
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        if (sceneLoadBusy || playVisuals)
            ImGui::EndDisabled();
    }
    ImGui::End();

    // Custom viewport target does not add an ImGui window over the scene.
    float dropX,dropY,dropW,dropH;
    EngineGUI_GetGameViewportRect(dropX,dropY,dropW,dropH);
    const ImRect dropRect(ImVec2(dropX,dropY),ImVec2(dropX+dropW,dropY+dropH));
    if(!sceneLoadBusy && !playVisuals && g_editMode==EditorEditMode::Scene3D
        && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
        && ImGui::BeginDragDropTargetViewport(ImGui::GetMainViewport(),&dropRect)) {
        if(const auto* payload=ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const std::string path=static_cast<const char*>(payload->Data);
            std::string ext=fs::path(path).extension().string();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(ext==".png" || ext==".jpg" || ext==".jpeg" || ext==".bmp") {
                const auto mouse=ImGui::GetIO().MousePos;
                DirectX::XMVECTOR origin,direction;
                if(BuildPickRay(int(mouse.x),int(mouse.y),origin,direction)) {
                    const float dz=DirectX::XMVectorGetZ(direction);
                    const float distance=std::fabs(dz)>1e-6f?-DirectX::XMVectorGetZ(origin)/dz:-1;
                    if(distance>=0) {
                        auto node=std::make_unique<GameObject>(fs::path(path).stem().string());
                        auto* transform=node->AddComponent<engine::Transform>();
                        DirectX::XMStoreFloat3(&transform->localPosition,DirectX::XMVectorMultiplyAdd(direction,DirectX::XMVectorReplicate(distance),origin));
                        node->AddComponent<SpriteRendererComponent>()->texturePath=path;
                        SpawnRootGameObject(std::move(node));
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    float gizmoX,gizmoY,gizmoW,gizmoH;
    EngineGUI_GetGameViewportRect(gizmoX,gizmoY,gizmoW,gizmoH);
    auto* gizmoLayer=ImGui::GetBackgroundDrawList();
    gizmoLayer->PushClipRect(ImVec2(gizmoX,gizmoY),ImVec2(gizmoX+gizmoW,gizmoY+gizmoH),true);
    if(!playVisuals && DebugCamera_Is2DView() && Grid_IsVisible() && gizmoH>0) {
        const auto& camera=GetCamera();
        const float units=camera.orthographicHeight/gizmoH;
        float step=std::pow(10.0f,std::floor(std::log10((std::max)(0.0001f,units*50.0f))));
        if(step/units<25) step*=5;
        const float centerX=camera.position.x, centerY=camera.position.y;
        const float halfW=gizmoW*units*0.5f, halfH=gizmoH*units*0.5f;
        for(float x=std::floor((centerX-halfW)/step)*step;x<centerX+halfW;x+=step) {
            const float px=gizmoX+gizmoW*0.5f+(x-centerX)/units;
            gizmoLayer->AddLine(ImVec2(px,gizmoY),ImVec2(px,gizmoY+gizmoH),IM_COL32(150,150,150,45));
        }
        for(float y=std::floor((centerY-halfH)/step)*step;y<centerY+halfH;y+=step) {
            const float py=gizmoY+gizmoH*0.5f-(y-centerY)/units;
            gizmoLayer->AddLine(ImVec2(gizmoX,py),ImVec2(gizmoX+gizmoW,py),IM_COL32(150,150,150,45));
        }
    }
    if (!sceneLoadBusy && !playVisuals && g_editMode == EditorEditMode::Scene3D) {
        TransformGizmo::HandleHotkeys();
        if (g_selNode)
            TransformGizmo::Draw(g_selNode);
        if(!DebugCamera_Is2DView()) CameraOrientationGizmo::Draw();
    }

    if (!sceneLoadBusy && !playVisuals && g_editMode == EditorEditMode::UI
        && g_selUiElement && g_showUiOverlay) {
        UiGizmo::Draw(g_selUiElement);
    }

    gizmoLayer->PopClipRect();

    JobSystem_DebugDraw();

    // Whole-screen modal dim (same path as UnsavedChangesPopup) ? not
    // BackgroundDrawList, which sits under opaque panel windows.
    if (showSceneLoadModal) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);
        const ImGuiWindowFlags loadFlags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;
        if (ImGui::BeginPopupModal("SceneAssetPreloadModal", nullptr, loadFlags)) {
            g_loadingDrawn = true;
            ImGui::TextUnformatted(g_loadingStage);
            if (g_sceneLoadPending && g_scenePreparation && !SceneLoader_PreparationReading(*g_scenePreparation) && !g_sceneReadyToApply) {
                const auto completed = SceneLoader_PreparationCompleted(*g_scenePreparation);
                const auto total = SceneLoader_PreparationTotal(*g_scenePreparation);
                const std::string label = std::to_string(completed) + " / " + std::to_string(total);
                ImGui::ProgressBar(total ? float(completed)/float(total) : 1.0f, ImVec2(-1.0f, 0.0f), label.c_str());
            } else if (g_sceneLoadPending) {
                const char* dots[] = {"Working", "Working.", "Working..", "Working..."};
                ImGui::TextUnformatted(dots[int(ImGui::GetTime()*3.0) % 4]);
            }
            if (preloadActive) {
                const std::string& label = g_sceneAssetPreloader.CurrentLabel();
                if (!label.empty())
                    ImGui::TextUnformatted(label.c_str());
                ImGui::Spacing();
                ImGui::ProgressBar(g_sceneAssetPreloader.Progress01(), ImVec2(-1.0f, 0.0f));
            }
            ImGui::EndPopup();
        }
    }

    if (!sceneLoadBusy && ImGui::BeginPopupModal("SceneAssetPreloadModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::CloseCurrentPopup(); ImGui::EndPopup();
    }
    if (!g_sceneLoadError.empty()) ImGui::OpenPopup("Scene load failed");
    // AlwaysAutoResize plus TextWrapped collapses to one character if the wrap
    // width is the window itself. Pin a minimum width before the text wraps.
    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 0.0f), ImVec2(720.0f, 4096.0f));
    if (ImGui::BeginPopupModal("Scene load failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (g_sceneLoadError.empty()) ImGui::CloseCurrentPopup();
        else {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 420.0f);
            ImGui::TextWrapped("%s", g_sceneLoadError.c_str());
            ImGui::PopTextWrapPos();
        }
        if (ImGui::Button("OK")) { g_sceneLoadError.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // Abandon unfinished inspector component-edit capture when no widget is active
    // (e.g. user clicked the viewport mid-drag without DeactivatedAfterEdit).
    if (!ImGui::IsAnyItemActive()) {
        EditorUndo_DiscardComponentEdit();
        EditorUndo_DiscardUiEdit();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::GetIO().WantCaptureMouse)
        Mouse_ClearWheel();

    ConsumePendingFolderPick();
}

#ifdef _DEBUG
#include "tools/scene_hot_load_check.h"
#endif
