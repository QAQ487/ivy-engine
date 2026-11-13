// runtime_test_world_apply.h
// Main-thread world-manipulation apply for the Debug test bridge
// (GAME_RUNTIME and Editor Play-mode hosts).

#pragma once

#include "engine/testing/runtime_test_bridge.h"

#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#include <string>

bool RuntimeTestWorld_Apply(const ivy::testing::WorldOpRequest& op, std::string& errorOut);
#endif
