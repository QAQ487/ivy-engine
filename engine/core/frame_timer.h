/**===========================================================================
 File   : frame_timer.h
 Brief  : Per-frame QPC stage timers (update / draw / present / joinWait / frame).
          Begin/End of a given stage is single-threaded; accumulators are atomic
          so draw/present on the render thread can mix with update/joinWait on
          WinMain. Print-and-reset from the main thread at 1 Hz.
 ===========================================================================**/
#pragma once

enum class FrameTimerStage {
	Update = 0,
	Draw,
	Present,
	JoinWait,
	Frame,
	Count
};

void FrameTimer_Begin(FrameTimerStage stage);
void FrameTimer_End(FrameTimerStage stage);

void FrameTimer_BeginFrame();
void FrameTimer_EndFrame();

// Last completed interval for this stage (QPC ticks). start==end==0 if none.
void FrameTimer_LastInterval(FrameTimerStage stage, long long* startQpc, long long* endQpc);

// Last completed interval duration in milliseconds (0 when unavailable).
double FrameTimer_LastMs(FrameTimerStage stage);

// Prints 1 Hz averages (ms) and resets accumulators. rtEnabled is informational.
void FrameTimer_FlushPrint(double fps, bool rtEnabled);
