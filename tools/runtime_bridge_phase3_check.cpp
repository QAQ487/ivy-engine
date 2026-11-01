// runtime_bridge_phase3_check.cpp
// Validates runtime bridge phase-3 observe/capture features.

#include "engine/testing/runtime_test_bridge.h"
#include <iostream>
#include <windows.h>
using namespace ivy::testing;
static void check(bool b, const char* label) {
    if (!b) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}
struct Peer {
    HANDLE h = INVALID_HANDLE_VALUE;
    int id = 0;
    explicit Peer(const Bridge& b) {
        const auto deadline = GetTickCount64() + 2000;
        do {
            h = CreateFileA(b.PipeName().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) break;
            Sleep(1);
        } while (GetTickCount64() < deadline);
        check(h != INVALID_HANDLE_VALUE, "connect phase3 peer");
    }
    ~Peer() { close(); }
    void close() { if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    Json request(const char* verb, Json payload = Json::object()) {
        payload["protocolVersion"] = 1; payload["requestId"] = ++id; payload["runId"] = "phase3-check"; payload["verb"] = verb;
        const auto frame = payload.dump() + "\n";
        DWORD n = 0;
        check(WriteFile(h, frame.data(), static_cast<DWORD>(frame.size()), &n, nullptr) != FALSE, "send phase3 request");
        std::string result; char c = 0;
        while (ReadFile(h, &c, 1, &n, nullptr) && n == 1 && c != '\n') result += c;
        check(c == '\n', "read phase3 reply");
        return Json::parse(result);
    }
};

int main() {
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE", "1");
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID", "phase3-check");
    Bridge b; check(b.StartFromEnvironment(), "phase3 bridge starts");

    std::vector<ObservedTarget> targets = {
        {"Player", 1.5f, 2.5f, 3.5f, 0.0f, 0.0f, 6.0f},
        {"Landmark_North", 0.0f, 1.0f, 5.0f, 0.0f, 0.0f, 0.0f}
    };
    b.PublishAndTakeQuit(true, true, "testbed/rom/scene/scene_runtime_bridge_player.json", targets);

    Peer p(b);
    auto status = p.request("status");
    check(status["capabilities"].is_array(), "status has capabilities");
    bool hasObserve = false;
    bool hasCapture = false;
    for (const auto& cap : status["capabilities"]) {
        if (cap == "observe") hasObserve = true;
        if (cap == "capture") hasCapture = true;
    }
    check(hasObserve, "observe capability present in status");
    check(hasCapture, "capture capability present in status");
    check(status["capture"]["status"] == "idle", "initial capture status is idle");

    auto rPlayer = p.request("observe", {{"target", "Player"}});
    check(rPlayer["ok"] == true, "observe Player succeeds");
    check(rPlayer["found"] == true, "Player found");
    check(rPlayer["target"] == "Player", "target matches");
    check(rPlayer["position"]["x"] == 1.5f && rPlayer["position"]["z"] == 3.5f, "position snapshot matches");
    check(rPlayer["velocity"]["z"] == 6.0f, "velocity snapshot matches");

    auto rNotFound = p.request("observe", {{"target", "NonExistent"}});
    check(rNotFound["ok"] == true, "observe non-existent succeeds");
    check(rNotFound["found"] == false, "non-existent not found");

    auto rBadTarget = p.request("observe", {{"target", ""}});
    check(rBadTarget["ok"] == false && rBadTarget["error"] == "invalid_request", "empty target rejected");

    // Capture protocol check
    auto rCap = p.request("capture");
    check(rCap["ok"] == true && rCap["status"] == "queued", "capture request queued");

    auto rCapFull = p.request("capture");
    check(rCapFull["ok"] == false && rCapFull["error"] == "queue_full", "duplicate capture rejected with queue_full");

    auto statusQueued = p.request("status");
    check(statusQueued["capture"]["status"] == "queued", "status reports capture queued");

    // Test capture with extra field rejected
    auto rExtra = p.request("capture", {{"arbitraryPath", "c:/bad.png"}});
    check(rExtra["ok"] == false && rExtra["error"] == "invalid_request", "extra fields in capture rejected");

    std::cout << "PASS Phase 3 observe named target + capture protocol check\n";
    return 0;
}
