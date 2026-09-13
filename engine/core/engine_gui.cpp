/******************************************************
 * File: engine_gui.cpp
 * Project: d3d11_engine
 * Author: Terry
 * Created: 2026-03-12
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   - engine::GameObject → ::GameObject throughout (alias, zero logic change).
 *   - Removed PrefabProxyLoader dependency.  DrawEditorSceneProxies() now
 *     reads the model path exclusively from VisualComponent (already in memory
 *     from deserialization), with Box_Blue.fbx as the absolute fallback.
 *   - GetTransform() helper calls GetComponent<engine::Transform>() because
 *     the standalone ::GameObject does not have a GetTransform() shortcut.
 *****************************************************/
#include "engine_gui.h"
#include "engine_gui_internal.h"
#include "engine/scene/scene_loader.h"
#include "engine/scene/scene_asset_preload.h"
#include "engine/scene/render_settings.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/d3d11/post_process.h"
#include "engine/core/job_system.h"
#include "engine/core/debug_system.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/icons_font_awesome.h"

#include <cstdio>
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
#include <unordered_map>
#include <Windows.h>
#include <filesystem>

#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/physics/collider/collider.h"
#include "engine/physics/core/physics_system.h"
#include "engine/ecs/game_object.h"          // unified ::GameObject
#include "engine/ecs/tag_registry.h"
#include "debug_camera.h"
#include "engine/input/mouse.h"
#include "engine/debug_draw/debug_drawer.h"
#include "camera.h"

 // Scene-graph / component-system headers
#include "engine/ecs/transform.h"
#include "engine/rendering/model/model.h"
#include "engine/ecs/prefab_serializer.h"
#include "engine/ecs/component_factory.h"
#include "engine/editor/editor_project_browser.h"
#include "engine/editor/editor_ui_panel.h"
#include "engine/editor/project_asset_index.h"
#include "engine/editor/project_manager.h"
#include "engine/editor/playable_package_builder.h"
#include "engine/audio/audio_manager.h"
#include "engine/editor/transform_gizmo.h"
#include "engine/editor/ui_gizmo.h"
#include "engine/editor/editor_undo.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/ecs/material_component.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/ecs/directional_light_component.h"
#include "engine/ecs/point_light_component.h"
#include "engine/ecs/spot_light_component.h"
#include "engine/ecs/camera_component.h"
#include "engine/editor/light_gizmo.h"
#include "engine/editor/camera_gizmo.h"
#include "engine/ecs/collider_component.h"
#include "engine/ecs/effect_component.h"
#include "engine/ecs/audio_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/model_asset_cache.h"
#include "engine/scene/scene_runtime.h"
#include "engine/ecs/prefab_runtime.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/Script.h"
#include "third_person_camera.h"
#include "scene_manager.h"
#include "engine/ui/scene_canvas.h"
#include "engine/ui/ui_canvas.h"
#include "engine/ui/ui_element.h"
// ── DLL hot-reload ────────────────────────────────────────────────────────────
#include "engine/scripting/script_factory.h"
#include "engine/scripting/script_watcher.h"


// nlohmann/json for JSON scene format
#include "libraries/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// ── State ─────────────────────────────────────────────────────────────────────
static bool        g_enabled = true;
bool              g_isPlaying = false;
static bool        g_holdPlayVisuals = false;
static bool        g_isOnStart = true;
static bool        g_imguiInitialized = false;
static SceneAssetPreloader g_sceneAssetPreloader;
static bool        g_playWhenPreloadDone = false;
bool               g_sceneLoadPending = false;
static std::string g_pendingSceneLoadPath;
static CameraMode  g_cameraMode = CameraMode::ThirdPerson;
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
static constexpr const char* kUiPrefsPath = "rom/editor/ui_prefs.json";
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
        std::error_code ec;
        fs::create_directories("rom/editor", ec);
        json j;
        j["uiZoomPercent"] = g_uiZoomPercent;
        std::ofstream file(kUiPrefsPath);
        if (file.is_open())
            file << j.dump(2);
    }
    catch (...) {
    }
}

