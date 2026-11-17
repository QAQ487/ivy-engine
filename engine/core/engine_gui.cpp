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
#include "engine/editor/editor_gui_internal.h"
#include "engine/editor/editor_layout.h"
#include "engine/editor/editor_docking.h"
#include "engine/editor/editor_docking_math.h"
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
#include "engine/editor/editor_icon_button.h"
#include "engine/editor/editor_theme.h"
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
bool        g_enabled = true;
bool              g_isPlaying = false;
static std::atomic<bool> g_exitGameRequested{false};
PlayMode    g_playMode = PlayMode::None;
PlayMode    g_pendingPlayMode = PlayMode::Viewport;
bool        g_holdPlayVisuals = false;
bool        g_isOnStart = true;
bool        g_imguiInitialized = false;

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
bool IsSceneLoadBusy();
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

CameraMode  g_cameraMode = CameraMode::SceneCamera;
std::string g_activeCameraNode;
float g_scrollSensitivity = 1.0f;
char  g_newSceneName[128] = "new_scene";
char  g_newTagName[64] = "";
bool  g_showPostFxShowcase = false;
bool  g_showShortcutHints = false;
bool  g_showNewScenePopup = false;
bool  g_showAddTagPopup = false;
bool  g_showDeleteTagPopup = false;
int   g_pendingDeleteTagId = -1;
bool  g_showUiOverlay = true;
bool g_newProject2D=false;
bool  g_showNewProjectPopup = false;
char  g_newProjectName[128] = "NewProject";
std::string g_newProjectParentDir;
std::string g_projectStatusMessage;
PendingFolderPickAction g_pendingFolderPick = PendingFolderPickAction::None;
bool g_showBuildUnsavedPopup = false;
bool g_showBuildPreflight = false;
bool g_showBuildLog = false;
char g_buildDest[1024] = {};
int g_buildConfigIndex = 0;
EditorEditMode g_editMode = EditorEditMode::Scene3D;

// Toolbar chrome uses compact local padding (see PushToolbarChromeStyle) so
// FontSizeBase 15.5/16 + theme FramePadding no longer clip in a fixed 36px row.

int g_uiZoomPercent = 100;
float g_uiZoom = 1.0f;
float g_uiDpiScale = 1.0f;
ImGuiStyle g_baseStyle{};
bool g_baseStyleCaptured = false;
editor_theme::StylePreset g_uiStylePreset = editor_theme::StylePreset::Trellis;
editor_theme::ColorScheme g_uiColorScheme = editor_theme::ColorScheme::Ivy;


bool IsValidUiZoomPercent(int percent) {
    for (int p : kUiZoomPercents) {
        if (p == percent) return true;
    }
    return false;
}

float QueryEditorDpiScale() {
    if (!g_imguiInitialized)
        return 1.0f;
    if (const ImGuiViewport* vp = ImGui::GetMainViewport()) {
        if (vp->DpiScale > 0.0f && std::isfinite(vp->DpiScale))
            return NormalizeDpiScale(vp->DpiScale);
    }
    // Fallback when viewport DPI is not yet populated.
    if (HWND hwnd = GetWindowHandle()) {
        const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        return NormalizeDpiScale(dpi);
    }
    return 1.0f;
}

void CaptureBaseStyleFromCurrent() {
    g_baseStyle = ImGui::GetStyle();
    // Base must stay unscaled so repeated ApplyUiZoom never compounds.
    g_baseStyle.FontScaleMain = 1.0f;
    g_baseStyle.FontScaleDpi = 1.0f;
    g_baseStyleCaptured = true;
}

void ApplyEditorUiScaleFromBase() {
    if (!g_imguiInitialized)
        return;
    if (!g_baseStyleCaptured)
        CaptureBaseStyleFromCurrent();

    g_uiDpiScale = QueryEditorDpiScale();
    // Single effective scale = user UI Scale × monitor DPI (never compounded).
    g_uiZoom = ComputeEffectiveUiScale(g_uiZoomPercent, g_uiDpiScale);

    ImGuiStyle& style = ImGui::GetStyle();
    style = g_baseStyle;
    style.ScaleAllSizes(g_uiZoom);
    // ImGui 1.92 dynamic atlas re-rasterizes from FontScaleMain — keep crisp.
    // DPI is folded into g_uiZoom; keep FontScaleDpi at 1 to avoid double-count.
    style.FontScaleMain = g_uiZoom;
    style.FontScaleDpi = 1.0f;
    // Keep the mouse cursor visible/usable at 50%/75% UI scale.
    if (style.MouseCursorScale < 0.75f)
        style.MouseCursorScale = 0.75f;
}

