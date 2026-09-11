/**===========================================================================
 File   : job_system.h
 Brief  : Frame-level worker-pool scheduler with a dependency graph (phase B).
          Submit* only from the main thread; no nested Submit from workers.

 HANDLE LIFETIME (loud): JobHandles are FRAME-SCOPED. After JobSystem_WaitAll()
 returns, all handles from that frame are invalid - do not store them across
 frames. Null means already-done / invalid (sequential fallback).
 ===========================================================================**/
#pragma once

#include <functional>
#include <vector>

struct JobNode; // opaque
using JobHandle = JobNode*; // null = done/invalid; FRAME-SCOPED - see brief above

void JobSystem_Init(int workerCount);
void JobSystem_Shutdown();

// Zero deps. Inline when UseParallel()==false or not inited.
JobHandle JobSystem_Submit(std::function<void()> job);

// deps = handles returned earlier this frame (already-completed deps are fine).
JobHandle JobSystem_SubmitDep(std::function<void()> job,
	const std::vector<JobHandle>& deps);

// Convenience single-dep overload.
JobHandle JobSystem_SubmitAfter(JobHandle after, std::function<void()> job);

// Main thread: block until this node is done (no-op if null / already done).
void JobSystem_WaitFor(JobHandle h);

// Main thread: frame fence - block until ALL nodes of the current frame are
// done, then recycle node storage. Invalidates every handle from this frame.
void JobSystem_WaitAll();

bool JobSystem_UseParallel();
void JobSystem_SetUseParallel(bool enabled);

// Editor-only debug overlay (compiled out of GAME_RUNTIME / deploy).
void JobSystem_SetDebugWindowVisible(bool visible);
bool JobSystem_IsDebugWindowVisible();
void JobSystem_ToggleDebugWindow();
void JobSystem_DebugDraw();
