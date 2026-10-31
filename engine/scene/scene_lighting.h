// scene_lighting.h
// Scene lighting setup and sync helpers.

#pragma once
#include <memory>
#include <vector>

class GameObject;

void SceneLightingSyncFromNodes(const std::vector<std::unique_ptr<GameObject>>& roots);
