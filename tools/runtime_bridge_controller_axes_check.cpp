// Platform-independent controller injection protocol checks.
// No Win32, XInput, or Direct3D headers.
#include "engine/testing/controller_inject_protocol.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace ivy::testing;

static int g_failures = 0;

static void check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL " << label << "\n";
        ++g_failures;
    }
}

static std::string Envelope(const std::string& body) {
    return std::string("{\"protocolVersion\":1,\"requestId\":1,\"runId\":\"axes\",\"verb\":\"controller\",") + body + "}";
}

static void ExpectInvalid(const std::string& json, const char* label) {
    ControllerCommand cmd;
    check(ParseControllerCommand(json, cmd) == ControllerParseStatus::Invalid, label);
}

int main() {
    ControllerCommand legacy;
    check(ParseControllerCommand(Envelope("\"lx\":0,\"ly\":1,\"updates\":3"), legacy) == ControllerParseStatus::Ok,
        "legacy lx/ly/updates accepted");
    check(legacy.updates == 3, "legacy updates");
    check(legacy.sample.lx == 0.0f && legacy.sample.ly == 1.0f, "legacy left stick");
    check(legacy.sample.rx == 0.0f && legacy.sample.ry == 0.0f, "legacy right stick defaults neutral");
    check(legacy.sample.lt == 0.0f && legacy.sample.rt == 0.0f, "legacy triggers default neutral");
    check(legacy.sample.buttons == 0, "legacy buttons default neutral");

    ControllerCommand right;
    check(ParseControllerCommand(
              Envelope("\"lx\":0,\"ly\":0,\"rx\":0.5,\"ry\":-1,\"updates\":2"), right) == ControllerParseStatus::Ok,
        "right stick accepted");
    check(right.sample.lx == 0.0f && right.sample.ly == 0.0f, "right-stick payload keeps left at zero");
    check(right.sample.rx == 0.5f && right.sample.ry == -1.0f, "right stick values");

    ControllerCommand triggers;
    check(ParseControllerCommand(Envelope("\"lx\":0,\"ly\":0,\"lt\":0.25,\"rt\":1,\"updates\":1"), triggers)
            == ControllerParseStatus::Ok,
        "triggers accepted");
    check(triggers.sample.lt == 0.25f && triggers.sample.rt == 1.0f, "trigger values");
    check(triggers.sample.rx == 0.0f && triggers.sample.ry == 0.0f, "omitted right stick stays zero");

    std::uint16_t bitA = 0;
    std::uint16_t bitStart = 0;
    std::uint16_t bitDpadUp = 0;
    check(ControllerButtonFromName("A", bitA) && ControllerButtonFromName("START", bitStart)
            && ControllerButtonFromName("DPAD_UP", bitDpadUp),
        "known button names");
    ControllerCommand buttons;
    check(ParseControllerCommand(
              Envelope("\"lx\":0,\"ly\":0,\"updates\":2,\"buttons\":[\"A\",\"START\",\"DPAD_UP\"]"), buttons)
            == ControllerParseStatus::Ok,
        "buttons accepted");
    check(buttons.sample.buttons == (bitA | bitStart | bitDpadUp), "button mask");
    check(buttons.sample.lx == 0.0f && buttons.sample.ly == 0.0f, "button payload keeps left at zero");

    ControllerCommand partial;
    check(ParseControllerCommand(Envelope("\"lx\":-1,\"ly\":1,\"lt\":0,\"updates\":120"), partial) == ControllerParseStatus::Ok,
        "boundary sticks and trigger accepted");
    check(partial.updates == 120 && partial.sample.lx == -1.0f && partial.sample.ly == 1.0f && partial.sample.lt == 0.0f,
        "boundary values");
    check(ParseControllerCommand(Envelope("\"lx\":1,\"ly\":0,\"buttons\":[],\"updates\":1"), partial) == ControllerParseStatus::Ok,
        "empty button list accepted");
    check(partial.sample.buttons == 0, "empty button list is neutral");

    ExpectInvalid(Envelope("\"lx\":1.1,\"ly\":0,\"updates\":1"), "reject lx above 1");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":-1.01,\"updates\":1"), "reject ly below -1");
    ExpectInvalid(Envelope("\"lx\":true,\"ly\":0,\"updates\":1"), "reject bool lx");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":\"1\",\"updates\":1"), "reject string ly");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":0"), "reject updates 0");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":121"), "reject updates 121");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1.5"), "reject fractional updates");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1,\"extra\":1"), "reject extra field");
    ExpectInvalid(Envelope("\"ly\":0,\"updates\":1"), "reject missing lx");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"rx\":1.01,\"ry\":0,\"updates\":1"), "reject rx above 1");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"rx\":0,\"ry\":true,\"updates\":1"), "reject bool ry");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"lt\":-0.01,\"updates\":1"), "reject lt below 0");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"rt\":1.01,\"updates\":1"), "reject rt above 1");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"lt\":false,\"updates\":1"), "reject bool trigger");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1,\"buttons\":\"A\""), "reject buttons string");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1,\"buttons\":[\"LT\"]"), "reject trigger name as button");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1,\"buttons\":[\"a\"]"), "reject lowercase button");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1,\"buttons\":[\"A\",\"A\"]"), "reject duplicate button");
    ExpectInvalid(Envelope("\"lx\":0,\"ly\":0,\"updates\":1,\"buttons\":[\"NOPE\"]"), "reject unknown button");
    ExpectInvalid("[]", "reject non-object");
    ExpectInvalid("{", "reject malformed json");

    {
        nlohmann::json q = nlohmann::json::parse(Envelope("\"lx\":0,\"ly\":0,\"updates\":1"));
        q["rx"] = std::numeric_limits<double>::quiet_NaN();
        ControllerCommand cmd;
        check(ParseControllerCommand(q, cmd) == ControllerParseStatus::Invalid, "reject NaN rx");
        q = nlohmann::json::parse(Envelope("\"lx\":0,\"ly\":0,\"updates\":1"));
        q["lt"] = std::numeric_limits<double>::infinity();
        check(ParseControllerCommand(q, cmd) == ControllerParseStatus::Invalid, "reject infinite lt");
    }

    {
        ControllerOperation op;
        check(op.TryQueue(7, right), "queue right stick");
        check(!op.Isolating(), "queued is not isolating yet");
        check(std::string(op.Status()) == "queued", "queued status");
        check(!op.TryQueue(8, legacy), "second command rejected while queued");

        ControllerInjectOverlay overlay;
        op.Begin(true);
        overlay.Apply(op.Sample());
        check(op.Isolating() && std::string(op.Status()) == "queued", "preparation status stays queued");
        check(!overlay.Active() || (overlay.LeftX() == 0.0f && overlay.RightX() == 0.0f), "prep sample neutral");
        check(overlay.RightX() == 0.0f && overlay.RightY() == 0.0f && overlay.LeftX() == 0.0f && overlay.LeftY() == 0.0f,
            "preparation injects neutral sticks");
        op.End();

        op.Begin(true);
        overlay.Apply(op.Sample());
        check(std::string(op.Status()) == "active", "first hold is active");
        check(overlay.LeftX() == 0.0f && overlay.LeftY() == 0.0f, "hold keeps left stick unchanged");
        check(overlay.RightX() == 0.5f && overlay.RightY() == -1.0f, "hold applies right stick");
        check(overlay.LeftTrigger() == 0.0f && overlay.RightTrigger() == 0.0f, "hold triggers stay zero");
        op.End();
        check(op.Remaining() == 1, "one update consumed");

        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.RightY() == -1.0f && overlay.LeftX() == 0.0f, "second hold still right only");
        op.End();
        check(op.Remaining() == 0, "budget exhausted while still active");

        op.Begin(true);
        overlay.Apply(op.Sample());
        check(std::string(op.Status()) == "releasing", "release phase");
        check(overlay.LeftX() == 0.0f && overlay.LeftY() == 0.0f && overlay.RightX() == 0.0f && overlay.RightY() == 0.0f,
            "release returns sticks to neutral");
        check(overlay.LeftTrigger() == 0.0f && overlay.RightTrigger() == 0.0f, "release returns triggers to neutral");
        op.End();
        check(std::string(op.Status()) == "completed", "completed after release");
        check(!op.Isolating(), "completed is not isolating");
        check(op.TryQueue(9, legacy), "queue accepted after completion");
    }

    {
        ControllerOperation op;
        ControllerInjectOverlay overlay;
        check(op.TryQueue(1, buttons), "queue buttons");
        op.Begin(true);
        overlay.Apply(op.Sample());
        op.End();
        check(!overlay.Pressed(bitA) && !overlay.PressedEdge(bitA) && !overlay.ReleasedEdge(bitA),
            "preparation has no button edges");

        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.Pressed(bitA) && overlay.Pressed(bitStart) && overlay.Pressed(bitDpadUp), "first hold presses buttons");
        check(overlay.PressedEdge(bitA) && overlay.PressedEdge(bitStart), "first hold is a press edge");
        check(!overlay.ReleasedEdge(bitA), "first hold is not a release");
        check(!overlay.Pressed(kInjectButtonB), "unlisted button stays up");
        check(overlay.LeftX() == 0.0f && overlay.LeftY() == 0.0f, "buttons do not move the left stick");
        op.End();

        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.Pressed(bitA) && !overlay.PressedEdge(bitA) && !overlay.ReleasedEdge(bitA),
            "later hold stays down without another edge");
        op.End();

        op.Begin(true);
        overlay.Apply(op.Sample());
        check(!overlay.Pressed(bitA) && !overlay.Pressed(bitStart), "release clears buttons");
        check(overlay.ReleasedEdge(bitA) && overlay.ReleasedEdge(bitStart) && overlay.ReleasedEdge(bitDpadUp),
            "release reports release edges");
        check(!overlay.PressedEdge(bitA), "release is not a press");
        op.End();
        check(std::string(op.Status()) == "completed", "button operation completed");
    }

    {
        ControllerOperation op;
        ControllerInjectOverlay overlay;
        check(op.TryQueue(1, triggers), "queue triggers");
        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.LeftTrigger() == 0.0f && overlay.RightTrigger() == 0.0f, "trigger prep is neutral");
        op.End();
        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.LeftTrigger() == 0.25f && overlay.RightTrigger() == 1.0f, "hold applies triggers");
        check(overlay.LeftX() == 0.0f && overlay.RightX() == 0.0f, "triggers do not move sticks");
        op.End();
        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.LeftTrigger() == 0.0f && overlay.RightTrigger() == 0.0f, "trigger release is neutral");
        op.End();
        check(std::string(op.Status()) == "completed", "trigger operation completed");
    }

    {
        ControllerOperation op;
        check(op.TryQueue(1, legacy), "queue for cancel");
        op.Cancel();
        check(std::string(op.Status()) == "cancelled", "queued cancel completes immediately");
        check(!op.Isolating(), "cancelled queue is not isolating");
        check(op.TryQueue(2, right), "can queue after cancel");
        op.Begin(false);
        check(std::string(op.Status()) == "cancelled", "not playing cancels a queued command");
        check(!op.Isolating(), "not-playing queued command does not isolate");
    }

    {
        ControllerOperation op;
        ControllerInjectOverlay overlay;
        check(op.TryQueue(1, right), "queue before active cancel");
        op.Begin(true);
        overlay.Apply(op.Sample());
        op.End();
        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.RightX() == 0.5f, "active before cancel");
        op.Cancel();
        check(op.Isolating(), "cancel during hold waits for release");
        op.Begin(true);
        overlay.Apply(op.Sample());
        check(overlay.RightX() == 0.0f && overlay.LeftX() == 0.0f, "cancel releases to neutral");
        op.End();
        check(std::string(op.Status()) == "cancelled", "release after cancel is cancelled");
    }

    {
        ControllerInjectOverlay overlay;
        overlay.SetLeftStick(0.25f, -0.5f);
        check(overlay.Active() && overlay.LeftX() == 0.25f && overlay.LeftY() == -0.5f, "left inject sets left");
        check(overlay.RightX() == 0.0f && overlay.RightY() == 0.0f && overlay.LeftTrigger() == 0.0f
                && overlay.RightTrigger() == 0.0f && !overlay.Pressed(bitA),
            "left inject alone clears right, triggers, and buttons");
        overlay.SetRightStick(1.0f, 0.0f);
        check(overlay.LeftX() == 0.25f && overlay.LeftY() == -0.5f, "right inject keeps the left stick");
        check(overlay.RightX() == 1.0f && overlay.RightY() == 0.0f, "right inject sets the right stick");
        overlay.SetTriggers(0.5f, 0.0f);
        check(overlay.LeftTrigger() == 0.5f && overlay.RightTrigger() == 0.0f, "trigger inject");
        check(overlay.LeftX() == 0.25f && overlay.RightX() == 1.0f, "trigger inject keeps sticks");
        overlay.SetButtons(bitA);
        check(overlay.Pressed(bitA) && overlay.PressedEdge(bitA), "button inject presses");
        check(overlay.LeftX() == 0.25f && overlay.RightX() == 1.0f && overlay.LeftTrigger() == 0.5f,
            "button inject keeps sticks and triggers");
        overlay.Clear();
        check(!overlay.Active() && overlay.LeftX() == 0.0f && overlay.RightX() == 0.0f && !overlay.Pressed(bitA),
            "clear removes injection");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " controller axes checks failed\n";
        return 1;
    }
    std::cout << "PASS controller axes protocol\n";
    return 0;
}
