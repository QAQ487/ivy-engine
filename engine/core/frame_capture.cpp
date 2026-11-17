// frame_capture.cpp
// Captures backbuffer frames to image files.

#include "frame_capture.h"
#include "frame_timer.h"
#include "engine/rendering/d3d11/renderer.h"

#include <Psapi.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <wrl/client.h>
#include "engine/debug_draw/debug_ofstream.h"

#pragma comment(lib, "Psapi.lib")

namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned kWarmup = 300;
constexpr unsigned kSamples = 300;
struct Slot {
    ComPtr<ID3D11Query> disjoint, begin, end, pipeline;
    bool pending = false;
    unsigned frame = 0;
};
struct Average {
    double sum = 0;
    unsigned count = 0;
    void Add(double value) { sum += value; ++count; }
    double Mean() const { return count ? sum / count : 0; }
};
std::once_flag initialized;
std::atomic<bool> enabled{ false };
std::mutex mutex;
std::string path;
std::array<Slot, 32> slots;
unsigned cpuFrames = 0, drawFrames = 0, spriteDraws = 0, textDraws = 0;
int activeSlot = -1;
bool reported = false, queriesAttempted = false;
LONGLONG frequency = 1, previousFrame = 0, drawStart = 0;
float width = 0, height = 0;
Average cpuFrame, update, draw, gpu, vertices, sprites, texts;

LONGLONG Now() { LARGE_INTEGER value{}; QueryPerformanceCounter(&value); return value.QuadPart; }
double Milliseconds(LONGLONG ticks) { return 1000.0 * ticks / frequency; }
bool Sample(unsigned frame) { return frame >= kWarmup && frame < kWarmup + kSamples; }
void Initialize() {
    char value[32768]{};
    const DWORD size = GetEnvironmentVariableA("IVY_FRAME_CAPTURE", value, sizeof(value));
    if (!size || size >= sizeof(value)) return;
    path.assign(value, size);
    LARGE_INTEGER valueFrequency{};
    QueryPerformanceFrequency(&valueFrequency);
    if (valueFrequency.QuadPart > 0) frequency = valueFrequency.QuadPart;
    enabled.store(true, std::memory_order_release);
}
bool Enabled() {
    std::call_once(initialized, Initialize);
    return enabled.load(std::memory_order_acquire);
}
void CreateQueries() {
    queriesAttempted = true;
    auto* device = GetDevice();
    if (!device) return;
    for (auto& slot : slots) {
        D3D11_QUERY_DESC desc{};
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (FAILED(device->CreateQuery(&desc, slot.disjoint.GetAddressOf()))) break;
        desc.Query = D3D11_QUERY_TIMESTAMP;
        if (FAILED(device->CreateQuery(&desc, slot.begin.GetAddressOf()))) break;
        if (FAILED(device->CreateQuery(&desc, slot.end.GetAddressOf()))) break;
        desc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
        if (FAILED(device->CreateQuery(&desc, slot.pipeline.GetAddressOf()))) break;
    }
}
void Poll() {
    auto* context = GetDeviceContext();
    if (!context) return;
    for (auto& slot : slots) {
        if (!slot.pending) continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timing{};
        D3D11_QUERY_DATA_PIPELINE_STATISTICS stats{};
        UINT64 begin = 0, end = 0;
        // Never flush or wait for the GPU. Unavailable results stay in the ring.
        constexpr UINT flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
        if (context->GetData(slot.disjoint.Get(), &timing, sizeof(timing), flags) != S_OK ||
            context->GetData(slot.begin.Get(), &begin, sizeof(begin), flags) != S_OK ||
            context->GetData(slot.end.Get(), &end, sizeof(end), flags) != S_OK ||
            context->GetData(slot.pipeline.Get(), &stats, sizeof(stats), flags) != S_OK) continue;
        if (Sample(slot.frame)) {
            vertices.Add(static_cast<double>(stats.IAVertices));
            if (!timing.Disjoint && timing.Frequency && end >= begin)
                gpu.Add(1000.0 * static_cast<double>(end - begin) / timing.Frequency);
        }
        slot.pending = false;
    }
}
void Report() {
    if (reported) return;
    std::error_code error;
    const std::filesystem::path output(path);
    if (output.has_parent_path()) std::filesystem::create_directories(output.parent_path(), error);
    std::ofstream file(output);
    if (!file) { hal::DebugWriteA("FrameCapture: cannot write capture JSON.\n"); reported = true; return; }
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    const bool memoryValid = GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE;
    file << "{\n  \"warmupFrames\": " << kWarmup
        << ",\n  \"requestedSamples\": " << kSamples
        << ",\n  \"backBufferWidth\": " << width << ",\n  \"backBufferHeight\": " << height
        << ",\n  \"frameIntervalMeanMs\": " << cpuFrame.Mean()
        << ",\n  \"frameIntervalSamples\": " << cpuFrame.count
        << ",\n  \"updateMeanMs\": " << update.Mean() << ",\n  \"updateSamples\": " << update.count
        << ",\n  \"drawMeanMs\": " << draw.Mean() << ",\n  \"drawSamples\": " << draw.count
        << ",\n  \"gpuMeanMs\": " << gpu.Mean() << ",\n  \"gpuValidSamples\": " << gpu.count
        << ",\n  \"iaVerticesMean\": " << vertices.Mean() << ",\n  \"pipelineSamples\": " << vertices.count
        << ",\n  \"spriteDrawCallsMean\": " << sprites.Mean() << ",\n  \"spriteDrawSamples\": " << sprites.count
        << ",\n  \"textDrawCallsMean\": " << texts.Mean()
        << ",\n  \"memoryValid\": " << (memoryValid ? "true" : "false")
        << ",\n  \"workingSetBytes\": " << memory.WorkingSetSize
        << ",\n  \"privateBytes\": " << memory.PrivateUsage << "\n}\n";
    reported = true;
}
}

