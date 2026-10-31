#pragma once

#include "engine_gui.h"
#include "engine/scene/scene_loader.h"
#include "libraries/nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

class GameObject;
class UIElement;

#define g_sceneNodes SceneLoader_GetSceneNodes()

extern bool g_isPlaying;
extern bool g_sceneLoadPending;
extern GameObject* g_selNode;
extern UIElement* g_selUiElement;

void ApplyEditorColliders();
void RestoreEditorDebugColliderFlags();
void TeardownPlaySession(bool resetEditorState);
void ReturnToStart();
void SoftStopPlayForSceneLoad();

// Loads a scene from disk without the unsaved-changes dirty gate.
// Must be used only when caller already handled unsaved changes.
// Queues the load so Draw can Present a dim frame before JSON/preload work.
bool EngineGUI_LoadFromFileCore(const char* filename);
void EngineGUI_QueueSceneLoad(const char* filename);

// Editor scene-asset preload (time-sliced after queued load is flushed).
void EngineGUI_BeginSceneAssetPreload();
void EngineGUI_TickSceneAssetPreload();
void EngineGUI_FlushSceneLoadPending();
