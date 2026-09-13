/**===========================================================================
 File   : scene_asset_preload.cpp
 Brief  : BuildTaskList + time-budgeted SceneAssetPreloader (main thread).
 ===========================================================================**/
#include "scene_asset_preload.h"

#include "engine/ecs/game_object.h"
#include "engine/ecs/model_asset_cache.h"
#include "engine/ecs/visual_component.h"
#include "engine/rendering/model/model.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace {

std::string ToLowerAscii(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::string NormalizePathKey(std::string path) {
	std::replace(path.begin(), path.end(), '\\', '/');
	return ToLowerAscii(std::move(path));
}

std::string FilenameLabel(const std::string& path) {
	if (path.empty())
		return "?";
	const std::filesystem::path p(path);
	const std::string name = p.filename().string();
	return name.empty() ? path : name;
}

void CollectVisual(GameObject* node,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex)
{
	if (!node)
		return;

	if (VisualComponent* vc = node->GetComponent<VisualComponent>()) {
		if (!vc->modelPath.empty()) {
			const std::string modelKey = NormalizePathKey(vc->modelPath);
			auto it = modelIndex.find(modelKey);
			size_t taskIdx = 0;
			if (it == modelIndex.end()) {
				SceneAssetPreloadTask task;
				task.modelPath = vc->modelPath;
				task.displayLabel = FilenameLabel(vc->modelPath);
				taskIdx = outTasks.size();
				modelIndex.emplace(modelKey, taskIdx);
				outTasks.push_back(std::move(task));
			}
			else {
				taskIdx = it->second;
			}

			SceneAssetPreloadTask& task = outTasks[taskIdx];
			std::unordered_set<std::string> seenClipKeys;
			for (const auto& existing : task.animations)
				seenClipKeys.insert(NormalizePathKey(existing.second));

			std::vector<std::pair<std::string, std::string>> pending;
			pending.reserve(vc->animationPaths.size());
			for (const auto& [name, path] : vc->animationPaths) {
				if (name.empty() || path.empty())
					continue;
				const std::string clipKey = NormalizePathKey(path);
				if (!seenClipKeys.insert(clipKey).second)
					continue;
				pending.emplace_back(name, path);
			}
			std::sort(pending.begin(), pending.end(),
				[](const auto& a, const auto& b) { return a.first < b.first; });
			for (auto& clip : pending)
				task.animations.push_back(std::move(clip));
		}
	}

	for (const auto& child : node->GetChildren())
		CollectVisual(child.get(), outTasks, modelIndex);
}

} // namespace

std::vector<SceneAssetPreloadTask> SceneAssetPreload_BuildTaskList(
	const std::vector<std::unique_ptr<GameObject>>& roots)
{
	std::vector<SceneAssetPreloadTask> tasks;
	std::unordered_map<std::string, size_t> modelIndex;
	for (const auto& root : roots) {
		if (root)
			CollectVisual(root.get(), tasks, modelIndex);
	}
	return tasks;
}

void SceneAssetPreloader::Begin(const std::vector<SceneAssetPreloadTask>& tasks) {
	Cancel();
	m_steps.clear();
	m_next = 0;
	m_total = 0;
	m_currentLabel.clear();

	for (const auto& task : tasks) {
		if (!task.modelPath.empty()) {
			Step step;
			step.kind = Step::Kind::Model;
			step.modelPath = task.modelPath;
			step.fileLabel = FilenameLabel(task.modelPath);
			m_steps.push_back(std::move(step));
		}
		for (const auto& [clipName, animPath] : task.animations) {
			if (clipName.empty() || animPath.empty())
				continue;
			Step step;
			step.kind = Step::Kind::Animation;
			step.modelPath = task.modelPath;
			step.clipName = clipName;
			step.animPath = animPath;
			step.fileLabel = FilenameLabel(animPath);
			m_steps.push_back(std::move(step));
		}
	}

	m_total = m_steps.size();
	m_active = m_total > 0;
	if (m_active)
		RebuildLabel();
}

void SceneAssetPreloader::Cancel() {
	m_active = false;
	m_steps.clear();
	m_next = 0;
	m_total = 0;
	m_currentLabel.clear();
}

float SceneAssetPreloader::Progress01() const {
	if (m_total == 0)
		return 1.0f;
	return static_cast<float>(m_next) / static_cast<float>(m_total);
}

void SceneAssetPreloader::RebuildLabel() {
	if (!m_active || m_next >= m_total) {
		m_currentLabel = "Loading\xe2\x80\xa6 done";
		return;
	}
	const Step& step = m_steps[m_next];
	m_currentLabel = "Loading\xe2\x80\xa6 " + step.fileLabel
		+ " (" + std::to_string(m_next + 1) + "/" + std::to_string(m_total) + ")";
}

bool SceneAssetPreloader::RunOneStep() {
	if (m_next >= m_total)
		return false;

	const Step& step = m_steps[m_next];
	RebuildLabel();

	if (step.kind == Step::Kind::Model) {
		Model3D* model = ModelAssetCache::Instance().GetModel(step.modelPath);
		if (!model) {
			OutputDebugStringA(("[SceneAssetPreload] GetModel failed: "
				+ step.modelPath + "\n").c_str());
		}
	}
	else {
		Model3D* shared = ModelAssetCache::Instance().GetModel(step.modelPath);
		if (!shared) {
			OutputDebugStringA(("[SceneAssetPreload] EnsureAnimation skipped (no model): "
				+ step.modelPath + " / " + step.animPath + "\n").c_str());
		}
		else {
			shared->EnsureAnimation(step.clipName, step.animPath.c_str());
			if (!shared->HasAnimation(step.clipName)) {
				OutputDebugStringA(("[SceneAssetPreload] EnsureAnimation failed: "
					+ step.clipName + " -> " + step.animPath + "\n").c_str());
			}
		}
	}

	++m_next;
	return true;
}

bool SceneAssetPreloader::Tick() {
	if (!m_active)
		return true;

	LARGE_INTEGER freq{};
	LARGE_INTEGER start{};
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	const LONGLONG budgetTicks =
		start.QuadPart + static_cast<LONGLONG>(0.008 * static_cast<double>(freq.QuadPart));

	while (m_next < m_total) {
		RunOneStep();
		LARGE_INTEGER now{};
		QueryPerformanceCounter(&now);
		if (now.QuadPart >= budgetTicks)
			break;
	}

	if (m_next >= m_total) {
		m_active = false;
		m_currentLabel.clear();
		return true;
	}

	RebuildLabel();
	return false;
}
