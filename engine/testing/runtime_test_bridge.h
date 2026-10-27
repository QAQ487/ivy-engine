#pragma once
#if defined(_DEBUG) && defined(GAME_RUNTIME)
#define IVY_RUNTIME_TEST_BRIDGE_AVAILABLE 1
#include "libraries/nlohmann/json.hpp"
#include <cstdint>
#include <memory>
#include <string>
namespace ivy::testing {
using Json = nlohmann::json;
std::uint32_t RemainingFrameTimeout(std::uint64_t deadline, std::uint64_t now) noexcept;
class Protocol {
public:
    Protocol(std::string runId, unsigned long pid, unsigned long session);
    void Publish(bool ready, bool playing, const std::string& scene);
    bool TakeQuit();
    void ConfirmQuitReceipt();
    void AbandonQuit();
    Json Request(const std::string& frame);
private:
    bool quitPending_ = false, quitReady_ = false;
    bool ready_ = false, playing_ = false;
    std::string scene_;
    std::int64_t lastId_ = 0;
    std::string runId_;
    unsigned long pid_, session_;
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
    bool PublishAndTakeQuit(bool ready, bool playing, const std::string& scene);
    std::string PipeName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
#endif
