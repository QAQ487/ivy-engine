/**===========================================================================
 File   : job_system.cpp
 Brief  : Minimal Win32 worker-pool job system (phase A).
 ===========================================================================**/
#include "job_system.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <Windows.h>

#ifndef GAME_RUNTIME
#include "imgui/imgui.h"
#endif

namespace {

constexpr int kMaxWorkers = 8;
constexpr int kTimelineCapacity = 128;

struct TimelineEntry {
	int workerIndex = -1;
	LONGLONG startQpc = 0;
	LONGLONG endQpc = 0;
};

std::mutex g_queueMutex;
std::queue<std::function<void()>> g_queue;
std::atomic<int> g_pending{ 0 };
std::atomic<bool> g_running{ false };
std::atomic<bool> g_useParallel{ true };
std::atomic<bool> g_initialized{ false };

HANDLE g_wakeSemaphore = nullptr; // one count per submitted job (no lost wakeups)
HANDLE g_doneEvent = nullptr;     // manual-reset: signaled when pending hits 0
HANDLE g_stopEvent = nullptr;

std::vector<std::thread> g_workers;
int g_workerCount = 0;

std::mutex g_timelineMutex;
TimelineEntry g_timeline[kTimelineCapacity];
std::atomic<int> g_timelineWrite{ 0 };
LARGE_INTEGER g_qpcFreq{};

void RecordTimeline(int workerIndex, LONGLONG startQpc, LONGLONG endQpc) {
	const int slot = g_timelineWrite.fetch_add(1) % kTimelineCapacity;
	std::lock_guard<std::mutex> lock(g_timelineMutex);
	g_timeline[slot] = TimelineEntry{ workerIndex, startQpc, endQpc };
}

void WorkerMain(int workerIndex) {
	HANDLE waits[2] = { g_wakeSemaphore, g_stopEvent };
	while (g_running.load(std::memory_order_acquire)) {
		const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
		if (wait == WAIT_OBJECT_0 + 1)
			break; // stop
		if (wait != WAIT_OBJECT_0)
			break;

		std::function<void()> job;
		{
			std::lock_guard<std::mutex> lock(g_queueMutex);
			if (g_queue.empty())
				continue;
			job = std::move(g_queue.front());
			g_queue.pop();
		}

		LARGE_INTEGER start{};
		QueryPerformanceCounter(&start);
		job();
		LARGE_INTEGER end{};
		QueryPerformanceCounter(&end);
		RecordTimeline(workerIndex, start.QuadPart, end.QuadPart);

		if (g_pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
			SetEvent(g_doneEvent);
	}
}

} // namespace

void JobSystem_Init(int workerCount) {
	if (g_initialized.load(std::memory_order_acquire))
		return;

	QueryPerformanceFrequency(&g_qpcFreq);

	if (workerCount < 1)
		workerCount = 1;
	if (workerCount > kMaxWorkers)
		workerCount = kMaxWorkers;

	g_wakeSemaphore = CreateSemaphoreA(nullptr, 0, 0x7FFFFFFF, nullptr);
	g_doneEvent = CreateEventA(nullptr, TRUE, TRUE, nullptr);  // manual-reset, start signaled
	g_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr); // manual-reset

	g_pending.store(0, std::memory_order_release);
	g_running.store(true, std::memory_order_release);
	g_workerCount = workerCount;
	g_workers.reserve(static_cast<size_t>(workerCount));
	for (int i = 0; i < workerCount; ++i)
		g_workers.emplace_back(WorkerMain, i);

	g_initialized.store(true, std::memory_order_release);
}

void JobSystem_Shutdown() {
	if (!g_initialized.load(std::memory_order_acquire))
		return;

	g_running.store(false, std::memory_order_release);
	if (g_stopEvent)
		SetEvent(g_stopEvent); // wakes all workers waiting on the stop handle

	for (auto& t : g_workers) {
		if (t.joinable())
			t.join();
	}
	g_workers.clear();
	g_workerCount = 0;

	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		while (!g_queue.empty())
			g_queue.pop();
	}
	g_pending.store(0, std::memory_order_release);

	if (g_wakeSemaphore) { CloseHandle(g_wakeSemaphore); g_wakeSemaphore = nullptr; }
	if (g_doneEvent) { CloseHandle(g_doneEvent); g_doneEvent = nullptr; }
	if (g_stopEvent) { CloseHandle(g_stopEvent); g_stopEvent = nullptr; }

	g_initialized.store(false, std::memory_order_release);
}

