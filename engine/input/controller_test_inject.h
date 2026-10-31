// controller_test_inject.h
// Declares ControllerInjectSample.

#pragma once

// Debug-runtime controller injection state. No Win32 or XInput types.
// Button bits match ControllerButton in controller.h.

#include <cstdint>
#include <string>

namespace ivy::testing {

constexpr std::uint16_t kInjectButtonDpadUp = 0x0001;
constexpr std::uint16_t kInjectButtonDpadDown = 0x0002;
constexpr std::uint16_t kInjectButtonDpadLeft = 0x0004;
constexpr std::uint16_t kInjectButtonDpadRight = 0x0008;
constexpr std::uint16_t kInjectButtonStart = 0x0010;
constexpr std::uint16_t kInjectButtonBack = 0x0020;
constexpr std::uint16_t kInjectButtonLeftThumb = 0x0040;
constexpr std::uint16_t kInjectButtonRightThumb = 0x0080;
constexpr std::uint16_t kInjectButtonLB = 0x0100;
constexpr std::uint16_t kInjectButtonRB = 0x0200;
constexpr std::uint16_t kInjectButtonA = 0x1000;
constexpr std::uint16_t kInjectButtonB = 0x2000;
constexpr std::uint16_t kInjectButtonX = 0x4000;
constexpr std::uint16_t kInjectButtonY = 0x8000;

struct ControllerInjectSample {
    float lx = 0.0f;
    float ly = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float lt = 0.0f;
    float rt = 0.0f;
    std::uint16_t buttons = 0;
};

inline bool ControllerButtonFromName(const std::string& name, std::uint16_t& bit) {
    if (name == "A") { bit = kInjectButtonA; return true; }
    if (name == "B") { bit = kInjectButtonB; return true; }
    if (name == "X") { bit = kInjectButtonX; return true; }
    if (name == "Y") { bit = kInjectButtonY; return true; }
    if (name == "LB") { bit = kInjectButtonLB; return true; }
    if (name == "RB") { bit = kInjectButtonRB; return true; }
    if (name == "START") { bit = kInjectButtonStart; return true; }
    if (name == "BACK") { bit = kInjectButtonBack; return true; }
    if (name == "LEFT_THUMB") { bit = kInjectButtonLeftThumb; return true; }
    if (name == "RIGHT_THUMB") { bit = kInjectButtonRightThumb; return true; }
    if (name == "DPAD_UP") { bit = kInjectButtonDpadUp; return true; }
    if (name == "DPAD_DOWN") { bit = kInjectButtonDpadDown; return true; }
    if (name == "DPAD_LEFT") { bit = kInjectButtonDpadLeft; return true; }
    if (name == "DPAD_RIGHT") { bit = kInjectButtonDpadRight; return true; }
    return false;
}

// One applied sample per engine Update. Each Apply records the previous button
// mask so press and release edges are visible to that frame's readers.
class ControllerInjectOverlay {
public:
    void Clear() {
        previousButtons_ = 0;
        current_ = {};
        active_ = false;
    }

    void Apply(const ControllerInjectSample& sample) {
        previousButtons_ = current_.buttons;
        current_ = sample;
        active_ = true;
    }

    // Left-stick helper used by the original inject API: replaces the whole sample.
    void SetLeftStick(float lx, float ly) {
        ControllerInjectSample sample;
        sample.lx = lx;
        sample.ly = ly;
        Apply(sample);
    }

    void SetRightStick(float rx, float ry) {
        ControllerInjectSample sample = current_;
        sample.rx = rx;
        sample.ry = ry;
        Apply(sample);
    }

    void SetTriggers(float lt, float rt) {
        ControllerInjectSample sample = current_;
        sample.lt = lt;
        sample.rt = rt;
        Apply(sample);
    }

    void SetButtons(std::uint16_t buttons) {
        ControllerInjectSample sample = current_;
        sample.buttons = buttons;
        Apply(sample);
    }

    bool Active() const { return active_; }
    float LeftX() const { return current_.lx; }
    float LeftY() const { return current_.ly; }
    float RightX() const { return current_.rx; }
    float RightY() const { return current_.ry; }
    float LeftTrigger() const { return current_.lt; }
    float RightTrigger() const { return current_.rt; }
    bool Pressed(std::uint16_t bit) const { return (current_.buttons & bit) != 0; }
    bool PressedEdge(std::uint16_t bit) const {
        return (current_.buttons & bit) != 0 && (previousButtons_ & bit) == 0;
    }
    bool ReleasedEdge(std::uint16_t bit) const {
        return (current_.buttons & bit) == 0 && (previousButtons_ & bit) != 0;
    }

private:
    ControllerInjectSample current_{};
    std::uint16_t previousButtons_ = 0;
    bool active_ = false;
};

}  // namespace ivy::testing
