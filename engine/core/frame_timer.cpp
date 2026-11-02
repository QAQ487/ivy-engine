/**===========================================================================
 File   : frame_timer.cpp
 Brief  : QPC accumulators for Phase C frame-stage reporting.
 ===========================================================================**/
#include "frame_timer.h"

#include <Windows.h>
#include <atomic>
#include <cstdio>

namespace {

constexpr int kStageCount = static_cast<int>(FrameTimerStage::Count);

LARGE_INTEGER g_freq{};
LONGLONG g_start[kStageCount]{};
std::atomic<LONGLONG> g_sumTicks[kStageCount]{};
std::atomic<LONGLONG> g_lastStart[kStageCount]{};
std::atomic<LONGLONG> g_lastEnd[kStageCount]{};
std::atomic<int> g_samples{ 0 };
bool g_haveFreq = false;

void EnsureFreq() {
	if (g_haveFreq)
		return;
	QueryPerformanceFrequency(&g_freq);
	if (g_freq.QuadPart <= 0)
		g_freq.QuadPart = 1;
	g_haveFreq = true;
}

double TicksToMs(LONGLONG ticks) {
	EnsureFreq();
	return (1000.0 * static_cast<double>(ticks)) / static_cast<double>(g_freq.QuadPart);
}

} // namespace

void FrameTimer_Begin(FrameTimerStage stage) {
	EnsureFreq();
	const int i = static_cast<int>(stage);
	if (i < 0 || i >= kStageCount)
		return;
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	g_start[i] = now.QuadPart;
}

void FrameTimer_End(FrameTimerStage stage) {
	const int i = static_cast<int>(stage);
	if (i < 0 || i >= kStageCount)
		return;
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	const LONGLONG start = g_start[i];
	if (start <= 0 || now.QuadPart < start)
		return;
	g_sumTicks[i].fetch_add(now.QuadPart - start, std::memory_order_relaxed);
	g_lastStart[i].store(start, std::memory_order_relaxed);
	g_lastEnd[i].store(now.QuadPart, std::memory_order_relaxed);
}

void FrameTimer_LastInterval(FrameTimerStage stage, long long* startQpc, long long* endQpc) {
	const int i = static_cast<int>(stage);
	if (startQpc)
		*startQpc = 0;
	if (endQpc)
		*endQpc = 0;
	if (i < 0 || i >= kStageCount)
		return;
	const LONGLONG start = g_lastStart[i].load(std::memory_order_relaxed);
	const LONGLONG end = g_lastEnd[i].load(std::memory_order_relaxed);
	if (startQpc)
		*startQpc = static_cast<long long>(start);
	if (endQpc)
		*endQpc = static_cast<long long>(end);
}

double FrameTimer_LastMs(FrameTimerStage stage) {
	long long start = 0, end = 0;
	FrameTimer_LastInterval(stage, &start, &end);
	if (start <= 0 || end < start)
		return 0.0;
	return TicksToMs(end - start);
}

void FrameTimer_BeginFrame() {
	FrameTimer_Begin(FrameTimerStage::Frame);
}

void FrameTimer_EndFrame() {
	FrameTimer_End(FrameTimerStage::Frame);
	g_samples.fetch_add(1, std::memory_order_relaxed);
}

void FrameTimer_FlushPrint(double fps, bool rtEnabled) {
	const int n = g_samples.exchange(0, std::memory_order_relaxed);
	LONGLONG sums[kStageCount]{};
	for (int i = 0; i < kStageCount; ++i)
		sums[i] = g_sumTicks[i].exchange(0, std::memory_order_relaxed);

	if (n <= 0) {
		std::printf("FPS: %.1f  (no frame samples)  rt=%s\n",
			fps, rtEnabled ? "on" : "off");
		std::fflush(stdout);
		return;
	}

	const double invN = 1.0 / static_cast<double>(n);
	const double updateMs = TicksToMs(sums[static_cast<int>(FrameTimerStage::Update)]) * invN;
	const double drawMs = TicksToMs(sums[static_cast<int>(FrameTimerStage::Draw)]) * invN;
	const double presentMs = TicksToMs(sums[static_cast<int>(FrameTimerStage::Present)]) * invN;
	const double joinMs = TicksToMs(sums[static_cast<int>(FrameTimerStage::JoinWait)]) * invN;
	const double frameMs = TicksToMs(sums[static_cast<int>(FrameTimerStage::Frame)]) * invN;

	std::printf(
		"FPS: %.1f  update=%.2fms draw=%.2fms present=%.2fms joinWait=%.2fms frame=%.2fms rt=%s n=%d\n",
		fps, updateMs, drawMs, presentMs, joinMs, frameMs,
		rtEnabled ? "on" : "off", n);
	std::fflush(stdout);
}
