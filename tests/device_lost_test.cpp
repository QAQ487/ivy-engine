// device_lost_test.cpp
// Linux-runnable unit tests for engine/rendering/d3d11/device_lost.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/device_lost_test tests/device_lost_test.cpp
//   /tmp/device_lost_test

#include "engine/rendering/d3d11/device_lost.h"

#include <cstdint>
#include <cstdio>
#include <string>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static std::int32_t Hr(std::uint32_t u) { return static_cast<std::int32_t>(u); }

int main() {
    using namespace device_lost;

    Expect(IsDeviceLostResult(Hr(0x887A0005u)), "DEVICE_REMOVED is lost");
    Expect(IsDeviceLostResult(Hr(0x887A0007u)), "DEVICE_RESET is lost");
    Expect(!IsDeviceLostResult(0), "S_OK is not lost");
    Expect(!IsDeviceLostResult(Hr(0x087A0001u)), "DXGI_STATUS_OCCLUDED is not lost");
    Expect(!IsDeviceLostResult(Hr(0x887A0001u)), "INVALID_CALL is not lost");
    Expect(!IsDeviceLostResult(Hr(0x887A0006u)), "DEVICE_HUNG is a reason, not a Present result");
    Expect(!IsDeviceLostResult(Hr(0x80004005u)), "E_FAIL is not lost");

    Expect(std::string(ReasonName(Hr(0x887A0006u))) == "DXGI_ERROR_DEVICE_HUNG", "hung name");
    Expect(std::string(ReasonName(Hr(0x887A0020u))) == "DXGI_ERROR_DRIVER_INTERNAL_ERROR", "driver error name");
    Expect(std::string(ReasonName(0)) == "S_OK", "S_OK name");
    Expect(std::string(ReasonName(Hr(0x80004005u))) == "unknown", "unknown name");

    Expect(HexResult(Hr(0x887A0005u)) == "0x887A0005", "hex upper-case, 8 digits");
    Expect(HexResult(1) == "0x00000001", "hex zero-padded");

    const std::string log = LogLine(Hr(0x887A0005u), Hr(0x887A0006u));
    Expect(log == "Renderer: GPU device lost (Present DXGI_ERROR_DEVICE_REMOVED 0x887A0005, "
                  "reason DXGI_ERROR_DEVICE_HUNG 0x887A0006)", "log line");

    const std::string user = UserMessage(Hr(0x887A0007u), Hr(0x887A0007u));
    Expect(user.find("will close now") != std::string::npos, "user message says the host closes");
    Expect(user.find("Present: DXGI_ERROR_DEVICE_RESET (0x887A0007)") != std::string::npos, "user message present line");
    Expect(user.find("Reason: DXGI_ERROR_DEVICE_RESET (0x887A0007)") != std::string::npos, "user message reason line");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("device_lost_test: all checks passed\n");
    return 0;
}
