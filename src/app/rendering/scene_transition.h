/**===========================================================================

 File   :  [scene_transition.h]

 Author : Terry
 Date   : 2026/6/11
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef SCENE_TRANSITION_H
#define SCENE_TRANSITION_H

#include <string>

void SceneTransition_Initialize();
void SceneTransition_Finalize();
void SceneTransition_Update(double elapsed_time);
void SceneTransition_Draw();

bool SceneTransition_IsActive();
bool SceneTransition_BlocksSceneUpdate();
bool SceneTransition_TryBegin(const std::string& targetScene, bool playOnLoad);
bool SceneTransition_TryHandleSceneRequest(const std::string& currentScene,
                                           const std::string& nextScene,
                                           bool playOnLoad);

#endif // !SCENE_TRANSITION_H
