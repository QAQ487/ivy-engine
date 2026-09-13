#include "engine_gui.h"
#include "engine_gui_internal.h"
#include "engine/scene/scene_loader.h"

bool EngineGUI_SaveToFile(const char* filename) {
    return SceneLoader_SaveToFile(filename);
}

bool EngineGUI_LoadFromFileCore(const char* filename) {
    if (!filename || !filename[0])
        return false;

    // Queue only: dim/gate this frame; JSON + preload run on next Update flush
    // so the modal can Present before the hitch.
    EngineGUI_QueueSceneLoad(filename);

    if (g_isPlaying)
        ReturnToStart();

    return true;
}

bool EngineGUI_LoadFromFile(const char* filename) {
    if (g_isPlaying)
        ReturnToStart();

    if (EngineGUI_IsSceneDirty()) {
        EngineGUI_QueueUnsavedLoad(filename);
        return false;
    }

    return EngineGUI_LoadFromFileCore(filename);
}
