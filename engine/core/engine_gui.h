/**===========================================================================
 File   :  [engine_gui.h]
 Brief  :  ImGui-based editor GUI for placing GameObjects in the scene.
           Toggle with F2.
 ===========================================================================**/
#ifndef ENGINE_GUI_H
#define ENGINE_GUI_H

#include <d3d11.h>
#include <string>

class GameObject;
bool EngineGUI_IsSceneGraphObject(GameObject* obj);

void EngineGUI_Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
void EngineGUI_Finalize();

void EngineGUI_Update(double elapsed_time);
void EngineGUI_Draw();
void EngineGUI_OnBackbufferResized();
// Wireframe overlay for the selected GameObject (mesh + collider), drawn over the 3D viewport.
void EngineGUI_DrawSelectionIndicator();

enum class PlayMode { None = 0, Viewport = 1, Fullscreen = 2 };

bool EngineGUI_IsPlaying();
PlayMode EngineGUI_GetPlayMode();
bool EngineGUI_IsDebugCameraPlayInputActive();
bool EngineGUI_IsOnStart();
// True while playing, or during a play→play scene switch before StartPlay resumes.
// Use for draw presentation only (not simulation).
bool EngineGUI_PreferPlayVisuals();
// None resumes the held mode during scene loads; otherwise starts viewport play.
void EngineGUI_StartPlay(PlayMode mode = PlayMode::None);
void EngineGUI_StopPlay();
// Deferred until the next host update: stop Editor Play or close Runtime.
void EngineGUI_RequestExitGame();
void EngineGUI_SetEnabled(bool enabled);
bool EngineGUI_IsEnabled();
void EngineGUI_GetGameViewportRect(float& x, float& y, float& width, float& height);
bool EngineGUI_MouseToViewportLocal(float& x, float& y);

bool EngineGUI_SaveToFile(const char* filename);
bool EngineGUI_LoadFromFile(const char* filename);
const std::string& EngineGUI_GetDefaultScenePath();
// Editor: last-opened scene from the active project, or empty to use default scene path.
std::string EngineGUI_ResolveStartupScenePath();

// Editor scene dirty-state (unsaved changes).
// Dirty true → show UnsavedChangesPopup when switching scenes / exiting.
bool EngineGUI_IsSceneDirty();
void EngineGUI_MarkSceneDirty();
void EngineGUI_ClearSceneDirty();

// Queues an editor action behind the unsaved-changes popup.
void EngineGUI_QueueUnsavedLoad(const char* filename);
void EngineGUI_QueueUnsavedExit();

void DrawEditorSceneProxies();
void DrawSceneModels();
// One-shot box fit for a newly created mesh object.
void EngineGUI_FitNewCollider(GameObject* node);

enum class CameraMode { ThirdPerson = 0, SceneCamera = 1, Intro = 2, FirstPerson = 3 };
enum class EditorEditMode { Scene3D = 0, UI = 1 };

CameraMode         EngineGUI_GetCameraMode();
void               EngineGUI_SetCameraMode(CameraMode mode);
const std::string& EngineGUI_GetActiveCameraNodeName();
void               EngineGUI_SetActiveCameraNodeName(const std::string& name);

bool EngineGUI_IsUiOverlayVisible();
void EngineGUI_SetUiOverlayVisible(bool visible);

bool EngineGUI_IsSceneAssetPreloadActive();

EditorEditMode EngineGUI_GetEditMode();
void EngineGUI_SetEditMode(EditorEditMode mode);
bool EngineGUI_IsUiEditMode();

#endif // ENGINE_GUI_H

// Called on the main thread after the editor frame has been presented.
void EngineGUI_NotifyFramePresented();
