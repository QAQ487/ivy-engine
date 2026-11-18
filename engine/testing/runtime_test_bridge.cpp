// runtime_test_bridge.cpp
// Implements EventsRecorder.

#include "runtime_test_bridge.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <set>
#include "engine/input/controller.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/debug_draw/debug_ofstream.h"
float GetBackBufferWidth();
float GetBackBufferHeight();
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace ivy::testing {
namespace {
struct EventsRecorder {
    bool enabled = false;
    std::mutex mutex;
    std::vector<RuntimeTestEvent> events;
    std::int64_t nextSequence = 1;
    std::uint32_t dropped = 0;
};
EventsRecorder g_events;

struct ControllerProbeRecorder {
    bool enabled = false;
    std::mutex mutex;
    ControllerProbeRing ring;
};
ControllerProbeRecorder g_controllerProbe;

constexpr ControllerButton kProbeButtons[] = {
    ControllerButton::A, ControllerButton::B, ControllerButton::X, ControllerButton::Y,
    ControllerButton::LB, ControllerButton::RB, ControllerButton::Start, ControllerButton::Back,
    ControllerButton::LeftThumb, ControllerButton::RightThumb,
    ControllerButton::DPadUp, ControllerButton::DPadDown, ControllerButton::DPadLeft, ControllerButton::DPadRight,
};

bool ValidEventName(const char* name) {
    if (!name || !*name) return false;
    const std::size_t n = std::strlen(name);
    if (n == 0 || n > 64) return false;
    for (std::size_t i = 0; i < n; ++i) {
        const char ch = name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
            return false;
    }
    return true;
}

Json ControllerProbeFrameToJson(const ControllerProbeFrame& frame) {
    Json pressed = Json::array();
    Json pressedEdge = Json::array();
    Json releasedEdge = Json::array();
    for (ControllerButton button : kProbeButtons) {
        const auto bit = static_cast<std::uint16_t>(button);
        std::string name;
        if (bit == kInjectButtonA) name = "A";
        else if (bit == kInjectButtonB) name = "B";
        else if (bit == kInjectButtonX) name = "X";
        else if (bit == kInjectButtonY) name = "Y";
        else if (bit == kInjectButtonLB) name = "LB";
        else if (bit == kInjectButtonRB) name = "RB";
        else if (bit == kInjectButtonStart) name = "START";
        else if (bit == kInjectButtonBack) name = "BACK";
        else if (bit == kInjectButtonLeftThumb) name = "LEFT_THUMB";
        else if (bit == kInjectButtonRightThumb) name = "RIGHT_THUMB";
        else if (bit == kInjectButtonDpadUp) name = "DPAD_UP";
        else if (bit == kInjectButtonDpadDown) name = "DPAD_DOWN";
        else if (bit == kInjectButtonDpadLeft) name = "DPAD_LEFT";
        else if (bit == kInjectButtonDpadRight) name = "DPAD_RIGHT";
        else continue;
        if ((frame.pressed & bit) != 0) pressed.push_back(name);
        if ((frame.pressedEdge & bit) != 0) pressedEdge.push_back(name);
        if ((frame.releasedEdge & bit) != 0) releasedEdge.push_back(name);
    }
    return {
        {"sequence", frame.sequence},
        {"lx", frame.lx}, {"ly", frame.ly},
        {"rx", frame.rx}, {"ry", frame.ry},
        {"lt", frame.lt}, {"rt", frame.rt},
        {"pressed", std::move(pressed)},
        {"pressedEdge", std::move(pressedEdge)},
        {"releasedEdge", std::move(releasedEdge)},
        {"connected", frame.connected},
    };
}
}

void RuntimeTestEvents_SetEnabled(bool enabled) {
    std::lock_guard lock(g_events.mutex);
    g_events.enabled = enabled;
    if (!enabled) {
        g_events.events.clear();
        g_events.nextSequence = 1;
        g_events.dropped = 0;
    }
}
void RuntimeTestEvents_Reset() {
    std::lock_guard lock(g_events.mutex);
    g_events.events.clear();
    g_events.nextSequence = 1;
    g_events.dropped = 0;
}
bool RuntimeTestEvents_IsEnabled() {
    std::lock_guard lock(g_events.mutex);
    return g_events.enabled;
}
bool RuntimeTestEvents_RecordCheckpoint(const char* checkpoint, const char* actor, int score) {
    if (!ValidEventName(checkpoint) || !ValidEventName(actor)) return false;
    if (score != 1) return false;
    std::lock_guard lock(g_events.mutex);
    if (!g_events.enabled) return false;
    if (g_events.events.size() >= kRuntimeTestEventCapacity) {
        ++g_events.dropped;
        return false;
    }
    RuntimeTestEvent ev;
    ev.sequence = g_events.nextSequence++;
    ev.type = "checkpoint_reached";
    ev.checkpoint = checkpoint;
    ev.actor = actor;
    ev.score = score;
    g_events.events.push_back(std::move(ev));
    return true;
}
RuntimeTestEventsSnapshot RuntimeTestEvents_CopySnapshot() {
    std::lock_guard lock(g_events.mutex);
    RuntimeTestEventsSnapshot snap;
    snap.events = g_events.events;
    snap.latestSequence = g_events.nextSequence - 1;
    if (snap.latestSequence < 0) snap.latestSequence = 0;
    snap.dropped = g_events.dropped;
    return snap;
}

void ControllerProbeApiDetect(ControllerProbeApiTag) noexcept {
    // Presence-only hook for compile-time detection; touch the API so the
    // positive control cannot pass against a stub that omits the real probes.
    (void)RuntimeTestControllerProbe_IsEnabled();
}

void RuntimeTestControllerProbe_SetEnabled(bool enabled) {
    std::lock_guard lock(g_controllerProbe.mutex);
    g_controllerProbe.enabled = enabled;
    if (!enabled)
        g_controllerProbe.ring.Reset();
}
void RuntimeTestControllerProbe_Reset() {
    std::lock_guard lock(g_controllerProbe.mutex);
    g_controllerProbe.ring.Reset();
}
bool RuntimeTestControllerProbe_IsEnabled() {
    std::lock_guard lock(g_controllerProbe.mutex);
    return g_controllerProbe.enabled;
}
void RuntimeTestControllerProbe_SampleFromGetters() {
    std::lock_guard lock(g_controllerProbe.mutex);
    if (!g_controllerProbe.enabled) return;
    ControllerProbeFrame frame;
    const auto left = Controller_GetLeftStick(0);
    const auto right = Controller_GetRightStick(0);
    frame.lx = left.x;
    frame.ly = left.y;
    frame.rx = right.x;
    frame.ry = right.y;
    frame.lt = Controller_GetLeftTrigger(0);
    frame.rt = Controller_GetRightTrigger(0);
    frame.connected = Controller_IsConnected(0);
    for (ControllerButton button : kProbeButtons) {
        const auto bit = static_cast<std::uint16_t>(button);
        if (Controller_IsPressed(button, 0)) frame.pressed = static_cast<std::uint16_t>(frame.pressed | bit);
        if (Controller_IsTrigger(button, 0)) frame.pressedEdge = static_cast<std::uint16_t>(frame.pressedEdge | bit);
        if (Controller_IsRelease(button, 0)) frame.releasedEdge = static_cast<std::uint16_t>(frame.releasedEdge | bit);
    }
    g_controllerProbe.ring.Push(frame);
}
ControllerProbeSnapshot RuntimeTestControllerProbe_CopySnapshot() {
    std::lock_guard lock(g_controllerProbe.mutex);
    return g_controllerProbe.ring.Snapshot();
}

std::uint32_t RemainingFrameTimeout(std::uint64_t deadline, std::uint64_t now) noexcept {
    if (now >= deadline) return 0;
    const auto remaining = deadline - now;
    return static_cast<std::uint32_t>(
        remaining > kRuntimeTestFrameBodyTimeoutMs ? kRuntimeTestFrameBodyTimeoutMs : remaining);
}

std::uint32_t RequestFrameByteTimeout(bool frameStarted, std::uint64_t bodyDeadline, std::uint64_t now) noexcept {
    if (!frameStarted) return INFINITE;
    return RemainingFrameTimeout(bodyDeadline, now);
}

std::uint64_t InputInjectDeadlineMs(int updates) noexcept {
    if (updates < 1) updates = 1;
    if (updates > kInputInjectMaxUpdates) updates = kInputInjectMaxUpdates;
    return kInputInjectDeadlineBaseMs +
        static_cast<std::uint64_t>(updates) * kInputInjectDeadlinePerUpdateMs;
}

std::uint64_t InputInjectDeadlineFromDurationMs(int durationMs) noexcept {
    if (durationMs < 1) durationMs = 1;
    if (durationMs > kInputInjectMaxDurationMs) durationMs = kInputInjectMaxDurationMs;
    return kInputInjectDeadlineBaseMs + static_cast<std::uint64_t>(durationMs);
}

ObservePose ResolveObservedPose(
    bool hasTransformComponent,
    const ObservePose& legacy,
    const ObservePose& transform) noexcept {
    return hasTransformComponent ? transform : legacy;
}

void AssignObservedPose(ObservedTarget& target, const ObservePose& pose) noexcept {
    target.posX = pose.position.x;
    target.posY = pose.position.y;
    target.posZ = pose.position.z;
    target.hasRotation = true;
    target.rotX = pose.rotation.x;
    target.rotY = pose.rotation.y;
    target.rotZ = pose.rotation.z;
    target.hasScale = true;
    target.scaleX = pose.scale.x;
    target.scaleY = pose.scale.y;
    target.scaleZ = pose.scale.z;
}
Protocol::Protocol(std::string runId, unsigned long pid, unsigned long session, BridgeHostKind host)
    : runId_(std::move(runId)), pid_(pid), session_(session), host_(host) {}

bool IsRuntimeTestBridgeSettingKey(const std::string& key) {
    return key == "frustumCullingEnabled"
        || key == "shadowCascadeCullingEnabled"
        || key == "spriteCullingEnabled"
        || key == "lodEnabled";
}

bool ParseRuntimeTestBridgeSettingValue(const Json& value, bool& outBool, std::string& err) {
    err.clear();
    if (!value.is_boolean()) {
        err = "value must be boolean";
        return false;
    }
    outBool = value.get<bool>();
    return true;
}

EditorPlayModeKind ParseRuntimeTestBridgePlayMode(const char* mode) noexcept {
    if (mode == nullptr || mode[0] == '\0') return EditorPlayModeKind::Viewport;
    if (std::strcmp(mode, "viewport") == 0) return EditorPlayModeKind::Viewport;
    if (std::strcmp(mode, "fullscreen") == 0) return EditorPlayModeKind::Fullscreen;
    return EditorPlayModeKind::Invalid;
}

const char* EditorPlayModeKindName(EditorPlayModeKind mode) noexcept {
    switch (mode) {
    case EditorPlayModeKind::Viewport: return "viewport";
    case EditorPlayModeKind::Fullscreen: return "fullscreen";
    case EditorPlayModeKind::None: return "none";
    default: return "invalid";
    }
}

EditorBridgeQuitPlan PlanEditorBridgeHostQuit(bool playing) noexcept {
    // Mirror EngineGUI_RequestBridgeHostQuit: StopPlay if needed, then destroy
    // via WM_APP+48 (bypasses unsaved/close dialogs). Runtime keeps RequestExitGame.
    EditorBridgeQuitPlan plan;
    plan.stopPlayFirst = playing;
    plan.postDestroyWindow = true;
    plan.skipUnsavedConfirm = true;
    return plan;
}
namespace {
std::string g_sceneOverride;
Json ObserveTargetJson(const ObservedTarget& t) {
    Json body = {
        {"protocolVersion", 1},
        {"found", true},
        {"target", t.name},
        {"active", t.active},
        {"position", {{"x", t.posX}, {"y", t.posY}, {"z", t.posZ}}},
        {"velocity", {{"x", t.velX}, {"y", t.velY}, {"z", t.velZ}}},
    };
    if (t.hasRotation) {
        body["rotation"] = {{"x", t.rotX}, {"y", t.rotY}, {"z", t.rotZ}};
        body["rotationQuaternion"] = {
            {"x", t.quatX}, {"y", t.quatY}, {"z", t.quatZ}, {"w", t.quatW}};
    }
    if (t.hasScale)
        body["scale"] = {{"x", t.scaleX}, {"y", t.scaleY}, {"z", t.scaleZ}};
    if (t.hasAngularVelocity)
        body["angularVelocity"] = {{"x", t.angVelX}, {"y", t.angVelY}, {"z", t.angVelZ}};
    if (t.hasRigidbody) {
        Json rb = {
            {"kinematic", t.kinematic},
            {"mass", t.mass},
            {"drag", t.drag},
            {"angularDrag", t.angularDrag},
            {"friction", t.friction},
            {"useGravity", t.useGravity},
        };
        if (t.hasLocalInverseInertia) {
            rb["localInverseInertia"] = {
                {"x", t.invInertiaX}, {"y", t.invInertiaY}, {"z", t.invInertiaZ}};
        }
        body["rigidbody"] = std::move(rb);
    }
    return body;
}
Json PerfStatsJson(const std::vector<float>& values) {
    if (values.empty())
        return {{"avg", 0.0}, {"min", 0.0}, {"max", 0.0}, {"p95", 0.0}};
    double sum = 0.0;
    float mn = values[0], mx = values[0];
    for (float v : values) {
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    std::vector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t idx = (sorted.size() * 95) / 100;
    const float p95 = sorted[(std::min)(idx, sorted.size() - 1)];
    return {
        {"avg", sum / static_cast<double>(values.size())},
        {"min", mn}, {"max", mx}, {"p95", p95},
    };
}
}
void RuntimeTestSceneOverride_Set(std::string romPath) { g_sceneOverride = std::move(romPath); }
bool RuntimeTestSceneOverride_Has() { return !g_sceneOverride.empty(); }
const std::string& RuntimeTestSceneOverride_Get() { return g_sceneOverride; }
void RuntimeTestSceneOverride_Clear() { g_sceneOverride.clear(); }

void Protocol::Publish(bool ready, bool playing, const std::string& scene,
    std::vector<ObservedTarget> targets, std::vector<ListedObject> objects,
    const char* readyDetail, EditorPlayModeKind playMode) {
    ready_ = ready; playing_ = playing;
    if (host_ == BridgeHostKind::Editor) {
        if (!playing_) playMode_ = EditorPlayModeKind::None;
        else if (playMode == EditorPlayModeKind::Viewport || playMode == EditorPlayModeKind::Fullscreen)
            playMode_ = playMode;
        else if (playMode_ != EditorPlayModeKind::Viewport && playMode_ != EditorPlayModeKind::Fullscreen)
            playMode_ = EditorPlayModeKind::Viewport;
    } else {
        playMode_ = EditorPlayModeKind::None;
    }
    std::size_t end = scene.size() < 1024 ? scene.size() : 1024;
    if (end < scene.size()) {
        while (end > 0 && (static_cast<unsigned char>(scene[end]) & 0xC0) == 0x80) --end;
    }
    scene_ = scene.substr(0, end);
    targets_ = std::move(targets);
    objects_ = std::move(objects);
    // Seal world ops only after the observe/list snapshot is replaced so
    // worldOp.status==completed implies a subsequent observe sees the apply.
    SealAppliedWorldOp();
    SealAppliedEditorOp();
    if (readyDetail && readyDetail[0])
        readyDetail_ = readyDetail;
    else if (!ready_)
        readyDetail_ = scene_.empty() ? "scene_path_empty" : "not_ready";
    else if (!playing_)
        readyDetail_ = "not_playing";
    else
        readyDetail_.clear();
    const auto snap = RuntimeTestEvents_CopySnapshot();
    events_ = snap.events;
    eventsLatestSequence_ = snap.latestSequence;
    eventsDropped_ = snap.dropped;
}
bool Protocol::ShouldRunSimTick() {
    if (!paused_) return true;
    if (pendingStepFrames_ <= 0) return false;
    --pendingStepFrames_;
    return true;
}
void Protocol::NoteSimFrame() { ++frameCounter_; }
void Protocol::NotePhysicsTick() { ++physicsTickCounter_; }
void Protocol::RecordPerfSample(const PerfSample& sample) {
    perfRing_[perfWrite_ % kRuntimeTestPerfCapacity] = sample;
    ++perfWrite_;
    if (perfCount_ < kRuntimeTestPerfCapacity) ++perfCount_;
}
bool Protocol::TakeQuit() { const bool result=quitReady_; quitReady_=false; return result; }
void Protocol::ConfirmQuitReceipt() { if(quitPending_) { quitReady_=true; quitPending_=false; } }
void Protocol::AbandonQuit() { quitPending_=false; }
// Explicit bridge key-name table (never raw VK codes). Keep in sync with
// tools/runtime_bridge_keyboard_keys.py KEYBOARD_KEYS — checked by
// tools/runtime_bridge_keyboard_keys_check.py. Omits Win keys; ALT+F4 rejected separately.
struct BridgeKeyEntry { const char* name; Keyboard_Keys code; };
static const BridgeKeyEntry kBridgeKeyTable[] = {
    {"BACK", KK_BACK},
    {"TAB", KK_TAB},
    {"ENTER", KK_ENTER},
    {"PAUSE", KK_PAUSE},
    {"CAPSLOCK", KK_CAPSLOCK},
    {"KANA", KK_KANA},
    {"KANJI", KK_KANJI},
    {"ESCAPE", KK_ESCAPE},
    {"IMECONVERT", KK_IMECONVERT},
    {"IMENOCONVERT", KK_IMENOCONVERT},
    {"SPACE", KK_SPACE},
    {"PAGEUP", KK_PAGEUP},
    {"PAGEDOWN", KK_PAGEDOWN},
    {"END", KK_END},
    {"HOME", KK_HOME},
    {"LEFT", KK_LEFT},
    {"UP", KK_UP},
    {"RIGHT", KK_RIGHT},
    {"DOWN", KK_DOWN},
    {"SELECT", KK_SELECT},
    {"PRINT", KK_PRINT},
    {"EXECUTE", KK_EXECUTE},
    {"PRINTSCREEN", KK_PRINTSCREEN},
    {"INSERT", KK_INSERT},
    {"DELETE", KK_DELETE},
    {"HELP", KK_HELP},
    {"0", KK_D0},
    {"1", KK_D1},
    {"2", KK_D2},
    {"3", KK_D3},
    {"4", KK_D4},
    {"5", KK_D5},
    {"6", KK_D6},
    {"7", KK_D7},
    {"8", KK_D8},
    {"9", KK_D9},
    {"A", KK_A},
    {"B", KK_B},
    {"C", KK_C},
    {"D", KK_D},
    {"E", KK_E},
    {"F", KK_F},
    {"G", KK_G},
    {"H", KK_H},
    {"I", KK_I},
    {"J", KK_J},
    {"K", KK_K},
    {"L", KK_L},
    {"M", KK_M},
    {"N", KK_N},
    {"O", KK_O},
    {"P", KK_P},
    {"Q", KK_Q},
    {"R", KK_R},
    {"S", KK_S},
    {"T", KK_T},
    {"U", KK_U},
    {"V", KK_V},
    {"W", KK_W},
    {"X", KK_X},
    {"Y", KK_Y},
    {"Z", KK_Z},
    {"APPS", KK_APPS},
    {"SLEEP", KK_SLEEP},
    {"NUMPAD0", KK_NUMPAD0},
    {"NUMPAD1", KK_NUMPAD1},
    {"NUMPAD2", KK_NUMPAD2},
    {"NUMPAD3", KK_NUMPAD3},
    {"NUMPAD4", KK_NUMPAD4},
    {"NUMPAD5", KK_NUMPAD5},
    {"NUMPAD6", KK_NUMPAD6},
    {"NUMPAD7", KK_NUMPAD7},
    {"NUMPAD8", KK_NUMPAD8},
    {"NUMPAD9", KK_NUMPAD9},
    {"MULTIPLY", KK_MULTIPLY},
    {"ADD", KK_ADD},
    {"SEPARATOR", KK_SEPARATOR},
    {"SUBTRACT", KK_SUBTRACT},
    {"DECIMAL", KK_DECIMAL},
    {"DIVIDE", KK_DIVIDE},
    {"F1", KK_F1},
    {"F2", KK_F2},
    {"F3", KK_F3},
    {"F4", KK_F4},
    {"F5", KK_F5},
    {"F6", KK_F6},
    {"F7", KK_F7},
    {"F8", KK_F8},
    {"F9", KK_F9},
    {"F10", KK_F10},
    {"F11", KK_F11},
    {"F12", KK_F12},
    {"F13", KK_F13},
    {"F14", KK_F14},
    {"F15", KK_F15},
    {"F16", KK_F16},
    {"F17", KK_F17},
    {"F18", KK_F18},
    {"F19", KK_F19},
    {"F20", KK_F20},
    {"F21", KK_F21},
    {"F22", KK_F22},
    {"F23", KK_F23},
    {"F24", KK_F24},
    {"NUMLOCK", KK_NUMLOCK},
    {"SCROLL", KK_SCROLL},
    {"LSHIFT", KK_LEFTSHIFT},
    {"RSHIFT", KK_RIGHTSHIFT},
    {"LCTRL", KK_LEFTCONTROL},
    {"RCTRL", KK_RIGHTCONTROL},
    {"LALT", KK_LEFTALT},
    {"RALT", KK_RIGHTALT},
    {"BROWSERBACK", KK_BROWSERBACK},
    {"BROWSERFORWARD", KK_BROWSERFORWARD},
    {"BROWSERREFRESH", KK_BROWSERREFRESH},
    {"BROWSERSTOP", KK_BROWSERSTOP},
    {"BROWSERSEARCH", KK_BROWSERSEARCH},
    {"BROWSERFAVORITES", KK_BROWSERFAVORITES},
    {"BROWSERHOME", KK_BROWSERHOME},
    {"VOLUMEMUTE", KK_VOLUMEMUTE},
    {"VOLUMEDOWN", KK_VOLUMEDOWN},
    {"VOLUMEUP", KK_VOLUMEUP},
    {"MEDIANEXTTRACK", KK_MEDIANEXTTRACK},
    {"MEDIAPREVIOUSTRACK", KK_MEDIAPREVIOUSTRACK},
    {"MEDIASTOP", KK_MEDIASTOP},
    {"MEDIAPLAYPAUSE", KK_MEDIAPLAYPAUSE},
    {"LAUNCHMAIL", KK_LAUNCHMAIL},
    {"SELECTMEDIA", KK_SELECTMEDIA},
    {"LAUNCHAPPLICATION1", KK_LAUNCHAPPLICATION1},
    {"LAUNCHAPPLICATION2", KK_LAUNCHAPPLICATION2},
    {"OEMSEMICOLON", KK_OEMSEMICOLON},
    {"OEMPLUS", KK_OEMPLUS},
    {"OEMCOMMA", KK_OEMCOMMA},
    {"OEMMINUS", KK_OEMMINUS},
    {"OEMPERIOD", KK_OEMPERIOD},
    {"OEMQUESTION", KK_OEMQUESTION},
    {"OEMTILDE", KK_OEMTILDE},
    {"OEMOPENBRACKETS", KK_OEMOPENBRACKETS},
    {"OEMPIPE", KK_OEMPIPE},
    {"OEMCLOSEBRACKETS", KK_OEMCLOSEBRACKETS},
    {"OEMQUOTES", KK_OEMQUOTES},
    {"OEM8", KK_OEM8},
    {"OEMBACKSLASH", KK_OEMBACKSLASH},
    {"PROCESSKEY", KK_PROCESSKEY},
    {"OEMCOPY", KK_OEMCOPY},
    {"OEMAUTO", KK_OEMAUTO},
    {"OEMENLW", KK_OEMENLW},
    {"ATTN", KK_ATTN},
    {"CRSEL", KK_CRSEL},
    {"EXSEL", KK_EXSEL},
    {"ERASEEOF", KK_ERASEEOF},
    {"PLAY", KK_PLAY},
    {"ZOOM", KK_ZOOM},
    {"PA1", KK_PA1},
    {"OEMCLEAR", KK_OEMCLEAR},
};
bool LookupBridgeKey(const std::string& name, Keyboard_Keys* out = nullptr) {
    for(const auto& entry : kBridgeKeyTable) {
        if(name == entry.name) {
            if(out) *out = entry.code;
            return true;
        }
    }
    return false;
}
bool ForbiddenKeyboardCombo(const std::vector<std::string>& keys) {
    bool alt = false, f4 = false;
    for(const auto& name : keys) {
        if(name == "LALT" || name == "RALT") alt = true;
        if(name == "F4") f4 = true;
    }
    return alt && f4;
}

const char* Protocol::InputStatus() const {
    switch(inputState_) {
    case InputState::Queued:
    case InputState::Preparing: return "queued";
    case InputState::Active: return "active";
    case InputState::Releasing: return "releasing";
    case InputState::Completed: return "completed";
    case InputState::Cancelled: return "cancelled";
    default: return "idle";
    }
}
const char* Protocol::MouseStatus() const {
    switch(mouseState_) {
    case InputState::Queued:
    case InputState::Preparing: return "queued";
    case InputState::Active: return "active";
    case InputState::Releasing: return "releasing";
    case InputState::Completed: return "completed";
    case InputState::Cancelled: return "cancelled";
    default: return "idle";
    }
}
const char* Protocol::ControllerStatus() const {
    return controllerOp_.Status();
}
bool Protocol::KeyboardIsolating() const {
    return inputState_ == InputState::Preparing || inputState_ == InputState::Active || inputState_ == InputState::Releasing;
}
bool Protocol::MouseIsolating() const {
    return mouseState_ == InputState::Preparing || mouseState_ == InputState::Active || mouseState_ == InputState::Releasing;
}
bool Protocol::ControllerIsolating() const {
    return controllerOp_.Isolating();
}
std::vector<std::string> Protocol::InputKeys() const {
    return inputState_ == InputState::Active ? keyboardKeys_ : std::vector<std::string>{};
}
void Protocol::GetMouseInput(int& dx, int& dy, unsigned int& buttonsMask, int& wheel,
                             bool& hasAbsolute, int& absX, int& absY) const {
    if (mouseState_ == InputState::Active) {
        dx = mouseDx_;
        dy = mouseDy_;
        buttonsMask = mouseButtons_;
        wheel = mouseWheel_;
    } else {
        dx = 0;
        dy = 0;
        buttonsMask = 0;
        wheel = 0;
    }
    if (MouseIsolating() && mouseHasAbsolute_) {
        hasAbsolute = true;
        absX = mouseAbsX_;
        absY = mouseAbsY_;
    } else {
        hasAbsolute = false;
        absX = 0;
        absY = 0;
    }
}
void Protocol::GetControllerInput(float& lx, float& ly, float& rx, float& ry, float& lt, float& rt, std::uint16_t& buttons) const {
    const ControllerInjectSample sample = controllerOp_.Sample();
    lx = sample.lx;
    ly = sample.ly;
    rx = sample.rx;
    ry = sample.ry;
    lt = sample.lt;
    rt = sample.rt;
    buttons = sample.buttons;
}
void Protocol::CancelKeyboard() {
    if(inputState_ == InputState::Queued) {
        inputState_ = InputState::Cancelled;
        keyboardUpdatesRemaining_ = 0;
        keyboardKeys_.clear();
    } else if(KeyboardIsolating()) {
        cancelInput_ = true;
    }
}
bool Protocol::MouseHoldFinished(std::uint64_t now) const noexcept {
    if(mouseDurationMs_ > 0) {
        return mouseHoldStartMs_ != 0 &&
            now >= mouseHoldStartMs_ + static_cast<std::uint64_t>(mouseDurationMs_);
    }
    return mouseUpdatesRemaining_ == 0;
}
void Protocol::CancelMouse() {
    if(mouseState_ == InputState::Queued) {
        mouseState_ = InputState::Cancelled;
        mouseUpdatesRemaining_ = 0;
        mouseDurationMs_ = 0;
        mouseHoldStartMs_ = 0;
        mouseDx_ = 0; mouseDy_ = 0; mouseButtons_ = 0; mouseWheel_ = 0;
        mouseHasAbsolute_ = false; mouseAbsX_ = 0; mouseAbsY_ = 0;
    } else if(MouseIsolating()) {
        cancelMouse_ = true;
    }
}
void Protocol::CancelController() {
    controllerOp_.Cancel();
}
void Protocol::ExpireKeyboard(std::uint64_t now) {
    if(now >= inputDeadline_) CancelKeyboard();
}
void Protocol::ExpireMouse(std::uint64_t now) {
    if(now >= mouseDeadline_) CancelMouse();
}
void Protocol::ExpireController(std::uint64_t now) {
    if(now >= controllerDeadline_) CancelController();
}
void Protocol::BeginInput(bool playing, std::uint64_t now) {
    ExpireKeyboard(now);
    ExpireMouse(now);
    ExpireController(now);
    if(!playing) { CancelKeyboard(); CancelMouse(); }
    if(inputState_ == InputState::Queued) inputState_ = InputState::Preparing;
    if(inputState_ == InputState::Active && (cancelInput_ || keyboardUpdatesRemaining_ == 0)) {
        inputState_ = InputState::Releasing;
        keyboardUpdatesRemaining_ = 0;
        keyboardKeys_.clear();
    }
    if(mouseState_ == InputState::Queued) mouseState_ = InputState::Preparing;
    if(mouseState_ == InputState::Active && mouseDurationMs_ > 0 && mouseHoldStartMs_ == 0)
        mouseHoldStartMs_ = now;
    if(mouseState_ == InputState::Active && (cancelMouse_ || MouseHoldFinished(now))) {
        mouseState_ = InputState::Releasing;
        mouseUpdatesRemaining_ = 0;
        mouseDx_ = 0; mouseDy_ = 0; mouseButtons_ = 0; mouseWheel_ = 0;
        // Keep mouseHasAbsolute_/mouseAbsX_/mouseAbsY_ through release so
        // Mouse_GetPosition stays overridden for the whole isolating lifecycle.
    }
    controllerOp_.Begin(playing);
}
void Protocol::EndInput() {
    if(inputState_ == InputState::Preparing) inputState_ = InputState::Active;
    else if(inputState_ == InputState::Active && keyboardUpdatesRemaining_ > 0) --keyboardUpdatesRemaining_;
    else if(inputState_ == InputState::Releasing) {
        inputState_ = cancelInput_ ? InputState::Cancelled : InputState::Completed;
    }
    if(mouseState_ == InputState::Preparing) mouseState_ = InputState::Active;
    else if(mouseState_ == InputState::Active && mouseDurationMs_ == 0 && mouseUpdatesRemaining_ > 0)
        --mouseUpdatesRemaining_;
    else if(mouseState_ == InputState::Releasing) {
        mouseState_ = cancelMouse_ ? InputState::Cancelled : InputState::Completed;
        mouseDurationMs_ = 0;
        mouseHoldStartMs_ = 0;
    }
    controllerOp_.End();
}

bool Protocol::HasPendingCapture(std::int64_t& outId) const {
    if (captureState_.status == CaptureState::Status::Queued) {
        outId = captureState_.requestId;
        return true;
    }
    return false;
}

void Protocol::CompleteCapture(std::int64_t id, bool ok, uint32_t w, uint32_t h, const std::string& basename,
    const std::string& path, const std::string& err) {
    if (captureState_.status == CaptureState::Status::Queued && captureState_.requestId == id) {
        captureState_.status = ok ? CaptureState::Status::Completed : CaptureState::Status::Failed;
        captureState_.width = w;
        captureState_.height = h;
        captureState_.basename = basename;
        captureState_.path = path;
        captureState_.error = err;
    }
}

const char* Protocol::WorldOpState::VerbName() const {
    switch (request.kind) {
    case WorldOpKind::ResetScene: return "reset_scene";
    case WorldOpKind::SetTransform: return "set_transform";
    case WorldOpKind::SetVelocity: return "set_velocity";
    case WorldOpKind::ApplyImpulse: return "apply_impulse";
    case WorldOpKind::ApplyTorqueImpulse: return "apply_torque_impulse";
    }
    return "unknown";
}

bool Protocol::HasPendingWorldOp(WorldOpRequest& out) const {
    if (worldOp_.status == WorldOpState::Status::Queued) {
        out = worldOp_.request;
        return true;
    }
    return false;
}

void Protocol::CompleteWorldOp(std::int64_t id, bool ok, const std::string& err) {
    if (worldOp_.status == WorldOpState::Status::Queued && worldOp_.request.requestId == id) {
        worldOp_.status = ok ? WorldOpState::Status::Completed : WorldOpState::Status::Failed;
        worldOp_.appliedOk = ok;
        worldOp_.error = err;
    }
}

void Protocol::MarkWorldOpApplied(std::int64_t id, bool ok, const std::string& err) {
    if (worldOp_.status == WorldOpState::Status::Queued && worldOp_.request.requestId == id) {
        worldOp_.status = WorldOpState::Status::Applied;
        worldOp_.appliedOk = ok;
        worldOp_.error = err;
    }
}

void Protocol::SealAppliedWorldOp() {
    if (worldOp_.status != WorldOpState::Status::Applied)
        return;
    worldOp_.status = worldOp_.appliedOk ? WorldOpState::Status::Completed
                                         : WorldOpState::Status::Failed;
}

void Protocol::CancelWorldOp() {
    if (worldOp_.status == WorldOpState::Status::Queued
        || worldOp_.status == WorldOpState::Status::Applied) {
        worldOp_.status = WorldOpState::Status::Failed;
        worldOp_.appliedOk = false;
        worldOp_.error = "cancelled";
    }
}

const char* Protocol::EditorOpState::VerbName() const {
    switch (request.kind) {
    case EditorOpKind::PlayEnter: return "play_enter";
    case EditorOpKind::PlayExit: return "play_exit";
    case EditorOpKind::LoadScene: return "load_scene";
    case EditorOpKind::SetSetting: return "set_setting";
    }
    return "unknown";
}

bool Protocol::HasPendingEditorOp(EditorOpRequest& out) const {
    if (editorOp_.status == EditorOpState::Status::Queued) {
        out = editorOp_.request;
        return true;
    }
    return false;
}

void Protocol::CompleteEditorOp(std::int64_t id, bool ok, const std::string& err) {
    if (editorOp_.status == EditorOpState::Status::Queued && editorOp_.request.requestId == id) {
        editorOp_.status = ok ? EditorOpState::Status::Completed : EditorOpState::Status::Failed;
        editorOp_.appliedOk = ok;
        editorOp_.error = err;
    }
}

void Protocol::MarkEditorOpApplied(std::int64_t id, bool ok, const std::string& err) {
    if (editorOp_.status == EditorOpState::Status::Queued && editorOp_.request.requestId == id) {
        editorOp_.status = EditorOpState::Status::Applied;
        editorOp_.appliedOk = ok;
        editorOp_.error = err;
    }
}

void Protocol::SealAppliedEditorOp() {
    if (editorOp_.status != EditorOpState::Status::Applied)
        return;
    editorOp_.status = editorOp_.appliedOk ? EditorOpState::Status::Completed
                                           : EditorOpState::Status::Failed;
}

void Protocol::CancelEditorOp() {
    if (editorOp_.status == EditorOpState::Status::Queued
        || editorOp_.status == EditorOpState::Status::Applied) {
        editorOp_.status = EditorOpState::Status::Failed;
        editorOp_.appliedOk = false;
        editorOp_.error = "cancelled";
    }
}

namespace {
bool FiniteNumber(const Json& v, float& out) {
    if (!v.is_number() || v.is_boolean()) return false;
    const double d = v.get<double>();
    if (!std::isfinite(d)) return false;
    out = static_cast<float>(d);
    return true;
}
bool ParseVec3(const Json& v, float& x, float& y, float& z) {
    if (!v.is_array() || v.size() != 3) return false;
    return FiniteNumber(v[0], x) && FiniteNumber(v[1], y) && FiniteNumber(v[2], z);
}
bool ParseVec4(const Json& v, float& x, float& y, float& z, float& w) {
    if (!v.is_array() || v.size() != 4) return false;
    return FiniteNumber(v[0], x) && FiniteNumber(v[1], y) && FiniteNumber(v[2], z) && FiniteNumber(v[3], w);
}
bool ValidObjectName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    for (char ch : name) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
            return false;
    }
    return true;
}
bool OnlyKeys(const Json& q, std::initializer_list<const char*> allowed) {
    for (auto it = q.begin(); it != q.end(); ++it) {
        bool ok = false;
        for (const char* key : allowed) {
            if (it.key() == key) { ok = true; break; }
        }
        if (!ok) return false;
    }
    return true;
}
}

