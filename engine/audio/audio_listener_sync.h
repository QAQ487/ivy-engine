#pragma once

#include <memory>
#include <vector>

class GameObject;

void AudioListenerSync_Tick(const std::vector<std::unique_ptr<GameObject>>& roots);
void AudioListenerSync_EnsureOnCamera(GameObject* cameraNode);