static void LoadUiPrefs() {
    g_uiZoomPercent = 100;
    g_uiZoom = 1.0f;
    try {
        std::ifstream file(kUiPrefsPath);
        if (!file.is_open()) return;
        json j;
        file >> j;
        const int percent = j.value("uiZoomPercent", 100);
        if (IsValidUiZoomPercent(percent)) {
            g_uiZoomPercent = percent;
            g_uiZoom = static_cast<float>(percent) / 100.0f;
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
void EngineGUI_ClearSceneDirty() { g_sceneDirty = false; }

// Unsaved-changes popup state. Used to gate scene switches and exit.
enum class PendingEditorAction {
    None = 0,
    LoadScene,
    NewScene,
    OpenProject,
    CreateProject,
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

static std::string g_scriptReloadStatus = "Script DLL reload: not initialized";

static void SaveScene();
static void SaveSceneToFile(const std::string& sceneFile);
static void NewScene();
static void SaveProject();
static void ApplyProjectSessionState();
static bool SwitchToOpenedProject();
static void RequestOpenProject(const std::string& rootPath);
static void BeginNewProjectFlow();
static void BeginOpenProjectFlow();
static void OpenBuildPreflight();
static void RequestBuildCurrentProject();

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

static VisualComponent* EnsureVisualComponent(GameObject* node) {
    if (!node) return nullptr;
    if (auto* vc = node->GetComponent<VisualComponent>())
        return vc;
    return node->AddComponent<VisualComponent>();
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

static std::unique_ptr<GameObject> CreateCameraObject() {
    auto node = std::make_unique<GameObject>("Camera");
    auto* t = node->AddComponent<engine::Transform>();
    t->localPosition = { 0.0f, 20.0f, -20.0f };
    t->localRotation = { 35.0f, 0.0f, 0.0f };
    node->AddComponent<CameraComponent>();
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

// ── Helpers ───────────────────────────────────────────────────────────────────
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
        if (s_editNode != editNode || s_editKey != key) {
            s_editNode = editNode; s_editKey = key;
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
        }
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText(wid.c_str(), s_strBuf, sizeof(s_strBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            val = std::string(s_strBuf);
            return true;
        }
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

    static float s_rgb = 0.0f;
    static float s_mosaic = 0.0f;
    static bool s_synced = false;
    if (!s_synced) {
        PostProcess_GetEffects(s_rgb, s_mosaic);
        s_synced = true;
    }

    ImGui::TextWrapped(
        "Live post-process over the game viewport. "
        "Use this to verify RGB shift / mosaic without a match GOAL.");
    ImGui::Separator();

    if (ImGui::SliderFloat("RGB Shift", &s_rgb, 0.0f, 0.04f, "%.4f"))
        PostProcess_SetEffects(s_rgb, s_mosaic);
    if (ImGui::SliderFloat("Mosaic Blocks", &s_mosaic, 0.0f, 80.0f, "%.0f"))
        PostProcess_SetEffects(s_rgb, s_mosaic);

    ImGui::Spacing();
    if (ImGui::Button("Apply Passthrough (clear)")) {
        s_rgb = 0.0f;
        s_mosaic = 0.0f;
        PostProcess_SetEffects(0.0f, 0.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset: Mild RGB")) {
        s_rgb = 0.008f;
        s_mosaic = 0.0f;
        PostProcess_SetEffects(s_rgb, s_mosaic);
    }
    if (ImGui::Button("Preset: Mosaic")) {
        s_rgb = 0.0f;
        s_mosaic = 40.0f;
        PostProcess_SetEffects(s_rgb, s_mosaic);
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset: Combined")) {
        s_rgb = 0.01f;
        s_mosaic = 32.0f;
        PostProcess_SetEffects(s_rgb, s_mosaic);
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

        row("F2", "Toggle editor GUI");
        row("F3", "Play / stop");
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

        row("F7", "Cycle time scale (1.0 → 0.5 → 0.25 → freeze)");
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

    ImGui::Text("Cubemap");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##SkyboxCubemap", cubemapBuf, sizeof(cubemapBuf),
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        rs.skyboxCubemapPath = cubemapBuf;
        syncedCubemapPath = rs.skyboxCubemapPath;
        RenderSettings_ReloadSkybox();
    }

    ImGui::ColorEdit4("Tint", &rs.skyboxTint.x);
    ImGui::DragFloat("Exposure", &rs.skyboxExposure, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("Rotation Y", &rs.skyboxRotationYDeg, 1.0f, -180.0f, 180.0f);
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
    ImGui::Text("Fog");
    ImGui::Separator();
    ImGui::Checkbox("Height Fog", &rs.fogEnabled);
    ImGui::ColorEdit3("Fog Color", &rs.fogColor.x);
    ImGui::DragFloat("Density", &rs.fogDensity, 0.001f, 0.0f, 1.0f, "%.4f");
    ImGui::DragFloat("Start", &rs.fogStart, 0.5f, 0.0f, 500.0f);
    ImGui::DragFloat("Height", &rs.fogHeight, 0.1f, -50.0f, 200.0f);
    ImGui::DragFloat("Height Falloff", &rs.fogHeightFalloff, 0.01f, 0.0f, 5.0f, "%.3f");

    ImGui::Spacing();
    ImGui::TextDisabled("Cubemap DDS files live in rom/texture/sky/.");
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
}

static bool SwitchToOpenedProject() {
    if (!ProjectManager::Instance().IsOpen())
        return false;

    if (g_isPlaying)
        ReturnToStart();

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

static void DrawEditorMenuBar() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project..."))
            g_pendingFolderPick = PendingFolderPickAction::NewProject;
        if (ImGui::MenuItem("Open Project..."))
            g_pendingFolderPick = PendingFolderPickAction::OpenProject;
        if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S"))
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
                    if (ImGui::MenuItem(label, nullptr, isCurrent, !isCurrent))
                        RequestOpenProject(path);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        {
            const bool canBuild = ProjectManager::Instance().IsOpen() &&
                                  !PlayablePackage::IsBuildRunning();
            if (ImGui::MenuItem("Build Current Project...", nullptr, false, canBuild))
                RequestBuildCurrentProject();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            SaveScene();
        if (ImGui::MenuItem("New Scene..."))
            g_showNewScenePopup = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Scene..."))
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
        if (ImGui::MenuItem("Create Empty"))
            SpawnRootGameObject(CreateEmptyGameObject());
        if (ImGui::MenuItem("Camera"))
            SpawnRootGameObject(CreateCameraObject());
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
        if (ImGui::MenuItem("Default Scene..."))
            g_showDefaultScenePopup = true;
        if (ImGui::MenuItem("Gravity...", nullptr, false, ProjectManager::Instance().IsOpen()))
            g_showGravityPopup = true;
        if (ImGui::MenuItem("Audio...", nullptr, false, ProjectManager::Instance().IsOpen()))
            g_showAudioPopup = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Engine Settings")) {
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
            if (!ProjectManager::Instance().CreateProject(createParent, createName, &error)) {
                MessageBoxA(GetWindowHandle(),
                            error.empty() ? "Failed to create project." : error.c_str(),
                            "New Project", MB_OK | MB_ICONERROR);
            }
            else {
                SwitchToOpenedProject();
            }
        }
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
                if (!ProjectManager::Instance().CreateProject(g_newProjectParentDir, name, &error)) {
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
                g_selNode->SetTag(newId);
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
        ImGui::Text("Build playable package");
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
        ImGui::TextDisabled("Debug shows the bottom-left FPS overlay. Release does not.");

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
            if (!buildDone)
                ImGui::Text("Building playable package...");
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
    g_isOnStart = true;
    g_cameraMode = CameraMode::ThirdPerson;
    ThirdPersonCamera_ShowCursor(true);
    ThirdPersonCamera_SetTarget(nullptr, nullptr);
    TeardownScenePlaySession(resetEditorState);
}

// Stop play simulation for an in-game scene switch without restoring editor
// chrome. Keep the current world until Flush replaces it (runtime loads
// atomically — clearing early caused a clear-color gap under the transition).
void SoftStopPlayForSceneLoad() {
    if (!g_isPlaying && g_holdPlayVisuals)
        return;
    g_isPlaying = false;
    g_isOnStart = true;
    g_holdPlayVisuals = true;
    g_enabled = false;
    DebugCamera_SetEnabled(false);
    DebugSystem::ResetTimeScale();
}

void ReturnToStart() {
    g_holdPlayVisuals = false;
    TeardownPlaySession(true);
    DebugSystem::ResetTimeScale();
    g_enabled = true;
    g_editMode = EditorEditMode::Scene3D;
    DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
    DebugCamera_SetEnabled(true);
    ApplyEditorColliders();
    RestoreEditorDebugColliderFlags();
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
    return !HasCameraComponentInScene() && !HasThirdPersonPlayer();
}

bool EngineGUI_IsDebugCameraPlayInputActive() {
    return g_isPlaying && DebugCamera_IsEnabled();
}

static void StartPlay() {
    if (g_sceneAssetPreloader.IsActive() || g_sceneLoadPending) {
        g_playWhenPreloadDone = true;
        return;
    }

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
    g_holdPlayVisuals = false;
    if (ShouldUseDebugCameraDuringPlay()) {
        g_enabled = false;
        if (SceneLoader_DebugCameraEnabledOnPlay() && SceneLoader_DebugCameraFlyOnPlay())
            DebugCamera_SetControlMode(DebugCameraControlMode::Fly);
        else
            DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
        DebugCamera_SetEnabled(true);
        ThirdPersonCamera_ShowCursor(true);
    } else {
        g_enabled = false;
        DebugCamera_SetEnabled(false);
        const bool hideCursor = (g_cameraMode == CameraMode::ThirdPerson) && HasThirdPersonPlayer();
        ThirdPersonCamera_ShowCursor(!hideCursor);
    }
}

void EngineGUI_QueueSceneLoad(const char* filename) {
    // Amendment 1: each new scene switch resets deferred play so a stale F3
    // cannot auto-play into the wrong scene.
    g_playWhenPreloadDone = false;
    g_sceneLoadPending = true;
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

    const std::string path = std::move(g_pendingSceneLoadPath);
    g_pendingSceneLoadPath.clear();

    // Tear down the previous play world immediately before replacing nodes
    // (SoftStop no longer clears early — matches runtime atomic load).
    TeardownScenePlaySession(false);

    if (!SceneLoader_LoadFromFile(path.c_str())) {
        g_sceneLoadPending = false;
        g_holdPlayVisuals = false;
        return;
    }

    g_selNode = nullptr;
    g_selUiElement = nullptr;
    EngineGUI_SetEditMode(EditorEditMode::Scene3D);
    EngineGUI_ClearSceneDirty();
    EditorUndo_Clear();

    if (ProjectManager::Instance().IsOpen()) {
        const std::filesystem::path filePath(path);
        ProjectManager::Instance().SetLastOpenedScene(filePath.filename().string());
    }

    EngineGUI_BeginSceneAssetPreload();
    g_sceneLoadPending = false;

    // Empty preload never becomes Active, so Tick never resumes play.
    // Match runtime: stay in / re-enter play immediately when deferred.
    if (g_playWhenPreloadDone && !g_sceneAssetPreloader.IsActive()) {
        g_playWhenPreloadDone = false;
        StartPlay();
    }
}

void EngineGUI_TickSceneAssetPreload() {
    if (!g_sceneAssetPreloader.IsActive())
        return;

    if (!g_sceneAssetPreloader.Tick())
        return;

    if (g_playWhenPreloadDone) {
        g_playWhenPreloadDone = false;
        StartPlay();
    }
}

bool EngineGUI_IsSceneAssetPreloadActive() {
    return g_sceneAssetPreloader.IsActive() || g_sceneLoadPending;
}

static bool IsSceneLoadBusy() {
    return g_sceneAssetPreloader.IsActive() || g_sceneLoadPending;
}

// ── Public API ────────────────────────────────────────────────────────────────

void EngineGUI_Initialize(HWND hwnd, ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    ProjectManager::Instance().BootstrapAtStartup();

    ModelAssetCache::Instance().RefreshAssetDirectory();
    SceneLoader_Initialize(true);
    g_scriptReloadStatus = "Watching script DLLs";

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
        if (!io.Fonts->AddFontFromFileTTF(
                "rom/font/fa-solid-900.ttf", 13.0f, &iconsConfig, iconRanges)) {
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
bool EngineGUI_PreferPlayVisuals() { return g_isPlaying || g_holdPlayVisuals; }
bool EngineGUI_IsOnStart() { return g_isOnStart; }
void EngineGUI_StartPlay() { StartPlay(); }
void EngineGUI_StopPlay() { ReturnToStart(); }
CameraMode EngineGUI_GetCameraMode() { return g_cameraMode; }
void EngineGUI_SetCameraMode(CameraMode m) { g_cameraMode = m; }
const std::string& EngineGUI_GetActiveCameraNodeName() { return g_activeCameraNode; }
void EngineGUI_SetActiveCameraNodeName(const std::string& n) { g_activeCameraNode = n; }
void EngineGUI_SetEnabled(bool e) { g_enabled = e; DebugCamera_SetEnabled(e); }
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

    if (g_isPlaying || !g_enabled) {
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

// ── Ray helpers ───────────────────────────────────────────────────────────────

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

// Möller–Trumbore ray-triangle test (local/world space; t is along rayDir).
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

// ── Wireframe highlight ───────────────────────────────────────────────────────

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

    auto* cc = node->GetComponent<ColliderComponent>();
    if (!cc || cc->colliderType == ColliderType::NONE) return;

    static const XMFLOAT4 kColliderColor = { 0.0f, 1.0f, 1.0f, 1.0f };

    GameObject* poseSource = GetSelectionPoseSource(node);
    if (!poseSource) return;

    poseSource->SyncLegacyTransformFromComponents();

    XMFLOAT3 pos, rot, scale;
    poseSource->GetWorldPose(pos, rot, scale);

    // Match runtime collider/debug draw: radius, box size, and center offset are
    // not multiplied by owner scale.
    switch (cc->colliderType) {
    case ColliderType::BOX:
        drawer.DrawBox(pos, cc->colliderCenter,
            { cc->halfExtents.x * 2.0f, cc->halfExtents.y * 2.0f, cc->halfExtents.z * 2.0f },
            rot, kColliderColor);
        break;
    case ColliderType::SPHERE:
        drawer.DrawSphere(pos, cc->colliderCenter, cc->radius, rot, kColliderColor);
        break;
    case ColliderType::CAPSULE:
        drawer.DrawCapsule(pos, cc->colliderCenter, cc->radius, cc->height,
            rot, kColliderColor);
        break;
    case ColliderType::CYLINDER:
        drawer.DrawCylinder(pos, cc->colliderCenter, cc->radius, cc->height,
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

    if (!g_isPlaying) {
        LightGizmo::DrawSceneLights(g_sceneNodes, drawer);
        CameraGizmo::DrawSceneCameras(g_sceneNodes, drawer);
    }

    if (g_selNode && !g_isPlaying) {
        if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
            if (!NodeHasLightComponent(g_selNode) && !NodeHasCameraComponent(g_selNode)) {
                Model3D* model = nullptr;
                if (auto* vc = g_selNode->GetComponent<VisualComponent>())
                    model = vc->GetModel();
                if (model && !model->GetVerticesCPU().empty()) {
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

// ── EngineGUI_Update ──────────────────────────────────────────────────────────

void EngineGUI_Update(double elapsed_time) {

    // Finish a queued scene switch (JSON + preload Begin) after the prior
    // frame already Presented the dim-only overlay.
    if (g_sceneLoadPending)
        EngineGUI_FlushSceneLoadPending();

    if (!g_isPlaying) {
        SceneLoader_ScanScriptDllsForChanges(false);
        SceneLoader_DrainScriptReloadQueue(
            [](const std::string& name) {
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
    }

    // Time-sliced asset preload / scene-switch busy: gate editor interaction.
    if (IsSceneLoadBusy()) {
        if (KeyLogger_IsTrigger(KK_F3) && !g_isPlaying)
            g_playWhenPreloadDone = true;
        if (KeyLogger_IsTrigger(KK_F8))
            JobSystem_ToggleDebugWindow();
        if (g_sceneAssetPreloader.IsActive())
            EngineGUI_TickSceneAssetPreload();
        // Still loading, or deferred StartPlay just entered play — skip editors.
        if (IsSceneLoadBusy() || g_isPlaying)
            return;
    }

    if (KeyLogger_IsTrigger(KK_F2)) {
        g_enabled = !g_enabled;
        DebugCamera_SetEnabled(g_enabled);
        ThirdPersonCamera_ShowCursor(g_enabled);
    }

    if (KeyLogger_IsTrigger(KK_F3)) {
        if (g_isPlaying) {
            ReturnToStart();
            g_enabled = true;
            DebugCamera_SetEnabled(true);
        }
        else {
            StartPlay();
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
    if (KeyLogger_IsTrigger(KK_F) && g_selNode) {
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
                if (const engine::Transform* tr = node->GetTransform()) {
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

// ── Proxy / scene drawing ─────────────────────────────────────────────────────

static void DrawEditorSceneProxiesNode(GameObject* node) {
    if (!node) return;
    node->DrawVisual(false);
}

void DrawEditorSceneProxies() {
    for (const auto& root : g_sceneNodes)
        if (root) DrawEditorSceneProxiesNode(root.get());
}

// ── Scene hierarchy ImGui helpers ─────────────────────────────────────────────

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

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        g_selNode = node;
        g_selUiElement = nullptr;
    }

    if (ImGui::BeginPopupContextItem()) {
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
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        GameObject* ptr = node;
        ImGui::SetDragDropPayload("HIERARCHY_GO", &ptr, sizeof(ptr));
        ImGui::Text("Move: %s", node->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target
    if (ImGui::BeginDragDropTarget()) {
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

// ── EngineGUI_Draw ────────────────────────────────────────────────────────────

void EngineGUI_OnBackbufferResized() {
    if (!g_imguiInitialized) return;
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();
}

void EngineGUI_Draw() {
    // Play mode sets g_enabled=false (hides editor chrome). Still draw the
    // Job System overlay when F8 has opened it so it survives play.
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
    if (sceneLoadBusy)
        ImGui::OpenPopup("SceneAssetPreloadModal");

    if (!sceneLoadBusy) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            SaveScene();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S))
            SaveProject();
        // RouteAlways: default RouteFocused fails when focus is on the game viewport.
        if (!g_isPlaying && !ImGui::GetIO().WantTextInput) {
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

    // ── Toolbar ───────────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(screenW, Ui(kToolbarHeight)));
    ImGui::Begin("##Toolbar", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    {
        if (g_isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.70f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.40f, 0.15f, 1.0f));
            if (ToolbarIconButton(ICON_FA_PAUSE, "Pause")) {
                g_isPlaying = false;
                g_enabled = true;
                DebugCamera_SetControlMode(DebugCameraControlMode::Orbit);
                DebugCamera_SetEnabled(true);
                ThirdPersonCamera_ShowCursor(true);
                ClearSceneRuntime(false);
                ApplyEditorColliders();
                RestoreEditorDebugColliderFlags();
            }
            ImGui::PopStyleColor(3);
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.55f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.30f, 0.55f, 1.0f));
            if (ToolbarIconButton(ICON_FA_PLAY, "Play (F3)")) StartPlay();
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        if (ToolbarIconButton(ICON_FA_FLOPPY_DISK, "Save Scene (Ctrl+S)"))
            SaveScene();

        ImGui::SameLine();
        {
            const bool canEditUndo = !g_isPlaying && !sceneLoadBusy;
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
            ImGui::SetTooltip("Reload Scripts\n%s", g_scriptReloadStatus.c_str());

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.15f, 0.15f, 1.0f));
        if (ToolbarIconButton(ICON_FA_BACKWARD_STEP, "Return to Start")) ReturnToStart();
        ImGui::PopStyleColor(3);

        if (!g_isPlaying) {
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
            if (ImGui::RadioButton(ICON_FA_CUBE, g_editMode == EditorEditMode::Scene3D))
                EngineGUI_SetEditMode(EditorEditMode::Scene3D);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Scene Edit Mode");
            ImGui::SameLine();
            if (ImGui::RadioButton(ICON_FA_LAYER_GROUP, g_editMode == EditorEditMode::UI))
                EngineGUI_SetEditMode(EditorEditMode::UI);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("UI Edit Mode");
        }

        ImGui::SameLine();
        TransformGizmo::DrawToolbarControls();

        ImGui::SameLine();
        // Camera selector
        {
            std::vector<std::string> camNames;
            std::function<void(GameObject*)> collectCams = [&](GameObject* node) {
                if (!node) return;
                if (node->GetComponent<CameraComponent>())
                    camNames.push_back(node->GetName());
                for (const auto& child : node->GetChildren())
                    collectCams(child.get());
            };
            for (const auto& node : g_sceneNodes)
                collectCams(node.get());

            if (g_activeCameraNode.empty() && camNames.size() == 1)
                g_activeCameraNode = camNames[0];

            std::string currentLabel =
                (g_cameraMode == CameraMode::ThirdPerson || g_activeCameraNode.empty())
                ? "Third Person" : g_activeCameraNode;

            ImGui::Text("Camera:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::BeginCombo("##Camera", currentLabel.c_str())) {
                bool tpSel = (g_cameraMode == CameraMode::ThirdPerson);
                if (ImGui::Selectable("Third Person", tpSel))
                    g_cameraMode = CameraMode::ThirdPerson;
                for (const auto& name : camNames) {
                    bool sel = (g_cameraMode == CameraMode::SceneCamera && g_activeCameraNode == name);
                    if (ImGui::Selectable(name.c_str(), sel)) {
                        g_cameraMode = CameraMode::SceneCamera;
                        g_activeCameraNode = name;
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
    ImGui::End();

    // ── Scene Hierarchy (left) ────────────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(0, centerTop));
    ImGui::SetNextWindowSize(ImVec2(leftWidth, centerH));
    ImGui::Begin("Scene Hierarchy", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (ImGui::CollapsingHeader("Game Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& rootNode : g_sceneNodes)
                if (rootNode) DrawSceneNodeHierarchy(rootNode.get(), rootNode.get());

            // Root drop zone
            ImGui::Selectable("##RootDropZone", false,
                ImGuiSelectableFlags_AllowOverlap, ImVec2(-1, 6));
            if (ImGui::BeginDragDropTarget()) {
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
    }
    ImGui::End();

    // ── Properties (right) ────────────────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(screenW - rightWidth, centerTop));
    ImGui::SetNextWindowSize(ImVec2(rightWidth, centerH));
    ImGui::Begin("Properties", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (g_selNode) {
            bool runtimeDirty = false;
            static GameObject* s_lastSelNode = nullptr;
            static char nodeNameBuf[128] = {};
            if (g_selNode != s_lastSelNode) {
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = g_selNode;
                strncpy_s(nodeNameBuf, g_selNode->GetName().c_str(), sizeof(nodeNameBuf) - 1);
            }

            const bool hadVisual = g_selNode->GetComponent<VisualComponent>() != nullptr;
            VisualComponent* vc = EnsureVisualComponent(g_selNode);
            if (!hadVisual)
                runtimeDirty = true;

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
                ImGui::Text("Position");
                ImGui::Text("X:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NPX", &t->localPosition.x, 0.1f);
                runtimeDirty |= ScrollValue(&t->localPosition.x);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NPY", &t->localPosition.y, 0.1f);
                runtimeDirty |= ScrollValue(&t->localPosition.y);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NPZ", &t->localPosition.z, 0.1f);
                runtimeDirty |= ScrollValue(&t->localPosition.z);
                ImGui::Spacing();
                ImGui::Text("Rotation");
                ImGui::Text("X:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NRX", &t->localRotation.x, 1.0f);
                runtimeDirty |= ScrollValue(&t->localRotation.x, 1.0f);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NRY", &t->localRotation.y, 1.0f);
                runtimeDirty |= ScrollValue(&t->localRotation.y, 1.0f);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NRZ", &t->localRotation.z, 1.0f);
                runtimeDirty |= ScrollValue(&t->localRotation.z, 1.0f);
                ImGui::Spacing();
                ImGui::Text("Scale");
                ImGui::Text("X:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NSX", &t->localScale.x, 0.1f, 0.001f, 1000.0f);
                runtimeDirty |= ScrollValue(&t->localScale.x);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NSY", &t->localScale.y, 0.1f, 0.001f, 1000.0f);
                runtimeDirty |= ScrollValue(&t->localScale.y);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                runtimeDirty |= ImGui::DragFloat("##NSZ", &t->localScale.z, 0.1f, 0.001f, 1000.0f);
                runtimeDirty |= ScrollValue(&t->localScale.z);
                ImGui::Spacing();
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
                if (ImGui::DragFloat("FOV", &cam->fov, 0.5f, 1.0f, 179.0f))
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

            if (ColliderComponent* cc = g_selNode->GetComponent<ColliderComponent>()) {
                ImGui::Separator();
                ImGui::Text("Collider");
                EditorUndo_BeginComponentEdit(g_selNode, "Collider");
                const char* colTypes[] = { "NONE","BOX","SPHERE","CAPSULE","MESH","CYLINDER" };
                int ct = static_cast<int>(cc->colliderType);
                ImGui::SameLine(80);
                if (ImGui::Combo("##ColType", &ct, colTypes, IM_ARRAYSIZE(colTypes))) {
                    cc->colliderType = static_cast<ColliderType>(ct);
                    runtimeDirty = true;
                }
                if (cc->colliderType != ColliderType::NONE) {
                    ImGui::Text("Center");
                    ImGui::Text("X:"); ImGui::SameLine(40);
                    if (ImGui::DragFloat("##CCX", &cc->colliderCenter.x, 0.1f)) runtimeDirty = true;
                    ScrollValue(&cc->colliderCenter.x);
                    ImGui::Text("Y:"); ImGui::SameLine(40);
                    if (ImGui::DragFloat("##CCY", &cc->colliderCenter.y, 0.1f)) runtimeDirty = true;
                    ScrollValue(&cc->colliderCenter.y);
                    ImGui::Text("Z:"); ImGui::SameLine(40);
                    if (ImGui::DragFloat("##CCZ", &cc->colliderCenter.z, 0.1f)) runtimeDirty = true;
                    ScrollValue(&cc->colliderCenter.z);
                }
                if (cc->colliderType == ColliderType::BOX) {
                    ImGui::Text("Half Extents");
                    ImGui::Text("X:"); ImGui::SameLine(40);
                    if (ImGui::DragFloat("##HEX", &cc->halfExtents.x, 0.1f)) runtimeDirty = true;
                    ImGui::Text("Y:"); ImGui::SameLine(40);
                    if (ImGui::DragFloat("##HEY", &cc->halfExtents.y, 0.1f)) runtimeDirty = true;
                    ImGui::Text("Z:"); ImGui::SameLine(40);
                    if (ImGui::DragFloat("##HEZ", &cc->halfExtents.z, 0.1f)) runtimeDirty = true;
                }
                else if (cc->colliderType == ColliderType::SPHERE) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##Rad", &cc->radius, 0.1f)) runtimeDirty = true;
                }
                else if (cc->colliderType == ColliderType::CAPSULE) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##CRad", &cc->radius, 0.1f)) runtimeDirty = true;
                    ImGui::Text("Height:"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##CHei", &cc->height, 0.1f)) runtimeDirty = true;
                }
                else if (cc->colliderType == ColliderType::CYLINDER) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##CyRad", &cc->radius, 0.1f)) runtimeDirty = true;
                    ImGui::Text("Height:"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##CyHei", &cc->height, 0.1f)) runtimeDirty = true;
                }
                else if (cc->colliderType == ColliderType::MESH) {
                    ImGui::Text("Size Padding:"); ImGui::SameLine(100);
                    if (ImGui::DragFloat("##MSP", &cc->meshSizePadding, 0.01f, 0.0f, 10.0f)) runtimeDirty = true;
                }
                if (ImGui::Checkbox("Show Debug Collider", &cc->showDebugCollider)) {
                    g_selNode->m_debugCollider = cc->showDebugCollider;
                    runtimeDirty = true;
                }
                if (ImGui::Checkbox("Is Trigger", &cc->isTrigger)) {
                    runtimeDirty = true;
                }
                EditorUndo_EndComponentEdit(g_selNode, "Collider");
                if (ImGui::Button("Remove Collider")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Collider");
                    g_selNode->RemoveComponent<ColliderComponent>();
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
            if (!g_selNode->GetComponent<engine::Transform>() && ImGui::Button("Add Transform")) {
                g_selNode->AddComponent<engine::Transform>();
                EditorUndo_PushAddComponent(g_selNode, "SceneTransform");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<BillboardComponent>() && ImGui::Button("Add Billboard")) {
                g_selNode->AddComponent<BillboardComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Billboard");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<MaterialComponent>() && ImGui::Button("Add Material")) {
                g_selNode->AddComponent<MaterialComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Material");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<RigidbodyComponent>() && ImGui::Button("Add Rigidbody")) {
                g_selNode->AddComponent<RigidbodyComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Rigidbody");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<ColliderComponent>() && ImGui::Button("Add Collider")) {
                g_selNode->AddComponent<ColliderComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Collider");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<ScriptComponent>() && ImGui::Button("Add Script")) {
                g_selNode->AddComponent<ScriptComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Script");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<EffectComponent>() && ImGui::Button("Add Effect")) {
                g_selNode->AddComponent<EffectComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Effect");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<AudioComponent>() && ImGui::Button("Add Audio")) {
                g_selNode->AddComponent<AudioComponent>();
                EditorUndo_PushAddComponent(g_selNode, "Audio");
                runtimeDirty = true;
            }
            if (!g_selNode->GetComponent<AudioListenerComponent>() && ImGui::Button("Add AudioListener")) {
                g_selNode->AddComponent<AudioListenerComponent>();
                EditorUndo_PushAddComponent(g_selNode, "AudioListener");
                runtimeDirty = true;
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
    }
    ImGui::End();

    // ── Asset Browser (bottom) ────────────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(0, screenH - bottomHeight));
    ImGui::SetNextWindowSize(ImVec2(screenW, bottomHeight));
    ImGui::Begin("Asset Browser", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    {
        if (sceneLoadBusy)
            ImGui::BeginDisabled(true);
        if (GameObject* spawned = MapEditor::ProjectBrowserPanel(g_sceneNodes)) {
            if (!sceneLoadBusy) {
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
        if (sceneLoadBusy)
            ImGui::EndDisabled();
    }
    ImGui::End();

    if (!sceneLoadBusy && !g_isPlaying && g_editMode == EditorEditMode::Scene3D) {
        TransformGizmo::HandleHotkeys();
        if (g_selNode)
            TransformGizmo::Draw(g_selNode);
    }

    if (!sceneLoadBusy && !g_isPlaying && g_editMode == EditorEditMode::UI
        && g_selUiElement && g_showUiOverlay) {
        UiGizmo::Draw(g_selUiElement);
    }

    JobSystem_DebugDraw();

    // Whole-screen modal dim (same path as UnsavedChangesPopup) — not
    // BackgroundDrawList, which sits under opaque panel windows.
    if (sceneLoadBusy) {
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
            ImGui::TextUnformatted("Loading scene...");
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

    // Run Win32 folder dialogs after ImGui finishes the frame (avoids menu reentrancy).
    if (g_pendingFolderPick != PendingFolderPickAction::None) {
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
}
