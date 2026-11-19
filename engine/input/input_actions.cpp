// input_actions.cpp
// See input_actions.h.

#include "engine/input/input_actions.h"
#include "engine/input/key_names.h"

#include <algorithm>
#include <cmath>
#include <set>

using json = nlohmann::json;

namespace input_actions {

namespace {

struct NamedCode {
    const char* name;
    int code;
};

// XInput button masks (ControllerButton in controller.h).
constexpr NamedCode kPadButtons[] = {
    {"DPadUp", 0x0001}, {"DPadDown", 0x0002}, {"DPadLeft", 0x0004}, {"DPadRight", 0x0008},
    {"Start", 0x0010},  {"Back", 0x0020},     {"LeftThumb", 0x0040}, {"RightThumb", 0x0080},
    {"LB", 0x0100},     {"RB", 0x0200},       {"A", 0x1000},         {"B", 0x2000},
    {"X", 0x4000},      {"Y", 0x8000},
};
constexpr NamedCode kPadTriggers[] = {{"LT", 0}, {"RT", 1}};
constexpr NamedCode kPadSticks[] = {{"LeftStickX", 0}, {"LeftStickY", 1}, {"RightStickX", 2}, {"RightStickY", 3}};
constexpr NamedCode kMouseButtons[] = {{"Left", 0}, {"Right", 1}, {"Middle", 2}};

constexpr unsigned char kVkEscape = 0x1B;

template <std::size_t N>
bool LookupName(const NamedCode (&table)[N], std::string_view name, int* out) {
    for (const NamedCode& e : table) {
        if (name == e.name) {
            *out = e.code;
            return true;
        }
    }
    return false;
}

template <std::size_t N>
const char* LookupCode(const NamedCode (&table)[N], int code) {
    for (const NamedCode& e : table) {
        if (e.code == code)
            return e.name;
    }
    return nullptr;
}

std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

bool Fail(std::string* error, std::string message) {
    if (error)
        *error = std::move(message);
    return false;
}

float StickValue(const Snapshot& s, int axis) {
    switch (axis) {
        case 0: return s.leftX;
        case 1: return s.leftY;
        case 2: return s.rightX;
        default: return s.rightY;
    }
}

bool Pressed(float value) {
    return std::fabs(value) > kPressThreshold;
}

}  // namespace

bool ParseSource(std::string_view text, Source* out, std::string* error) {
    text = Trim(text);
    Source s;
    if (text.substr(0, 4) == "Key.") {
        unsigned char vk = 0;
        if (!key_names::FromName(text.substr(4), &vk))
            return Fail(error, "unknown key '" + std::string(text.substr(4)) + "'");
        s.kind = SourceKind::Key;
        s.code = vk;
    } else if (text.substr(0, 4) == "Pad.") {
        std::string_view rest = text.substr(4);
        int sign = 0;
        if (!rest.empty() && (rest.back() == '+' || rest.back() == '-')) {
            sign = rest.back() == '+' ? 1 : -1;
            rest.remove_suffix(1);
        }
        int code = 0;
        if (LookupName(kPadSticks, rest, &code)) {
            s.kind = SourceKind::PadStick;
            s.sign = sign;
        } else if (sign != 0) {
            return Fail(error, "+ / - only applies to stick axes: '" + std::string(text) + "'");
        } else if (LookupName(kPadButtons, rest, &code)) {
            s.kind = SourceKind::PadButton;
        } else if (LookupName(kPadTriggers, rest, &code)) {
            s.kind = SourceKind::PadTrigger;
        } else {
            return Fail(error, "unknown pad input '" + std::string(rest) + "'");
        }
        s.code = code;
    } else if (text.substr(0, 6) == "Mouse.") {
        int code = 0;
        if (!LookupName(kMouseButtons, text.substr(6), &code))
            return Fail(error, "unknown mouse button '" + std::string(text.substr(6)) + "'");
        s.kind = SourceKind::MouseButton;
        s.code = code;
    } else {
        return Fail(error, "'" + std::string(text) + "' must start with Key., Pad. or Mouse.");
    }
    if (out)
        *out = s;
    return true;
}

std::string SourceToString(const Source& s) {
    switch (s.kind) {
        case SourceKind::Key: {
            const char* name = key_names::ToName(static_cast<unsigned char>(s.code));
            return std::string("Key.") + (name ? name : "?");
        }
        case SourceKind::PadButton: {
            const char* name = LookupCode(kPadButtons, s.code);
            return std::string("Pad.") + (name ? name : "?");
        }
        case SourceKind::PadTrigger: {
            const char* name = LookupCode(kPadTriggers, s.code);
            return std::string("Pad.") + (name ? name : "?");
        }
        case SourceKind::PadStick: {
            const char* name = LookupCode(kPadSticks, s.code);
            return std::string("Pad.") + (name ? name : "?") + (s.sign > 0 ? "+" : s.sign < 0 ? "-" : "");
        }
        case SourceKind::MouseButton: {
            const char* name = LookupCode(kMouseButtons, s.code);
            return std::string("Mouse.") + (name ? name : "?");
        }
    }
    return "?";
}

bool ParseBinding(std::string_view text, Binding* out, std::string* error) {
    Binding b;
    const std::size_t slash = text.find('/');
    if (slash == std::string_view::npos) {
        if (!ParseSource(text, &b.positive, error))
            return false;
    } else {
        if (text.find('/', slash + 1) != std::string_view::npos)
            return Fail(error, "a pair has exactly one '/': '" + std::string(text) + "'");
        if (!ParseSource(text.substr(0, slash), &b.positive, error) ||
            !ParseSource(text.substr(slash + 1), &b.negative, error))
            return false;
        b.hasNegative = true;
    }
    if (out)
        *out = b;
    return true;
}

std::string BindingToString(const Binding& b) {
    std::string s = SourceToString(b.positive);
    if (b.hasNegative)
        s += "/" + SourceToString(b.negative);
    return s;
}

float SourceValue(const Source& s, const Snapshot& snap) {
    switch (s.kind) {
        case SourceKind::Key:
            return (s.code >= 0 && s.code < 256 && snap.keys[s.code]) ? 1.0f : 0.0f;
        case SourceKind::PadButton:
            return (snap.padConnected && (snap.padButtons & s.code)) ? 1.0f : 0.0f;
        case SourceKind::PadTrigger:
            if (!snap.padConnected) return 0.0f;
            return std::clamp(s.code == 0 ? snap.leftTrigger : snap.rightTrigger, 0.0f, 1.0f);
        case SourceKind::PadStick: {
            if (!snap.padConnected) return 0.0f;
            const float v = std::clamp(StickValue(snap, s.code), -1.0f, 1.0f);
            if (s.sign > 0) return std::max(v, 0.0f);
            if (s.sign < 0) return std::max(-v, 0.0f);
            return v;
        }
        case SourceKind::MouseButton:
            return (s.code >= 0 && s.code < 3 && snap.mouse[s.code]) ? 1.0f : 0.0f;
    }
    return 0.0f;
}

float BindingValue(const Binding& b, const Snapshot& snap) {
    float v = SourceValue(b.positive, snap);
    if (b.hasNegative)
        v -= SourceValue(b.negative, snap);
    return std::clamp(v, -1.0f, 1.0f);
}

bool IsValidActionName(std::string_view name) {
    if (name.empty() || name.size() > 64)
        return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
               c == '.' || c == '-';
    });
}

