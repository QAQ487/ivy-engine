/**===========================================================================

 File   :  [scene_manager.h]

 Author : Terry Yeung
 Date   : 2025/9/19
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H
#include <algorithm>
#include <vector>

class GameObject;

class SceneBase {
protected:
	std::vector<GameObject*> m_objects;

public:
	virtual ~SceneBase() = default;
	virtual void Initialize() = 0;
	virtual void Finalize() = 0;
	virtual void Update(double elapsed_time) = 0;
	virtual void Draw() = 0;

	void AddObject(GameObject* obj) {
		if (obj && std::find(m_objects.begin(), m_objects.end(), obj) == m_objects.end()) {
			m_objects.push_back(obj);
		}
	}
	void RemoveObject(GameObject* obj) {
		m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());
	}
	void ClearObjects() { m_objects.clear(); }
	const std::vector<GameObject*>& GetObjects() const { return m_objects; }
};

class SceneManager {
public:
	static SceneManager& Instance();

	void ChangeScene(SceneBase* pNextScene);
	void DestroyCurrentScene();
	// When false, Finalize() still runs but the scene shell is not deleted (app exit only).
	void SetDeleteSceneAfterFinalize(bool deleteScene) { m_deleteSceneAfterFinalize = deleteScene; }
	void Update(double elapsed_time);
	void Draw();

	SceneBase* GetCurrentScene();

private:
	SceneManager();
	~SceneManager();
	SceneBase* m_CurrentScene = nullptr;
	bool m_deleteSceneAfterFinalize = true;
};
#endif // !SCENE_MANAGER_H
