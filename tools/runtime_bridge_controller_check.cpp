#include "engine/testing/runtime_test_bridge.h"
#include "engine/input/controller.h"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

using namespace ivy::testing;

namespace ivy::testing {
template<class T>
concept HasControllerProbeApi = requires(T tag) {
    ControllerProbeApiDetect(tag);
};
}
static_assert(HasControllerProbeApi<ControllerProbeApiTag>,
    "positive control: probe API detector must be true in Debug GAME_RUNTIME");

static void check(bool b, const char* label) {
    if (!b) {
        std::cerr << "FAIL " << label << std::endl;
        std::exit(1);
    }
}

static int g_consumerAssertions = 0;

static void consumerCheck(bool b, const char* label) {
    ++g_consumerAssertions;
    check(b, label);
}

static void consumerCheckOk(const ProbeValidation& v, const char* label) {
    ++g_consumerAssertions;
    if (!v.pass) {
        std::cerr << "FAIL " << label << ": " << v.error << std::endl;
        std::exit(1);
    }
}

// Real consumer path: inject -> Controller_Update -> public getters -> probe.
static void StepConsumer(Bridge& b, bool playing = true) {
    b.PollAndApplyInput(playing);
    Controller_Update();
    RuntimeTestControllerProbe_SampleFromGetters();
    b.FinishInputUpdate();
}

static std::vector<ControllerProbeFrame> TakeRecent(std::size_t count) {
    const auto snap = RuntimeTestControllerProbe_CopySnapshot();
    check(!snap.overflow, "probe ring must not overflow during controller_check");
    check(snap.frames.size() >= count, "probe has enough recent frames");
    return std::vector<ControllerProbeFrame>(snap.frames.end() - static_cast<std::ptrdiff_t>(count), snap.frames.end());
}

static const char* ButtonName(std::uint16_t bit) {
    if (bit == kInjectButtonA) return "A";
    if (bit == kInjectButtonB) return "B";
    if (bit == kInjectButtonX) return "X";
    if (bit == kInjectButtonY) return "Y";
    if (bit == kInjectButtonLB) return "LB";
    if (bit == kInjectButtonRB) return "RB";
    if (bit == kInjectButtonStart) return "START";
    if (bit == kInjectButtonBack) return "BACK";
    if (bit == kInjectButtonLeftThumb) return "LEFT_THUMB";
    if (bit == kInjectButtonRightThumb) return "RIGHT_THUMB";
    if (bit == kInjectButtonDpadUp) return "DPAD_UP";
    if (bit == kInjectButtonDpadDown) return "DPAD_DOWN";
    if (bit == kInjectButtonDpadLeft) return "DPAD_LEFT";
    if (bit == kInjectButtonDpadRight) return "DPAD_RIGHT";
    return "UNKNOWN";
}

