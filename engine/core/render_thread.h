/**===========================================================================
 File   : render_thread.h
 Brief  : One worker that runs a Kick() callback (GAME_RUNTIME Draw/Present).
          Join is idempotent. Contains no D3D calls.
 ===========================================================================**/
#pragma once

#include <Windows.h>

void RenderThread_Start();
void RenderThread_Stop();
bool RenderThread_IsRunning();

// Queue work on the render thread. No-op thread: runs work inline on the caller.
// Caller must Join before the next Kick (and before touching the D3D context).
void RenderThread_Kick(void (*work)());

// Block until idle. Safe to call when not running, already idle, or nested
// (e.g. WndProc during MsgWait).
void RenderThread_Join();

bool RenderThread_IsBusy();

// Manual-reset event, signaled when idle. Null if the thread is not running.
HANDLE RenderThread_DoneEvent();