Json Protocol::Request(const std::string& frame) {
    ExpireKeyboard(GetTickCount64());
    ExpireMouse(GetTickCount64());
    ExpireController(GetTickCount64());
    // Echo requestId in all error replies once the envelope validated it.
    // Unparseable frames / missing-or-invalid requestId keep requestId null.
    Json requestIdForReply = nullptr;
    auto error = [this, &requestIdForReply](const char* name, Json id = nullptr) -> Json {
        if (id.is_null()) id = requestIdForReply;
        return {{"protocolVersion",1},{"requestId",std::move(id)},{"runId",runId_},{"ok",false},{"error",name}};
    };
    if(frame.size()>4096) return error("message_too_large");
    auto q=Json::parse(frame,nullptr,false);
    if(q.is_discarded()) return error("malformed_json");
    if(!q.is_object() || !q.contains("protocolVersion") || !q["protocolVersion"].is_number_integer()
        || !q.contains("requestId") || !q["requestId"].is_number_integer() || q["requestId"]<1 || q["requestId"]>2147483647
        || !q.contains("runId") || !q["runId"].is_string() || !q.contains("verb") || !q["verb"].is_string())
        return error("invalid_request");
    requestIdForReply = q["requestId"];
    const bool input=q["verb"]=="keyboard";
    const bool mouse=q["verb"]=="mouse";
    const bool controller=q["verb"]=="controller";
    const bool observe=q["verb"]=="observe";
    const bool capture=q["verb"]=="capture";
    const bool events=q["verb"]=="events";
    const bool controllerProbe=q["verb"]=="controller_probe";
    const bool listObjects=q["verb"]=="list_objects";
    const bool pause=q["verb"]=="pause";
    const bool resume=q["verb"]=="resume";
    const bool step=q["verb"]=="step";
    const bool perf=q["verb"]=="perf";
    const bool resetScene=q["verb"]=="reset_scene";
    const bool setTransform=q["verb"]=="set_transform";
    const bool setVelocity=q["verb"]=="set_velocity";
    const bool applyImpulse=q["verb"]=="apply_impulse";
    const bool applyTorqueImpulse=q["verb"]=="apply_torque_impulse";
    const bool playEnter=q["verb"]=="play_enter";
    const bool playExit=q["verb"]=="play_exit";
    const bool loadScene=q["verb"]=="load_scene";
    const bool setSetting=q["verb"]=="set_setting";
    const bool worldOp=resetScene||setTransform||setVelocity||applyImpulse||applyTorqueImpulse;
    std::vector<std::string> keys;
    int updates=0;
    int mouseDx=0, mouseDy=0, mouseWheel=0, mouseUpdates=0, mouseDurationMs=0;
    unsigned int mouseButtonsMask=0;
    bool mouseHasAbsolute=false;
    int mouseAbsX=0, mouseAbsY=0;
    ControllerCommand controllerCmd;
    std::string observeTarget;
    int listOffset=0, listLimit=static_cast<int>(kRuntimeTestListObjectsDefaultLimit);
    int stepFrames=0;
    int perfFrames=0;
    WorldOpRequest pendingWorld{};
    EditorOpRequest pendingEditor{};
    bool pendingEditorValid=false;
    if(input) {
        if(q.size()!=6 || !q.contains("keys") || !q["keys"].is_array() || q["keys"].empty() || q["keys"].size()>9
            || !q.contains("updates") || !q["updates"].is_number_integer()
            || q["updates"]<1 || q["updates"]>kInputInjectMaxUpdates)
            return error("invalid_request");
        std::set<std::string> seen;
        for(const auto& item:q["keys"]) {
            if(!item.is_string()) return error("invalid_request");
            auto name=item.get<std::string>();
            if(!LookupBridgeKey(name) || !seen.insert(name).second) return error("invalid_request");
            keys.push_back(std::move(name));
        }
        if(ForbiddenKeyboardCombo(keys)) return error("invalid_request");
        updates=q["updates"].get<int>();
    } else if(mouse) {
        const bool hasWheel = q.contains("wheel");
        const bool hasX = q.contains("x");
        const bool hasY = q.contains("y");
        const bool hasUpdates = q.contains("updates");
        const bool hasDurationMs = q.contains("durationMs");
        // Exactly one of updates (frame hold) or durationMs (wall-clock hold).
        if(hasUpdates == hasDurationMs) return error("invalid_request");
        if(hasX != hasY) return error("invalid_request");
        const size_t expectedSize = 8u + (hasWheel ? 1u : 0u) + (hasX ? 2u : 0u);
        if(q.size() != expectedSize) return error("invalid_request");
        if(!q.contains("dx") || !q["dx"].is_number_integer() || q["dx"] < -600 || q["dx"] > 600)
            return error("invalid_request");
        if(!q.contains("dy") || !q["dy"].is_number_integer() || q["dy"] < -600 || q["dy"] > 600)
            return error("invalid_request");
        if(!q.contains("buttons") || !q["buttons"].is_array() || q["buttons"].size() > 3)
            return error("invalid_request");
        if(hasUpdates) {
            if(!q["updates"].is_number_integer() || q["updates"] < 1 || q["updates"] > kInputInjectMaxUpdates)
                return error("invalid_request");
            mouseUpdates = q["updates"].get<int>();
            mouseDurationMs = 0;
        } else {
            if(!q["durationMs"].is_number_integer() || q["durationMs"] < 1
                || q["durationMs"] > kInputInjectMaxDurationMs)
                return error("invalid_request");
            mouseDurationMs = q["durationMs"].get<int>();
            mouseUpdates = 0;
        }
        if(hasWheel) {
            if(!q["wheel"].is_number_integer() || q["wheel"] < -120 || q["wheel"] > 120)
                return error("invalid_request");
            mouseWheel = q["wheel"].get<int>();
        } else {
            mouseWheel = 0;
        }
        if(hasX) {
            // Finite ints in current backbuffer pixel bounds [0, width) x [0, height).
            if(!q["x"].is_number_integer() || !q["y"].is_number_integer())
                return error("invalid_request");
            const int bbW = static_cast<int>(GetBackBufferWidth());
            const int bbH = static_cast<int>(GetBackBufferHeight());
            mouseAbsX = q["x"].get<int>();
            mouseAbsY = q["y"].get<int>();
            if(bbW <= 0 || bbH <= 0 || mouseAbsX < 0 || mouseAbsY < 0
                || mouseAbsX >= bbW || mouseAbsY >= bbH)
                return error("invalid_request");
            mouseHasAbsolute = true;
        }
        std::set<std::string> seen;
        for(const auto& item : q["buttons"]) {
            if(!item.is_string()) return error("invalid_request");
            auto bname = item.get<std::string>();
            if(!seen.insert(bname).second) return error("invalid_request");
            if(bname == "LEFT") mouseButtonsMask |= MOUSE_BUTTON_LEFT;
            else if(bname == "RIGHT") mouseButtonsMask |= MOUSE_BUTTON_RIGHT;
            else if(bname == "MIDDLE") mouseButtonsMask |= MOUSE_BUTTON_MIDDLE;
            else return error("invalid_request");
        }
        mouseDx = q["dx"].get<int>();
        mouseDy = q["dy"].get<int>();
    } else if(controller) {
        if(ParseControllerCommand(q, controllerCmd) != ControllerParseStatus::Ok) return error("invalid_request");
    } else if(observe) {
        if(q.size()!=5 || !q.contains("target") || !q["target"].is_string())
            return error("invalid_request");
        observeTarget=q["target"].get<std::string>();
        if(observeTarget.empty() || observeTarget.size()>64)
            return error("invalid_request");
        for(char ch:observeTarget) {
            if(!((ch>='a'&&ch<='z') || (ch>='A'&&ch<='Z') || (ch>='0'&&ch<='9') || ch=='_' || ch=='-'))
                return error("invalid_request");
        }
    } else if(capture) {
        if(q.size()!=4) return error("invalid_request");
    } else if(events) {
        if(q.size()!=4) return error("invalid_request");
    } else if(controllerProbe) {
        if(q.size()!=4) return error("invalid_request");
    } else if(listObjects) {
        if(q.size()!=4 && q.size()!=5 && q.size()!=6) return error("invalid_request");
        if(q.contains("offset")) {
            if(!q["offset"].is_number_integer() || q["offset"]<0 || q["offset"]>1000000)
                return error("invalid_request");
            listOffset=q["offset"].get<int>();
        }
        if(q.contains("limit")) {
            if(!q["limit"].is_number_integer() || q["limit"]<1
                || q["limit"]>static_cast<int>(kRuntimeTestListObjectsMaxLimit))
                return error("invalid_request");
            listLimit=q["limit"].get<int>();
        }
        for(auto it=q.begin(); it!=q.end(); ++it) {
            const auto& k=it.key();
            if(k!="protocolVersion" && k!="requestId" && k!="runId" && k!="verb"
                && k!="offset" && k!="limit")
                return error("invalid_request");
        }
    } else if(pause || resume) {
        if(q.size()!=4) return error("invalid_request");
    } else if(step) {
        if(q.size()!=5 || !q.contains("frames") || !q["frames"].is_number_integer()
            || q["frames"]<1 || q["frames"]>600)
            return error("invalid_request");
        stepFrames=q["frames"].get<int>();
    } else if(perf) {
        if(q.size()!=5 || !q.contains("frames") || !q["frames"].is_number_integer()
            || q["frames"]<1 || q["frames"]>static_cast<int>(kRuntimeTestPerfCapacity))
            return error("invalid_request");
        perfFrames=q["frames"].get<int>();
    } else if(resetScene) {
        if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","scene"}))
            return error("invalid_request");
        pendingWorld.kind = WorldOpKind::ResetScene;
        if(q.contains("scene")) {
            if(!q["scene"].is_string()) return error("invalid_request");
            const auto raw = q["scene"].get<std::string>();
            std::string normalized, err;
            if(!ValidateRuntimeTestSceneArg(raw, &normalized, &err))
                return error("invalid_request");
            pendingWorld.scenePath = std::move(normalized);
        }
    } else if(setTransform) {
        if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","object","position","rotation","scale","zeroVelocity"}))
            return error("invalid_request");
        if(!q.contains("object") || !q["object"].is_string()) return error("invalid_request");
        pendingWorld.object = q["object"].get<std::string>();
        if(!ValidObjectName(pendingWorld.object)) return error("invalid_request");
        pendingWorld.kind = WorldOpKind::SetTransform;
        if(q.contains("position")) {
            if(!ParseVec3(q["position"], pendingWorld.posX, pendingWorld.posY, pendingWorld.posZ))
                return error("invalid_request");
            pendingWorld.hasPosition = true;
        }
        if(q.contains("rotation")) {
            if(q["rotation"].is_array() && q["rotation"].size()==3) {
                if(!ParseVec3(q["rotation"], pendingWorld.rotX, pendingWorld.rotY, pendingWorld.rotZ))
                    return error("invalid_request");
                pendingWorld.hasRotationEuler = true;
            } else if(q["rotation"].is_array() && q["rotation"].size()==4) {
                if(!ParseVec4(q["rotation"], pendingWorld.quatX, pendingWorld.quatY, pendingWorld.quatZ, pendingWorld.quatW))
                    return error("invalid_request");
                // Reject near-zero quaternions (undefined orientation).
                const float qlen2 = pendingWorld.quatX*pendingWorld.quatX + pendingWorld.quatY*pendingWorld.quatY
                    + pendingWorld.quatZ*pendingWorld.quatZ + pendingWorld.quatW*pendingWorld.quatW;
                if(!(qlen2 > 1e-12f)) return error("invalid_request");
                pendingWorld.hasRotationQuat = true;
            } else {
                return error("invalid_request");
            }
        }
        if(q.contains("scale")) {
            if(!ParseVec3(q["scale"], pendingWorld.scaleX, pendingWorld.scaleY, pendingWorld.scaleZ))
                return error("invalid_request");
            pendingWorld.hasScale = true;
        }
        if(q.contains("zeroVelocity")) {
            if(!q["zeroVelocity"].is_boolean()) return error("invalid_request");
            pendingWorld.zeroVelocity = q["zeroVelocity"].get<bool>();
        } else {
            pendingWorld.zeroVelocity = true;
        }
        if(!pendingWorld.hasPosition && !pendingWorld.hasRotationEuler && !pendingWorld.hasRotationQuat && !pendingWorld.hasScale)
            return error("invalid_request");
    } else if(setVelocity) {
        if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","object","linear","angular"}))
            return error("invalid_request");
        if(!q.contains("object") || !q["object"].is_string()) return error("invalid_request");
        pendingWorld.object = q["object"].get<std::string>();
        if(!ValidObjectName(pendingWorld.object)) return error("invalid_request");
        pendingWorld.kind = WorldOpKind::SetVelocity;
        if(q.contains("linear")) {
            if(!ParseVec3(q["linear"], pendingWorld.linX, pendingWorld.linY, pendingWorld.linZ))
                return error("invalid_request");
            pendingWorld.hasLinear = true;
        }
        if(q.contains("angular")) {
            if(!ParseVec3(q["angular"], pendingWorld.angX, pendingWorld.angY, pendingWorld.angZ))
                return error("invalid_request");
            pendingWorld.hasAngular = true;
        }
        if(!pendingWorld.hasLinear && !pendingWorld.hasAngular) return error("invalid_request");
    } else if(applyImpulse) {
        if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","object","impulse","point"}))
            return error("invalid_request");
        if(!q.contains("object") || !q["object"].is_string()) return error("invalid_request");
        pendingWorld.object = q["object"].get<std::string>();
        if(!ValidObjectName(pendingWorld.object)) return error("invalid_request");
        if(!q.contains("impulse") || !ParseVec3(q["impulse"], pendingWorld.impX, pendingWorld.impY, pendingWorld.impZ))
            return error("invalid_request");
        pendingWorld.kind = WorldOpKind::ApplyImpulse;
        pendingWorld.hasImpulse = true;
        if(q.contains("point")) {
            if(!ParseVec3(q["point"], pendingWorld.pointX, pendingWorld.pointY, pendingWorld.pointZ))
                return error("invalid_request");
            pendingWorld.hasPoint = true;
        }
    } else if(applyTorqueImpulse) {
        if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","object","torqueImpulse"}))
            return error("invalid_request");
        if(!q.contains("object") || !q["object"].is_string()) return error("invalid_request");
        pendingWorld.object = q["object"].get<std::string>();
        if(!ValidObjectName(pendingWorld.object)) return error("invalid_request");
        if(!q.contains("torqueImpulse")
            || !ParseVec3(q["torqueImpulse"], pendingWorld.torX, pendingWorld.torY, pendingWorld.torZ))
            return error("invalid_request");
        pendingWorld.kind = WorldOpKind::ApplyTorqueImpulse;
        pendingWorld.hasTorqueImpulse = true;
    } else if(playEnter) {
        if(host_ != BridgeHostKind::Editor) {
            // unsupported_host after envelope/id checks
        } else if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","mode"})) {
            return error("invalid_request");
        } else {
            std::string modeOwned;
            const char* modeStr = nullptr;
            if(q.contains("mode")) {
                if(!q["mode"].is_string()) return error("invalid_request");
                modeOwned = q["mode"].get<std::string>();
                modeStr = modeOwned.c_str();
            }
            const auto mode = ParseRuntimeTestBridgePlayMode(modeStr);
            if(mode == EditorPlayModeKind::Invalid) return error("invalid_request");
            pendingEditor.kind = EditorOpKind::PlayEnter;
            pendingEditor.playMode = mode;
            pendingEditorValid = true;
        }
    } else if(playExit) {
        if(host_ != BridgeHostKind::Editor) {
        } else if(q.size()!=4) {
            return error("invalid_request");
        } else {
            pendingEditor.kind = EditorOpKind::PlayExit;
            pendingEditorValid = true;
        }
    } else if(loadScene) {
        if(host_ != BridgeHostKind::Editor) {
        } else if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","scene"})) {
            return error("invalid_request");
        } else if(!q.contains("scene") || !q["scene"].is_string()) {
            return error("invalid_request");
        } else {
            std::string normalized, err;
            if(!ValidateRuntimeTestSceneArg(q["scene"].get<std::string>(), &normalized, &err))
                return error("invalid_request");
            pendingEditor.kind = EditorOpKind::LoadScene;
            pendingEditor.scenePath = std::move(normalized);
            pendingEditorValid = true;
        }
    } else if(setSetting) {
        if(!OnlyKeys(q, {"protocolVersion","requestId","runId","verb","key","value"}))
            return error("invalid_request");
        if(!q.contains("key") || !q["key"].is_string() || !q.contains("value"))
            return error("invalid_request");
        const auto key = q["key"].get<std::string>();
        if(!IsRuntimeTestBridgeSettingKey(key)) return error("invalid_request");
        bool value = false;
        std::string serr;
        if(!ParseRuntimeTestBridgeSettingValue(q["value"], value, serr))
            return error("invalid_request");
        pendingEditor.kind = EditorOpKind::SetSetting;
        pendingEditor.settingKey = key;
        pendingEditor.settingBoolValue = value;
        pendingEditorValid = true;
    } else if(q.size()!=4) return error("invalid_request");

    if(q["protocolVersion"]!=1) return error("unsupported_version",q["requestId"]);
    if(q["runId"]!=runId_) return error("run_mismatch",q["requestId"]);
    const auto id=q["requestId"].get<std::int64_t>();
    if(id<=lastId_) return error("duplicate_request_id",id);
    lastId_=id;
    auto playGateError = [&]() -> const char* {
        if(host_ == BridgeHostKind::Editor) {
            if(!ready_) return "not_ready";
            if(!playing_) return "not_playing";
            return nullptr;
        }
        if(!ready_ || !playing_) return "not_ready";
        return nullptr;
    };
    if(playEnter || playExit || loadScene) {
        if(host_ != BridgeHostKind::Editor) return error("unsupported_host",id);
    }
    if(playEnter) {
        if(playing_) return error("already_playing",id);
        if(editorOp_.status == EditorOpState::Status::Queued
            || editorOp_.status == EditorOpState::Status::Applied)
            return error("queue_full",id);
        editorOp_.status = EditorOpState::Status::Queued;
        editorOp_.request = pendingEditor;
        editorOp_.request.requestId = id;
        editorOp_.error.clear();
        editorOp_.appliedOk = true;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(playExit) {
        if(!playing_) {
            return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
                {"playing",false},{"playMode","none"}};
        }
        if(editorOp_.status == EditorOpState::Status::Queued
            || editorOp_.status == EditorOpState::Status::Applied)
            return error("queue_full",id);
        editorOp_.status = EditorOpState::Status::Queued;
        editorOp_.request = pendingEditor;
        editorOp_.request.requestId = id;
        editorOp_.error.clear();
        editorOp_.appliedOk = true;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(loadScene || setSetting) {
        if(!pendingEditorValid) return error("invalid_request",id);
        if(editorOp_.status == EditorOpState::Status::Queued
            || editorOp_.status == EditorOpState::Status::Applied)
            return error("queue_full",id);
        editorOp_.status = EditorOpState::Status::Queued;
        editorOp_.request = pendingEditor;
        editorOp_.request.requestId = id;
        editorOp_.error.clear();
        editorOp_.appliedOk = true;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(input) {
        if(const char* gate = playGateError()) return error(gate,id);
        if(inputState_==InputState::Queued || KeyboardIsolating()) return error("queue_full",id);
        inputState_=InputState::Queued;
        cancelInput_=false;
        keyboardReqId_=id;
        keyboardKeys_=std::move(keys);
        keyboardUpdatesRemaining_=updates;
        inputDeadline_=GetTickCount64()+InputInjectDeadlineMs(updates);
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(mouse) {
        if(const char* gate = playGateError()) return error(gate,id);
        if(mouseState_==InputState::Queued || MouseIsolating()) return error("queue_full",id);
        mouseState_=InputState::Queued;
        cancelMouse_=false;
        mouseReqId_=id;
        mouseDx_=mouseDx;
        mouseDy_=mouseDy;
        mouseButtons_=mouseButtonsMask;
        mouseWheel_=mouseWheel;
        mouseHasAbsolute_=mouseHasAbsolute;
        mouseAbsX_=mouseAbsX;
        mouseAbsY_=mouseAbsY;
        mouseUpdatesRemaining_=mouseUpdates;
        mouseDurationMs_=mouseDurationMs;
        mouseHoldStartMs_=0;
        mouseDeadline_=GetTickCount64() + (mouseDurationMs_ > 0
            ? InputInjectDeadlineFromDurationMs(mouseDurationMs_)
            : InputInjectDeadlineMs(mouseUpdates));
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(controller) {
        if(const char* gate = playGateError()) return error(gate,id);
        if(!controllerOp_.TryQueue(id, controllerCmd)) return error("queue_full",id);
        controllerDeadline_=GetTickCount64()+5000;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(observe) {
        if(const char* gate = playGateError()) return error(gate,id);
        for(const auto& t : targets_) {
            if(t.name == observeTarget) {
                Json body = ObserveTargetJson(t);
                body["requestId"] = id;
                body["runId"] = runId_;
                body["ok"] = true;
                return body;
            }
        }
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"found",false},
            {"target",observeTarget}
        };
    }
    if(listObjects) {
        if(const char* gate = playGateError()) return error(gate,id);
        const int total = static_cast<int>(objects_.size());
        Json arr = Json::array();
        int emitted = 0;
        bool truncated = false;
        for(int i = listOffset; i < total && emitted < listLimit; ++i) {
            const auto& obj = objects_[static_cast<std::size_t>(i)];
            Json comps = Json::array();
            constexpr std::size_t kMaxComponents = 32;
            for(std::size_t c = 0; c < obj.components.size() && c < kMaxComponents; ++c)
                comps.push_back(obj.components[c]);
            if(obj.components.size() > kMaxComponents) truncated = true;
            arr.push_back({{"name", obj.name}, {"components", std::move(comps)}});
            ++emitted;
        }
        if(listOffset + emitted < total) truncated = true;
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"objects", std::move(arr)},
            {"offset", listOffset},
            {"limit", listLimit},
            {"total", total},
            {"truncated", truncated},
        };
    }
    if(pause) {
        if(const char* gate = playGateError()) return error(gate,id);
        paused_ = true;
        pendingStepFrames_ = 0;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"paused",true},{"frame",frameCounter_},{"physicsTick",physicsTickCounter_}};
    }
    if(resume) {
        if(const char* gate = playGateError()) return error(gate,id);
        paused_ = false;
        pendingStepFrames_ = 0;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"paused",false},{"frame",frameCounter_},{"physicsTick",physicsTickCounter_}};
    }
    if(step) {
        if(const char* gate = playGateError()) return error(gate,id);
        if(!paused_) return error("not_paused",id);
        if(pendingStepFrames_ > 0) return error("queue_full",id);
        pendingStepFrames_ = stepFrames;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"paused",true},{"frames",stepFrames},{"pendingSteps",pendingStepFrames_},
            {"frame",frameCounter_},{"physicsTick",physicsTickCounter_}};
    }
    if(perf) {
        if(const char* gate = playGateError()) return error(gate,id);
        const int available = static_cast<int>(perfCount_);
        const int take = (std::min)(perfFrames, available);
        std::vector<float> frameMs, updateMs, drawMs, presentMs;
        frameMs.reserve(static_cast<std::size_t>(take));
        updateMs.reserve(static_cast<std::size_t>(take));
        drawMs.reserve(static_cast<std::size_t>(take));
        presentMs.reserve(static_cast<std::size_t>(take));
        std::vector<float> cullConsidered, cullCulled, cullDrawn;
        cullConsidered.reserve(static_cast<std::size_t>(take));
        cullCulled.reserve(static_cast<std::size_t>(take));
        cullDrawn.reserve(static_cast<std::size_t>(take));
        std::vector<float> shadowCullConsidered, shadowCullCulled, shadowCullDrawn;
        shadowCullConsidered.reserve(static_cast<std::size_t>(take));
        shadowCullCulled.reserve(static_cast<std::size_t>(take));
        shadowCullDrawn.reserve(static_cast<std::size_t>(take));
        std::vector<float> spriteCullConsidered, spriteCullCulled, spriteCullDrawn;
        spriteCullConsidered.reserve(static_cast<std::size_t>(take));
        spriteCullCulled.reserve(static_cast<std::size_t>(take));
        spriteCullDrawn.reserve(static_cast<std::size_t>(take));
        for(int i = 0; i < take; ++i) {
            const std::size_t idx = (perfWrite_ + kRuntimeTestPerfCapacity - static_cast<std::size_t>(take) + static_cast<std::size_t>(i))
                % kRuntimeTestPerfCapacity;
            const auto& s = perfRing_[idx];
            frameMs.push_back(s.frameMs);
            updateMs.push_back(s.updateMs);
            drawMs.push_back(s.drawMs);
            presentMs.push_back(s.presentMs);
            cullConsidered.push_back(static_cast<float>(s.cullConsidered));
            cullCulled.push_back(static_cast<float>(s.cullCulled));
            cullDrawn.push_back(static_cast<float>(s.cullDrawn));
            shadowCullConsidered.push_back(static_cast<float>(s.shadowCullConsidered));
            shadowCullCulled.push_back(static_cast<float>(s.shadowCullCulled));
            shadowCullDrawn.push_back(static_cast<float>(s.shadowCullDrawn));
            spriteCullConsidered.push_back(static_cast<float>(s.spriteCullConsidered));
            spriteCullCulled.push_back(static_cast<float>(s.spriteCullCulled));
            spriteCullDrawn.push_back(static_cast<float>(s.spriteCullDrawn));
        }
        double fpsAvg = 0.0;
        if(!frameMs.empty()) {
            double invSum = 0.0;
            int finite = 0;
            for(float ms : frameMs) {
                if(ms > 1e-6f) { invSum += 1000.0 / static_cast<double>(ms); ++finite; }
            }
            if(finite > 0) fpsAvg = invSum / static_cast<double>(finite);
        }
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"frames", take},{"available", available},{"requested", perfFrames},
            {"frameMs", PerfStatsJson(frameMs)},
            {"updateMs", PerfStatsJson(updateMs)},
            {"drawMs", PerfStatsJson(drawMs)},
            {"presentMs", PerfStatsJson(presentMs)},
            {"fps", {{"avg", fpsAvg}}},
            {"frustumCull", {
                {"considered", PerfStatsJson(cullConsidered)},
                {"culled", PerfStatsJson(cullCulled)},
                {"drawn", PerfStatsJson(cullDrawn)}
            }},
            {"shadowCull", {
                {"considered", PerfStatsJson(shadowCullConsidered)},
                {"culled", PerfStatsJson(shadowCullCulled)},
                {"drawn", PerfStatsJson(shadowCullDrawn)}
            }},
            {"spriteCull", {
                {"considered", PerfStatsJson(spriteCullConsidered)},
                {"culled", PerfStatsJson(spriteCullCulled)},
                {"drawn", PerfStatsJson(spriteCullDrawn)}
            }},
        };
    }
    if(worldOp) {
        if(const char* gate = playGateError()) return error(gate,id);
        if(worldOp_.status == WorldOpState::Status::Queued
            || worldOp_.status == WorldOpState::Status::Applied)
            return error("queue_full",id);
        pendingWorld.requestId = id;
        worldOp_.status = WorldOpState::Status::Queued;
        worldOp_.request = std::move(pendingWorld);
        worldOp_.error.clear();
        worldOp_.appliedOk = true;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"status","queued"},{"verb",worldOp_.VerbName()}};
    }
    if(capture) {
        if(const char* gate = playGateError()) return error(gate,id);
        if(captureState_.status == CaptureState::Status::Queued) return error("queue_full",id);
        captureState_.status = CaptureState::Status::Queued;
        captureState_.requestId = id;
        captureState_.width = 0;
        captureState_.height = 0;
        captureState_.basename.clear();
        captureState_.path.clear();
        captureState_.error.clear();
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(events) {
        if(const char* gate = playGateError()) return error(gate,id);
        Json arr = Json::array();
        for (const auto& ev : events_) {
            arr.push_back({
                {"sequence", ev.sequence},
                {"type", ev.type},
                {"checkpoint", ev.checkpoint},
                {"actor", ev.actor},
                {"score", ev.score},
            });
        }
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"events", std::move(arr)},
            {"latestSequence", eventsLatestSequence_},
            {"dropped", eventsDropped_},
        };
    }
    if(controllerProbe) {
        if(const char* gate = playGateError()) return error(gate,id);
        const auto snap = RuntimeTestControllerProbe_CopySnapshot();
        Json arr = Json::array();
        for (const auto& frame : snap.frames)
            arr.push_back(ControllerProbeFrameToJson(frame));
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"frames", std::move(arr)},
            {"latestSequence", snap.latestSequence},
            {"dropped", snap.dropped},
            {"overflow", snap.overflow},
        };
    }
    if(q["verb"]=="quit") {
        if(quitPending_ || quitReady_) return error("queue_full",id);
        quitPending_=true;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"requiresReceipt",true}};
    }
    if(q["verb"]!="hello" && q["verb"]!="status") return error("unknown_verb",id);

    const char* capStatus = captureState_.status == CaptureState::Status::Queued ? "queued" :
                            captureState_.status == CaptureState::Status::Completed ? "completed" :
                            captureState_.status == CaptureState::Status::Failed ? "failed" : "idle";
    // Applied still reports as queued: clients must wait until Publish seals it.
    const char* worldStatus = (worldOp_.status == WorldOpState::Status::Queued
                               || worldOp_.status == WorldOpState::Status::Applied) ? "queued" :
                              worldOp_.status == WorldOpState::Status::Completed ? "completed" :
                              worldOp_.status == WorldOpState::Status::Failed ? "failed" : "idle";
    const char* editorStatus = (editorOp_.status == EditorOpState::Status::Queued
                               || editorOp_.status == EditorOpState::Status::Applied) ? "queued" :
                              editorOp_.status == EditorOpState::Status::Completed ? "completed" :
                              editorOp_.status == EditorOpState::Status::Failed ? "failed" : "idle";

    Json caps = Json::array({
            "hello","status","quit","keyboard","observe","capture","mouse","controller","events",
            "controller_probe","list_objects","pause","resume","step","perf",
            "reset_scene","set_transform","set_velocity","apply_impulse","apply_torque_impulse",
            "set_setting"});
    if(host_ == BridgeHostKind::Editor) {
        caps.push_back("play_enter");
        caps.push_back("play_exit");
        caps.push_back("load_scene");
    }

    Json reply = {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
        {"pid",pid_},{"sessionId",session_},{"ready",ready_},{"playing",playing_},
        {"host", host_ == BridgeHostKind::Editor ? "editor" : "runtime"},
        {"state",ready_ ? (playing_ ? (paused_ ? "paused" : "playing") : "ready") : "loading"},
        {"scene",scene_},
        {"paused",paused_},
        {"pendingSteps",pendingStepFrames_},
        {"frame",frameCounter_},
        {"physicsTick",physicsTickCounter_},
        {"capabilities", std::move(caps)},
        {"keyboard",{{"status",InputStatus()},{"requestId",keyboardReqId_},
            {"remainingUpdates",keyboardUpdatesRemaining_},{"cancelRequested",cancelInput_},{"preparing",inputState_==InputState::Preparing}}},
        {"mouse",{{"status",MouseStatus()},{"requestId",mouseReqId_},
            {"remainingUpdates",mouseUpdatesRemaining_},{"cancelRequested",cancelMouse_}}},
        {"controller",{{"status",ControllerStatus()},{"requestId",controllerOp_.RequestId()},
            {"remainingUpdates",controllerOp_.Remaining()},{"cancelRequested",controllerOp_.CancelRequested()}}},
        {"capture",{{"status",capStatus},{"requestId",captureState_.requestId},
            {"width",captureState_.width},{"height",captureState_.height},
            {"basename",captureState_.basename},{"path",captureState_.path},{"error",captureState_.error}}},
        {"worldOp",{{"status",worldStatus},{"requestId",worldOp_.request.requestId},
            {"verb",worldOp_.status==WorldOpState::Status::Idle ? Json(nullptr) : Json(worldOp_.VerbName())},
            {"error",worldOp_.error}}},
        {"editorOp",{{"status",editorStatus},{"requestId",editorOp_.request.requestId},
            {"verb",editorOp_.status==EditorOpState::Status::Idle ? Json(nullptr) : Json(editorOp_.VerbName())},
            {"error",editorOp_.error}}}};
    if(host_ == BridgeHostKind::Editor)
        reply["playMode"] = EditorPlayModeKindName(
            playing_ ? playMode_ : EditorPlayModeKind::None);
    if(!readyDetail_.empty())
        reply["readyDetail"] = readyDetail_;
    return reply;
}
}

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#include <mutex>
#include <thread>
#include <vector>
namespace ivy::testing {
struct Bridge::Impl {
    HANDLE stop = nullptr, pipe = INVALID_HANDLE_VALUE;
    std::string name;
    Protocol protocol;
    CaptureHandler captureHandler = nullptr;
    WorldOpHandler worldOpHandler = nullptr;
    EditorOpHandler editorOpHandler = nullptr;
    std::mutex mutex;
    std::thread worker;
    Impl(std::string run, DWORD pid, DWORD session, BridgeHostKind host)
        : protocol(run, pid, session, host) {
        name="\\\\.\\pipe\\IvyRuntimeTest-"+std::to_string(pid)+"-"+run;
    }
    ~Impl() {
        if(stop) SetEvent(stop);
        if(worker.joinable()) worker.join();
        if(pipe!=INVALID_HANDLE_VALUE) CloseHandle(pipe);
        if(stop) CloseHandle(stop);
    }
    bool Finish(BOOL immediate, OVERLAPPED& ov, DWORD& bytes, DWORD timeout) {
        if(immediate) return true;
        if(GetLastError()!=ERROR_IO_PENDING) return false;
        HANDLE waits[]{stop,ov.hEvent};
        const auto result=WaitForMultipleObjects(2,waits,FALSE,timeout);
        if(result!=WAIT_OBJECT_0+1) {
            CancelIoEx(pipe,&ov);
            GetOverlappedResult(pipe,&ov,&bytes,TRUE);
            return false;
        }
        return GetOverlappedResult(pipe,&ov,&bytes,FALSE)!=FALSE;
    }
    bool Byte(char& c, DWORD timeout) {
        OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!ov.hEvent) return false;
        DWORD n=0;
        bool ok=Finish(ReadFile(pipe,&c,1,&n,&ov),ov,n,timeout) && n==1;
        CloseHandle(ov.hEvent); return ok;
    }
    bool Send(const std::string& text) {
        OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!ov.hEvent) return false;
        DWORD n=0;
        bool ok=Finish(WriteFile(pipe,text.data(),static_cast<DWORD>(text.size()),&n,&ov),ov,n,2000)
            && n==text.size();
        CloseHandle(ov.hEvent); return ok;
    }
    void Run() {
        while(WaitForSingleObject(stop,0)==WAIT_TIMEOUT) {
            OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!ov.hEvent) break;
            DWORD n=0;
            const BOOL connected=ConnectNamedPipe(pipe,&ov);
            const bool already=!connected && GetLastError()==ERROR_PIPE_CONNECTED;
            const bool ok=already || Finish(connected,ov,n,INFINITE);
            CloseHandle(ov.hEvent);
            if(!ok) break;
            bool receipt=false;
            for(;;) {
                std::string frame;
                char c=0;
                // Idle between requests: wait for first byte (or stop / client close).
                // Applying the mid-frame 2s body deadline here disconnected live Clients
                // after quiet gaps and produced ERROR_PIPE_NOT_CONNECTED (233) on the next op.
                bool complete=false;
                {
                    const auto idleTimeout = RequestFrameByteTimeout(false, 0, GetTickCount64());
                    if (!Byte(c, idleTimeout)) break;
                    if (c == '\n') {
                        complete = true;
                    } else {
                        frame += c;
                        if (frame.size() > 4096) {
                            complete = true;
                        } else {
                            const ULONGLONG deadline = GetTickCount64() + kRuntimeTestFrameBodyTimeoutMs;
                            for (;;) {
                                const auto timeout = RequestFrameByteTimeout(true, deadline, GetTickCount64());
                                if (timeout == 0 || !Byte(c, timeout)) break;
                                if (c == '\n') { complete = true; break; }
                                frame += c;
                                if (frame.size() > 4096) { complete = true; break; }
                            }
                        }
                    }
                }
                if(!complete) break;
                Json response;
                { std::lock_guard lock(mutex); response=protocol.Request(frame); }
                if(!Send(response.dump(-1, ' ', false, Json::error_handler_t::replace)+"\n")) break;
                if(response.value("requiresReceipt",false)) {
                    if(Byte(c,2000) && c=='\n') {
                        std::lock_guard lock(mutex); protocol.ConfirmQuitReceipt(); receipt=true;
                    }
                    break;
                }
                if(frame.size()>4096) break;
            }
            { std::lock_guard lock(mutex); protocol.AbandonQuit(); protocol.CancelKeyboard(); protocol.CancelMouse(); protocol.CancelController(); protocol.CancelWorldOp(); protocol.CancelEditorOp(); }
            if(receipt) {
                WaitForSingleObject(stop,INFINITE); break;
            }
            DisconnectNamedPipe(pipe);
        }
    }
};
Bridge::Bridge() = default;
Bridge::~Bridge() { Stop(); }
bool Bridge::StartFromEnvironment() {
    if(impl_) return true;
    char enabled[4]{}, run[65]{};
    if(GetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE",enabled,4)!=1 || enabled[0]!='1') return false;
    const DWORD length=GetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID",run,65);
    if(length<1 || length>64) return false;
    for(DWORD i=0;i<length;++i) if(!((run[i]>='a'&&run[i]<='z') || (run[i]>='A'&&run[i]<='Z') || (run[i]>='0'&&run[i]<='9') || run[i]=='-')) return false;
    DWORD session=0;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)) return false;
