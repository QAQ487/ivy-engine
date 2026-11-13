// runtime_test_editor_apply.h
// Main-thread apply for Editor Play-mode bridge ops (Phase 7).

#pragma once

#include "engine/testing/runtime_test_bridge.h"

#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
bool RuntimeTestEditor_Apply(const ivy::testing::EditorOpRequest& op, std::string& errorOut);
#endif
