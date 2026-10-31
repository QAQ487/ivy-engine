// audio_listener_sync.h
// Syncs the audio listener to a game object / camera.

#pragma once

#include <memory>
#include <vector>

class GameObject;

void AudioListenerSync_Tick(const std::vector<std::unique_ptr<GameObject>>& roots);
void AudioListenerSync_EnsureOnCamera(GameObject* cameraNode);
