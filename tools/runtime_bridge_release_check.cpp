// runtime_bridge_release_check.cpp
// Validates runtime bridge release-build acceptance hooks.

#include "engine/testing/runtime_test_bridge.h"
#include "engine/input/keyboard.h"
#include "engine/input/controller.h"
#ifdef IVY_RUNTIME_TEST_BRIDGE_AVAILABLE
#error Non-runtime-Debug builds must not contain a Runtime Test Bridge
#endif
#include <iostream>
template<class T> concept HasTestInjection = requires(T keys) { Keyboard_TestInject(keys, 1); };
static_assert(!HasTestInjection<Keyboard_Keys*>, "test injection must be absent");
template<class T> concept HasControllerInjection = requires(T axis) { Controller_TestInjectLeftStick(axis, axis); };
static_assert(!HasControllerInjection<float>, "controller injection must be absent");
template<class T> concept HasRightStickInjection = requires(T axis) { Controller_TestInjectRightStick(axis, axis); };
static_assert(!HasRightStickInjection<float>, "right-stick injection must be absent");
template<class T> concept HasTriggerInjection = requires(T axis) { Controller_TestInjectTriggers(axis, axis); };
static_assert(!HasTriggerInjection<float>, "trigger injection must be absent");
template<class T> concept HasButtonInjection = requires(const T* buttons, int count) { Controller_TestInjectButtons(buttons, count); };
static_assert(!HasButtonInjection<ControllerButton>, "button injection must be absent");
template<class T> concept HasSampleInjection = requires(T axis, unsigned short buttons) {
    Controller_TestInjectSample(axis, axis, axis, axis, axis, axis, buttons);
};
static_assert(!HasSampleInjection<float>, "controller sample injection must be absent");
namespace ivy::testing {
template<class T>
concept HasRuntimeTestEventsRecord = requires(T p) {
    RuntimeTestEvents_RecordCheckpoint(p, p, 1);
};
// Dependent on T so missing ControllerProbeApiDetect yields false under MSVC
// (C3861) instead of a hard error from a non-dependent unqualified call.
template<class T>
concept HasControllerProbeApi = requires(T tag) {
    ControllerProbeApiDetect(tag);
};
}
static_assert(!ivy::testing::HasRuntimeTestEventsRecord<const char*>,
    "event recorder API must be compile-out absent");
static_assert(!ivy::testing::HasControllerProbeApi<ivy::testing::ControllerProbeApiTag>,
    "controller consumer probe API must be compile-out absent");
int main() {
    Keyboard_Initialize();
    Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0);
    if(!Keyboard_IsKeyDown(KK_W)) return 1;
    Keyboard_ProcessMessage(WM_KEYUP, 'W', 0);
    if(Keyboard_IsKeyDown(KK_W)) return 1;
#ifdef GAME_RUNTIME
    std::cout << "PASS Release compile-out (bridge/injection/events API absent), normal physical keyboard\n";
#else
    std::cout << "PASS Editor compile-out (bridge/injection/events API absent), normal physical keyboard\n";
#endif
}
