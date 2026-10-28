#include "engine/testing/runtime_test_bridge.h"
#include "engine/input/keyboard.h"
#ifdef IVY_RUNTIME_TEST_BRIDGE_AVAILABLE
#error Non-runtime-Debug builds must not contain a Runtime Test Bridge
#endif
#include <iostream>
template<class T> concept HasTestInjection = requires(T keys) { Keyboard_TestInject(keys, 1); };
static_assert(!HasTestInjection<Keyboard_Keys*>, "test injection must be absent");
int main() {
    Keyboard_Initialize();
    Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0);
    if(!Keyboard_IsKeyDown(KK_W)) return 1;
    Keyboard_ProcessMessage(WM_KEYUP, 'W', 0);
    if(Keyboard_IsKeyDown(KK_W)) return 1;
#ifdef GAME_RUNTIME
    std::cout << "PASS Release compile-out (bridge/injection API absent), normal physical keyboard\n";
#else
    std::cout << "PASS Editor compile-out (bridge/injection API absent), normal physical keyboard\n";
#endif
}