const char* ActionTypeName(ActionType type) {
    return type == ActionType::Axis ? "axis" : "button";
}

std::vector<ActionDef> ParseActions(const json& actions, std::vector<Issue>* issues) {
    std::vector<ActionDef> defs;
    auto report = [&](const std::string& action, const std::string& message) {
        if (issues) issues->push_back({action, message});
    };
    if (actions.is_null())
        return defs;
    if (!actions.is_object()) {
        report("", "input.actions must be an object of action name -> definition");
        return defs;
    }
    for (auto it = actions.begin(); it != actions.end(); ++it) {
        const std::string& name = it.key();
        if (!IsValidActionName(name)) {
            report(name, "invalid action name (use 1-64 of A-Z a-z 0-9 _ . -)");
            continue;
        }
        if (!it.value().is_object()) {
            report(name, "definition must be an object");
            continue;
        }
        ActionDef def;
        def.name = name;
        const json& v = it.value();
        const auto type = v.find("type");
        if (type != v.end()) {
            if (*type == "axis")
                def.type = ActionType::Axis;
            else if (*type != "button") {
                report(name, "type must be \"button\" or \"axis\"");
                continue;
            }
        }
        const auto bindings = v.find("bindings");
        if (bindings != v.end()) {
            if (!bindings->is_array()) {
                report(name, "bindings must be an array of strings");
            } else {
                for (const json& b : *bindings) {
                    if (b.is_string())
                        def.bindings.push_back(b.get<std::string>());
                    else
                        report(name, "a binding is not a string");
                }
            }
        }
        defs.push_back(std::move(def));
    }
    return defs;
}