JobHandle JobSystem_Submit(std::function<void()> job) {
	if (!job)
		return 0;

	if (!JobSystem_UseParallel() || !g_initialized.load(std::memory_order_acquire)) {
		job();
		return 0;
	}

	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		const int prev = g_pending.fetch_add(1, std::memory_order_acq_rel);
		if (prev == 0)
			ResetEvent(g_doneEvent);
		g_queue.push(std::move(job));
	}
	ReleaseSemaphore(g_wakeSemaphore, 1, nullptr);
	return 0;
}

void JobSystem_WaitAll() {
	if (!JobSystem_UseParallel() || !g_initialized.load(std::memory_order_acquire))
		return;

	if (g_pending.load(std::memory_order_acquire) == 0)
		return;

	WaitForSingleObject(g_doneEvent, INFINITE);
}

bool JobSystem_UseParallel() {
	return g_useParallel.load(std::memory_order_acquire);
}

void JobSystem_SetUseParallel(bool enabled) {
	g_useParallel.store(enabled, std::memory_order_release);
}

void JobSystem_DebugDraw() {
#ifndef GAME_RUNTIME
	static bool s_open = true;
	if (!s_open)
		return;
	if (!ImGui::Begin("Job System", &s_open)) {
		ImGui::End();
		return;
	}

	ImGui::Text("Workers: %d  Parallel: %s  Pending: %d",
		g_workerCount,
		JobSystem_UseParallel() ? "on" : "off",
		g_pending.load(std::memory_order_relaxed));
	if (ImGui::Button(JobSystem_UseParallel() ? "Disable Parallel" : "Enable Parallel"))
		JobSystem_SetUseParallel(!JobSystem_UseParallel());

	TimelineEntry snapshot[kTimelineCapacity];
	int writeIdx = 0;
	{
		std::lock_guard<std::mutex> lock(g_timelineMutex);
		writeIdx = g_timelineWrite.load(std::memory_order_relaxed);
		memcpy(snapshot, g_timeline, sizeof(snapshot));
	}

	const int count = (writeIdx < kTimelineCapacity) ? writeIdx : kTimelineCapacity;
	LONGLONG minStart = 0;
	LONGLONG maxEnd = 0;
	bool haveRange = false;
	for (int i = 0; i < count; ++i) {
		const TimelineEntry& e = snapshot[i];
		if (e.workerIndex < 0)
			continue;
		if (!haveRange) {
			minStart = e.startQpc;
			maxEnd = e.endQpc;
			haveRange = true;
		}
		else {
			if (e.startQpc < minStart) minStart = e.startQpc;
			if (e.endQpc > maxEnd) maxEnd = e.endQpc;
		}
	}

	const double range = (haveRange && maxEnd > minStart)
		? static_cast<double>(maxEnd - minStart)
		: 1.0;
	const float barW = ImGui::GetContentRegionAvail().x;
	const float rowH = 14.0f;

	for (int w = 0; w < g_workerCount; ++w) {
		ImGui::Text("W%d", w);
		ImGui::SameLine();
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(cursor, ImVec2(cursor.x + barW, cursor.y + rowH),
			IM_COL32(40, 40, 40, 255));

		for (int i = 0; i < count; ++i) {
			const TimelineEntry& e = snapshot[i];
			if (e.workerIndex != w)
				continue;
			const float x0 = static_cast<float>((e.startQpc - minStart) / range) * barW;
			const float x1 = static_cast<float>((e.endQpc - minStart) / range) * barW;
			const float width = (x1 > x0 + 1.0f) ? (x1 - x0) : 2.0f;
			dl->AddRectFilled(
				ImVec2(cursor.x + x0, cursor.y + 1.0f),
				ImVec2(cursor.x + x0 + width, cursor.y + rowH - 1.0f),
				IM_COL32(80, 160, 220, 255));
		}
		ImGui::Dummy(ImVec2(barW, rowH));
	}

	ImGui::End();
#endif // !GAME_RUNTIME
}
