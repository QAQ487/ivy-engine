/**===========================================================================

 File   :  [scene_manager.cpp]

 Author : Terry Yeung
 Date   : 2025/9/19
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "scene_manager.h"
#include "engine/physics/core/physics_system.h"

 /**===========================================================================
	???V???O???g???C???X?^???X??????
	?????@?F
	?????l?F
  ===========================================================================**/
SceneManager& SceneManager::Instance() {
	static SceneManager instance;
	return instance;
}

/**===========================================================================
   ???R???X?g???N?^?E?f?X?g???N?^??
   ?????@?F
   ?????l?F
 ===========================================================================**/
SceneManager::SceneManager() {}
SceneManager::~SceneManager() {
	DestroyCurrentScene();
}

/**===========================================================================
   ???V?[????X??
   ?????@?F
   ?????l?F
 ===========================================================================**/
void SceneManager::DestroyCurrentScene() {
	if (!m_CurrentScene) return;
	m_CurrentScene->Finalize();
	if (m_deleteSceneAfterFinalize)
		delete m_CurrentScene;
	m_CurrentScene = nullptr;
}

void SceneManager::ChangeScene(SceneBase* pNextScene) {
	PhysicsSystem_ClearTriggerPairs();
	DestroyCurrentScene();
	m_CurrentScene = pNextScene;
	if (m_CurrentScene) m_CurrentScene->Initialize();
}

/**===========================================================================
   ???X?V??????
   ?????@?F
   ?????l?F
 ===========================================================================**/
void SceneManager::Update(double elapsed_time) {
	if (m_CurrentScene) m_CurrentScene->Update(elapsed_time);
}

/**===========================================================================
   ???`???????
   ?????@?F
   ?????l?F
 ===========================================================================**/
void SceneManager::Draw() {
	if (m_CurrentScene) m_CurrentScene->Draw();
}

/**===========================================================================
   ?????????V?[????????
   ?????@?F
   ?????l?F
 ===========================================================================**/
SceneBase* SceneManager::GetCurrentScene() {
	return m_CurrentScene;
}