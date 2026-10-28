#include "engine/testing/runtime_test_bridge.h"
#include "engine/input/controller.h"
#include <iostream>
#include <windows.h>

using namespace ivy::testing;

static void check(bool b, const char* label) {
    if (!b) {
        std::cerr << "FAIL " << label << std::endl;
        std::exit(1);
    }
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
        check(h != INVALID_HANDLE_VALUE, "connect controller peer");
    }
    ~Peer() { close(); }
    void close() {
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    }
    Json request(const char* verb, Json payload = Json::object()) {
        payload["protocolVersion"] = 1;
        payload["requestId"] = ++id;
        payload["runId"] = "controller-check";
        payload["verb"] = verb;
        const auto frame = payload.dump() + "\n";
        DWORD n = 0;
        check(WriteFile(h, frame.data(), static_cast<DWORD>(frame.size()), &n, nullptr) != FALSE, "send controller request");
        std::string result;
        char c = 0;
        while (ReadFile(h, &c, 1, &n, nullptr) && n == 1 && c != '\n') {
            result += c;
        }
        check(c == '\n', "read controller reply");
        return Json::parse(result);
    }
};

int main() {
    Controller_Initialize();

    SetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE", "1");
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID", "controller-check");
    Bridge b;
    check(b.StartFromEnvironment(), "controller bridge starts");
    b.PublishAndTakeQuit(true, true, "fixture");

    Peer p(b);

    // Verify hello capabilities includes controller
    const auto hello = p.request("hello");
    bool hasController = false;
    if (hello.contains("capabilities") && hello["capabilities"].is_array()) {
        for (const auto& cap : hello["capabilities"]) {
            if (cap == "controller") {
                hasController = true;
                break;
            }
        }
    }
    check(hasController, "hello capabilities includes controller");

    // Request controller with lx0 ly1 updates3 must return ok true status queued
    const auto resp = p.request("controller", {
        {"lx", 0},
        {"ly", 1},
        {"updates", 3}
    });
    check(resp.value("ok", false) == true, "controller request ok true");
    check(resp.value("status", "") == "queued", "controller request status queued");

    // After queued updates3: b.PollAndApplyInput(true); assert injected active and GetLeftStick0 zero preparation; b.FinishInputUpdate();
    b.PollAndApplyInput(true);
    check(Controller_IsInjectionActive(), "injected active during preparation");
    const auto prepStick = Controller_GetLeftStick(0);
    check(prepStick.x == 0.0f && prepStick.y == 0.0f, "GetLeftStick0 zero preparation");
    b.FinishInputUpdate();

    // then for3 loops Poll true assert left=(0,1), IsConnected0 true, right=(0,0), triggers0, buttons false; Finish.
    for (int i = 0; i < 3; ++i) {
        b.PollAndApplyInput(true);
        check(Controller_IsInjectionActive(), "injected active during hold");
        const auto left = Controller_GetLeftStick(0);
        check(left.x == 0.0f && left.y == 1.0f, "left stick is (0, 1)");
        check(Controller_IsConnected(0), "IsConnected0 true");
        const auto right = Controller_GetRightStick(0);
        check(right.x == 0.0f && right.y == 0.0f, "right stick is (0, 0)");
        check(Controller_GetLeftTrigger(0) == 0.0f, "left trigger 0");
        check(Controller_GetRightTrigger(0) == 0.0f, "right trigger 0");
        check(!Controller_IsPressed(ControllerButton::A, 0), "button A not pressed");
        check(!Controller_IsTrigger(ControllerButton::A, 0), "button A not trigger");
        check(!Controller_IsRelease(ControllerButton::A, 0), "button A not release");
        check(!Controller_IsPressed(ControllerButton::Start, 0), "button Start not pressed");
        b.FinishInputUpdate();
    }

    // Next Poll assert neutral then Finish; status.controller completed remaining0.
    b.PollAndApplyInput(true);
    check(Controller_IsInjectionActive(), "isolating during release");
    const auto neutralStick = Controller_GetLeftStick(0);
    check(neutralStick.x == 0.0f && neutralStick.y == 0.0f, "release neutral left stick");
    b.FinishInputUpdate();

    const auto statusCompleted = p.request("status");
    check(statusCompleted["controller"]["status"] == "completed", "status.controller completed");
    check(statusCompleted["controller"]["remainingUpdates"] == 0, "status.controller remaining0");

    // Subsequent Poll clears active.
    b.PollAndApplyInput(true);
    check(!Controller_IsInjectionActive(), "subsequent Poll clears active");
    b.FinishInputUpdate();

    // Send invalid raw payload lx2, lx bool, ly string, updates0, updates121, extra field; require ok false
    const auto errLx2 = p.request("controller", {{"lx", 2.0}, {"ly", 0.0}, {"updates", 1}});
    check(errLx2.value("ok", true) == false, "reject lx2");

    const auto errLxBool = p.request("controller", {{"lx", true}, {"ly", 0.0}, {"updates", 1}});
    check(errLxBool.value("ok", true) == false, "reject lx bool");

    const auto errLyString = p.request("controller", {{"lx", 0.0}, {"ly", "1"}, {"updates", 1}});
    check(errLyString.value("ok", true) == false, "reject ly string");

    const auto errUpdates0 = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"updates", 0}});
    check(errUpdates0.value("ok", true) == false, "reject updates0");

    const auto errUpdates121 = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"updates", 121}});
    check(errUpdates121.value("ok", true) == false, "reject updates121");

    const auto errExtraField = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"updates", 1}, {"extra", 123}});
    check(errExtraField.value("ok", true) == false, "reject extra field");

    // Test queue_full by two valid commands back-to-back, then close peer and bounded loop Poll/Finish until cleared/cancelled <=2sec; verify neutral and inactive after settling.
    const auto cmd1 = p.request("controller", {{"lx", 0.5}, {"ly", -0.5}, {"updates", 60}});
    check(cmd1.value("ok", false) == true, "first command queued");
    check(cmd1.value("status", "") == "queued", "first command status queued");

    const auto cmd2 = p.request("controller", {{"lx", -0.5}, {"ly", 0.5}, {"updates", 10}});
    check(cmd2.value("ok", true) == false, "second command rejected");
    check(cmd2.value("error", "") == "queue_full", "second command queue_full");

    p.close();

    const auto deadline = GetTickCount64() + 2000;
    bool settled = false;
    while (GetTickCount64() <= deadline) {
        b.PollAndApplyInput(true);
        const bool active = Controller_IsInjectionActive();
        b.FinishInputUpdate();
        if (!active) {
            settled = true;
            break;
        }
        Sleep(5);
    }
    check(settled, "cleared/cancelled <=2sec");
    check(!Controller_IsInjectionActive(), "controller inactive after settling");
    const auto stickSettled = Controller_GetLeftStick(0);
    check(stickSettled.x == 0.0f && stickSettled.y == 0.0f, "controller neutral after settling");

    // Add direct Protocol deterministic deadline expiry and not-playing cancel using existing APIs if simple.
    {
        Protocol proto("direct-protocol-check", 1234, 1);
        proto.Publish(true, true, "fixture");

        // Deterministic deadline expiry
        const auto reqExpiry = Json{
            {"protocolVersion", 1},
            {"requestId", 1},
            {"runId", "direct-protocol-check"},
            {"verb", "controller"},
            {"lx", 0.5},
            {"ly", 0.5},
            {"updates", 10}
        };
        const auto respExpiry = proto.Request(reqExpiry.dump());
        check(respExpiry.value("ok", false) == true, "direct protocol queued");
        check(!proto.ControllerIsolating(), "queued waits for owner thread before isolation");

        proto.ExpireController(GetTickCount64() + 60000);
        check(!proto.ControllerIsolating(), "direct protocol not isolating after deadline expiry");

        const auto stExpired = proto.Request(Json{
            {"protocolVersion", 1},
            {"requestId", 2},
            {"runId", "direct-protocol-check"},
            {"verb", "status"}
        }.dump());
        check(stExpired["controller"]["status"] == "cancelled", "direct protocol status cancelled on deadline expiry");

        // Not-playing cancel
        const auto reqNotPlaying = Json{
            {"protocolVersion", 1},
            {"requestId", 3},
            {"runId", "direct-protocol-check"},
            {"verb", "controller"},
            {"lx", -0.5},
            {"ly", -0.5},
            {"updates", 10}
        };
        const auto respNotPlaying = proto.Request(reqNotPlaying.dump());
        check(respNotPlaying.value("ok", false) == true, "direct protocol queued for not-playing test");
        check(!proto.ControllerIsolating(), "queued before not-playing cancellation");

        proto.BeginInput(false, GetTickCount64());
        check(!proto.ControllerIsolating(), "direct protocol not isolating after not-playing BeginInput");
        proto.EndInput();

        const auto stNotPlaying = proto.Request(Json{
            {"protocolVersion", 1},
            {"requestId", 4},
            {"runId", "direct-protocol-check"},
            {"verb", "status"}
        }.dump());
        check(stNotPlaying["controller"]["status"] == "cancelled", "direct protocol status cancelled when not playing");
    }

    b.Stop();
    std::cout << "All controller bridge checks PASSED\n";
    return 0;
}