void ApplyEditorTheme(editor_theme::StylePreset preset,
                             editor_theme::ColorScheme scheme) {
    g_uiStylePreset = editor_theme::ClampStylePreset(static_cast<int>(preset));
    g_uiColorScheme = editor_theme::ClampColorScheme(static_cast<int>(scheme));
    if (!g_imguiInitialized)
        return;
    // Reset to unscaled theme geometry+colors, then apply effective scale once.
    editor_theme::Apply(ImGui::GetStyle(), g_uiStylePreset, g_uiColorScheme);
    CaptureBaseStyleFromCurrent();
    ApplyEditorUiScaleFromBase();
}

void ApplyUiZoom(int percent) {
    // Discrete radio list is authoritative; unknown values fall back to 100%.
    if (!IsValidUiZoomPercent(percent))
        percent = 100;
    g_uiZoomPercent = percent;
    if (!g_imguiInitialized) {
        g_uiZoom = ComputeEffectiveUiScale(percent, 1.0f);
        return;
    }

    // Scale from the last unscaled theme base (never from an already-scaled style).
    if (!g_baseStyleCaptured) {
        editor_theme::Apply(ImGui::GetStyle(), g_uiStylePreset, g_uiColorScheme);
        CaptureBaseStyleFromCurrent();
    }
    ApplyEditorUiScaleFromBase();
}

