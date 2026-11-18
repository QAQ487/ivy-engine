// device_lost.h
// Pure logic for GPU device loss: which Present / GetDeviceRemovedReason
// results mean the D3D11 device is gone, and the text shown to the user.
// HRESULT values are spelled out so this has no Windows dependency and is
// unit-testable on Linux.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace device_lost {

// DXGI_ERROR_* values from winerror.h.
inline constexpr std::uint32_t kInvalidCall = 0x887A0001u;
inline constexpr std::uint32_t kDeviceRemoved = 0x887A0005u;
inline constexpr std::uint32_t kDeviceHung = 0x887A0006u;
inline constexpr std::uint32_t kDeviceReset = 0x887A0007u;
inline constexpr std::uint32_t kDriverInternalError = 0x887A0020u;

// True when a Present / ResizeBuffers result means the device must be recreated.
inline bool IsDeviceLostResult(std::int32_t hr) {
    const std::uint32_t u = static_cast<std::uint32_t>(hr);
    return u == kDeviceRemoved || u == kDeviceReset;
}

// Short name of a GetDeviceRemovedReason value (or the Present result).
inline const char* ReasonName(std::int32_t hr) {
    switch (static_cast<std::uint32_t>(hr)) {
        case 0u: return "S_OK";
        case kInvalidCall: return "DXGI_ERROR_INVALID_CALL";
        case kDeviceRemoved: return "DXGI_ERROR_DEVICE_REMOVED";
        case kDeviceHung: return "DXGI_ERROR_DEVICE_HUNG";
        case kDeviceReset: return "DXGI_ERROR_DEVICE_RESET";
        case kDriverInternalError: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
        default: return "unknown";
    }
}

inline std::string HexResult(std::int32_t hr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(static_cast<std::uint32_t>(hr)));
    return buf;
}

// One-line log text, e.g.
// "Renderer: GPU device lost (Present DXGI_ERROR_DEVICE_REMOVED 0x887A0005, reason DXGI_ERROR_DEVICE_HUNG 0x887A0006)"
inline std::string LogLine(std::int32_t presentHr, std::int32_t reasonHr) {
    return std::string("Renderer: GPU device lost (Present ") + ReasonName(presentHr) + " " +
           HexResult(presentHr) + ", reason " + ReasonName(reasonHr) + " " + HexResult(reasonHr) + ")";
}

// Message box body shown before the host exits.
inline std::string UserMessage(std::int32_t presentHr, std::int32_t reasonHr) {
    return "The graphics device was lost (driver update, GPU reset or GPU removed).\n"
           "Ivy Engine cannot recover it and will close now.\n\n"
           "Present: " + std::string(ReasonName(presentHr)) + " (" + HexResult(presentHr) + ")\n"
           "Reason: " + std::string(ReasonName(reasonHr)) + " (" + HexResult(reasonHr) + ")";
}

}  // namespace device_lost
