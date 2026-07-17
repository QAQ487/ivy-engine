/**===========================================================================

 File   :  [scene_transition.cpp]

 Author : Terry
 Date   : 2026/6/11
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "scene_transition.h"

#include "engine/scripting/script_factory.h"

namespace {

using InitFn = void(*)();
using UpdateFn = void(*)(double);
using DrawFn = void(*)();
using FinalizeFn = void(*)();
using TryBeginFn = bool(*)(const char*, bool);
using TryHandleFn = bool(*)(const char*, const char*, bool);
using BoolFn = bool(*)();

template <typename Fn>
Fn ResolveExport(const char* exportName) {
    return reinterpret_cast<Fn>(ScriptFactory::Instance().ResolveDllExport(exportName));
}

} // namespace

void SceneTransition_Initialize() {
    if (InitFn fn = ResolveExport<InitFn>("SceneTransition_Initialize"))
        fn();
}

void SceneTransition_Finalize() {
    if (FinalizeFn fn = ResolveExport<FinalizeFn>("SceneTransition_Finalize"))
        fn();
}

bool SceneTransition_IsActive() {
    if (BoolFn fn = ResolveExport<BoolFn>("SceneTransition_IsActive"))
        return fn();
    return false;
}

bool SceneTransition_BlocksSceneUpdate() {
    if (BoolFn fn = ResolveExport<BoolFn>("SceneTransition_BlocksSceneUpdate"))
        return fn();
    return false;
}

bool SceneTransition_TryBegin(const std::string& targetScene, bool playOnLoad) {
    if (targetScene.empty())
        return false;
    if (TryBeginFn fn = ResolveExport<TryBeginFn>("SceneTransition_TryBegin"))
        return fn(targetScene.c_str(), playOnLoad);
    return false;
}

bool SceneTransition_TryHandleSceneRequest(const std::string& currentScene,
                                           const std::string& nextScene,
                                           bool playOnLoad) {
    if (nextScene.empty())
        return false;
    if (TryHandleFn fn = ResolveExport<TryHandleFn>("SceneTransition_TryHandleSceneRequest"))
        return fn(currentScene.c_str(), nextScene.c_str(), playOnLoad);
    return false;
}

void SceneTransition_Update(double elapsed_time) {
    if (UpdateFn fn = ResolveExport<UpdateFn>("SceneTransition_Update"))
        fn(elapsed_time);
}

void SceneTransition_Draw() {
    if (DrawFn fn = ResolveExport<DrawFn>("SceneTransition_Draw"))
        fn();
}
