/**===========================================================================

 File   :  [scene_manager.cpp]

 Author : Terry Yeung
 Date   : 2025/9/19
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "scene_manager.h"
#include "engine/physics/core/physics_system.h"

 /**===========================================================================
	Singleton instance accessor.
	Args: none
	Returns: SceneManager reference
  ===========================================================================**/
SceneManager& SceneManager::Instance() {
	static SceneManager instance;
	return instance;
}

/**===========================================================================
   Constructor / destructor.
   Args: none
   Returns: none
 ===========================================================================**/
SceneManager::SceneManager() {}
SceneManager::~SceneManager() {
	DestroyCurrentScene();
}

/**===========================================================================
   Destroy the current scene.
   Args: none
   Returns: none
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
   Update the current scene.
   Args: elapsed_time - seconds since last frame
   Returns: none
 ===========================================================================**/
void SceneManager::Update(double elapsed_time) {
	if (m_CurrentScene) m_CurrentScene->Update(elapsed_time);
}

/**===========================================================================
   Draw the current scene.
   Args: none
   Returns: none
 ===========================================================================**/
void SceneManager::Draw() {
	if (m_CurrentScene) m_CurrentScene->Draw();
}

/**===========================================================================
   Get the current scene.
   Args: none
   Returns: SceneBase pointer (may be null)
 ===========================================================================**/
SceneBase* SceneManager::GetCurrentScene() {
	return m_CurrentScene;
}