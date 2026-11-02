// runtime_test_world_apply.h
// Main-thread world-manipulation apply for the Debug GAME_RUNTIME test bridge.

#pragma once

#if defined(_DEBUG) && defined(GAME_RUNTIME)
#include "engine/testing/runtime_test_bridge.h"
#include <string>

bool RuntimeTestWorld_Apply(const ivy::testing::WorldOpRequest& op, std::string& errorOut);
#endif
