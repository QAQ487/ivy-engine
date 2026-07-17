#pragma once
#include <memory>
#include <vector>

class GameObject;

// When CameraMode::SceneCamera is active, copy the named CameraComponent node's
// transform (+Z forward, +Y up) into the global Camera.
void SceneCameraSyncFromActiveNode(const std::vector<std::unique_ptr<GameObject>>& roots);
