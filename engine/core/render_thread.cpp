/**===========================================================================
 File   : render_thread.cpp
 Brief  : Join-fenced render worker (Phase C). Kick/Join from WinMain only.
 ===========================================================================**/
#include "render_thread.h"

#include <atomic>
#include <thread>

namespace {

std::atomic<bool> g_running{ false };
std::atomic<bool> g_busy{ false };

HANDLE g_kickEvent = nullptr; // auto-reset
HANDLE g_doneEvent = nullptr; // manual-reset; signaled == idle
HANDLE g_stopEvent = nullptr; // manual-reset

void (*g_work)() = nullptr;
std::thread g_thread;

void WorkerMain() {
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	HANDLE waits[2] = { g_kickEvent, g_stopEvent };
	while (g_running.load(std::memory_order_acquire)) {
		const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
		if (wait == WAIT_OBJECT_0 + 1)
			break;
		if (wait != WAIT_OBJECT_0)
			break;

		void (*work)() = g_work;
		g_work = nullptr;
		if (work)
			work();

		g_busy.store(false, std::memory_order_release);
		SetEvent(g_doneEvent);
	}

	CoUninitialize();
}

} // namespace

void RenderThread_Start() {
	if (g_running.load(std::memory_order_acquire))
		return;

	g_kickEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr); // auto-reset
	g_doneEvent = CreateEventA(nullptr, TRUE, TRUE, nullptr);   // manual-reset, idle
	g_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	g_work = nullptr;
	g_busy.store(false, std::memory_order_release);
	g_running.store(true, std::memory_order_release);
	g_thread = std::thread(WorkerMain);
}

void RenderThread_Stop() {
	if (!g_running.load(std::memory_order_acquire))
		return;

	RenderThread_Join();
	g_running.store(false, std::memory_order_release);
	if (g_stopEvent)
		SetEvent(g_stopEvent);

	if (g_thread.joinable())
		g_thread.join();

	g_work = nullptr;
	g_busy.store(false, std::memory_order_release);

	if (g_kickEvent) { CloseHandle(g_kickEvent); g_kickEvent = nullptr; }
	if (g_doneEvent) { CloseHandle(g_doneEvent); g_doneEvent = nullptr; }
	if (g_stopEvent) { CloseHandle(g_stopEvent); g_stopEvent = nullptr; }
}

bool RenderThread_IsRunning() {
	return g_running.load(std::memory_order_acquire);
}

void RenderThread_Kick(void (*work)()) {
	if (!work)
		return;

	if (!g_running.load(std::memory_order_acquire)) {
		work();
		return;
	}

	RenderThread_Join();
	g_work = work;
	g_busy.store(true, std::memory_order_release);
	ResetEvent(g_doneEvent);
	SetEvent(g_kickEvent);
}

void RenderThread_Join() {
	if (!g_running.load(std::memory_order_acquire))
		return;
	if (!g_doneEvent)
		return;
	if (!g_busy.load(std::memory_order_acquire)
		&& WaitForSingleObject(g_doneEvent, 0) == WAIT_OBJECT_0) {
		return;
	}
	WaitForSingleObject(g_doneEvent, INFINITE);
}

bool RenderThread_IsBusy() {
	return g_running.load(std::memory_order_acquire)
		&& g_busy.load(std::memory_order_acquire);
}

HANDLE RenderThread_DoneEvent() {
	if (!g_running.load(std::memory_order_acquire))
		return nullptr;
	return g_doneEvent;
}
