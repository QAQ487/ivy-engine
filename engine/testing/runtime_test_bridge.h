// runtime_test_bridge.h
// Declares ControllerProbeApiTag.

#pragma once

namespace ivy::testing {
// Always-visible tag for Debug-runtime API detection (ADL). Declared even when
// the bridge is compiled out so Release/Editor checks can prove absence.
struct ControllerProbeApiTag {};
}

#if defined(_DEBUG) && defined(GAME_RUNTIME)
#define IVY_RUNTIME_TEST_BRIDGE_AVAILABLE 1
#include "libraries/nlohmann/json.hpp"
#include "engine/testing/controller_inject_protocol.h"
#include "engine/testing/controller_probe_trace.h"
#include "engine/testing/runtime_test_scene_arg.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ivy::testing {
using Json = nlohmann::json;
using CaptureHandler = bool(*)(const wchar_t*, uint32_t*, uint32_t*);

std::uint32_t RemainingFrameTimeout(std::uint64_t deadline, std::uint64_t now) noexcept;

constexpr std::size_t kRuntimeTestPerfCapacity = 600;
constexpr std::size_t kRuntimeTestListObjectsDefaultLimit = 32;
constexpr std::size_t kRuntimeTestListObjectsMaxLimit = 64;

struct ObservedTarget {
    std::string name;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    bool hasRotation = false;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
    float quatX = 0.0f, quatY = 0.0f, quatZ = 0.0f, quatW = 1.0f;
    bool hasScale = false;
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
    bool hasAngularVelocity = false;
    float angVelX = 0.0f, angVelY = 0.0f, angVelZ = 0.0f;
    bool hasRigidbody = false;
    bool kinematic = false;
    // Read-only rigidbody params for angular acceptance (mass / damping / inertia).
    float mass = 1.0f;
    float drag = 0.0f;
    float angularDrag = 0.0f;
    float friction = 0.0f;
    bool useGravity = false;
    bool hasLocalInverseInertia = false;
    float invInertiaX = 0.0f, invInertiaY = 0.0f, invInertiaZ = 0.0f;
};

struct ListedObject {
    std::string name;
    std::vector<std::string> components;
};

struct PerfSample {
    float frameMs = 0.0f;
    float updateMs = 0.0f;
    float drawMs = 0.0f;
    float presentMs = 0.0f;
};

// Consumer probe: samples public Controller_* getters once per Update after
// Controller_Update. Enabled with the bridge; does not read inject internals.
void RuntimeTestControllerProbe_SetEnabled(bool enabled);
void RuntimeTestControllerProbe_Reset();
bool RuntimeTestControllerProbe_IsEnabled();
void RuntimeTestControllerProbe_SampleFromGetters();
ControllerProbeSnapshot RuntimeTestControllerProbe_CopySnapshot();
// ADL detection hook: present only when the probe API is compiled in.
void ControllerProbeApiDetect(ControllerProbeApiTag) noexcept;

// Debug-only main-thread event recorder (opt-in with bridge Start).
constexpr std::size_t kRuntimeTestEventCapacity = 64;
struct RuntimeTestEvent {
    std::int64_t sequence = 0;
    std::string type;
    std::string checkpoint;
    std::string actor;
    int score = 0;
};
struct RuntimeTestEventsSnapshot {
    std::vector<RuntimeTestEvent> events;
    std::int64_t latestSequence = 0;
    std::uint32_t dropped = 0;
};
void RuntimeTestEvents_SetEnabled(bool enabled);
void RuntimeTestEvents_Reset();
bool RuntimeTestEvents_IsEnabled();
// Validated fixed semantic fields; returns false if disabled/invalid/rejected.
bool RuntimeTestEvents_RecordCheckpoint(const char* checkpoint, const char* actor, int score);
RuntimeTestEventsSnapshot RuntimeTestEvents_CopySnapshot();

// Optional --scene override (set from argv when bridge is enabled).
void RuntimeTestSceneOverride_Set(std::string romPath);
bool RuntimeTestSceneOverride_Has();
const std::string& RuntimeTestSceneOverride_Get();
void RuntimeTestSceneOverride_Clear();

// Queued world-manipulation op (validated on the pipe worker; applied on WinMain).
enum class WorldOpKind {
    ResetScene,
    SetTransform,
    SetVelocity,
    ApplyImpulse,
    ApplyTorqueImpulse,
};

struct WorldOpRequest {
    WorldOpKind kind = WorldOpKind::ResetScene;
    std::int64_t requestId = 0;
    std::string object;     // GameObject name (same as list_objects/observe)
    std::string scenePath;  // reset_scene optional rom/...json
    bool hasPosition = false;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    bool hasRotationEuler = false;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
    bool hasRotationQuat = false;
    float quatX = 0.0f, quatY = 0.0f, quatZ = 0.0f, quatW = 1.0f;
    bool hasScale = false;
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
    bool zeroVelocity = true;
    bool hasLinear = false;
    float linX = 0.0f, linY = 0.0f, linZ = 0.0f;
    bool hasAngular = false;
    float angX = 0.0f, angY = 0.0f, angZ = 0.0f;
    bool hasImpulse = false;
    float impX = 0.0f, impY = 0.0f, impZ = 0.0f;
    bool hasPoint = false;
    float pointX = 0.0f, pointY = 0.0f, pointZ = 0.0f;
    bool hasTorqueImpulse = false;
    float torX = 0.0f, torY = 0.0f, torZ = 0.0f;
};

using WorldOpHandler = bool(*)(const WorldOpRequest& op, std::string& errorOut);