#if defined(GAME_RUNTIME)
    const BridgeHostKind host = BridgeHostKind::Runtime;
#else
    const BridgeHostKind host = BridgeHostKind::Editor;
#endif
    auto p=std::make_unique<Impl>(run,GetCurrentProcessId(),session,host);
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return false;
    DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<unsigned char> buffer(size);
    const bool tokenOk=GetTokenInformation(token,TokenUser,buffer.data(),size,&size)!=FALSE;
    CloseHandle(token);
    if(!tokenOk) return false;
    LPSTR sid=nullptr;
    if(!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid)) return false;
    const std::string dacl="D:P(A;;GA;;;"+std::string(sid)+")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorA(dacl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)) return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
    p->stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(p->stop) p->pipe=CreateNamedPipeA(p->name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,8192,8192,0,&security);
    LocalFree(descriptor);
    if(!p->stop || p->pipe==INVALID_HANDLE_VALUE) return false;
    try { p->worker=std::thread([ptr=p.get()] {
        try { ptr->Run(); }
        catch (const std::exception&) {
            hal::DebugWriteA("RuntimeTestBridge: worker failed; disconnecting endpoint.\n");
            { std::lock_guard lock(ptr->mutex); ptr->protocol.CancelKeyboard(); ptr->protocol.CancelMouse(); ptr->protocol.CancelController(); }
            DisconnectNamedPipe(ptr->pipe);
            SetEvent(ptr->stop);
        }
    }); }
    catch(...) { return false; }
    RuntimeTestEvents_Reset();
    RuntimeTestEvents_SetEnabled(true);
    RuntimeTestControllerProbe_Reset();
    RuntimeTestControllerProbe_SetEnabled(true);
    impl_=std::move(p); return true;
}
void Bridge::Stop() {
    if (impl_) {
        if (Keyboard_IsInjectionActive()) Keyboard_ClearInjection();
        if (Mouse_IsInjectionActive()) Mouse_ClearInjection();
        if (Controller_IsInjectionActive()) Controller_ClearInjection();
    }
    impl_.reset();
    RuntimeTestEvents_SetEnabled(false);
    RuntimeTestControllerProbe_SetEnabled(false);
}
std::string Bridge::PipeName() const { return impl_ ? impl_->name : std::string{}; }