json ActionsToJson(const std::vector<ActionDef>& defs) {
    json out = json::object();
    for (const ActionDef& d : defs)
        out[d.name] = {{"type", ActionTypeName(d.type)}, {"bindings", d.bindings}};
    return out;
}

std::vector<Issue> Validate(const std::vector<ActionDef>& defs) {
    std::vector<Issue> issues;
    std::set<std::string> seen;
    for (const ActionDef& d : defs) {
        if (!IsValidActionName(d.name))
            issues.push_back({d.name, "invalid action name (use 1-64 of A-Z a-z 0-9 _ . -)"});
        else if (!seen.insert(d.name).second)
            issues.push_back({d.name, "duplicate action name"});
        for (const std::string& text : d.bindings) {
            Binding b;
            std::string error;
            if (!ParseBinding(text, &b, &error))
                issues.push_back({d.name, error});
            else if (b.hasNegative && d.type == ActionType::Button)
                issues.push_back({d.name, "'" + text + "': pairs (A/B) need an axis action"});
        }
    }
    return issues;
}

std::string ComposeCapturedBinding(ActionType type, CapturePart part, std::string_view existing,
                                   Source captured) {
    if (captured.kind == SourceKind::PadStick && type == ActionType::Axis && part == CapturePart::Whole)
        captured.sign = 0;
    Binding old;
    const bool hasOld = !existing.empty() && ParseBinding(existing, &old);
    Binding result;
    result.positive = captured;
    if (part == CapturePart::Positive && hasOld && old.hasNegative) {
        result.hasNegative = true;
        result.negative = old.negative;
    } else if (part == CapturePart::Negative && hasOld) {
        result.positive = old.positive;
        result.hasNegative = true;
        result.negative = captured;
    }
    return BindingToString(result);
}

// --- Capture -----------------------------------------------------------------

void Capture::Start(const Snapshot& now, bool includeMouse) {
    m_state = State::Listening;
    m_includeMouse = includeMouse;
    m_prev = now;
    m_result = Source{};
}

void Capture::Cancel() {
    if (m_state == State::Listening)
        m_state = State::Cancelled;
}

Capture::State Capture::Update(const Snapshot& now) {
    if (m_state != State::Listening)
        return m_state;
    auto done = [&](Source s) {
        m_result = s;
        m_state = State::Done;
    };
    // Escape cancels; other keys bind.
    if (now.keys[kVkEscape] && !m_prev.keys[kVkEscape]) {
        m_state = State::Cancelled;
        return m_state;
    }
    for (int k = 1; k < 256 && m_state == State::Listening; ++k) {
        if (now.keys[k] && !m_prev.keys[k] && key_names::ToName(static_cast<unsigned char>(k)))
            done({SourceKind::Key, k, 0});
    }
    if (m_state == State::Listening && now.padConnected) {
        const std::uint16_t fresh = static_cast<std::uint16_t>(now.padButtons & ~m_prev.padButtons);
        for (const NamedCode& e : kPadButtons) {
            if (fresh & e.code) {
                done({SourceKind::PadButton, e.code, 0});
                break;
            }
        }
        const float triggers[2][2] = {{m_prev.leftTrigger, now.leftTrigger}, {m_prev.rightTrigger, now.rightTrigger}};
        for (int t = 0; t < 2 && m_state == State::Listening; ++t) {
            if (triggers[t][0] <= kPressThreshold && triggers[t][1] > kPressThreshold)
                done({SourceKind::PadTrigger, t, 0});
        }
        for (int a = 0; a < 4 && m_state == State::Listening; ++a) {
            const float before = StickValue(m_prev, a);
            const float after = StickValue(now, a);
            if (before <= kPressThreshold && after > kPressThreshold)
                done({SourceKind::PadStick, a, 1});
            else if (before >= -kPressThreshold && after < -kPressThreshold)
                done({SourceKind::PadStick, a, -1});
        }
    }
    if (m_state == State::Listening && m_includeMouse) {
        for (int b = 0; b < 3; ++b) {
            if (now.mouse[b] && !m_prev.mouse[b]) {
                done({SourceKind::MouseButton, b, 0});
                break;
            }
        }
    }
    // Released inputs may be captured on a later press.
    m_prev = now;
    return m_state;
}