class Protocol {
public:
    Protocol(std::string runId, unsigned long pid, unsigned long session);
    void Publish(bool ready, bool playing, const std::string& scene,
        std::vector<ObservedTarget> targets = {},
        std::vector<ListedObject> objects = {},
        const char* readyDetail = nullptr);
    bool TakeQuit();
    void ConfirmQuitReceipt();
    void AbandonQuit();
    Json Request(const std::string& frame);

    // Caller serializes protocol access; only Bridge applies live input.
    void BeginInput(bool playing, std::uint64_t now);
    void EndInput();
    void CancelKeyboard();
    void ExpireKeyboard(std::uint64_t now);
    bool KeyboardIsolating() const;
    std::vector<std::string> InputKeys() const;

    void CancelMouse();
    void ExpireMouse(std::uint64_t now);
    bool MouseIsolating() const;
    void GetMouseInput(int& dx, int& dy, unsigned int& buttonsMask, int& wheel) const;

    void CancelController();
    void ExpireController(std::uint64_t now);
    bool ControllerIsolating() const;
    void GetControllerInput(float& lx, float& ly, float& rx, float& ry, float& lt, float& rt, std::uint16_t& buttons) const;

    bool HasPendingCapture(std::int64_t& outId) const;
    void CompleteCapture(std::int64_t id, bool ok, uint32_t w, uint32_t h, const std::string& basename, const std::string& err);
    bool HasPendingWorldOp(WorldOpRequest& out) const;
    // In-process tests: Queued -> Completed/Failed immediately.
    void CompleteWorldOp(std::int64_t id, bool ok, const std::string& err);
    // Live bridge: Queued -> Applied after WinMain apply; Publish seals to Completed/Failed
    // so clients waiting on worldOp.completed see a fresh observe/list snapshot.
    void MarkWorldOpApplied(std::int64_t id, bool ok, const std::string& err);
    void CancelWorldOp();
    const std::string& RunId() const { return runId_; }

    bool IsPaused() const { return paused_; }
    // When paused, returns true and consumes one pending step; when not paused, true.
    bool ShouldRunSimTick();
    void NoteSimFrame();
    void NotePhysicsTick();
    void RecordPerfSample(const PerfSample& sample);
private:
    bool quitPending_ = false, quitReady_ = false;
    bool ready_ = false, playing_ = false;
    std::string scene_;
    std::string readyDetail_;
    std::vector<ObservedTarget> targets_;
    std::vector<ListedObject> objects_;
    std::vector<RuntimeTestEvent> events_;
    std::int64_t eventsLatestSequence_ = 0;
    std::uint32_t eventsDropped_ = 0;
    std::int64_t lastId_ = 0;
    std::string runId_;
    unsigned long pid_, session_;
    enum class InputState { Idle, Queued, Preparing, Active, Releasing, Completed, Cancelled };
    InputState inputState_ = InputState::Idle;
    bool cancelInput_ = false;
    std::uint64_t inputDeadline_ = 0;
    std::int64_t keyboardReqId_ = 0;
    std::vector<std::string> keyboardKeys_;
    int keyboardUpdatesRemaining_ = 0;
    const char* InputStatus() const;

    InputState mouseState_ = InputState::Idle;
    bool cancelMouse_ = false;
    std::uint64_t mouseDeadline_ = 0;
    std::int64_t mouseReqId_ = 0;
    int mouseDx_ = 0;
    int mouseDy_ = 0;
    unsigned int mouseButtons_ = 0;
    int mouseWheel_ = 0;
    int mouseUpdatesRemaining_ = 0;
    const char* MouseStatus() const;

    ControllerOperation controllerOp_;
    std::uint64_t controllerDeadline_ = 0;
    const char* ControllerStatus() const;

    struct CaptureState {
        enum class Status { Idle, Queued, Completed, Failed };
        Status status = Status::Idle;
        std::int64_t requestId = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string basename;
        std::string error;
    } captureState_;

    struct WorldOpState {
        enum class Status { Idle, Queued, Applied, Completed, Failed };
        Status status = Status::Idle;
        WorldOpRequest request;
        std::string error;
        bool appliedOk = true;
        const char* VerbName() const;
    } worldOp_;
    void SealAppliedWorldOp();

    bool paused_ = false;
    int pendingStepFrames_ = 0;
    std::uint64_t frameCounter_ = 0;
    std::uint64_t physicsTickCounter_ = 0;
    PerfSample perfRing_[kRuntimeTestPerfCapacity]{};
    std::size_t perfWrite_ = 0;
    std::size_t perfCount_ = 0;
};
// Main-thread owner; destruction cancels I/O and joins. No scene/D3D references.
class Bridge {
public:
    Bridge();
    ~Bridge();
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;
    bool StartFromEnvironment();
    void Stop();
    void SetCaptureHandler(CaptureHandler handler);
    void SetWorldOpHandler(WorldOpHandler handler);
    bool PublishAndTakeQuit(bool ready, bool playing, const std::string& scene,
        std::vector<ObservedTarget> targets = {},
        std::vector<ListedObject> objects = {},
        const char* readyDetail = nullptr);
    void PollAndApplyInput(bool inGamePlaying);
    void FinishInputUpdate();
    void ExpireInputDeadlines();
    void MaybeExecuteCapture();
    void MaybeApplyWorldOps();
    bool IsPaused();
    bool ShouldRunSimTick();
    void NoteSimFrame();
    void NotePhysicsTick();
    void RecordPerfSample(const PerfSample& sample);
    std::string PipeName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
#endif
