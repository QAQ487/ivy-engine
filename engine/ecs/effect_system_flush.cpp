// effect_system_flush.cpp
// ECS: effect system flush implementation.

#include "effect_system_flush.h"
#include "effect_component.h"

#include <algorithm>
#include <vector>

namespace {

std::vector<EffectComponent*> g_effectComponents;

} // namespace

void EffectSystem_RegisterComponent(EffectComponent* component) {
	if (!component) return;
	if (std::find(g_effectComponents.begin(), g_effectComponents.end(), component)
		!= g_effectComponents.end())
		return;
	g_effectComponents.push_back(component);
}

void EffectSystem_UnregisterComponent(EffectComponent* component) {
	if (!component) return;
	g_effectComponents.erase(
		std::remove(g_effectComponents.begin(), g_effectComponents.end(), component),
		g_effectComponents.end());
}

void EffectSystem_FlushCommands() {
	for (EffectComponent* c : g_effectComponents) {
		if (c)
			c->FlushCommands();
	}
}
