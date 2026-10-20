#include "engine/ecs/sprite_animator_component.h"
/**===========================================================================
 File   : scene_asset_preload.cpp
 Brief  : BuildTaskList (+ JSON scene scan) + time-budgeted SceneAssetPreloader.
 ===========================================================================**/
#include "scene_asset_preload.h"

#include "engine/ecs/game_object.h"
#include "engine/ecs/model_asset_cache.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/sprite_renderer_component.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/rendering/model/model.h"
#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace {

using json = nlohmann::json;

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

void MergeVisualPaths(const std::string& modelPath,
	const std::unordered_map<std::string, std::string>& animationPaths,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex)
{
	if (modelPath.empty())
		return;

	const std::string modelKey = NormalizePathKey(modelPath);
	auto it = modelIndex.find(modelKey);
	size_t taskIdx = 0;
	if (it == modelIndex.end()) {
		SceneAssetPreloadTask task;
		task.modelPath = modelPath;
		task.displayLabel = FilenameLabel(modelPath);
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
	pending.reserve(animationPaths.size());
	for (const auto& [name, path] : animationPaths) {
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

bool IsProjectRelativeRomPath(const std::string& path);

void MergeTexturePath(const std::string& texturePath,
    std::vector<SceneAssetPreloadTask>& outTasks,
    std::unordered_map<std::string, size_t>& assetIndex)
{
    if (!IsProjectRelativeRomPath(texturePath))
        return;
    const std::string key = "texture:" + NormalizePathKey(texturePath);
    if (assetIndex.find(key) != assetIndex.end())
        return;
    SceneAssetPreloadTask task;
    task.texturePath = texturePath;
    task.displayLabel = FilenameLabel(texturePath);
    assetIndex.emplace(key, outTasks.size());
    outTasks.push_back(std::move(task));
}

void CollectVisual(GameObject* node,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex)
{
	if (!node)
		return;

	if (VisualComponent* vc = node->GetComponent<VisualComponent>()) {
		MergeVisualPaths(vc->modelPath, vc->animationPaths, outTasks, modelIndex);
	}

    if(auto* animator=node->GetComponent<SpriteAnimatorComponent>())
        for(const auto& entry:animator->clips) MergeTexturePath(entry.second.texture,outTasks,modelIndex);
    if (auto* sprite = node->GetComponent<SpriteRendererComponent>())
        MergeTexturePath(sprite->texturePath, outTasks, modelIndex);

	for (const auto& child : node->GetChildren())
		CollectVisual(child.get(), outTasks, modelIndex);
}

bool IsProjectRelativeRomPath(const std::string& path) {
	if (path.empty())
		return false;
	std::string n = path;
	std::replace(n.begin(), n.end(), '\\', '/');
	if (n.size() >= 2 && std::isalpha(static_cast<unsigned char>(n[0])) && n[1] == ':')
		return false;
	if (!n.empty() && n[0] == '/')
		return false;
	const std::string key = ToLowerAscii(std::move(n));
	return key.compare(0, 4, "rom/") == 0;
}

bool TryReadJsonFile(const std::string& path, json& outRoot) {
	std::ifstream f(path, std::ios::binary);
	if (!f.is_open())
		return false;

	std::string content((std::istreambuf_iterator<char>(f)),
		std::istreambuf_iterator<char>());
	if (content.size() >= 3
		&& static_cast<unsigned char>(content[0]) == 0xEF
		&& static_cast<unsigned char>(content[1]) == 0xBB
		&& static_cast<unsigned char>(content[2]) == 0xBF)
	{
		content.erase(0, 3);
	}

	try {
		outRoot = json::parse(content);
	}
	catch (const json::parse_error&) {
		OutputDebugStringA(("[SceneAssetPreload] bad json: " + path + "\n").c_str());
		return false;
	}
	return true;
}

void WalkJsonNode(const json& node,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex,
	std::unordered_set<std::string>& visitedPrefabs);

void ScanPrefabFile(const std::string& prefabPath,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex,
	std::unordered_set<std::string>& visitedPrefabs)
{
	if (prefabPath.empty())
		return;

	const std::string key = NormalizePathKey(prefabPath);
	if (!visitedPrefabs.insert(key).second)
		return;

	json root;
	if (!TryReadJsonFile(prefabPath, root))
		return;

	if (!root.is_object())
		return;

	if (root.contains("nodes") && root["nodes"].is_array()) {
		for (const auto& nodeJson : root["nodes"]) {
			if (nodeJson.is_object())
				WalkJsonNode(nodeJson, outTasks, modelIndex, visitedPrefabs);
		}
		return;
	}

	if (root.contains("prefab") && root["prefab"].is_object()) {
		WalkJsonNode(root["prefab"], outTasks, modelIndex, visitedPrefabs);
		return;
	}

	WalkJsonNode(root, outTasks, modelIndex, visitedPrefabs);
}

void CollectVisualFromJson(const json& visual,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex)
{
	if (!visual.is_object())
		return;

	const std::string modelPath = visual.value("model", std::string());
	if (modelPath.empty())
		return;

	if (!IsProjectRelativeRomPath(modelPath)) {
		OutputDebugStringA(("[SceneAssetPreload] skip non-rom model path: "
			+ modelPath + "\n").c_str());
		return;
	}

	std::unordered_map<std::string, std::string> animationPaths;
	if (visual.contains("animations") && visual["animations"].is_object()) {
		for (auto it = visual["animations"].begin(); it != visual["animations"].end(); ++it) {
			if (!it.value().is_string())
				continue;
			const std::string clipName = it.key();
			const std::string animPath = it.value().get<std::string>();
			if (clipName.empty() || animPath.empty())
				continue;
			if (!IsProjectRelativeRomPath(animPath)) {
				OutputDebugStringA(("[SceneAssetPreload] skip non-rom anim path: "
					+ animPath + "\n").c_str());
				continue;
			}
			animationPaths.emplace(clipName, animPath);
		}
	}

	MergeVisualPaths(modelPath, animationPaths, outTasks, modelIndex);
}

void WalkJsonNode(const json& node,
	std::vector<SceneAssetPreloadTask>& outTasks,
	std::unordered_map<std::string, size_t>& modelIndex,
	std::unordered_set<std::string>& visitedPrefabs)
{
	if (!node.is_object())
		return;

	if (node.contains("prefabAssetPath") && node["prefabAssetPath"].is_string()) {
		ScanPrefabFile(node["prefabAssetPath"].get<std::string>(),
			outTasks, modelIndex, visitedPrefabs);
	}

	if (node.contains("components") && node["components"].is_object()) {
		const json& comps = node["components"];
        if(comps.contains("SpriteAnimator") && comps["SpriteAnimator"].is_object()) {
            const auto clips=comps["SpriteAnimator"].value("clips",json::object());
            if(clips.is_object()) for(const auto& clip:clips)
                if(clip.is_object()) MergeTexturePath(clip.value("texture",std::string{}),outTasks,modelIndex);
        }
        if (comps.contains("SpriteRenderer") && comps["SpriteRenderer"].is_object()) {
            const auto& sprite = comps["SpriteRenderer"];
            if (sprite.contains("texture") && sprite["texture"].is_string())
                MergeTexturePath(sprite["texture"].get<std::string>(), outTasks, modelIndex);
        }
		if (comps.contains("visual"))
			CollectVisualFromJson(comps["visual"], outTasks, modelIndex);
	}

	if (node.contains("children") && node["children"].is_array()) {
		for (const auto& child : node["children"])
			WalkJsonNode(child, outTasks, modelIndex, visitedPrefabs);
	}
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

std::vector<SceneAssetPreloadTask> SceneAssetPreload_BuildTaskListFromSceneFiles(
	const std::string& sceneDirRelative)
{
	std::vector<SceneAssetPreloadTask> tasks;
	std::unordered_map<std::string, size_t> modelIndex;
	std::unordered_set<std::string> visitedPrefabs;

	namespace fs = std::filesystem;
	std::error_code ec;
	if (!fs::exists(sceneDirRelative, ec))
		return tasks;

	for (const auto& entry : fs::recursive_directory_iterator(sceneDirRelative, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file(ec))
			continue;

		const fs::path path = entry.path();
		if (ToLowerAscii(path.extension().string()) != ".json")
			continue;

		json root;
		if (!TryReadJsonFile(path.string(), root))
			continue;
		if (!root.is_object())
			continue;

		if (root.contains("nodes") && root["nodes"].is_array()) {
			for (const auto& nodeJson : root["nodes"]) {
				if (nodeJson.is_object())
					WalkJsonNode(nodeJson, tasks, modelIndex, visitedPrefabs);
			}
		}
	}

	return tasks;
}

SceneAssetPreloader& SceneAssetPreloader::BootPreloader() {
	static SceneAssetPreloader s_boot;
	return s_boot;
}

void SceneAssetPreloader::Begin(const std::vector<SceneAssetPreloadTask>& tasks) {
	Cancel();
	m_steps.clear();
	m_next = 0;
	m_total = 0;
	m_currentLabel.clear();

    for (const auto& task : tasks) {
        if (!task.texturePath.empty()) {
            Step step;
            step.kind = Step::Kind::Texture;
            step.texturePath = task.texturePath;
            step.fileLabel = FilenameLabel(task.texturePath);
            m_steps.push_back(std::move(step));
        }
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

    if (step.kind == Step::Kind::Texture) {
        const auto path = std::filesystem::u8path(step.texturePath).wstring();
        if (TextureLoadQuiet(path.c_str(), true) < 0)
            OutputDebugStringA(("[SceneAssetPreload] texture load failed: "
                + step.texturePath + "\n").c_str());
    }
    else if (step.kind == Step::Kind::Model) {
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
