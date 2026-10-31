#pragma once
#if defined(_DEBUG) && defined(GAME_RUNTIME)
#define IVY_RUNTIME_TEST_BRIDGE_AVAILABLE 1
#include "libraries/nlohmann/json.hpp"
#include "engine/testing/controller_inject_protocol.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ivy::testing {
using Json = nlohmann::json;
using CaptureHandler = bool(*)(const wchar_t*, uint32_t*, uint32_t*);

std::uint32_t RemainingFrameTimeout(std::uint64_t deadline, std::uint64_t now) noexcept;
struct ObservedTarget {
    std::string name;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
};

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

class Protocol {
public:
    Protocol(std::string runId, unsigned long pid, unsigned long session);
    void Publish(bool ready, bool playing, const std::string& scene, std::vector<ObservedTarget> targets = {});
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
    const std::string& RunId() const { return runId_; }
private:
    bool quitPending_ = false, quitReady_ = false;
    bool ready_ = false, playing_ = false;
    std::string scene_;
    std::vector<ObservedTarget> targets_;
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
    bool PublishAndTakeQuit(bool ready, bool playing, const std::string& scene, std::vector<ObservedTarget> targets = {});
    void PollAndApplyInput(bool inGamePlaying);
    void FinishInputUpdate();
    void MaybeExecuteCapture();
    std::string PipeName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
#endif