void SaveUiPrefs() {
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
        j["uiStylePreset"] = editor_theme::StylePresetName(g_uiStylePreset);
        j["uiColorScheme"] = editor_theme::ColorSchemeName(g_uiColorScheme);
        PlayFpsOverlay::WritePrefs(j);
        EditorConsole_WritePrefs(j);
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

void LoadUiPrefs() {
    PlayFpsOverlay::SetEnabled(false);
    g_uiZoomPercent = 100;
    g_uiZoom = 1.0f;
    g_uiStylePreset = editor_theme::StylePreset::Trellis;
    g_uiColorScheme = editor_theme::ColorScheme::Ivy;
    try {
        std::ifstream file(content_path::ReadableFileCurrent(kUiPrefsPath));
        if (!file.is_open()) return;
        json j;
        file >> j;
        PlayFpsOverlay::LoadPrefs(j);
        EditorConsole_LoadPrefs(j);
        const int percent = j.value("uiZoomPercent", 100);
        if (IsValidUiZoomPercent(percent)) {
            g_uiZoomPercent = percent;
            g_uiZoom = ComputeEffectiveUiScale(percent, 1.0f);
        }
        g_uiStylePreset = editor_theme::ParseStylePreset(
            j.value("uiStylePreset", std::string(editor_theme::kDefaultPresetName)).c_str());
        g_uiColorScheme = editor_theme::ParseColorScheme(
            j.value("uiColorScheme", std::string(editor_theme::kDefaultSchemeName)).c_str());
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

PendingEditorAction g_pendingEditorAction = PendingEditorAction::None;
std::string g_pendingEditorSceneFile;
std::string g_pendingProjectPath;
std::string g_pendingCreateProjectParent;
std::string g_pendingCreateProjectName;
bool g_showUnsavedChangesPopup = false;

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


ReloadLight g_sceneReloadLight, g_scriptReloadLight;
unsigned long long g_scriptFeedbackRevision = 0;
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
ContentFileMonitor g_contentMonitor;
void EngineGUI_RequestContentRefresh() { g_contentMonitor.RefreshSoon(); }
std::vector<ContentFileMonitor::Change> g_contentChanges;
ReloadLight g_contentReloadLight;
std::string g_contentReloadStatus = "Watching project content";
bool g_contentSceneReload = false;
static PlayMode g_contentPlayMode = PlayMode::None;
SceneFileMonitor g_sceneFileMonitor;
static unsigned long long g_monitoredSceneRevision = 0;
std::optional<SceneFileMonitor::Version> g_sceneReloadConflict;
static std::optional<SceneFileMonitor::Version> g_pendingSceneHotLoad;
std::string g_sceneReloadStatus = "Watching current scene JSON";
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
void ResolveSceneReloadConflict(bool loadDisk) {
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
std::string g_scriptReloadStatus = "Script DLL reload: not initialized";

void SaveScene();
void SaveSceneToFile(const std::string& sceneFile);
void NewScene();
void SaveProject();
void ApplyProjectSessionState();
bool SwitchToOpenedProject();
void RequestOpenProject(const std::string& rootPath);
void RequestCloseProject();
void CloseOpenedProject();
void BeginNewProjectFlow();
void BeginOpenProjectFlow();
void OpenBuildPreflight();
void RequestBuildCurrentProject();
void ConsumePendingFolderPick();

void RemapNodeTagsAfterDelete(GameObject* node, int deletedId) {
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

void RemapSceneTagsAfterDelete(int deletedId) {
    for (auto& root : SceneLoader_GetSceneNodes())
        RemapNodeTagsAfterDelete(root.get(), deletedId);
}


std::string TrimString(const std::string& value) {
    const char* whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) return "";
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

std::string NormalizeSceneFileName(const std::string& sceneName) {
    std::string trimmed = TrimString(sceneName);
    if (trimmed.empty()) return "";

    for (char& c : trimmed) {
        if (c == '\\') c = '/';
    }

    fs::path path = PathFromUtf8(trimmed);
    if (path.extension().empty())
        path += ".json";

    return PathToUtf8(path);
}

bool EngineGUI_IsSceneGraphObject(GameObject* obj) {
    return SceneLoader_IsSceneGraphObject(obj);
}


std::unique_ptr<GameObject> CreateEmptyGameObject() {
    auto node = std::make_unique<GameObject>("GameObject");
    node->AddComponent<engine::Transform>();
    node->AddComponent<VisualComponent>();
    return node;
}

std::unique_ptr<GameObject> CreateDirectionalLightObject() {
    auto node = std::make_unique<GameObject>("Directional Light");
    auto* t = node->AddComponent<engine::Transform>();
    t->localRotation = { 50.0f, -30.0f, 0.0f };
    node->AddComponent<DirectionalLightComponent>();
    node->AddComponent<VisualComponent>();
    return node;
}

std::unique_ptr<GameObject> CreatePointLightObject() {
    auto node = std::make_unique<GameObject>("Point Light");
    auto* t = node->AddComponent<engine::Transform>();
    t->localPosition = { 0.0f, 8.0f, 0.0f };
    node->AddComponent<PointLightComponent>();
    node->AddComponent<VisualComponent>();
    return node;
}

std::unique_ptr<GameObject> CreateSpotLightObject() {
    auto node = std::make_unique<GameObject>("Spot Light");
    auto* t = node->AddComponent<engine::Transform>();
    t->localRotation = { 45.0f, 0.0f, 0.0f };
    node->AddComponent<SpotLightComponent>();
    node->AddComponent<VisualComponent>();
    return node;
}

std::unique_ptr<GameObject> CreateCameraObject(bool debugFly) {
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

void SpawnRootGameObject(std::unique_ptr<GameObject> node) {
    if (!node) return;
    GameObject* raw = node.get();
    g_sceneNodes.push_back(std::move(node));
    g_selNode = raw;
    g_selUiElement = nullptr;
    EditorUndo_PushCreate(raw, nullptr, static_cast<int>(g_sceneNodes.size()) - 1);
    EngineGUI_MarkSceneDirty();
    SyncSceneRuntime();
}

void SpawnChildGameObject(GameObject* parent) {
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

std::unique_ptr<GameObject> DuplicateGameObject(const GameObject& src) {
    return PrefabSerializer::Deserialize(PrefabSerializer::Serialize(src));
}

void SpawnDuplicate(GameObject* source) {
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

void DeleteGameObject(GameObject* toDelete, bool recordUndo) {
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

void ClearScene() {
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

void ApplyProjectSessionState() {
    auto& project = ProjectManager::Instance();
    if (!project.IsOpen())
        return;

    PlayFpsOverlay::LoadFromFile();

    {
        const auto preset = editor_theme::ParseStylePreset(
            project.GetEditorUiStylePreset().c_str());
        const auto scheme = editor_theme::ParseColorScheme(
            project.GetEditorUiColorScheme().c_str());
        // Persist canonical names so invalid prefs do not round-trip.
        project.SetEditorUiStylePreset(editor_theme::StylePresetName(preset));
        project.SetEditorUiColorScheme(editor_theme::ColorSchemeName(scheme));
        ApplyEditorTheme(preset, scheme);
    }
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

bool SwitchToOpenedProject() {
    if (!ProjectManager::Instance().IsOpen())
        return false;

    if (g_isPlaying)
        ReturnToStart();

    ResetSceneHotLoad();
    ApplyProjectSessionState();
    EditorDocking_OnProjectOpened(ProjectManager::Instance().GetRootPath());

    std::string scenePath = ProjectManager::Instance().ResolveSceneToLoad();
    if (scenePath.empty())
        scenePath = EngineGUI_GetDefaultScenePath();

    if (!scenePath.empty())
        EngineGUI_LoadFromFileCore(scenePath.c_str());

    ProjectManager::Instance().UpdateWindowTitle(GetWindowHandle());
    g_projectStatusMessage.clear();
    return true;
}

void RequestOpenTestbed() {
    const std::string path = ProjectManager::Instance().ResolveInRepoTestbedPath();
    if (path.empty()) {
        MessageBoxA(GetWindowHandle(),
                    "The in-repo testbed folder was not found next to the engine.",
                    "Open Testbed", MB_OK | MB_ICONWARNING);
        return;
    }
    RequestOpenProject(path);
}

void RequestOpenProject(const std::string& rootPath) {
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

void CloseOpenedProject() {
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
    EditorDocking_OnProjectClosed();
    ProjectManager::Instance().CloseProject();
    ProjectManager::Instance().UpdateWindowTitle(GetWindowHandle());
    g_projectStatusMessage.clear();
}

void RequestCloseProject() {
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

void BeginOpenProjectFlow() {
    std::string folder;
    if (!ProjectManager::PickFolder(GetWindowHandle(), L"Open Project", &folder))
        return;
    RequestOpenProject(folder);
}

void SetBuildDestBuffer(const std::string& path) {
    strncpy_s(g_buildDest, path.c_str(), _TRUNCATE);
}

void OpenBuildPreflight() {
    auto& project = ProjectManager::Instance();
    if (!project.IsOpen())
        return;

    PlayablePackage::Prefs prefs;
    PlayablePackage::LoadPrefs(project.GetRootPath(), &prefs);
    SetBuildDestBuffer(prefs.destination);
    g_buildConfigIndex = (prefs.configuration == PlayablePackage::Configuration::Release) ? 1 : 0;
    g_showBuildPreflight = true;
}

void RequestBuildCurrentProject() {
    if (!ProjectManager::Instance().IsOpen() || PlayablePackage::IsBuildRunning())
        return;

    if (EngineGUI_IsSceneDirty()) {
        g_showBuildUnsavedPopup = true;
        return;
    }

    OpenBuildPreflight();
}

void BeginNewProjectFlow() {
    std::string folder;
    if (!ProjectManager::PickFolder(GetWindowHandle(), L"Select Parent Folder for New Project",
                                    &folder)) {
        return;
    }
    g_newProjectParentDir = folder;
    snprintf(g_newProjectName, sizeof(g_newProjectName), "NewProject");
    g_showNewProjectPopup = true;
}

void SaveProject() {
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
    project.SetEditorUiStylePreset(editor_theme::StylePresetName(g_uiStylePreset));
    project.SetEditorUiColorScheme(editor_theme::ColorSchemeName(g_uiColorScheme));
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

void ConsumePendingFolderPick() {
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


void SaveSceneToFile(const std::string& sceneFile) {
    if (sceneFile.empty()) return;

    const std::string normalized = NormalizeSceneFileName(sceneFile);
    if (normalized.empty()) return;

    if (!fs::exists(SCENE_DIR)) fs::create_directories(SCENE_DIR);
    const fs::path scenePath = fs::path(SCENE_DIR) / PathFromUtf8(normalized);
    if (scenePath.has_parent_path())
        fs::create_directories(scenePath.parent_path());

    if (EngineGUI_SaveToFile(PathToUtf8(scenePath).c_str())) {
        SceneLoader_CopySceneFileName(normalized);
        SceneLoader_RefreshSceneAssets();
        EngineGUI_ClearSceneDirty();
    }
}

void SaveScene() {
    const std::string sceneFile = NormalizeSceneFileName(SceneLoader_GetSaveFileName());
    if (sceneFile.empty()) return;

    SaveSceneToFile(sceneFile);
}

void NewScene() {
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

void StartPlay(PlayMode mode) {
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
    EditorConsole_OnPlayStarted();
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
        ProjectManager::Instance().SetLastOpenedScene(
            PathToUtf8(PathFromUtf8(path).filename()));
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

bool IsSceneLoadBusy() {
    return g_sceneAssetPreloader.IsActive() || g_sceneLoadPending;
}

// --- Public API ---

void EngineGUI_Initialize(HWND hwnd, ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    // Install early so Bootstrap / script-load diagnostics reach the Console.
    EditorConsole_InstallLogHook();

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
    EditorDocking_ConfigureImGuiIO();
    editor_theme::LoadFonts(io, nullptr);

    ImGui_ImplWin32_EnableDpiAwareness();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);
    g_imguiInitialized = true;

    LoadUiPrefs();
    if (ProjectManager::Instance().IsOpen()) {
        g_uiStylePreset = editor_theme::ParseStylePreset(
            ProjectManager::Instance().GetEditorUiStylePreset().c_str());
        g_uiColorScheme = editor_theme::ParseColorScheme(
            ProjectManager::Instance().GetEditorUiColorScheme().c_str());
        const int zoom = ProjectManager::Instance().GetEditorUiZoomPercent();
        if (IsValidUiZoomPercent(zoom))
            g_uiZoomPercent = zoom;
        EditorDocking_OnProjectOpened(ProjectManager::Instance().GetRootPath());
    }
    ApplyEditorTheme(g_uiStylePreset, g_uiColorScheme); // includes scale apply

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

    // Hook is installed before ImGui init — always clear it.
    EditorConsole_RemoveLogHook();

    if (g_imguiInitialized) {
        EditorDocking_OnProjectClosed();
        EditorDocking_BeginFrame(); // apply deferred save before DestroyContext
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
    const float clientW = GetClientWidth();
    const float clientH = GetClientHeight();

    // Shipped runtime (!g_enabled and not F3): full backbuffer.
    // Editor edit + F2 use the dock central node; F3 uses the full client area.
    // In all editor cases the game rect is the largest centered 16:9 fit.
    if (!g_enabled && g_playMode != PlayMode::Fullscreen) {
        x = 0.0f;
        y = 0.0f;
        width = (std::max)(1.0f, screenW);
        height = (std::max)(1.0f, screenH);
        return;
    }

    EditorClientRect bounds{};
    if (g_playMode == PlayMode::Fullscreen) {
        bounds.x = 0.0f;
        bounds.y = 0.0f;
        bounds.w = (std::max)(1.0f, clientW);
        bounds.h = (std::max)(1.0f, clientH);
    } else if (EditorDocking_HasCentralNode()) {
        bounds = EditorDocking_GetCentralNodeClientRect();
    } else {
        const EditorLayoutMetrics layout = ComputeEditorLayout(clientW, clientH);
        bounds.x = layout.viewportX;
        bounds.y = layout.viewportY;
        bounds.w = layout.viewportW;
        bounds.h = layout.viewportH;
    }

    const EditorClientRect fitted = FitEditor16x9Viewport(bounds);
    const EditorBackbufferRect bb = ClientRectToBackbuffer(
        fitted, clientW, clientH, screenW, screenH);
    x = bb.x;
    y = bb.y;
    width = bb.w;
    height = bb.h;
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
            const bool directoryChanged = std::any_of(g_contentChanges.begin(), g_contentChanges.end(),
                [](const auto& change) { return change.added || change.removed; });
            if (directoryChanged) {
                try {
                    ProjectAssetIndex::Instance().Refresh();
                    SceneLoader_RefreshSceneAssets();
                } catch (const std::system_error& error) {
                    g_contentReloadLight.Failure();
                    g_contentReloadStatus = std::string("Content index refresh error: ") + error.what();
                    LogContentReload(g_contentReloadStatus);
                } catch (const std::exception& error) {
                    g_contentReloadLight.Failure();
                    g_contentReloadStatus = std::string("Content index refresh error: ") + error.what();
                    LogContentReload(g_contentReloadStatus);
                }
            }
            // Per-change try/catch so one bad path cannot drop the rest of a batch
            // after ContentFileMonitor::Observe has already advanced its baseline.
            for (const auto& change : g_contentChanges) {
                try {
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
                } catch (const std::system_error& error) {
                    g_contentReloadLight.Failure();
                    g_contentReloadStatus = std::string("Content path error: ") + error.what();
                    LogContentReload(g_contentReloadStatus);
                } catch (const std::exception& error) {
                    g_contentReloadLight.Failure();
                    g_contentReloadStatus = std::string("Content reload error: ") + error.what();
                    LogContentReload(g_contentReloadStatus);
                }
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

        if (clicked && !EditorDocking_WantCaptureMouseForGame()
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

bool MoveNodeOrder(GameObject* node, int offset) {
    if (!node) return false;
    if (GameObject* parent = node->GetParent())
        return MoveNodeInList(parent->GetChildren(), node, offset);
    return MoveNodeInList(g_sceneNodes, node, offset);
}

bool MoveNodeToBack(GameObject* node) {
    if (!node) return false;
    const int distance = static_cast<int>((node->GetParent()
        ? node->GetParent()->GetChildren().size()
        : g_sceneNodes.size()));
    return MoveNodeOrder(node, -distance);
}

bool MoveNodeToFront(GameObject* node) {
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

bool UndoableReorderNode(GameObject* node, bool (*moveFn)(GameObject*)) {
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

bool AutoFitColliderToMesh(GameObject* node, ColliderComponent* cc) {
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
    // Re-apply effective scale before NewFrame when moving across DPI monitors.
    {
        const float dpi = QueryEditorDpiScale();
        if (std::fabs(dpi - g_uiDpiScale) > 0.001f && g_baseStyleCaptured)
            ApplyEditorUiScaleFromBase();
    }
    ImGui::NewFrame();
    EditorDocking_BeginFrame();

    // No project open: product start screen only (folder-pick still runs after render).
    if (!ProjectManager::Instance().IsOpen()) {
        DrawEngineStartScreen();
        // New Project name modal (and unsaved guard) live in DrawEditorPopups.
        DrawEditorPopups();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        EditorDocking_FlushIniIfNeeded();
        if (ImGui::GetIO().WantCaptureMouse)
            Mouse_ClearWheel();
        ConsumePendingFolderPick();
        return;
    }

    TransformGizmo::BeginFrame();

    const float screenW = io.DisplaySize.x;
    const float menuBarHeight = EditorMenuBarHeight();

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
    DrawProjectSettingsWindow();
    DrawPreferencesWindow();
    DrawPostFxShowcaseWindow(&g_showPostFxShowcase);
    DrawShortcutHintsWindow(&g_showShortcutHints);

    DrawEditorToolbar(playVisuals, sceneLoadBusy, screenW, menuBarHeight);
    EditorDocking_DrawHost();

    auto& panels = EditorDocking_PanelVisibility();
    DrawEditorHierarchyPanel(playVisuals, &panels.hierarchy);
    DrawEditorInspectorPanel(playVisuals, &panels.properties);
    DrawEditorAssetPanel(playVisuals, sceneLoadBusy, &panels.assetBrowser);
    DrawEditorConsolePanel(&panels.console);
    EditorDocking_FocusDefaultBottomTabIfNeeded();

    EditorDocking_DrawLetterboxBars();

    // Drop / overlays use the 16:9 game rect (ImGui client), not the full node.
    const EditorClientRect gameView = EditorDocking_GetGameViewportClientRect();
    const ImRect dropRect(
        ImVec2(gameView.x, gameView.y),
        ImVec2(gameView.x + gameView.w, gameView.y + gameView.h));
    if(!sceneLoadBusy && !playVisuals && g_editMode==EditorEditMode::Scene3D
        && !EditorDocking_WantCaptureMouseForGame()
        && ImGui::BeginDragDropTargetViewport(ImGui::GetMainViewport(),&dropRect)) {
        if(const auto* payload=ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const std::string path=static_cast<const char*>(payload->Data);
            std::string ext=PathToUtf8(PathFromUtf8(path).extension());
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(ext==".png" || ext==".jpg" || ext==".jpeg" || ext==".bmp") {
                const auto mouse=ImGui::GetIO().MousePos;
                float bbX = mouse.x, bbY = mouse.y;
                Renderer_ClientToBackbuffer(mouse.x, mouse.y, bbX, bbY);
                DirectX::XMVECTOR origin,direction;
                if(BuildPickRay(int(bbX),int(bbY),origin,direction)) {
                    const float dz=DirectX::XMVectorGetZ(direction);
                    const float distance=std::fabs(dz)>1e-6f?-DirectX::XMVectorGetZ(origin)/dz:-1;
                    if(distance>=0) {
                        auto node=std::make_unique<GameObject>(PathToUtf8(PathFromUtf8(path).stem()));
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
    // Grid overlay is drawn in ImGui client space (16:9 game rect).
    const float gizmoX = gameView.x;
    const float gizmoY = gameView.y;
    const float gizmoW = gameView.w;
    const float gizmoH = gameView.h;
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

    EditorDocking_FlushIniIfNeeded();

    if (ImGui::GetIO().WantCaptureMouse)
        Mouse_ClearWheel();

    ConsumePendingFolderPick();
}

#ifdef _DEBUG
#include "tools/scene_hot_load_check.h"
#endif
