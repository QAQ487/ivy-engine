#include "engine/testing/runtime_test_bridge.h"
#ifdef IVY_RUNTIME_TEST_BRIDGE_AVAILABLE
#error Release must not contain a Runtime Test Bridge
#endif
#include <iostream>
int main() { std::cout << "PASS Release compile-out (bridge API absent)\n"; }
