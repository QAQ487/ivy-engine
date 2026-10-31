// controller_probe_trace.h
// Declares ControllerProbeFrame.

#pragma once

// Portable controller consumer-trace types and validators.
// Frames are observations of public getter results, not request payloads.

#include "engine/input/controller_test_inject.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ivy::testing {

constexpr std::size_t kControllerProbeCapacity = 256;

struct ControllerProbeFrame {
    std::int64_t sequence = 0;
    float lx = 0.0f;
    float ly = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float lt = 0.0f;
    float rt = 0.0f;
    std::uint16_t pressed = 0;
    std::uint16_t pressedEdge = 0;
    std::uint16_t releasedEdge = 0;
    bool connected = false;
};

struct ControllerProbeSnapshot {
    std::vector<ControllerProbeFrame> frames;
    std::int64_t latestSequence = 0;
    std::uint32_t dropped = 0;
    bool overflow = false;
};

// Ring that keeps the newest capacity frames. Overflow increments dropped and
// discards the oldest frame so later assertions still see a contiguous suffix.
class ControllerProbeRing {
public:
    void Reset() {
        frames_.clear();
        nextSequence_ = 1;
        dropped_ = 0;
        overflow_ = false;
    }

    bool Push(ControllerProbeFrame frame) {
        frame.sequence = nextSequence_++;
        if (frames_.size() >= kControllerProbeCapacity) {
            frames_.erase(frames_.begin());
            ++dropped_;
            overflow_ = true;
        }
        frames_.push_back(frame);
        return !overflow_;
    }

    ControllerProbeSnapshot Snapshot() const {
        ControllerProbeSnapshot snap;
        snap.frames = frames_;
        snap.latestSequence = nextSequence_ - 1;
        snap.dropped = dropped_;
        snap.overflow = overflow_;
        return snap;
    }

    bool Overflow() const { return overflow_; }
    std::uint32_t Dropped() const { return dropped_; }

private:
    std::vector<ControllerProbeFrame> frames_;
    std::int64_t nextSequence_ = 1;
    std::uint32_t dropped_ = 0;
    bool overflow_ = false;
};

inline bool ProbeFloatEq(float a, float b) {
    // Inject path returns the exact float applied by Controller_TestInject*.
    // Physical XInput remaps BYTE triggers through a deadzone; that path is not
    // what these acceptance checks exercise. Exact equality is intentional.
    return a == b;
}

inline bool ProbeAxesNeutral(const ControllerProbeFrame& frame) {
    return ProbeFloatEq(frame.lx, 0.0f) && ProbeFloatEq(frame.ly, 0.0f)
        && ProbeFloatEq(frame.rx, 0.0f) && ProbeFloatEq(frame.ry, 0.0f)
        && ProbeFloatEq(frame.lt, 0.0f) && ProbeFloatEq(frame.rt, 0.0f);
}

inline bool ProbeButtonsNeutral(const ControllerProbeFrame& frame) {
    return frame.pressed == 0 && frame.pressedEdge == 0 && frame.releasedEdge == 0;
}

inline bool ProbeFullyNeutral(const ControllerProbeFrame& frame) {
    return ProbeAxesNeutral(frame) && ProbeButtonsNeutral(frame);
}

struct ProbeValidation {
    bool pass = false;
    std::string error;
};

inline ProbeValidation ValidateTriggerHoldTrace(
    const std::vector<ControllerProbeFrame>& frames,
    bool left,
    float holdValue,
    int updates) {
    if (updates < 1)
        return {false, "updates < 1"};
    if (frames.size() != static_cast<std::size_t>(updates + 2))
        return {false, "trigger trace length"};
    if (!(holdValue == 0.0f || holdValue == 0.25f || holdValue == 0.5f || holdValue == 1.0f))
        return {false, "unsupported hold value for exact inject equality"};

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
        if (!ProbeFloatEq(f.lx, 0.0f) || !ProbeFloatEq(f.ly, 0.0f))
            return {false, "left stick moved during trigger-only input"};
        if (!ProbeFloatEq(f.rx, 0.0f) || !ProbeFloatEq(f.ry, 0.0f))
            return {false, "right stick moved during trigger-only input"};
        if (f.pressed != 0 || f.pressedEdge != 0 || f.releasedEdge != 0)
            return {false, "buttons changed during trigger-only input"};
        const float expected = (i == 0 || i + 1 == frames.size()) ? 0.0f : holdValue;
        const float actual = left ? f.lt : f.rt;
        const float other = left ? f.rt : f.lt;
        if (!ProbeFloatEq(actual, expected))
            return {false, "trigger value mismatch"};
        if (!ProbeFloatEq(other, 0.0f))
            return {false, "sibling trigger not neutral"};
        if (!f.connected)
            return {false, "connected false while sampling injected trigger"};
    }
    return {true, ""};
}

