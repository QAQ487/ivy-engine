// Platform-independent controller consumer-trace validators (no Win32).
#include "engine/testing/controller_probe_trace.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ivy::testing;

static int g_failures = 0;

static void check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL " << label << "\n";
        ++g_failures;
    }
}

static ControllerProbeFrame Frame(
    float lx, float ly, float rx, float ry, float lt, float rt,
    std::uint16_t pressed, std::uint16_t pressedEdge, std::uint16_t releasedEdge) {
    ControllerProbeFrame f;
    f.lx = lx; f.ly = ly; f.rx = rx; f.ry = ry; f.lt = lt; f.rt = rt;
    f.pressed = pressed; f.pressedEdge = pressedEdge; f.releasedEdge = releasedEdge;
    f.connected = true;
    return f;
}

static std::vector<ControllerProbeFrame> TriggerTrace(bool left, float value, int updates) {
    std::vector<ControllerProbeFrame> frames;
    frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, 0));
    for (int i = 0; i < updates; ++i) {
        frames.push_back(Frame(0, 0, 0, 0, left ? value : 0, left ? 0 : value, 0, 0, 0));
    }
    frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, 0));
    return frames;
}

static std::vector<ControllerProbeFrame> ButtonTrace(std::uint16_t bit, int updates) {
    std::vector<ControllerProbeFrame> frames;
    frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, 0));
    for (int i = 0; i < updates; ++i) {
        const bool first = (i == 0);
        frames.push_back(Frame(0, 0, 0, 0, 0, 0, bit, first ? bit : 0, 0));
    }
    frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, bit));
    return frames;
}

int main() {
    {
        ControllerProbeRing ring;
        for (std::size_t i = 0; i < kControllerProbeCapacity; ++i) {
            ControllerProbeFrame f;
            f.connected = true;
            check(ring.Push(f), "push within capacity reports no overflow");
        }
        check(!ring.Overflow(), "full capacity is not overflow yet");
        ControllerProbeFrame extra;
        extra.connected = true;
        check(!ring.Push(extra), "push beyond capacity reports overflow");
        check(ring.Overflow() && ring.Dropped() == 1, "overflow increments dropped");
        const auto snap = ring.Snapshot();
        check(snap.frames.size() == kControllerProbeCapacity, "ring keeps capacity frames");
        check(snap.overflow && snap.dropped == 1, "snapshot carries overflow");
        check(snap.frames.front().sequence == 2, "oldest frame discarded on overflow");
        check(snap.frames.back().sequence == static_cast<std::int64_t>(kControllerProbeCapacity + 1),
            "newest sequence retained");
    }

    for (float value : {0.0f, 0.25f, 0.5f, 1.0f}) {
        auto lt = ValidateTriggerHoldTrace(TriggerTrace(true, value, 2), true, value, 2);
        check(lt.pass, "LT hold validator accepts exact inject values");
        auto rt = ValidateTriggerHoldTrace(TriggerTrace(false, value, 3), false, value, 3);
        check(rt.pass, "RT hold validator accepts exact inject values");
    }
    {
        auto bad = TriggerTrace(true, 0.25f, 2);
        bad[1].lx = 0.1f;
        check(!ValidateTriggerHoldTrace(bad, true, 0.25f, 2).pass, "LT rejects left-stick drift");
        bad = TriggerTrace(true, 0.25f, 2);
        bad[2].rt = 1.0f;
        check(!ValidateTriggerHoldTrace(bad, true, 0.25f, 2).pass, "LT rejects sibling trigger");
        bad = TriggerTrace(false, 1.0f, 2);
        bad[0].lt = 0.25f;
        check(!ValidateTriggerHoldTrace(bad, false, 1.0f, 2).pass, "RT rejects non-neutral prep");
    }

    std::size_t buttonCount = 0;
    const std::uint16_t* bits = ProbeAllButtonBits(buttonCount);
    check(buttonCount == 14, "all fourteen supported buttons");
    for (std::size_t i = 0; i < buttonCount; ++i) {
        auto ok = ValidateButtonHoldTrace(ButtonTrace(bits[i], 2), bits[i], 2);
        check(ok.pass, "button hold validator accepts each bit");
        auto bad = ButtonTrace(bits[i], 2);
        bad[1].pressedEdge = 0;
        check(!ValidateButtonHoldTrace(bad, bits[i], 2).pass, "button rejects missing first press edge");
        bad = ButtonTrace(bits[i], 2);
        bad[2].pressedEdge = bits[i];
        check(!ValidateButtonHoldTrace(bad, bits[i], 2).pass, "button rejects repeated press edge");
        bad = ButtonTrace(bits[i], 2);
        bad[3].releasedEdge = 0;
        check(!ValidateButtonHoldTrace(bad, bits[i], 2).pass, "button rejects missing release edge");
        bad = ButtonTrace(bits[i], 2);
        bad[1].ly = 1.0f;
        check(!ValidateButtonHoldTrace(bad, bits[i], 2).pass, "button rejects left-stick drift");
    }

    {
        const std::uint16_t mask = kInjectButtonA | kInjectButtonLB | kInjectButtonDpadUp;
        std::vector<ControllerProbeFrame> frames;
        frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, 0));
        frames.push_back(Frame(0, 0, 0, 0, 0.25f, 1.0f, mask, mask, 0));
        frames.push_back(Frame(0, 0, 0, 0, 0.25f, 1.0f, mask, 0, 0));
        frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, mask));
        auto ok = ValidateCombinedTrace(frames, 0.25f, 1.0f, mask, 2);
        check(ok.pass, "combined triggers+buttons accepted");
        frames[1].lx = 0.2f;
        check(!ValidateCombinedTrace(frames, 0.25f, 1.0f, mask, 2).pass, "combined rejects left-stick drift");
    }

    {
        std::vector<ControllerProbeFrame> frames;
        frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, 0));
        frames.push_back(Frame(0, 0, 0.5f, -1.0f, 0, 0, 0, 0, 0));
        frames.push_back(Frame(0, 0, 0.5f, -1.0f, 0, 0, 0, 0, 0));
        frames.push_back(Frame(0, 0, 0, 0, 0, 0, 0, 0, 0));
        check(ValidateRightStickKeepsLeftNeutral(frames, 0.5f, -1.0f, 2).pass,
            "right-stick keeps left stick neutral");
        frames[2].lx = 0.01f;
        check(!ValidateRightStickKeepsLeftNeutral(frames, 0.5f, -1.0f, 2).pass,
            "right-stick rejects left drift");
    }

    {
        std::vector<ControllerProbeFrame> frames = {
            Frame(0, 0, 0.5f, 0, 0, 0, kInjectButtonA, kInjectButtonA, 0),
            Frame(0, 0, 0, 0, 0, 0, 0, 0, kInjectButtonA),
            Frame(0, 0, 0, 0, 0, 0, 0, 0, 0),
            Frame(0, 0, 0, 0, 0, 0, 0, 0, 0),
            Frame(0, 0, 0, 0, 0, 0, 0, 0, 0),
        };
        check(ValidateEventualNeutralSuffix(frames, 3).pass, "cancel/disconnect neutral suffix");
        frames[4].rt = 1.0f;
        check(!ValidateEventualNeutralSuffix(frames, 3).pass, "stuck trigger fails neutral suffix");
        frames[4].rt = 0.0f;
        frames[3].pressed = kInjectButtonB;
        check(!ValidateEventualNeutralSuffix(frames, 3).pass, "stuck button fails neutral suffix");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " controller probe validator checks failed\n";
        return 1;
    }
    std::cout << "PASS controller probe validators\n";
    return 0;
}
