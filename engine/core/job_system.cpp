/**===========================================================================
 File   : job_system.cpp
 Brief  : Frame-scoped dependency-graph job system (phase B).
 ===========================================================================**/
#include "job_system.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <Windows.h>

#ifndef GAME_RUNTIME
#include "imgui/imgui.h"
#endif

struct JobNode {
	std::function<void()> run;
	std::atomic<int> pendingDeps{ 0 };
	std::vector<JobNode*> dependents;
	std::atomic<bool> completed{ false };
	HANDLE doneEvent = nullptr; // manual-reset; Set on complete (WaitFor)
	int index = 0;
	int dependentsCount = 0; // debug: dependents.size() at complete

	~JobNode() {
		if (doneEvent) {
			CloseHandle(doneEvent);
			doneEvent = nullptr;
		}
	}
};

namespace {

constexpr int kMaxWorkers = 8;
constexpr int kTimelineCapacity = 128;

struct TimelineEntry {
	int workerIndex = -1;
	int nodeIndex = -1;
	int dependentsCount = 0;
	LONGLONG startQpc = 0;
	LONGLONG endQpc = 0;
};

std::mutex g_queueMutex;
std::queue<JobNode*> g_readyQueue;
std::mutex g_dependentsMutex; // orders completed check with dependents push/snapshot
std::atomic<int> g_remaining{ 0 };
std::atomic<bool> g_running{ false };
std::atomic<bool> g_useParallel{ true };
std::atomic<bool> g_initialized{ false };

HANDLE g_wakeSemaphore = nullptr;
HANDLE g_doneEvent = nullptr; // manual-reset: signaled when remaining hits 0
HANDLE g_stopEvent = nullptr;

std::vector<std::thread> g_workers;
int g_workerCount = 0;

std::vector<std::unique_ptr<JobNode>> g_frameNodes;

std::mutex g_timelineMutex;
TimelineEntry g_timeline[kTimelineCapacity];
std::atomic<int> g_timelineWrite{ 0 };
LARGE_INTEGER g_qpcFreq{};

void RecordTimeline(int workerIndex, int nodeIndex, int dependentsCount,
	LONGLONG startQpc, LONGLONG endQpc) {
	const int slot = g_timelineWrite.fetch_add(1) % kTimelineCapacity;
	std::lock_guard<std::mutex> lock(g_timelineMutex);
	g_timeline[slot] = TimelineEntry{ workerIndex, nodeIndex, dependentsCount, startQpc, endQpc };
}

void PushReady(JobNode* node) {
	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		g_readyQueue.push(node);
	}
	ReleaseSemaphore(g_wakeSemaphore, 1, nullptr);
}

void CompleteNode(JobNode* node, int workerIndex, LONGLONG startQpc, LONGLONG endQpc) {
	node->completed.store(true, std::memory_order_release);
	if (node->doneEvent)
		SetEvent(node->doneEvent);

	std::vector<JobNode*> depsSnapshot;
	{
		std::lock_guard<std::mutex> lock(g_dependentsMutex);
		depsSnapshot = node->dependents;
		node->dependentsCount = static_cast<int>(depsSnapshot.size());
	}

	RecordTimeline(workerIndex, node->index, node->dependentsCount, startQpc, endQpc);

	for (JobNode* dependent : depsSnapshot) {
		if (dependent->pendingDeps.fetch_sub(1, std::memory_order_acq_rel) == 1)
			PushReady(dependent);
	}

	if (g_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
		SetEvent(g_doneEvent);
}

void RecycleFrameNodes() {
	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		while (!g_readyQueue.empty())
			g_readyQueue.pop();
	}
	g_frameNodes.clear();
}

void WorkerMain(int workerIndex) {
	HANDLE waits[2] = { g_wakeSemaphore, g_stopEvent };
	while (g_running.load(std::memory_order_acquire)) {
		const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
		if (wait == WAIT_OBJECT_0 + 1)
			break;
		if (wait != WAIT_OBJECT_0)
			break;

		JobNode* node = nullptr;
		{
			std::lock_guard<std::mutex> lock(g_queueMutex);
			if (g_readyQueue.empty())
				continue;
			node = g_readyQueue.front();
			g_readyQueue.pop();
		}
		if (!node)
			continue;

		LARGE_INTEGER start{};
		QueryPerformanceCounter(&start);
		if (node->run)
			node->run();
		LARGE_INTEGER end{};
		QueryPerformanceCounter(&end);

		CompleteNode(node, workerIndex, start.QuadPart, end.QuadPart);
	}
}