void Bridge::SetCaptureHandler(CaptureHandler handler) {
    if (impl_) impl_->captureHandler = handler;
}
void Bridge::SetWorldOpHandler(WorldOpHandler handler) {
    if (impl_) impl_->worldOpHandler = handler;
}
void Bridge::SetEditorOpHandler(EditorOpHandler handler) {
    if (impl_) impl_->editorOpHandler = handler;
}

void Bridge::PollAndApplyInput(bool inGamePlaying) {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.BeginInput(inGamePlaying, GetTickCount64());
    if(impl_->protocol.KeyboardIsolating()) {
        std::vector<Keyboard_Keys> keys;
        for(const auto& name : impl_->protocol.InputKeys()) {
            Keyboard_Keys code = KK_NONE;
            LookupBridgeKey(name, &code);
            keys.push_back(code);
        }
        Keyboard_TestInject(keys.data(), static_cast<int>(keys.size()));
    } else {
        Keyboard_ClearInjection();
    }
    if(impl_->protocol.MouseIsolating()) {
        int mdx = 0, mdy = 0, mwheel = 0, max = 0, may = 0;
        unsigned int mbuttons = 0;
        bool hasAbs = false;
        impl_->protocol.GetMouseInput(mdx, mdy, mbuttons, mwheel, hasAbs, max, may);
        Mouse_TestInject(mdx, mdy, mbuttons, mwheel, hasAbs, max, may);
    } else {
        Mouse_ClearInjection();
    }
    if(impl_->protocol.ControllerIsolating()) {
        float clx = 0.0f, cly = 0.0f, crx = 0.0f, cry = 0.0f, clt = 0.0f, crt = 0.0f;
        std::uint16_t cbuttons = 0;
        impl_->protocol.GetControllerInput(clx, cly, crx, cry, clt, crt, cbuttons);
        Controller_TestInjectSample(clx, cly, crx, cry, clt, crt, cbuttons);
    } else {
        Controller_ClearInjection();
    }
}
void Bridge::FinishInputUpdate() {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.ExpireKeyboard(GetTickCount64());
    impl_->protocol.ExpireMouse(GetTickCount64());
    impl_->protocol.ExpireController(GetTickCount64());
    impl_->protocol.EndInput();
}
bool Bridge::PublishAndTakeQuit(bool ready, bool playing, const std::string& scene,
    std::vector<ObservedTarget> targets, std::vector<ListedObject> objects,
    const char* readyDetail, EditorPlayModeKind playMode) {
    if(!impl_) return false;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.Publish(ready,playing,scene,std::move(targets),std::move(objects),readyDetail,playMode);
    return impl_->protocol.TakeQuit();
}
void Bridge::ExpireInputDeadlines() {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.ExpireKeyboard(GetTickCount64());
    impl_->protocol.ExpireMouse(GetTickCount64());
    impl_->protocol.ExpireController(GetTickCount64());
}
bool Bridge::IsPaused() {
    if(!impl_) return false;
    std::lock_guard lock(impl_->mutex);
    return impl_->protocol.IsPaused();
}
bool Bridge::ShouldRunSimTick() {
    if(!impl_) return true;
    std::lock_guard lock(impl_->mutex);
    return impl_->protocol.ShouldRunSimTick();
}
void Bridge::NoteSimFrame() {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.NoteSimFrame();
}
void Bridge::NotePhysicsTick() {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.NotePhysicsTick();
}
void Bridge::RecordPerfSample(const PerfSample& sample) {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.RecordPerfSample(sample);
}

