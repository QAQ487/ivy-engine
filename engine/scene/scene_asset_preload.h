/**===========================================================================
 File   : scene_asset_preload.h
 Brief  : Time-sliced warming of shared models + animation clips (main thread,
          D3D11 / LoadFBX). Editor builds tasks from loaded GameObjects; GAME_RUNTIME
          may also build tasks by scanning scene JSON files at boot.
 ===========================================================================**/
#ifndef SCENE_ASSET_PRELOAD_H
#define SCENE_ASSET_PRELOAD_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

class GameObject;

struct SceneAssetPreloadTask {
	std::string modelPath;
	// clip name -> FBX path (EnsureAnimation needs both)
	std::vector<std::pair<std::string, std::string>> animations;
	std::string displayLabel;
};

std::vector<SceneAssetPreloadTask> SceneAssetPreload_BuildTaskList(
	const std::vector<std::unique_ptr<GameObject>>& roots);

// Scan rom/scene (or given dir) *.json + prefabAssetPath targets; dedupe like BuildTaskList.
std::vector<SceneAssetPreloadTask> SceneAssetPreload_BuildTaskListFromSceneFiles(
	const std::string& sceneDirRelative = "rom/scene");

class SceneAssetPreloader {
public:
	// Process-lifetime instance for GAME_RUNTIME boot preload (editor uses its own).
	static SceneAssetPreloader& BootPreloader();

	void Begin(const std::vector<SceneAssetPreloadTask>& tasks);
	// Returns true when all tasks are done (or inactive). Call once per frame.
	bool Tick();
	float Progress01() const;
	const std::string& CurrentLabel() const { return m_currentLabel; }
	bool IsActive() const { return m_active; }
	void Cancel();

private:
	struct Step {
		enum class Kind { Model, Animation } kind = Kind::Model;
		std::string modelPath;
		std::string clipName;
		std::string animPath;
		std::string fileLabel;
	};

	void RebuildLabel();
	bool RunOneStep();

	bool m_active = false;
	std::vector<Step> m_steps;
	size_t m_next = 0;
	size_t m_total = 0;
	std::string m_currentLabel;
};

#endif // SCENE_ASSET_PRELOAD_H