JobHandle SubmitInternal(std::function<void()> job, const std::vector<JobHandle>& deps) {
	if (!job)
		return nullptr;

	if (!JobSystem_UseParallel() || !g_initialized.load(std::memory_order_acquire)) {
		job();
		return nullptr; // already done
	}

	auto owned = std::make_unique<JobNode>();
	JobNode* node = owned.get();
	node->run = std::move(job);
	node->index = static_cast<int>(g_frameNodes.size());
	node->doneEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr); // manual-reset, start nonsignaled
	g_frameNodes.push_back(std::move(owned));

	// Wire deps under g_dependentsMutex (check completed + push ordered).
	for (JobHandle dep : deps) {
		std::lock_guard<std::mutex> lock(g_dependentsMutex);
		if (!dep || dep->completed.load(std::memory_order_acquire))
			continue;
		dep->dependents.push_back(node);
		node->pendingDeps.fetch_add(1, std::memory_order_relaxed);
	}

	const int prev = g_remaining.fetch_add(1, std::memory_order_acq_rel);
	if (prev == 0)
		ResetEvent(g_doneEvent);

	if (node->pendingDeps.load(std::memory_order_acquire) == 0)
		PushReady(node);

	return node;
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
	g_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);

	g_remaining.store(0, std::memory_order_release);
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
		SetEvent(g_stopEvent);

	for (auto& t : g_workers) {
		if (t.joinable())
			t.join();
	}
	g_workers.clear();
	g_workerCount = 0;

	RecycleFrameNodes();
	g_remaining.store(0, std::memory_order_release);

	if (g_wakeSemaphore) { CloseHandle(g_wakeSemaphore); g_wakeSemaphore = nullptr; }
	if (g_doneEvent) { CloseHandle(g_doneEvent); g_doneEvent = nullptr; }
	if (g_stopEvent) { CloseHandle(g_stopEvent); g_stopEvent = nullptr; }

	g_initialized.store(false, std::memory_order_release);
}

JobHandle JobSystem_Submit(std::function<void()> job) {
	return SubmitInternal(std::move(job), {});
}

JobHandle JobSystem_SubmitDep(std::function<void()> job, const std::vector<JobHandle>& deps) {
	return SubmitInternal(std::move(job), deps);
}

JobHandle JobSystem_SubmitAfter(JobHandle after, std::function<void()> job) {
	std::vector<JobHandle> deps;
	if (after)
		deps.push_back(after);
	return SubmitInternal(std::move(job), deps);
}

void JobSystem_WaitFor(JobHandle h) {
	if (!h)
		return;
	if (!JobSystem_UseParallel() || !g_initialized.load(std::memory_order_acquire))
		return;
	if (h->completed.load(std::memory_order_acquire))
		return;
	if (h->doneEvent)
		WaitForSingleObject(h->doneEvent, INFINITE);
}

void JobSystem_WaitAll() {
	if (!JobSystem_UseParallel() || !g_initialized.load(std::memory_order_acquire))
		return;

	if (g_remaining.load(std::memory_order_acquire) > 0)
		WaitForSingleObject(g_doneEvent, INFINITE);

	// Frame fence: recycle storage; all handles from this frame are now invalid.
	RecycleFrameNodes();
}

bool JobSystem_UseParallel() {
	return g_useParallel.load(std::memory_order_acquire);
}

void JobSystem_SetUseParallel(bool enabled) {
	g_useParallel.store(enabled, std::memory_order_release);
}

#ifndef GAME_RUNTIME
namespace {
bool g_debugWindowVisible = false;
}
#endif

void JobSystem_SetDebugWindowVisible(bool visible) {
#ifndef GAME_RUNTIME
	g_debugWindowVisible = visible;
#else
	(void)visible;
#endif
}

bool JobSystem_IsDebugWindowVisible() {
#ifndef GAME_RUNTIME
	return g_debugWindowVisible;
#else
	return false;
#endif
}

void JobSystem_ToggleDebugWindow() {
#ifndef GAME_RUNTIME
	g_debugWindowVisible = !g_debugWindowVisible;
#endif
}

void JobSystem_DebugDraw() {
#ifndef GAME_RUNTIME
	if (!g_debugWindowVisible)
		return;
	if (!ImGui::Begin("Job System", &g_debugWindowVisible)) {
		ImGui::End();
		return;
	}

	ImGui::Text("Workers: %d  Parallel: %s  Remaining: %d  FrameNodes: %zu",
		g_workerCount,
		JobSystem_UseParallel() ? "on" : "off",
		g_remaining.load(std::memory_order_relaxed),
		g_frameNodes.size());
	ImGui::TextUnformatted("Toggle: F8  |  Handles are frame-scoped (invalid after WaitAll)");
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

	ImGui::Separator();
	ImGui::TextUnformatted("Per-worker timeline");
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

	ImGui::Separator();
	ImGui::TextUnformatted("Frame graph order (by start)");
	std::vector<TimelineEntry> ordered;
	ordered.reserve(static_cast<size_t>(count));
	for (int i = 0; i < count; ++i) {
		if (snapshot[i].workerIndex >= 0 && snapshot[i].nodeIndex >= 0)
			ordered.push_back(snapshot[i]);
	}
	std::sort(ordered.begin(), ordered.end(),
		[](const TimelineEntry& a, const TimelineEntry& b) {
			return a.startQpc < b.startQpc;
		});
	const int showMax = 32;
	const int showCount = (static_cast<int>(ordered.size()) < showMax)
		? static_cast<int>(ordered.size()) : showMax;
	for (int i = 0; i < showCount; ++i) {
		const TimelineEntry& e = ordered[static_cast<size_t>(i)];
		ImGui::Text("N%d  depsOut=%d  -> W%d", e.nodeIndex, e.dependentsCount, e.workerIndex);
	}
	if (static_cast<int>(ordered.size()) > showMax)
		ImGui::Text("... %d more", static_cast<int>(ordered.size()) - showMax);
	if (ordered.empty())
		ImGui::TextUnformatted("(no jobs recorded yet)");

	ImGui::End();
#endif // !GAME_RUNTIME
}