// --- ActionSet ---------------------------------------------------------------

void ActionSet::Compile(Runtime& rt) {
    rt.compiled.clear();
    for (const std::string& text : rt.bindings) {
        Binding b;
        if (ParseBinding(text, &b) && !(b.hasNegative && rt.type == ActionType::Button))
            rt.compiled.push_back(b);
    }
}

void ActionSet::SetDefaults(std::vector<ActionDef> defaults, std::vector<Issue>* issues) {
    if (issues) {
        for (Issue& i : Validate(defaults))
            issues->push_back(std::move(i));
    }
    m_capture.Cancel();
    m_defaults.clear();
    m_actions.clear();
    for (ActionDef& d : defaults) {
        if (!IsValidActionName(d.name) || m_actions.count(d.name))
            continue;
        Runtime rt;
        rt.type = d.type;
        rt.bindings = d.bindings;
        Compile(rt);
        m_actions.emplace(d.name, std::move(rt));
        m_defaults.push_back(std::move(d));
    }
}

const ActionSet::Runtime* ActionSet::Find(std::string_view name) const {
    const auto it = m_actions.find(name);
    return it == m_actions.end() ? nullptr : &it->second;
}

ActionSet::Runtime* ActionSet::Find(std::string_view name) {
    const auto it = m_actions.find(name);
    return it == m_actions.end() ? nullptr : &it->second;
}

bool ActionSet::HasAction(std::string_view name) const {
    return Find(name) != nullptr;
}

const std::vector<std::string>* ActionSet::Bindings(std::string_view name) const {
    const Runtime* rt = Find(name);
    return rt ? &rt->bindings : nullptr;
}

bool ActionSet::SetBindings(std::string_view name, const std::vector<std::string>& bindings, std::string* error) {
    Runtime* rt = Find(name);
    if (!rt)
        return Fail(error, "unknown action '" + std::string(name) + "'");
    for (const std::string& text : bindings) {
        Binding b;
        if (!ParseBinding(text, &b, error))
            return false;
        if (b.hasNegative && rt->type == ActionType::Button)
            return Fail(error, "'" + text + "': pairs (A/B) need an axis action");
    }
    rt->bindings = bindings;
    Compile(*rt);
    return true;
}

bool ActionSet::ResetBindings(std::string_view name) {
    bool found = false;
    for (const ActionDef& d : m_defaults) {
        if (!name.empty() && d.name != name)
            continue;
        if (Runtime* rt = Find(d.name)) {
            rt->bindings = d.bindings;
            Compile(*rt);
            found = true;
        }
    }
    return found || (name.empty() && m_defaults.empty());
}

json ActionSet::OverridesToJson() const {
    json bindings = json::object();
    for (const ActionDef& d : m_defaults) {
        const Runtime* rt = Find(d.name);
        if (rt && rt->bindings != d.bindings)
            bindings[d.name] = rt->bindings;
    }
    return {{"version", 1}, {"bindings", bindings}};
}

