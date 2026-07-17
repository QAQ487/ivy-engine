#include "engine_gui.h"
#include "engine/scene/scene_loader.h"
#include "engine/ecs/model_asset_cache.h"
#include "engine/ecs/game_object.h"
#include "engine/scripting/script_component.h"
#include "src/app/camera/debug_camera.h"
#include "src/app/camera/third_person_camera.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/input/mouse.h"

#include <Windows.h>

static bool g_runtimePlaying = false;
static CameraMode g_cameraMode = CameraMode::ThirdPerson;
static std::string g_activeCameraNode;

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

static bool HasThirdPersonPlayer() {
    for (const auto& node : SceneLoader_GetSceneNodes()) {
        if (node && HasThirdPersonPlayerNode(node.get()))
            return true;
    }
    return false;
}

static void EnterRuntimePlay() {
    SnapshotSceneStartTransforms();
    SyncSceneRuntime();
    g_runtimePlaying = true;
    DebugCamera_SetEnabled(false);
    const bool hideCursor = (g_cameraMode == CameraMode::ThirdPerson) && HasThirdPersonPlayer();
    ThirdPersonCamera_ShowCursor(!hideCursor);
}

void EngineGUI_Initialize(HWND /*hwnd*/, ID3D11Device* /*device*/,
    ID3D11DeviceContext* /*context*/)
{
    ModelAssetCache::Instance().RefreshAssetDirectory();
    SceneLoader_Initialize(false);
}

void EngineGUI_Finalize() {
    SceneLoader_Finalize();
    g_runtimePlaying = false;
    g_activeCameraNode.clear();
}

void EngineGUI_Update(double elapsed_time) {
    if (!g_runtimePlaying) return;

    // In the editor build, engine_gui.cpp owns updating the scene graph while playing.
    // The runtime host must do the same so scripts/components run.
    for (auto& node : SceneLoader_GetSceneNodes())
        if (node) node->Update(elapsed_time);
}

void EngineGUI_Draw() {
}

void EngineGUI_OnBackbufferResized() {
}

void EngineGUI_DrawSelectionIndicator() {
}

bool EngineGUI_IsPlaying() {
    return g_runtimePlaying;
}

bool EngineGUI_IsOnStart() {
    return !g_runtimePlaying;
}

void EngineGUI_StartPlay() {
    EnterRuntimePlay();
}

void EngineGUI_StopPlay() {
    TeardownScenePlaySession(true);
    g_runtimePlaying = false;
    DebugCamera_SetEnabled(false);
    SyncSceneRuntime();
}

void EngineGUI_SetEnabled(bool /*enabled*/) {
}

void EngineGUI_GetGameViewportRect(float& x, float& y, float& width, float& height) {
    x = 0.0f;
    y = 0.0f;
    width = static_cast<float>(GetBackBufferWidth());
    height = static_cast<float>(GetBackBufferHeight());
}

bool EngineGUI_SaveToFile(const char* filename) {
    return SceneLoader_SaveToFile(filename);
}

bool EngineGUI_LoadFromFile(const char* filename) {
    if (!SceneLoader_LoadFromFile(filename))
        return false;
    EnterRuntimePlay();
    return true;
}

const std::string& EngineGUI_GetDefaultScenePath() {
    return SceneLoader_GetDefaultScenePath();
}

CameraMode EngineGUI_GetCameraMode() {
    return g_cameraMode;
}

void EngineGUI_SetCameraMode(CameraMode mode) {
    g_cameraMode = mode;
}

const std::string& EngineGUI_GetActiveCameraNodeName() {
    return g_activeCameraNode;
}

void EngineGUI_SetActiveCameraNodeName(const std::string& name) {
    g_activeCameraNode = name;
}

bool EngineGUI_IsUiOverlayVisible() {
    return true;
}

void EngineGUI_SetUiOverlayVisible(bool /*visible*/) {
}

EditorEditMode EngineGUI_GetEditMode() {
    return EditorEditMode::Scene3D;
}

void EngineGUI_SetEditMode(EditorEditMode /*mode*/) {
}

bool EngineGUI_IsUiEditMode() {
    return false;
}

bool EngineGUI_MouseToViewportLocal(float& x, float& y) {
    int mx = 0;
    int my = 0;
    Mouse_GetPosition(mx, my);
    x = static_cast<float>(mx);
    y = static_cast<float>(my);
    return x >= 0.0f && y >= 0.0f
        && x <= static_cast<float>(GetBackBufferWidth())
        && y <= static_cast<float>(GetBackBufferHeight());
}

bool EngineGUI_IsSceneGraphObject(GameObject* obj) {
    return SceneLoader_IsSceneGraphObject(obj);
}

void DrawEditorSceneProxies() {
}
