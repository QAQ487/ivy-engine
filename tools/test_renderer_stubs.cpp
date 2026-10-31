// test_renderer_stubs.cpp
// Stub renderer symbols for tool/check binaries.

float GetClientWidth() { return 1280.0f; }
float GetClientHeight() { return 720.0f; }
float GetBackBufferWidth() { return 1280.0f; }
float GetBackBufferHeight() { return 720.0f; }
void Renderer_ClientToBackbuffer(float cx, float cy, float& bx, float& by) { bx = cx; by = cy; }