struct Peer {
    HANDLE h = INVALID_HANDLE_VALUE;
    int& requestSequence;
    explicit Peer(const Bridge& b, int& sequence) : requestSequence(sequence) {
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
        payload["requestId"] = ++requestSequence;
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

    int requestSequence = 0;
    Peer p(b, requestSequence);

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

    // Right stick, triggers, and buttons share the controller queue. Left stick stays at the payload.
    const auto combined = p.request("controller", {
        {"lx", 0.0},
        {"ly", 0.0},
        {"rx", 0.5},
        {"ry", -1.0},
        {"lt", 0.25},
        {"rt", 1.0},
        {"buttons", Json::array({"A", "START", "LB", "DPAD_DOWN"})},
        {"updates", 2}
    });
    check(combined.value("ok", false) == true, "combined controller request ok");
    check(combined.value("status", "") == "queued", "combined controller request queued");

    b.PollAndApplyInput(true);
    check(Controller_IsInjectionActive(), "combined preparation isolates");
    check(Controller_GetLeftStick(0).x == 0.0f && Controller_GetLeftStick(0).y == 0.0f, "combined prep left neutral");
    check(Controller_GetRightStick(0).x == 0.0f && Controller_GetRightStick(0).y == 0.0f, "combined prep right neutral");
    check(Controller_GetLeftTrigger(0) == 0.0f && Controller_GetRightTrigger(0) == 0.0f, "combined prep triggers neutral");
    check(!Controller_IsPressed(ControllerButton::A, 0), "combined prep A up");
    check(!Controller_IsTrigger(ControllerButton::A, 0), "combined prep A has no press edge");
    b.FinishInputUpdate();

    b.PollAndApplyInput(true);
    check(Controller_GetLeftStick(0).x == 0.0f && Controller_GetLeftStick(0).y == 0.0f, "combined hold leaves left stick unchanged");
    check(Controller_GetRightStick(0).x == 0.5f && Controller_GetRightStick(0).y == -1.0f, "combined hold right stick");
    check(Controller_GetLeftTrigger(0) == 0.25f, "combined hold left trigger");
    check(Controller_GetRightTrigger(0) == 1.0f, "combined hold right trigger");
    check(Controller_IsPressed(ControllerButton::A, 0) && Controller_IsTrigger(ControllerButton::A, 0), "combined first hold A press edge");
    check(Controller_IsPressed(ControllerButton::Start, 0) && Controller_IsTrigger(ControllerButton::Start, 0), "combined first hold Start press edge");
    check(Controller_IsPressed(ControllerButton::LB, 0) && Controller_IsTrigger(ControllerButton::LB, 0), "combined first hold LB press edge");
    check(Controller_IsPressed(ControllerButton::DPadDown, 0) && Controller_IsTrigger(ControllerButton::DPadDown, 0), "combined first hold d-pad press edge");
    check(!Controller_IsPressed(ControllerButton::B, 0), "combined hold B stays up");
    check(!Controller_IsPressed(ControllerButton::RB, 0), "combined hold RB stays up");
    check(!Controller_IsPressed(ControllerButton::DPadUp, 0), "combined hold d-pad up stays up");
    b.FinishInputUpdate();

    b.PollAndApplyInput(true);
    check(Controller_IsPressed(ControllerButton::A, 0), "combined second hold A stays down");
    check(!Controller_IsTrigger(ControllerButton::A, 0), "combined second hold A is not a new edge");
    check(!Controller_IsRelease(ControllerButton::A, 0), "combined second hold A is not a release");
    check(Controller_GetLeftStick(0).x == 0.0f && Controller_GetLeftStick(0).y == 0.0f, "combined second hold left unchanged");
    check(Controller_GetRightStick(0).y == -1.0f, "combined second hold right stick");
    b.FinishInputUpdate();

    b.PollAndApplyInput(true);
    check(Controller_IsInjectionActive(), "combined release still isolates");
    check(!Controller_IsPressed(ControllerButton::A, 0) && Controller_IsRelease(ControllerButton::A, 0), "combined release A edge");
    check(Controller_IsRelease(ControllerButton::Start, 0), "combined release Start edge");
    check(Controller_IsRelease(ControllerButton::LB, 0), "combined release LB edge");
    check(Controller_IsRelease(ControllerButton::DPadDown, 0), "combined release d-pad edge");
    check(Controller_GetRightStick(0).x == 0.0f && Controller_GetRightStick(0).y == 0.0f, "combined release right neutral");
    check(Controller_GetLeftTrigger(0) == 0.0f && Controller_GetRightTrigger(0) == 0.0f, "combined release triggers neutral");
    check(Controller_GetLeftStick(0).x == 0.0f && Controller_GetLeftStick(0).y == 0.0f, "combined release left neutral");
    b.FinishInputUpdate();

    const auto combinedStatus = p.request("status");
    check(combinedStatus["controller"]["status"] == "completed", "combined status completed");
    check(combinedStatus["controller"]["remainingUpdates"] == 0, "combined remaining 0");
    b.PollAndApplyInput(true);
    check(!Controller_IsInjectionActive(), "combined injection cleared after completion");
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

    const auto errRx = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"rx", 1.1}, {"ry", 0.0}, {"updates", 1}});
    check(errRx.value("ok", true) == false, "reject rx above 1");

    const auto errRt = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"rt", true}, {"updates", 1}});
    check(errRt.value("ok", true) == false, "reject bool right trigger");

    const auto errLtLow = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"lt", -0.01}, {"updates", 1}});
    check(errLtLow.value("ok", true) == false, "reject left trigger below 0");

    const auto errButton = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"updates", 1}, {"buttons", Json::array({"LT"})}});
    check(errButton.value("ok", true) == false, "reject trigger name as a button");

    const auto errDup = p.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"updates", 1}, {"buttons", Json::array({"B", "B"})}});
    check(errDup.value("ok", true) == false, "reject duplicate button");

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

    // --- Consumer probe: getters after Controller_Update ---
    // Request IDs stay monotonic across peer reconnects for this whole Bridge run.
    {
        const int sequenceBeforeConsumer = requestSequence;
        Peer probePeer(b, requestSequence);
        consumerCheck(RuntimeTestControllerProbe_IsEnabled(), "controller probe enabled with bridge");

        const auto helloProbe = probePeer.request("hello");
        consumerCheck(helloProbe.value("requestId", 0) == sequenceBeforeConsumer + 1,
            "consumer peer continues run-wide request sequence");
        bool hasProbe = false;
        if (helloProbe.contains("capabilities") && helloProbe["capabilities"].is_array()) {
            for (const auto& cap : helloProbe["capabilities"]) {
                if (cap == "controller_probe") hasProbe = true;
            }
        }
        consumerCheck(hasProbe, "hello capabilities includes controller_probe");

        RuntimeTestControllerProbe_Reset();
        for (float value : {0.0f, 0.25f, 0.5f, 1.0f}) {
            const auto lt = probePeer.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"lt", value}, {"updates", 2}});
            consumerCheck(lt.value("ok", false), "queue LT consumer op");
            for (int i = 0; i < 4; ++i) StepConsumer(b);
            consumerCheckOk(ValidateTriggerHoldTrace(TakeRecent(4), true, value, 2), "LT consumer trace");

            const auto rt = probePeer.request("controller", {{"lx", 0.0}, {"ly", 0.0}, {"rt", value}, {"updates", 2}});
            consumerCheck(rt.value("ok", false), "queue RT consumer op");
            for (int i = 0; i < 4; ++i) StepConsumer(b);
            consumerCheckOk(ValidateTriggerHoldTrace(TakeRecent(4), false, value, 2), "RT consumer trace");
        }

        std::size_t buttonCount = 0;
        const std::uint16_t* bits = ProbeAllButtonBits(buttonCount);
        for (std::size_t i = 0; i < buttonCount; ++i) {
            const char* name = ButtonName(bits[i]);
            const auto resp = probePeer.request("controller", {
                {"lx", 0.0}, {"ly", 0.0}, {"updates", 2}, {"buttons", Json::array({name})}
            });
            consumerCheck(resp.value("ok", false), "queue button consumer op");
            for (int step = 0; step < 4; ++step) StepConsumer(b);
            consumerCheckOk(ValidateButtonHoldTrace(TakeRecent(4), bits[i], 2), "button consumer trace");
        }

        {
            const auto combined = probePeer.request("controller", {
                {"lx", 0.0}, {"ly", 0.0}, {"lt", 0.25}, {"rt", 1.0},
                {"buttons", Json::array({"A", "LB", "DPAD_UP"})}, {"updates", 2}
            });
            consumerCheck(combined.value("ok", false), "queue combined consumer op");
            for (int i = 0; i < 4; ++i) StepConsumer(b);
            const std::uint16_t mask = kInjectButtonA | kInjectButtonLB | kInjectButtonDpadUp;
            consumerCheckOk(ValidateCombinedTrace(TakeRecent(4), 0.25f, 1.0f, mask, 2), "combined consumer trace");
        }

        {
            const auto right = probePeer.request("controller", {
                {"lx", 0.0}, {"ly", 0.0}, {"rx", 0.5}, {"ry", -1.0}, {"updates", 2}
            });
            consumerCheck(right.value("ok", false), "queue right-stick consumer op");
            for (int i = 0; i < 4; ++i) StepConsumer(b);
            consumerCheckOk(ValidateRightStickKeepsLeftNeutral(TakeRecent(4), 0.5f, -1.0f, 2),
                "right-stick consumer keeps left stick neutral");
        }

        {
            RuntimeTestControllerProbe_Reset();
            const auto longOp = probePeer.request("controller", {
                {"lx", 0.0}, {"ly", 0.0}, {"lt", 1.0}, {"rt", 1.0},
                {"buttons", Json::array({"B", "START"})}, {"updates", 60}
            });
            consumerCheck(longOp.value("ok", false), "queue long op for disconnect cancel");
            StepConsumer(b); // prep
            StepConsumer(b); // first hold with press edge
            probePeer.close();
            const auto deadline = GetTickCount64() + 2000;
            bool settled = false;
            while (GetTickCount64() <= deadline) {
                StepConsumer(b);
                if (!Controller_IsInjectionActive()) {
                    settled = true;
                    break;
                }
                Sleep(1);
            }
            consumerCheck(settled, "disconnect cancel settled via consumer steps");
            // Extra neutral frames after release Update.
            for (int i = 0; i < 3; ++i) StepConsumer(b);
            const auto snap = RuntimeTestControllerProbe_CopySnapshot();
            consumerCheck(!snap.overflow, "cancel probe did not overflow");
            consumerCheckOk(ValidateEventualNeutralSuffix(snap.frames, 3),
                "cancel/disconnect consumer ends neutral with no stuck buttons/triggers");
        }

        // Read-only controller_probe verb returns the live getter snapshot.
        const int sequenceBeforeReader = requestSequence;
        Peer probeReader(b, requestSequence);
        RuntimeTestControllerProbe_Reset();
        const auto verbOp = probeReader.request("controller", {
            {"lx", 0.0}, {"ly", 0.0}, {"lt", 0.5}, {"updates", 1}
        });
        consumerCheck(verbOp.value("ok", false), "queue op before controller_probe verb");
        consumerCheck(verbOp.value("requestId", 0) == sequenceBeforeReader + 1,
            "reconnect peer continues run-wide request sequence");
        for (int i = 0; i < 3; ++i) StepConsumer(b);
        const auto probeReply = probeReader.request("controller_probe");
        consumerCheck(probeReply.value("ok", false), "controller_probe verb ok");
        consumerCheck(probeReply.contains("frames") && probeReply["frames"].is_array(), "controller_probe frames array");
        consumerCheck(probeReply.value("overflow", true) == false, "controller_probe overflow false");
        consumerCheck(probeReply["frames"].size() >= 3, "controller_probe has sampled frames");
        consumerCheck(probeReply["frames"].back().value("lt", -1.0) == 0.0, "controller_probe final frame lt neutral");
        const auto badProbe = probeReader.request("controller_probe", {{"extra", 1}});
        consumerCheck(badProbe.value("ok", true) == false, "controller_probe rejects extra fields");
    }

    // 3 setup + 16 trigger + 28 button + 2 combined + 2 right-stick + 4 cancel + 8 verb = 63
    constexpr int kExpectedConsumerAssertions = 63;
    check(g_consumerAssertions == kExpectedConsumerAssertions,
        "consumer assertions executed exactly once each");
    std::cout << "consumer assertions executed: " << g_consumerAssertions << "\n";

    b.Stop();
    check(!RuntimeTestControllerProbe_IsEnabled(), "controller probe disabled on bridge stop");
    std::cout << "All controller bridge checks PASSED\n";
    return 0;
}
