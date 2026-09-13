/**===========================================================================

 File   :  [scene_canvas.h]

 Author : Terry
 Date   : 2025/11/22
 -----------------------------------------------------------------------------

 ===========================================================================**/

#ifndef SCENE_CANVAS_H
#define SCENE_CANVAS_H
#include "ui_canvas.h"
#include "../../libraries/nlohmann/json.hpp"
#include <functional>
#include <string>

void SceneCanvas_Initialize();
void SceneCanvas_Finalize();
void SceneCanvas_Update(double elapsed_time);
void SceneCanvas_Draw();

// Returns the shared scene UI canvas so scripts can add their own elements.
UICanvas* SceneCanvas_GetCanvas();

void SceneCanvas_ClearSceneCanvas();
void SceneCanvas_LoadSceneCanvas(const nlohmann::json& canvasJson);
nlohmann::json SceneCanvas_SaveSceneCanvas();

// Single-element JSON helpers for editor undo (Phase 2).
std::string SceneCanvas_SerializeElement(const UIElement& element);
UIElement* SceneCanvas_InsertElementJson(const std::string& elementJson, int index);
int SceneCanvas_GetElementIndex(const UIElement* element);
void SceneCanvas_RemoveElementInternal(UIElement* element);

using SceneCanvasButtonActionBinder = std::function<void(UIButton*)>;
void SceneCanvas_RegisterButtonAction(const std::string& scriptName, SceneCanvasButtonActionBinder binder);
void SceneCanvas_BindButtonAction(UIButton* button);
void SceneCanvas_RequestSceneFile(const std::string& sceneFile, bool playOnLoad = false);
std::string SceneCanvas_ConsumeRequestedSceneFile(bool* playOnLoad = nullptr);

// Show/hide a red full-screen overlay (used during player respawn countdown).
void SceneCanvas_ShowRedOverlay();
void SceneCanvas_HideRedOverlay();
// Set the alpha of the red overlay (0.0 = fully transparent, 1.0 = fully opaque).
void SceneCanvas_SetRedOverlayAlpha(float alpha);
// Set scene timer text in seconds (displayed at top-middle of the screen).
void SceneCanvas_SetTimerSeconds(float seconds);

#endif // SCENE_CANVAS_H

