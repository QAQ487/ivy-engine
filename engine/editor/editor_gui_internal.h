// editor_gui_internal.h
// Shared editor-GUI state and cross-TU helpers for the phase-1 engine_gui.cpp split.
// Not part of the public EngineGUI_* API. Do not include from game content.

#pragma once

#include "engine/core/engine_gui.h"
#include "engine/core/engine_gui_internal.h"
#include "engine/core/play_fps_overlay.h"
#include "engine/editor/editor_layout.h"
#include "engine/editor/editor_theme.h"
#include "engine/editor/content_file_monitor.h"
#include "engine/physics/collider/collider.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/scene/scene_file_monitor.h"
#include "imgui/imgui.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GameObject;
class UIElement;
class ColliderComponent;

// --- Shared enums / small types (formerly file-static in engine_gui.cpp) ---

enum class PendingFolderPickAction { None = 0, NewProject, OpenProject, BuildOutput };

enum class PendingEditorAction {
    None = 0,
    LoadScene,
    NewScene,
    OpenProject,
    CreateProject,
    CloseProject,
    Exit,
};

enum class ReloadLightState { Paused, Watching, Pending, Failed };

struct ReloadLight {
    bool failed = false;
    double brightUntil = 0;
    void Success() { failed = false; brightUntil = ImGui::GetTime() + 1.2; }
    void Failure() { failed = true; brightUntil = 0; }
};

struct PrimitiveCreateSpec {
    const char* menuLabel;
    const char* displayName;
    const char* assetPath;
    ColliderType colliderType;
    float height;
};

// --- Shared mutable editor state (defined in engine_gui.cpp) ---

extern bool g_enabled;
extern bool g_isPlaying;
extern std::atomic<bool> g_exitGameRequested;
extern PlayMode g_playMode;
extern PlayMode g_pendingPlayMode;
extern bool g_holdPlayVisuals;
extern bool g_isOnStart;
extern bool g_imguiInitialized;

extern CameraMode g_cameraMode;
extern std::string g_activeCameraNode;
extern float g_scrollSensitivity;
extern char g_newSceneName[128];
extern char g_newTagName[64];
extern bool g_showRenderingSettings;
extern bool g_showPostFxShowcase;
extern bool g_showShortcutHints;
extern bool g_showNewScenePopup;
extern bool g_showAddTagPopup;
extern bool g_showDeleteTagPopup;
extern int g_pendingDeleteTagId;
extern bool g_showDefaultScenePopup;
extern bool g_showGravityPopup;
extern bool g_showAudioPopup;
extern bool g_showUiOverlay;
extern bool g_newProject2D;
extern bool g_showNewProjectPopup;
extern char g_newProjectName[128];
extern std::string g_newProjectParentDir;
extern std::string g_projectStatusMessage;
extern PendingFolderPickAction g_pendingFolderPick;
extern bool g_showBuildUnsavedPopup;
extern bool g_showBuildPreflight;
extern bool g_showBuildLog;
extern char g_buildDest[1024];
extern int g_buildConfigIndex;
extern EditorEditMode g_editMode;

extern int g_uiZoomPercent;
extern float g_uiZoom;
extern ImGuiStyle g_baseStyle;
extern bool g_baseStyleCaptured;
extern editor_theme::StylePreset g_uiStylePreset;
extern editor_theme::ColorScheme g_uiColorScheme;

extern PendingEditorAction g_pendingEditorAction;
extern std::string g_pendingEditorSceneFile;
extern std::string g_pendingProjectPath;
extern std::string g_pendingCreateProjectParent;
extern std::string g_pendingCreateProjectName;
extern bool g_showUnsavedChangesPopup;

