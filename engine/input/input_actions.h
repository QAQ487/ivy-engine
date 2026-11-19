// input_actions.h
// Named input actions ("Jump", "MoveX") bound to keys, pad 0 and mouse
// buttons. Defaults come from project.json input.actions; players can rebind
// at run time, and the differences from the defaults are saved separately.
//
// Binding text (case-sensitive):
//   Key.<NAME>                 a key from key_names.h (Key.SPACE, Key.W, Key.0)
//   Pad.A .B .X .Y .LB .RB .Start .Back .LeftThumb .RightThumb
//   Pad.DPadUp .DPadDown .DPadLeft .DPadRight
//   Pad.LT .RT                 triggers, 0..1
//   Pad.LeftStickX .LeftStickY .RightStickX .RightStickY   -1..1
//   Pad.LeftStickX+ / -        one direction of a stick axis, 0..1
//   Mouse.Left .Right .Middle
//   <A>/<B>                    a pair for axis actions: value = A - B (Key.D/Key.A)
//
// A button action is pressed while any binding reads above 0.5. An axis action
// reads the binding with the largest magnitude, and counts as pressed above 0.5.
// No Windows dependency (the host fills a Snapshot each frame), so it is
// unit-testable on Linux. Not thread-safe; the caller serialises access.

#pragma once

#include "libraries/nlohmann/json.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace input_actions {

enum class ActionType { Button, Axis };

enum class SourceKind { Key, PadButton, PadTrigger, PadStick, MouseButton };

struct Source {
    SourceKind kind = SourceKind::Key;
    int code = 0;  // Key: VK code; PadButton: XInput mask; PadTrigger: 0 LT, 1 RT;
                   // PadStick: 0 LX, 1 LY, 2 RX, 3 RY; MouseButton: 0 left, 1 right, 2 middle
    int sign = 0;  // PadStick only: 0 full axis, +1 / -1 one direction
};

struct Binding {
    Source positive;
    bool hasNegative = false;
    Source negative;
};

// One frame of raw input, filled by the host.
struct Snapshot {
    bool keys[256] = {};
    bool padConnected = false;
    std::uint16_t padButtons = 0;
    float leftTrigger = 0.0f, rightTrigger = 0.0f;
    float leftX = 0.0f, leftY = 0.0f, rightX = 0.0f, rightY = 0.0f;
    bool mouse[3] = {};
};

inline constexpr float kPressThreshold = 0.5f;

bool ParseSource(std::string_view text, Source* out, std::string* error = nullptr);
std::string SourceToString(const Source& source);
bool ParseBinding(std::string_view text, Binding* out, std::string* error = nullptr);
std::string BindingToString(const Binding& binding);

// 0..1 for buttons, keys, triggers and one-direction sticks; -1..1 for a full stick axis.
float SourceValue(const Source& source, const Snapshot& snapshot);
// Value of a binding: positive - negative for a pair.
float BindingValue(const Binding& binding, const Snapshot& snapshot);

struct ActionDef {
    std::string name;
    ActionType type = ActionType::Button;
    std::vector<std::string> bindings;
};

struct Issue {
    std::string action;   // empty for problems not tied to one action
    std::string message;
};

// Action names: 1-64 characters of A-Z a-z 0-9 _ . -
bool IsValidActionName(std::string_view name);
const char* ActionTypeName(ActionType type);  // "button" / "axis"

// Reads an input.actions object. Actions with a bad name or type are skipped;
// binding strings are kept as written (Validate reports bad ones).
std::vector<ActionDef> ParseActions(const nlohmann::json& actions, std::vector<Issue>* issues = nullptr);
nlohmann::json ActionsToJson(const std::vector<ActionDef>& defs);
// Everything wrong with a set: names, duplicates, unparsable bindings, pairs on buttons.
std::vector<Issue> Validate(const std::vector<ActionDef>& defs);

// Which part of a binding a capture replaces.
enum class CapturePart { Whole = 0, Positive = 1, Negative = 2 };

// The binding text a capture produces. existing is the binding being replaced
// (empty when appending). A whole axis binding takes the full stick axis;
// buttons and pair sides keep the stick direction. Positive / Negative keep the
// other side of an existing pair (Negative on a single binding makes a pair).
std::string ComposeCapturedBinding(ActionType type, CapturePart part, std::string_view existing,
                                   Source captured);

// Detects the next input that becomes active, for "press a key" rebinding.
// Inputs already held when Start is called are ignored until released.
class Capture {
public:
    enum class State { Idle, Listening, Done, Cancelled };

    void Start(const Snapshot& now, bool includeMouse);
    // Escape cancels. Stick axes and triggers count once they pass 0.5.
    State Update(const Snapshot& now);
    void Cancel();
    State GetState() const { return m_state; }
    // The captured source; sticks come back one-directional (Pad.LeftStickX+).
    const Source& Result() const { return m_result; }

private:
    State m_state = State::Idle;
    bool m_includeMouse = true;
    Snapshot m_prev;
    Source m_result;
};

class ActionSet {
public:
    // Replaces the defaults and resets every action to them.
    void SetDefaults(std::vector<ActionDef> defaults, std::vector<Issue>* issues = nullptr);
    const std::vector<ActionDef>& Defaults() const { return m_defaults; }

    bool HasAction(std::string_view name) const;
    const std::vector<std::string>* Bindings(std::string_view name) const;
    // Every string must parse (pairs only on axis actions); otherwise nothing changes.
    bool SetBindings(std::string_view name, const std::vector<std::string>& bindings, std::string* error = nullptr);
    // One action back to its default; an empty name resets all. False for an unknown name.
    bool ResetBindings(std::string_view name);

    // {"version":1,"bindings":{"Jump":["Key.J"]}} for actions that differ from the defaults.
    nlohmann::json OverridesToJson() const;
    // Applies saved overrides; unknown actions and bad bindings are skipped and reported.
    int ApplyOverrides(const nlohmann::json& overrides, std::vector<Issue>* issues = nullptr);

    void Update(const Snapshot& snapshot);
    bool IsPressed(std::string_view name) const;
    bool IsTriggered(std::string_view name) const;  // pressed this frame
    bool IsReleased(std::string_view name) const;   // released this frame
    float GetAxis(std::string_view name) const;     // button actions read 0 or 1

    // Rebinding: the next active input replaces binding `index` of the action
    // (index == count appends). Positive / Negative edit one side of a pair and
    // need an axis action. The result is applied when the capture completes.
    bool CaptureStart(std::string_view name, int index, CapturePart part, bool includeMouse,
                      std::string* error = nullptr);
    Capture::State CaptureState() const { return m_capture.GetState(); }
    // The binding written by the last completed capture.
    const std::string& CaptureResult() const { return m_captureResult; }
    void CaptureCancel();

private:
    struct Runtime {
        ActionType type = ActionType::Button;
        std::vector<std::string> bindings;
        std::vector<Binding> compiled;
        float value = 0.0f;
        bool pressed = false;
        bool wasPressed = false;
    };

    void Compile(Runtime& rt);
    const Runtime* Find(std::string_view name) const;
    Runtime* Find(std::string_view name);
    void FinishCapture();

    std::vector<ActionDef> m_defaults;
    std::map<std::string, Runtime, std::less<>> m_actions;
    Snapshot m_last;
    Capture m_capture;
    std::string m_captureAction;
    int m_captureIndex = 0;
    CapturePart m_capturePart = CapturePart::Whole;
    std::string m_captureResult;
};

}  // namespace input_actions