int ActionSet::ApplyOverrides(const json& overrides, std::vector<Issue>* issues) {
    auto report = [&](const std::string& action, const std::string& message) {
        if (issues) issues->push_back({action, message});
    };
    if (!overrides.is_object())
        return 0;
    const auto bindings = overrides.find("bindings");
    if (bindings == overrides.end() || !bindings->is_object())
        return 0;
    int applied = 0;
    for (auto it = bindings->begin(); it != bindings->end(); ++it) {
        Runtime* rt = Find(it.key());
        if (!rt) {
            report(it.key(), "saved binding for an action that no longer exists; ignored");
            continue;
        }
        if (!it.value().is_array()) {
            report(it.key(), "saved bindings are not an array; ignored");
            continue;
        }
        std::vector<std::string> kept;
        for (const json& b : it.value()) {
            Binding parsed;
            std::string error;
            if (!b.is_string() || !ParseBinding(b.get<std::string>(), &parsed, &error) ||
                (parsed.hasNegative && rt->type == ActionType::Button)) {
                report(it.key(), "saved binding dropped: " + (b.is_string() ? b.get<std::string>() : b.dump()));
                continue;
            }
            kept.push_back(b.get<std::string>());
        }
        rt->bindings = std::move(kept);
        Compile(*rt);
        ++applied;
    }
    return applied;
}

void ActionSet::Update(const Snapshot& snapshot) {
    m_last = snapshot;
    for (auto& [name, rt] : m_actions) {
        float best = 0.0f;
        bool pressed = false;
        for (const Binding& b : rt.compiled) {
            const float v = BindingValue(b, snapshot);
            if (std::fabs(v) > std::fabs(best))
                best = v;
            if (Pressed(v))
                pressed = true;
        }
        rt.wasPressed = rt.pressed;
        rt.pressed = pressed;
        rt.value = rt.type == ActionType::Button ? (pressed ? 1.0f : 0.0f) : best;
    }
    if (m_capture.GetState() == Capture::State::Listening &&
        m_capture.Update(snapshot) == Capture::State::Done)
        FinishCapture();
}

bool ActionSet::IsPressed(std::string_view name) const {
    const Runtime* rt = Find(name);
    return rt && rt->pressed;
}

bool ActionSet::IsTriggered(std::string_view name) const {
    const Runtime* rt = Find(name);
    return rt && rt->pressed && !rt->wasPressed;
}

bool ActionSet::IsReleased(std::string_view name) const {
    const Runtime* rt = Find(name);
    return rt && !rt->pressed && rt->wasPressed;
}

float ActionSet::GetAxis(std::string_view name) const {
    const Runtime* rt = Find(name);
    return rt ? rt->value : 0.0f;
}

bool ActionSet::CaptureStart(std::string_view name, int index, CapturePart part, bool includeMouse,
                             std::string* error) {
    const Runtime* rt = Find(name);
    if (!rt)
        return Fail(error, "unknown action '" + std::string(name) + "'");
    if (index < 0 || index > static_cast<int>(rt->bindings.size()))
        return Fail(error, "binding index out of range");
    if (part != CapturePart::Whole && rt->type != ActionType::Axis)
        return Fail(error, "only axis actions have positive / negative parts");
    if (part == CapturePart::Negative && index == static_cast<int>(rt->bindings.size()))
        return Fail(error, "set the positive side first: a new binding cannot start with its negative side");
    m_captureAction = std::string(name);
    m_captureIndex = index;
    m_capturePart = part;
    m_captureResult.clear();
    m_capture.Start(m_last, includeMouse);
    return true;
}

void ActionSet::CaptureCancel() {
    m_capture.Cancel();
}

void ActionSet::FinishCapture() {
    Runtime* rt = Find(m_captureAction);
    if (!rt)
        return;
    const bool replacing = m_captureIndex < static_cast<int>(rt->bindings.size());
    m_captureResult = ComposeCapturedBinding(rt->type, m_capturePart,
                                             replacing ? std::string_view(rt->bindings[m_captureIndex]) : "",
                                             m_capture.Result());
    if (replacing)
        rt->bindings[m_captureIndex] = m_captureResult;
    else
        rt->bindings.push_back(m_captureResult);
    Compile(*rt);
}

}  // namespace input_actions
