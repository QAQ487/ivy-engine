// input_actions_test.cpp
// Linux-runnable unit tests for engine/input/input_actions.{h,cpp} and key_names.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/input_actions_test tests/input_actions_test.cpp engine/input/input_actions.cpp
//   /tmp/input_actions_test

#include "engine/input/input_actions.h"
#include "engine/input/key_names.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace input_actions;
using nlohmann::json;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

static unsigned char Vk(const char* name) {
    unsigned char vk = 0;
    key_names::FromName(name, &vk);
    return vk;
}

static std::string RoundTrip(const char* text) {
    Binding b;
    return ParseBinding(text, &b) ? BindingToString(b) : std::string("<error>");
}

int main() {
    // --- key names -----------------------------------------------------------
    Expect(Vk("SPACE") == 0x20 && Vk("A") == 0x41 && Vk("0") == 0x30 && Vk("ESCAPE") == 0x1B, "key name codes");
    Expect(std::string(key_names::ToName(0x20)) == "SPACE", "code to name");
    Expect(!key_names::FromName("space", nullptr), "names are case-sensitive");

    // --- binding parse / print -----------------------------------------------
    for (const char* text : {"Key.SPACE", "Key.0", "Pad.A", "Pad.DPadLeft", "Pad.LT", "Pad.RT", "Pad.LeftStickX",
                             "Pad.RightStickY-", "Pad.LeftStickY+", "Mouse.Left", "Mouse.Middle", "Key.D/Key.A",
                             "Pad.RT/Pad.LT", "Key.UP/Pad.LeftStickY-"}) {
        Expect(RoundTrip(text) == text, text);
    }
    Expect(RoundTrip(" Key.D / Key.A ") == "Key.D/Key.A", "spaces around parts trimmed");
    std::string err;
    Binding b;
    Expect(!ParseBinding("Key.NOPE", &b, &err) && err.find("unknown key") != std::string::npos, "unknown key");
    Expect(!ParseBinding("Pad.A+", &b, &err) && err.find("stick") != std::string::npos, "sign on a button");
    Expect(!ParseBinding("Pad.Z", &b, &err), "unknown pad input");
    Expect(!ParseBinding("Mouse.Back", &b, &err), "unknown mouse button");
    Expect(!ParseBinding("SPACE", &b, &err) && err.find("must start") != std::string::npos, "missing prefix");
    Expect(!ParseBinding("Key.A/Key.B/Key.C", &b, &err), "two slashes");
    Expect(!ParseBinding("Key.A/", &b, &err), "empty pair side");
    Expect(!ParseBinding("", &b, &err), "empty binding");

    // --- values --------------------------------------------------------------
    Snapshot s;
    s.keys[Vk("D")] = true;
    s.padConnected = true;
    s.padButtons = 0x1000;  // A
    s.leftTrigger = 0.7f;
    s.leftX = -0.8f;
    s.mouse[1] = true;
    auto val = [&](const char* text) {
        Binding bb;
        ParseBinding(text, &bb);
        return BindingValue(bb, s);
    };
    Expect(Near(val("Key.D"), 1) && Near(val("Key.A"), 0), "key value");
    Expect(Near(val("Pad.A"), 1) && Near(val("Pad.B"), 0), "pad button value");
    Expect(Near(val("Pad.LT"), 0.7f) && Near(val("Pad.RT"), 0), "trigger value");
    Expect(Near(val("Pad.LeftStickX"), -0.8f), "full stick axis");
    Expect(Near(val("Pad.LeftStickX-"), 0.8f) && Near(val("Pad.LeftStickX+"), 0), "stick directions");
    Expect(Near(val("Mouse.Right"), 1) && Near(val("Mouse.Left"), 0), "mouse value");
    Expect(Near(val("Key.D/Key.A"), 1) && Near(val("Key.A/Key.D"), -1), "pair value");
    Snapshot unplugged = s;
    unplugged.padConnected = false;
    {
        Binding bb;
        ParseBinding("Pad.A", &bb);
        Expect(Near(BindingValue(bb, unplugged), 0), "disconnected pad reads 0");
    }

    // --- parse / validate actions -------------------------------------------
    const json actionsJson = json::parse(R"({
      "Jump":  { "type": "button", "bindings": ["Key.SPACE", "Pad.A"] },
      "MoveX": { "type": "axis", "bindings": ["Key.D/Key.A", "Pad.LeftStickX"] },
      "Fire":  { "bindings": ["Mouse.Left", "Pad.RT"] },
      "Bad Name": { "type": "button" },
      "Weird": { "type": "slider" },
      "Typo":  { "type": "button", "bindings": ["Key.SPCE", 3] }
    })");
    std::vector<Issue> issues;
    std::vector<ActionDef> defs = ParseActions(actionsJson, &issues);
    Expect(defs.size() == 4, "4 actions kept (bad name and bad type skipped)");
    Expect(issues.size() == 3, "3 parse issues (name, type, non-string binding)");
    const std::vector<Issue> problems = Validate(defs);
    Expect(problems.size() == 1 && problems[0].action == "Typo", "validate flags the misspelled key");
    {
        std::vector<ActionDef> dup = {{"A", ActionType::Button, {}}, {"A", ActionType::Button, {}},
                                      {"P", ActionType::Button, {"Key.D/Key.A"}}};
        const auto v = Validate(dup);
        Expect(v.size() == 2, "duplicate name and pair-on-button reported");
    }
    Expect(ActionsToJson(defs)["MoveX"]["type"] == "axis", "serialize type");
    Expect(ParseActions(ActionsToJson(defs)).size() == defs.size(), "serialize round trip");
    Expect(ParseActions(json()).empty(), "missing section -> no actions");
    issues.clear();
    Expect(ParseActions(json::array(), &issues).empty() && issues.size() == 1, "non-object section reported");
    Expect(IsValidActionName("Move.X-2_b") && !IsValidActionName("") && !IsValidActionName("a b") &&
               !IsValidActionName(std::string(65, 'a')), "action names");

    // --- runtime state -------------------------------------------------------
    ActionSet set;
    set.SetDefaults(defs);
    Snapshot none;
    none.padConnected = true;
    set.Update(none);
    Expect(!set.IsPressed("Jump") && !set.IsTriggered("Jump"), "idle");
    Snapshot jump = none;
    jump.keys[Vk("SPACE")] = true;
    set.Update(jump);
    Expect(set.IsPressed("Jump") && set.IsTriggered("Jump") && !set.IsReleased("Jump"), "press edge");
    set.Update(jump);
    Expect(set.IsPressed("Jump") && !set.IsTriggered("Jump"), "held");
    set.Update(none);
    Expect(!set.IsPressed("Jump") && set.IsReleased("Jump"), "release edge");
    Expect(Near(set.GetAxis("Jump"), 0), "button axis 0 when up");

    Snapshot move = none;
    move.keys[Vk("A")] = true;  // -1 from the pair
    move.leftX = 0.4f;          // weaker stick
    set.Update(move);
    Expect(Near(set.GetAxis("MoveX"), -1) && set.IsPressed("MoveX"), "axis takes largest magnitude");
    move.keys[Vk("A")] = false;
    set.Update(move);
    Expect(Near(set.GetAxis("MoveX"), 0.4f) && !set.IsPressed("MoveX"), "analog below threshold not pressed");
    Snapshot trigger = none;
    trigger.rightTrigger = 0.6f;
    set.Update(trigger);
    Expect(set.IsPressed("Fire") && Near(set.GetAxis("Fire"), 1), "trigger as button");
    Expect(!set.IsPressed("Nope") && Near(set.GetAxis("Nope"), 0) && !set.HasAction("Nope"), "unknown action");
    Expect(set.HasAction("Typo") && set.Bindings("Typo")->size() == 1, "bad binding kept as text");

    // --- rebinding and overrides ---------------------------------------------
    Expect(set.SetBindings("Jump", {"Key.J"}), "set bindings");
    Expect(!set.SetBindings("Jump", {"Key.D/Key.A"}, &err), "pair rejected on button");
    Expect(!set.SetBindings("Jump", {"Key.J", "Key.NOPE"}, &err) && (*set.Bindings("Jump"))[0] == "Key.J",
           "invalid set leaves bindings unchanged");
    Expect(!set.SetBindings("Nope", {}, &err), "set unknown action");
    const json overrides = set.OverridesToJson();
    Expect(overrides["version"] == 1 && overrides["bindings"].size() == 1 &&
               overrides["bindings"]["Jump"] == json({"Key.J"}), "only changed actions saved");
    Expect(set.ResetBindings("Jump") && (*set.Bindings("Jump"))[0] == "Key.SPACE", "reset one");
    Expect(set.OverridesToJson()["bindings"].empty(), "nothing to save after reset");
    issues.clear();
    const json saved = json::parse(R"({"version":1,"bindings":{
        "Jump":["Key.J","Key.NOPE"], "Gone":["Key.X"], "MoveX":["Pad.RightStickX"], "Fire":"oops"}})");
    Expect(set.ApplyOverrides(saved, &issues) == 2, "two overrides applied");
    Expect((*set.Bindings("Jump")) == std::vector<std::string>{"Key.J"}, "bad saved binding dropped");
    Expect(issues.size() == 3, "dropped binding, missing action and non-array reported");
    Expect(set.ResetBindings("") && (*set.Bindings("MoveX"))[0] == "Key.D/Key.A", "reset all");
    Expect(!set.ResetBindings("Nope"), "reset unknown");
    Expect(set.ApplyOverrides(json("x")) == 0 && set.ApplyOverrides(json::object()) == 0, "malformed overrides ignored");

    // --- capture through the action set --------------------------------------
    {
        ActionSet cs;
        cs.SetDefaults(defs);
        Snapshot held = none;
        held.keys[Vk("W")] = true;  // held before the capture starts
        cs.Update(held);
        Expect(cs.CaptureStart("Jump", 0, CapturePart::Whole, true), "capture start");
        cs.Update(held);
        Expect(cs.CaptureState() == Capture::State::Listening, "held key ignored");
        Snapshot k = held;
        k.keys[Vk("K")] = true;
        cs.Update(k);
        Expect(cs.CaptureState() == Capture::State::Done && cs.CaptureResult() == "Key.K", "captured K");
        Expect((*cs.Bindings("Jump"))[0] == "Key.K" && (*cs.Bindings("Jump"))[1] == "Pad.A", "replaced index 0");

        // Append a stick direction to a button.
        Expect(cs.CaptureStart("Jump", 2, CapturePart::Whole, true), "capture append");
        Snapshot stick = none;
        stick.leftY = 0.9f;
        cs.Update(stick);
        Expect(cs.CaptureResult() == "Pad.LeftStickY+" && cs.Bindings("Jump")->size() == 3,
               "button keeps the stick direction");

        // Whole axis binding takes the full stick axis.
        cs.Update(none);
        Expect(cs.CaptureStart("MoveX", 1, CapturePart::Whole, true), "capture axis");
        Snapshot right = none;
        right.rightX = -0.9f;
        cs.Update(right);
        Expect(cs.CaptureResult() == "Pad.RightStickX", "axis gets the full stick");

        // Pair sides.
        cs.Update(none);
        Expect(cs.CaptureStart("MoveX", 0, CapturePart::Negative, true), "capture negative side");
        Snapshot left = none;
        left.keys[Vk("LEFT")] = true;
        cs.Update(left);
        Expect((*cs.Bindings("MoveX"))[0] == "Key.D/Key.LEFT", "negative side replaced");
        cs.Update(none);
        Expect(cs.CaptureStart("MoveX", 0, CapturePart::Positive, true), "capture positive side");
        Snapshot rightKey = none;
        rightKey.keys[Vk("RIGHT")] = true;
        cs.Update(rightKey);
        Expect((*cs.Bindings("MoveX"))[0] == "Key.RIGHT/Key.LEFT", "positive side replaced");
        Expect(!cs.CaptureStart("MoveX", 2, CapturePart::Negative, true, &err), "negative cannot start a binding");
        Expect(!cs.CaptureStart("Jump", 0, CapturePart::Positive, true, &err), "parts need an axis action");
        Expect(!cs.CaptureStart("Jump", 9, CapturePart::Whole, true, &err), "index out of range");
        Expect(!cs.CaptureStart("Nope", 0, CapturePart::Whole, true, &err), "unknown action");

        // Escape cancels without changing anything.
        cs.Update(none);
        const std::vector<std::string> before = *cs.Bindings("Jump");
        Expect(cs.CaptureStart("Jump", 0, CapturePart::Whole, true), "capture for cancel");
        Snapshot esc = none;
        esc.keys[Vk("ESCAPE")] = true;
        cs.Update(esc);
        Expect(cs.CaptureState() == Capture::State::Cancelled && *cs.Bindings("Jump") == before, "escape cancels");

        // Mouse ignored when excluded; pad buttons and triggers captured.
        cs.Update(none);
        Expect(cs.CaptureStart("Jump", 0, CapturePart::Whole, false), "capture without mouse");
        Snapshot click = none;
        click.mouse[0] = true;
        cs.Update(click);
        Expect(cs.CaptureState() == Capture::State::Listening, "mouse ignored");
        Snapshot lt = click;
        lt.leftTrigger = 0.8f;
        cs.Update(lt);
        Expect(cs.CaptureResult() == "Pad.LT", "trigger captured");
        cs.Update(none);
        Expect(cs.CaptureStart("Jump", 0, CapturePart::Whole, true), "capture with mouse");
        cs.Update(click);
        Expect(cs.CaptureResult() == "Mouse.Left", "mouse captured");
        cs.Update(none);
        cs.CaptureStart("Jump", 0, CapturePart::Whole, true);
        Snapshot y = none;
        y.padButtons = 0x8000;
        cs.Update(y);
        Expect(cs.CaptureResult() == "Pad.Y", "pad button captured");
        cs.Update(none);
        cs.CaptureStart("Jump", 0, CapturePart::Whole, true);
        cs.CaptureCancel();
        Expect(cs.CaptureState() == Capture::State::Cancelled, "explicit cancel");
    }

    // --- standalone capture (editor Listen) -----------------------------------
    {
        Capture c;
        Expect(c.GetState() == Capture::State::Idle, "idle before start");
        Snapshot base;
        base.padConnected = true;
        base.leftX = 0.9f;  // stick already pushed
        c.Start(base, false);
        c.Update(base);
        Expect(c.GetState() == Capture::State::Listening, "already-pushed stick ignored");
        Snapshot back = base;
        back.leftX = 0.0f;
        c.Update(back);
        c.Update(base);
        Expect(c.GetState() == Capture::State::Done && SourceToString(c.Result()) == "Pad.LeftStickX+",
               "stick captured after release and push");
    }

    // Composition rules used by both the action set and the editor Listen button.
    {
        const Source stick{SourceKind::PadStick, 0, -1};
        const Source key{SourceKind::Key, 0x20, 0};
        Expect(ComposeCapturedBinding(ActionType::Axis, CapturePart::Whole, "", stick) == "Pad.LeftStickX",
               "axis whole -> full stick");
        Expect(ComposeCapturedBinding(ActionType::Button, CapturePart::Whole, "", stick) == "Pad.LeftStickX-",
               "button keeps direction");
        Expect(ComposeCapturedBinding(ActionType::Axis, CapturePart::Negative, "Key.D", key) == "Key.D/Key.SPACE",
               "negative on single makes a pair");
        Expect(ComposeCapturedBinding(ActionType::Axis, CapturePart::Positive, "Key.D/Key.A", key) ==
                   "Key.SPACE/Key.A", "positive keeps negative");
        Expect(ComposeCapturedBinding(ActionType::Axis, CapturePart::Positive, "Key.D", key) == "Key.SPACE",
               "positive on single replaces it");
        Expect(ComposeCapturedBinding(ActionType::Axis, CapturePart::Negative, "Key.D/Key.A", stick) ==
                   "Key.D/Pad.LeftStickX-", "pair side keeps the stick direction");
        Expect(ComposeCapturedBinding(ActionType::Button, CapturePart::Whole, "garbage", key) == "Key.SPACE",
               "unparsable existing replaced");
    }

    // Defaults reset runtime changes and skip duplicate names.
    {
        ActionSet d;
        d.SetDefaults(defs);
        d.SetBindings("Jump", {"Key.J"});
        std::vector<ActionDef> again = defs;
        again.push_back({"Jump", ActionType::Axis, {}});
        issues.clear();
        d.SetDefaults(again, &issues);
        Expect((*d.Bindings("Jump"))[0] == "Key.SPACE" && d.Defaults().size() == defs.size(),
               "set defaults resets and drops the duplicate");
        Expect(!issues.empty(), "duplicate reported");
    }

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("input_actions_test: all checks passed\n");
    return 0;
}