inline ProbeValidation ValidateButtonHoldTrace(
    const std::vector<ControllerProbeFrame>& frames,
    std::uint16_t bit,
    int updates) {
    if (updates < 1)
        return {false, "updates < 1"};
    if (frames.size() != static_cast<std::size_t>(updates + 2))
        return {false, "button trace length"};
    if (bit == 0)
        return {false, "empty button bit"};

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
        if (!ProbeFloatEq(f.lx, 0.0f) || !ProbeFloatEq(f.ly, 0.0f))
            return {false, "left stick moved during button-only input"};
        if (!ProbeFloatEq(f.rx, 0.0f) || !ProbeFloatEq(f.ry, 0.0f)
            || !ProbeFloatEq(f.lt, 0.0f) || !ProbeFloatEq(f.rt, 0.0f))
            return {false, "axes moved during button-only input"};
        if (!f.connected)
            return {false, "connected false while sampling injected button"};

        const bool prep = (i == 0);
        const bool release = (i + 1 == frames.size());
        const bool firstHold = (i == 1);
        if (prep) {
            if (f.pressed != 0 || f.pressedEdge != 0 || f.releasedEdge != 0)
                return {false, "preparation not button-neutral"};
        } else if (release) {
            if (f.pressed != 0 || f.pressedEdge != 0 || f.releasedEdge != bit)
                return {false, "release edge mismatch"};
        } else if (firstHold) {
            if (f.pressed != bit || f.pressedEdge != bit || f.releasedEdge != 0)
                return {false, "first-hold press edge mismatch"};
        } else {
            if (f.pressed != bit || f.pressedEdge != 0 || f.releasedEdge != 0)
                return {false, "held state mismatch"};
        }
    }
    return {true, ""};
}

inline ProbeValidation ValidateCombinedTrace(
    const std::vector<ControllerProbeFrame>& frames,
    float lt,
    float rt,
    std::uint16_t buttons,
    int updates) {
    if (updates < 1)
        return {false, "updates < 1"};
    if (frames.size() != static_cast<std::size_t>(updates + 2))
        return {false, "combined trace length"};

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
        if (!ProbeFloatEq(f.lx, 0.0f) || !ProbeFloatEq(f.ly, 0.0f))
            return {false, "left stick moved during combined input"};
        if (!ProbeFloatEq(f.rx, 0.0f) || !ProbeFloatEq(f.ry, 0.0f))
            return {false, "right stick moved during combined input"};
        if (!f.connected)
            return {false, "connected false during combined input"};

        const bool prep = (i == 0);
        const bool release = (i + 1 == frames.size());
        const bool firstHold = (i == 1);
        if (prep || release) {
            if (!ProbeFloatEq(f.lt, 0.0f) || !ProbeFloatEq(f.rt, 0.0f))
                return {false, "combined prep/release triggers not neutral"};
        } else {
            if (!ProbeFloatEq(f.lt, lt) || !ProbeFloatEq(f.rt, rt))
                return {false, "combined hold trigger mismatch"};
        }
        if (prep) {
            if (f.pressed != 0 || f.pressedEdge != 0 || f.releasedEdge != 0)
                return {false, "combined prep buttons not neutral"};
        } else if (release) {
            if (f.pressed != 0 || f.pressedEdge != 0 || f.releasedEdge != buttons)
                return {false, "combined release edge mismatch"};
        } else if (firstHold) {
            if (f.pressed != buttons || f.pressedEdge != buttons || f.releasedEdge != 0)
                return {false, "combined first-hold edge mismatch"};
        } else {
            if (f.pressed != buttons || f.pressedEdge != 0 || f.releasedEdge != 0)
                return {false, "combined held buttons mismatch"};
        }
    }
    return {true, ""};
}

inline ProbeValidation ValidateRightStickKeepsLeftNeutral(
    const std::vector<ControllerProbeFrame>& frames,
    float rx,
    float ry,
    int updates) {
    if (updates < 1)
        return {false, "updates < 1"};
    if (frames.size() != static_cast<std::size_t>(updates + 2))
        return {false, "right-stick trace length"};
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
        if (!ProbeFloatEq(f.lx, 0.0f) || !ProbeFloatEq(f.ly, 0.0f))
            return {false, "left stick moved during right-stick-only input"};
        if (f.pressed != 0 || f.pressedEdge != 0 || f.releasedEdge != 0)
            return {false, "buttons changed during right-stick-only input"};
        if (!ProbeFloatEq(f.lt, 0.0f) || !ProbeFloatEq(f.rt, 0.0f))
            return {false, "triggers changed during right-stick-only input"};
        const float expectRx = (i == 0 || i + 1 == frames.size()) ? 0.0f : rx;
        const float expectRy = (i == 0 || i + 1 == frames.size()) ? 0.0f : ry;
        if (!ProbeFloatEq(f.rx, expectRx) || !ProbeFloatEq(f.ry, expectRy))
            return {false, "right stick mismatch"};
    }
    return {true, ""};
}

inline ProbeValidation ValidateEventualNeutralSuffix(
    const std::vector<ControllerProbeFrame>& frames,
    std::size_t minNeutralTail) {
    if (frames.size() < minNeutralTail)
        return {false, "not enough frames for neutral suffix"};
    for (std::size_t i = frames.size() - minNeutralTail; i < frames.size(); ++i) {
        if (!ProbeFullyNeutral(frames[i]))
            return {false, "stuck non-neutral frame after cancel/disconnect"};
    }
    return {true, ""};
}

inline const std::uint16_t* ProbeAllButtonBits(std::size_t& count) {
    static const std::uint16_t kBits[] = {
        kInjectButtonA, kInjectButtonB, kInjectButtonX, kInjectButtonY,
        kInjectButtonLB, kInjectButtonRB, kInjectButtonStart, kInjectButtonBack,
        kInjectButtonLeftThumb, kInjectButtonRightThumb,
        kInjectButtonDpadUp, kInjectButtonDpadDown, kInjectButtonDpadLeft, kInjectButtonDpadRight,
    };
    count = sizeof(kBits) / sizeof(kBits[0]);
    return kBits;
}

}  // namespace ivy::testing