extern ReloadLight g_sceneReloadLight;
extern ReloadLight g_scriptReloadLight;
extern unsigned long long g_scriptFeedbackRevision;
extern ContentFileMonitor g_contentMonitor;
extern std::vector<ContentFileMonitor::Change> g_contentChanges;
extern ReloadLight g_contentReloadLight;
extern std::string g_contentReloadStatus;
extern bool g_contentSceneReload;
extern SceneFileMonitor g_sceneFileMonitor;
extern std::optional<SceneFileMonitor::Version> g_sceneReloadConflict;
extern std::string g_sceneReloadStatus;
extern std::string g_scriptReloadStatus;

inline constexpr const char* kUiPrefsPath = PlayFpsOverlay::kPrefsPath;
inline constexpr int kUiZoomPercents[] = { 100, 125, 150, 175, 200 };

// --- Cross-TU helpers still defined in engine_gui.cpp ---

bool IsSceneLoadBusy();
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
void RequestOpenTestbed();
void SetBuildDestBuffer(const std::string& path);

void RemapNodeTagsAfterDelete(GameObject* node, int deletedId);
void RemapSceneTagsAfterDelete(int deletedId);

std::unique_ptr<GameObject> CreateEmptyGameObject();
std::unique_ptr<GameObject> CreateDirectionalLightObject();
std::unique_ptr<GameObject> CreatePointLightObject();
std::unique_ptr<GameObject> CreateSpotLightObject();
std::unique_ptr<GameObject> CreateCameraObject(bool debugFly = false);
void SpawnRootGameObject(std::unique_ptr<GameObject> node);
void SpawnChildGameObject(GameObject* parent);
std::unique_ptr<GameObject> DuplicateGameObject(const GameObject& src);
void SpawnDuplicate(GameObject* source);
void DeleteGameObject(GameObject* toDelete, bool recordUndo = true);
void ClearScene();

void StartPlay(PlayMode mode = PlayMode::Viewport);

bool IsValidUiZoomPercent(int percent);
void CaptureBaseStyleFromCurrent();
void ApplyEditorTheme(editor_theme::StylePreset preset, editor_theme::ColorScheme scheme);
void ApplyUiZoom(int percent);
void SaveUiPrefs();
void LoadUiPrefs();

bool AutoFitColliderToMesh(GameObject* node, ColliderComponent* cc);

ReloadLightState GetReloadLightState(bool active, bool paused, bool pending, const ReloadLight& light);

// Asset-path / inspector helpers (defined in editor_inspector_panel.cpp)
std::string LowerAssetExtension(const char* path);
bool AssetPathHasExtension(const char* path, const char* const* extensions, int count);
bool AcceptAssetPathDrop(const char* const* extensions, int count, std::string& outPath);
bool ScrollValue(float* value, float baseSpeed = 1.0f);
std::string ShaderVariantPreviewLabel(Shader3DVariant v);

// --- Extracted draw entry points ---

void DrawEngineStartScreen();
void DrawEditorMenuBar();
void DrawEditorPopups();
void DrawPostFxShowcaseWindow(bool* open);
void DrawShortcutHintsWindow(bool* open);
void DrawRenderingSettingsWindow(bool* open);

void DrawEditorToolbar(bool playVisuals, bool sceneLoadBusy, float screenW, float menuBarHeight);
void DrawSceneNodeHierarchy(GameObject* node, GameObject* rootOwner);
void DrawEditorHierarchyPanel(bool playVisuals, float leftWidth, float centerTop, float centerH);
void DrawEditorInspectorPanel(bool playVisuals, float screenW, float rightWidth, float centerTop, float centerH);
void DrawEditorAssetPanel(bool playVisuals, bool sceneLoadBusy, float screenW, float screenH, float bottomHeight);

void PushToolbarChromeStyle();
void PopToolbarChromeStyle();
float ToolbarIconSize();
bool ToolbarIconButton(const char* icon, const char* tooltip);
bool ToolbarIconToggle(const char* icon, bool active, const char* tooltip);
void DrawReloadLight(const char* id, const char* name, ReloadLightState state,
    const ReloadLight& light, const std::string& detail);
