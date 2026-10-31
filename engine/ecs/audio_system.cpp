// audio_system.cpp
// ECS: audio system implementation.

#include "audio_system.h"
#include "audio_component.h"

#include <algorithm>
#include <vector>

namespace {

std::vector<AudioComponent*> g_audioComponents;

} // namespace

void AudioSystem_Register(AudioComponent* component) {
	if (!component) return;
	if (std::find(g_audioComponents.begin(), g_audioComponents.end(), component)
		!= g_audioComponents.end())
		return;
	g_audioComponents.push_back(component);
}

void AudioSystem_Unregister(AudioComponent* component) {
	if (!component) return;
	g_audioComponents.erase(
		std::remove(g_audioComponents.begin(), g_audioComponents.end(), component),
		g_audioComponents.end());
}

void AudioSystem_FlushCommands() {
	for (AudioComponent* c : g_audioComponents) {
		if (c)
			c->FlushCommands();
	}
}
