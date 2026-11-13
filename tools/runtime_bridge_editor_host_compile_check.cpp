// runtime_bridge_editor_host_compile_check.cpp
// Linux/Windows compile probe for Editor Debug bridge host wiring.
// Build with -D_DEBUG and WITHOUT GAME_RUNTIME (matches IvyEngine.vcxproj Debug).
// Catches gate mismatches such as:
//   - RuntimeTestWorld_Apply used under IVY_RUNTIME_TEST_BRIDGE_AVAILABLE but
//     declared only under _DEBUG && GAME_RUNTIME
//   - CommandLineToArgvW used without <shellapi.h>

#include "engine/testing/runtime_test_bridge.h"

#ifndef IVY_RUNTIME_TEST_BRIDGE_AVAILABLE
#error "Debug Editor must define IVY_RUNTIME_TEST_BRIDGE_AVAILABLE"
#endif

#include "src/app/runtime_test_world_apply.h"
#include "src/app/runtime_test_editor_apply.h"
#include <shellapi.h>
#include <string>

// Force the declarations to be usable (same refs main.cpp needs).
static bool (*g_worldApply)(const ivy::testing::WorldOpRequest&, std::string&) =
    &RuntimeTestWorld_Apply;
static bool (*g_editorApply)(const ivy::testing::EditorOpRequest&, std::string&) =
    &RuntimeTestEditor_Apply;

int main() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    (void)argv;
    (void)g_worldApply;
    (void)g_editorApply;
    // Provide definitions so the TU links without the full engine.
    return (g_worldApply && g_editorApply) ? 0 : 1;
}

// Local stubs: this check validates declarations/includes, not apply bodies.
bool RuntimeTestWorld_Apply(const ivy::testing::WorldOpRequest&, std::string& errorOut) {
    errorOut.clear();
    return true;
}
bool RuntimeTestEditor_Apply(const ivy::testing::EditorOpRequest&, std::string& errorOut) {
    errorOut.clear();
    return true;
}