void Bridge::MaybeExecuteCapture() {
    if(!impl_) return;
    std::int64_t reqId = 0;
    {
        std::lock_guard lock(impl_->mutex);
        if(!impl_->protocol.HasPendingCapture(reqId)) return;
    }
    if(!impl_->captureHandler) {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.CompleteCapture(reqId, false, 0, 0, "", "", "capture_unavailable");
        return;
    }
    char outDir[MAX_PATH]{};
    const DWORD len = GetEnvironmentVariableA("IVY_RUNTIME_TEST_OUTPUT_DIR", outDir, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.CompleteCapture(reqId, false, 0, 0, "", "", "output_dir_not_configured");
        return;
    }
    const std::string runId = impl_->protocol.RunId();
    const std::string basename = "capture-" + runId + "-" + std::to_string(reqId) + ".png";
    const std::string fullPath = std::string(outDir) + "\\" + basename;
    std::wstring wpath(fullPath.begin(), fullPath.end());
    uint32_t w = 0, h = 0;
    const bool ok = impl_->captureHandler(wpath.c_str(), &w, &h);
    {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.CompleteCapture(reqId, ok, w, h, ok ? basename : "", ok ? fullPath : "",
            ok ? "" : "capture_failed");
    }
}

void Bridge::MaybeApplyWorldOps() {
    if(!impl_) return;
    WorldOpRequest op;
    {
        std::lock_guard lock(impl_->mutex);
        if(!impl_->protocol.HasPendingWorldOp(op)) return;
    }
    if(!impl_->worldOpHandler) {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.MarkWorldOpApplied(op.requestId, false, "world_op_unavailable");
        return;
    }
    std::string err;
    const bool ok = impl_->worldOpHandler(op, err);
    {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.MarkWorldOpApplied(op.requestId, ok, ok ? std::string{} : err);
    }
}

void Bridge::MaybeApplyEditorOps() {
    if(!impl_) return;
    EditorOpRequest op;
    {
        std::lock_guard lock(impl_->mutex);
        if(!impl_->protocol.HasPendingEditorOp(op)) return;
    }
    if(!impl_->editorOpHandler) {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.MarkEditorOpApplied(op.requestId, false, "editor_op_unavailable");
        return;
    }
    std::string err;
    const bool ok = impl_->editorOpHandler(op, err);
    {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.MarkEditorOpApplied(op.requestId, ok, ok ? std::string{} : err);
    }
}
}

#endif
