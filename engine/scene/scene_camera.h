#pragma once
#include <memory>
#include <vector>
#include <string>

class GameObject;

// When CameraMode::SceneCamera is active, copy the named CameraComponent node's
// transform (+Z forward, +Y up) into the global Camera.
void SceneCameraSyncFromActiveNode(const std::vector<std::unique_ptr<GameObject>>& roots);

// Resolves only a unique enabled camera; empty means missing or ambiguous.
std::string SceneCameraResolveDefault(const std::vector<std::unique_ptr<GameObject>>& roots, const std::string& preferred);

// Initialize the selected debug camera for play; editor navigation remains independent.
void SceneCameraBeginPlay();