void FrameCapture_BeginFrame() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    const auto now = Now();
    if (previousFrame && cpuFrames && Sample(cpuFrames - 1)) cpuFrame.Add(Milliseconds(now - previousFrame));
    previousFrame = now;
}
void FrameCapture_EndFrame() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    if (Sample(cpuFrames)) {
        long long start = 0, end = 0;
        FrameTimer_LastInterval(FrameTimerStage::Update, &start, &end);
        if (start && end >= start) update.Add(Milliseconds(end - start));
    }
    ++cpuFrames;
    bool pendingSamples = false;
    for (const auto& slot : slots) if (slot.pending && Sample(slot.frame)) pendingSamples = true;
    if (cpuFrames > kWarmup + kSamples && drawFrames >= kWarmup + kSamples &&
        (!pendingSamples || cpuFrames >= kWarmup + kSamples + 120)) {
        Report();
        PostQuitMessage(0);
    }
}
void FrameCapture_BeginDraw() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    if (!queriesAttempted) CreateQueries();
    Poll();
    width = GetBackBufferWidth(); height = GetBackBufferHeight();
    spriteDraws = 0; textDraws = 0;
    drawStart = Now();
    activeSlot = -1;
    auto* context = GetDeviceContext();
    if (!context) return;
    for (unsigned i = 0; i < slots.size(); ++i) {
        auto& slot = slots[i];
        if (slot.pending || !slot.disjoint || !slot.begin || !slot.end || !slot.pipeline) continue;
        slot.frame = drawFrames;
        context->Begin(slot.disjoint.Get());
        context->Begin(slot.pipeline.Get());
        context->End(slot.begin.Get());
        activeSlot = static_cast<int>(i);
        break;
    }
}
void FrameCapture_RecordSpriteDraw() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    ++spriteDraws;
}
void FrameCapture_RecordTextDraw() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    ++textDraws;
}
void FrameCapture_EndDraw() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    if (Sample(drawFrames)) { draw.Add(Milliseconds(Now() - drawStart)); sprites.Add(spriteDraws); texts.Add(textDraws); }
    if (activeSlot >= 0) {
        auto& slot = slots[activeSlot];
        auto* context = GetDeviceContext();
        context->End(slot.end.Get());
        context->End(slot.pipeline.Get());
        context->End(slot.disjoint.Get());
        slot.pending = true;
        activeSlot = -1;
    }
    ++drawFrames;
}
void FrameCapture_Shutdown() {
    if (!Enabled()) return;
    std::lock_guard<std::mutex> lock(mutex);
    Poll();
    Report();
    for (auto& slot : slots) slot = Slot{};
    enabled.store(false, std::memory_order_release);
}
