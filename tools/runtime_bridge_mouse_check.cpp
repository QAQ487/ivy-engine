// runtime_bridge_mouse_check.cpp
// Validates runtime bridge mouse injection.

#include "engine/testing/runtime_test_bridge.h"
#include "engine/input/mouse.h"
#include <iostream>
#include <windows.h>

using namespace ivy::testing;
static void check(bool b, const char* label) {
    if (!b) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}

struct Peer {
    HANDLE h = INVALID_HANDLE_VALUE;
    int id = 0;
    explicit Peer(const Bridge& b) {
        const auto deadline = GetTickCount64() + 2000;
        do {
            h = CreateFileA(b.PipeName().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) break;
            Sleep(1);
        } while (GetTickCount64() < deadline);
        check(h != INVALID_HANDLE_VALUE, "connect mouse peer");
    }
    ~Peer() { close(); }
    void close() { if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    Json request(const char* verb, Json payload = Json::object()) {
        payload["protocolVersion"] = 1; payload["requestId"] = ++id; payload["runId"] = "mouse-check"; payload["verb"] = verb;
        const auto frame = payload.dump() + "\n";
        DWORD n = 0;
        check(WriteFile(h, frame.data(), static_cast<DWORD>(frame.size()), &n, nullptr) != FALSE, "send mouse request");
        std::string result; char c = 0;
        while (ReadFile(h, &c, 1, &n, nullptr) && n == 1 && c != '\n') result += c;
        check(c == '\n', "read mouse reply");
        return Json::parse(result);
    }
};

int main() {
    // 1. Direct unit verification of mouse injection, physical isolation, and release
    Mouse_Initialize();
    check(!Mouse_IsInjectionActive(), "init not active");
    
    // Simulate physical right click
    Mouse_ProcessMessage(WM_RBUTTONDOWN, 0, 0, nullptr);
    check(Mouse_IsRightDown(), "physical right click down");
    check(!Mouse_IsLeftDown(), "physical left not down");

    // Inject left click + movement + wheel
    Mouse_TestInject(10, -20, MOUSE_BUTTON_LEFT, 50);
    check(Mouse_IsInjectionActive(), "injection active");
    check(Mouse_IsLeftDown(), "injected left down");
    check(!Mouse_IsRightDown(), "physical right isolated while active");
    
    int dx = 0, dy = 0;
    Mouse_GetDelta(dx, dy);
    check(dx == 10 && dy == -20, "injected delta reported");
    check(Mouse_GetWheelDelta() == 50, "injected wheel delta reported");

    // Absolute cursor override (backbuffer pixels); does not call SetCursorPos
    Mouse_ClearInjection();
    Mouse_TestInject(0, 0, MOUSE_BUTTON_LEFT, 0, true, 320, 180);
    check(Mouse_IsInjectionActive(), "absolute injection active");
    int px = 0, py = 0;
    Mouse_GetPosition(px, py);
    check(px == 320 && py == 180, "Mouse_GetPosition returns injected absolute");
    check(Mouse_GetX() == 320 && Mouse_GetY() == 180, "GetX/GetY follow absolute inject");
    Mouse_TestInject(0, 0, MOUSE_BUTTON_NONE, 0, true, 11, 22);
    Mouse_GetPosition(px, py);
    check(px == 11 && py == 22, "absolute re-inject updates position");
    Mouse_ClearInjection();
    check(!Mouse_IsInjectionActive(), "absolute cleared with injection");

    // Physical message arrival while injection active does not overwrite injected state
    Mouse_TestInject(10, -20, MOUSE_BUTTON_LEFT, 50);
    Mouse_ProcessMessage(WM_LBUTTONUP, 0, 0, nullptr);
    check(Mouse_IsLeftDown(), "physical WM does not overwrite injected left");

    Mouse_ClearInjection();
    check(!Mouse_IsInjectionActive(), "injection cleared");
    check(Mouse_IsRightDown(), "physical right resumes after clear");

    // 2. Protocol bridge verification
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE", "1");
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID", "mouse-check");
    Bridge b;
    check(b.StartFromEnvironment(), "mouse bridge starts");
    b.PublishAndTakeQuit(true, true, "fixture");
    Peer p(b);

    // Bounded bounds check
    check(p.request("mouse", {{"dx", 601}, {"dy", 0}, {"buttons", Json::array()}, {"updates", 1}}).value("error", "") == "invalid_request", "reject dx>600");
    check(p.request("mouse", {{"dx", 0}, {"dy", -601}, {"buttons", Json::array()}, {"updates", 1}}).value("error", "") == "invalid_request", "reject dy<-600");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", {"INVALID"}}, {"updates", 1}}).value("error", "") == "invalid_request", "reject invalid button");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", {"LEFT", "LEFT"}}, {"updates", 1}}).value("error", "") == "invalid_request", "reject duplicate button");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"wheel", 121}, {"updates", 1}}).value("error", "") == "invalid_request", "reject wheel>120");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"updates", 0}}).value("error", "") == "invalid_request", "reject updates 0");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"updates", 121}}).value("error", "") == "invalid_request", "reject updates >120");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"updates", 1}, {"durationMs", 100}}).value("error", "") == "invalid_request", "reject updates+durationMs");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"durationMs", 0}}).value("error", "") == "invalid_request", "reject durationMs 0");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"durationMs", 30001}}).value("error", "") == "invalid_request", "reject durationMs >30000");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"updates", 1}, {"x", 1280}, {"y", 0}}).value("error", "") == "invalid_request", "reject absolute x at backbuffer width");
    check(p.request("mouse", {{"dx", 0}, {"dy", 0}, {"buttons", Json::array()}, {"updates", 1}, {"x", 0}}).value("error", "") == "invalid_request", "reject x without y over pipe");

    // Lifecycle: N=1 update with absolute cursor
    check(p.request("mouse", {{"dx", 30}, {"dy", -40}, {"buttons", {"LEFT", "MIDDLE"}}, {"wheel", 60}, {"updates", 1}, {"x", 400}, {"y", 300}}).value("ok", false), "N=1 mouse accepted with absolute");
    
    // Prep update: neutral (no delta, no buttons); absolute still applied
    b.PollAndApplyInput(true);
    check(Mouse_IsInjectionActive(), "active during prep");
    Mouse_GetDelta(dx, dy);
    check(dx == 0 && dy == 0, "neutral prep has 0 delta");
    check(!Mouse_IsLeftDown() && !Mouse_IsMiddleDown(), "neutral prep has 0 buttons");
    int absX = 0, absY = 0;
    Mouse_GetPosition(absX, absY);
    check(absX == 400 && absY == 300, "absolute during prep");
    b.FinishInputUpdate();

    // Active update 1
    b.PollAndApplyInput(true);
    check(Mouse_IsInjectionActive(), "active during hold");
    Mouse_GetDelta(dx, dy);
    check(dx == 30 && dy == -40, "active update has injected delta");
    check(Mouse_IsLeftDown() && Mouse_IsMiddleDown(), "active update has injected buttons");
    check(Mouse_GetWheelDelta() == 60, "active update has injected wheel");
    Mouse_GetPosition(absX, absY);
    check(absX == 400 && absY == 300, "absolute during hold");
    b.FinishInputUpdate();

    check(p.request("status")["mouse"]["status"] == "active", "status active before release");

    // Release update: neutral
    b.PollAndApplyInput(true);
    check(Mouse_IsInjectionActive(), "active during release");
    Mouse_GetDelta(dx, dy);
    check(dx == 0 && dy == 0, "neutral release has 0 delta");
    check(!Mouse_IsLeftDown() && !Mouse_IsMiddleDown(), "neutral release has 0 buttons");
    Mouse_GetPosition(absX, absY);
    check(absX == 400 && absY == 300, "absolute during release");
    check(p.request("status")["mouse"]["status"] == "releasing", "releasing status");
    b.FinishInputUpdate();

    check(p.request("status")["mouse"]["status"] == "completed", "completed status after release");

    // Subsequent frame: injection cleared
    b.PollAndApplyInput(true);
    check(!Mouse_IsInjectionActive(), "injection cleared after completion");
    b.FinishInputUpdate();

    // Disconnect cancellation
    check(p.request("mouse", {{"dx", 10}, {"dy", 10}, {"buttons", Json::array()}, {"updates", 100}}).value("ok", false), "queued accepted");
    p.close();
    Peer p2(b); p2.id = p.id;
    check(p2.request("status")["mouse"]["status"] == "cancelled", "disconnect cancels queued mouse");

    b.PollAndApplyInput(true);
    check(!Mouse_IsInjectionActive(), "cancelled not injected");
    b.FinishInputUpdate();

    b.Stop();
    std::cout << "All mouse bridge checks PASSED\n";
    return 0;
}
