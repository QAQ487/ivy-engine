/*==============================================================================

   System timer [system_timer.cpp]
														 Author : Terry Yeung
														 Date   : 2018/06/17
--------------------------------------------------------------------------------

==============================================================================*/
#include <Windows.h>

/*------------------------------------------------------------------------------
   Global variables
------------------------------------------------------------------------------*/
static bool g_bTimerStopped = true; // Stopped flag
static LONGLONG g_TicksPerSec = 0;  // Ticks per second
static LONGLONG g_StopTime;         // Time when stopped
static LONGLONG g_LastElapsedTime;  // Last recorded update time
static LONGLONG g_BaseTime;         // Base time

/*------------------------------------------------------------------------------
   Prototypes
------------------------------------------------------------------------------*/
// Return stop time if stopped, otherwise current time
static LARGE_INTEGER GetAdjustedCurrentTime(void);

/*------------------------------------------------------------------------------
   Function definitions
------------------------------------------------------------------------------*/

// Initialize the system timer
void SystemTimer_Initialize(void)
{
	g_bTimerStopped = true;
	g_TicksPerSec = 0;
	g_StopTime = 0;
	g_LastElapsedTime = 0;
	g_BaseTime = 0;

	// Query high-resolution performance-counter frequency
	LARGE_INTEGER ticksPerSec = { 0 };
	QueryPerformanceFrequency(&ticksPerSec);
	g_TicksPerSec = ticksPerSec.QuadPart;
}

// Reset the system timer
void SystemTimer_Reset(void)
{
	LARGE_INTEGER time = GetAdjustedCurrentTime();

	g_BaseTime = g_LastElapsedTime = time.QuadPart;
	g_StopTime = 0;
	g_bTimerStopped = false;
}

// Start the system timer
void SystemTimer_Start(void)
{
	// Get the current (absolute) time
	LARGE_INTEGER time = { 0 };
	QueryPerformanceCounter(&time);

	// If the timer was stopped,
	if (g_bTimerStopped) {
		// subtract stopped duration from base time
		g_BaseTime += time.QuadPart - g_StopTime;
	}

	g_StopTime = 0;
	g_LastElapsedTime = time.QuadPart;
	g_bTimerStopped = false;
}

// Stop the system timer
void SystemTimer_Stop(void)
{
	if (g_bTimerStopped) return;

	LARGE_INTEGER time = { 0 };
	QueryPerformanceCounter(&time);

	g_LastElapsedTime = g_StopTime = time.QuadPart; // Record stop time
	g_bTimerStopped = true;
}

// Advance the system timer by 0.1 seconds
void SystemTimer_Advance(void)
{
	g_StopTime += g_TicksPerSec / 10;
}

// Get measured time
double SystemTimer_GetTime(void)
{
	if (g_TicksPerSec == 0) return 0.0;

	LARGE_INTEGER time = GetAdjustedCurrentTime();

	return (double)(time.QuadPart - g_BaseTime) / (double)g_TicksPerSec;
}

// Get the current (absolute) time
double SystemTimer_GetAbsoluteTime(void)
{
	if (g_TicksPerSec == 0) return 0.0;

	LARGE_INTEGER time = { 0 };
	QueryPerformanceCounter(&time);

	return time.QuadPart / (double)g_TicksPerSec;
}

// Get elapsed time
float SystemTimer_GetElapsedTime(void)
{
	if (g_TicksPerSec == 0) return 0.0f;

	LARGE_INTEGER time = GetAdjustedCurrentTime();

	double elapsed_time = (float)((double)(time.QuadPart - g_LastElapsedTime) / (double)g_TicksPerSec);
	g_LastElapsedTime = time.QuadPart;

	// Clamp elapsed time to 0 so the timer stays accurate.
	// elapsed_time can go out of range if the CPU enters power-saving mode or the thread is migrated to another processor.
	// Therefore the main thread should call SetThreadAffinityMask so it is not shuffled to another processor.
	// Other worker threads should not call SetThreadAffinityMask; they should use a shared copy of timer data collected on the main thread.
	if (elapsed_time < 0.0f) {
		elapsed_time = 0.0f;
	}

	return (float)elapsed_time;
}

// Is the system timer stopped?
bool SystemTimer_IsStoped(void)
{
	return g_bTimerStopped;
}

// Pin the current thread to a single processor
void LimitThreadAffinityToCurrentProc(void)
{
	HANDLE hCurrentProcess = GetCurrentProcess();

	// Get the processor affinity mask for this process
	DWORD_PTR dwProcessAffinityMask = 0;
	DWORD_PTR dwSystemAffinityMask = 0;

	if (GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask) != 0 && dwProcessAffinityMask) {
		// Find the lowest processor that our process is allows to run against
		DWORD_PTR dwAffinityMask = (dwProcessAffinityMask & ((~dwProcessAffinityMask) + 1));

		// Set this as the processor that our thread must always run against
		// This must be a subset of the process affinity mask
		HANDLE hCurrentThread = GetCurrentThread();
		if (INVALID_HANDLE_VALUE != hCurrentThread) {
			SetThreadAffinityMask(hCurrentThread, dwAffinityMask);
			CloseHandle(hCurrentThread);
		}
	}

	CloseHandle(hCurrentProcess);
}

// Return stop time if stopped, otherwise current time
LARGE_INTEGER GetAdjustedCurrentTime(void)
{
	LARGE_INTEGER time;
	if (g_StopTime != 0) {
		time.QuadPart = g_StopTime;
	}
	else {
		QueryPerformanceCounter(&time);
	}

	return time;
}
