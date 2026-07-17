/**===========================================================================
 File   : game_scene.h
 Author : Terry
 Date   : 2025/1/12
 Brief  : GameScene – manages the map, lighting, shadows, and the enemy pool
          drawn by EnemyManagerComponent / EnemySpawnComponent nodes added
          through the engine_gui Prefab tab.
 ===========================================================================**/
#ifndef GAME_H
#define GAME_H
#include "scene_manager.h"

class GameScene : public SceneBase {
private:
	enum RENDER_OBJECT_TYPE {
		RENDER_SHADOW,
		RENDER_MAIN,
		RENDER_BILLBOARD,
		RENDER_UI,
		RENDER_MAX
	};

public:
	void Initialize() override;
	void Finalize() override;
	void Update(double elapsed_time) override;
	void Draw() override;

	void SetRenderObject(RENDER_OBJECT_TYPE type);
	void Object_Draw();
};

#endif // !GAME_H
