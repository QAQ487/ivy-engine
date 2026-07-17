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

bool EngineGUI_IsPlaying();
bool EngineGUI_IsDebugCameraPlayInputActive();
bool EngineGUI_IsOnStart();
void EngineGUI_StartPlay();
void EngineGUI_StopPlay();
void EngineGUI_SetEnabled(bool enabled);
void EngineGUI_GetGameViewportRect(float& x, float& y, float& width, float& height);
bool EngineGUI_MouseToViewportLocal(float& x, float& y);

bool EngineGUI_SaveToFile(const char* filename);
bool EngineGUI_LoadFromFile(const char* filename);
const std::string& EngineGUI_GetDefaultScenePath();

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

enum class CameraMode { ThirdPerson = 0, SceneCamera = 1, Intro = 2 };
enum class EditorEditMode { Scene3D = 0, UI = 1 };

CameraMode         EngineGUI_GetCameraMode();
void               EngineGUI_SetCameraMode(CameraMode mode);
const std::string& EngineGUI_GetActiveCameraNodeName();
void               EngineGUI_SetActiveCameraNodeName(const std::string& name);

bool EngineGUI_IsUiOverlayVisible();
void EngineGUI_SetUiOverlayVisible(bool visible);

EditorEditMode EngineGUI_GetEditMode();
void EngineGUI_SetEditMode(EditorEditMode mode);
bool EngineGUI_IsUiEditMode();

#endif // ENGINE_GUI_H
