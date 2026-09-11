/**===========================================================================
 File   : job_system.h
 Brief  : Minimal frame-level worker-pool scheduler (phase A).
          Submit only from the main thread; no nested Submit from workers.
 ===========================================================================**/
#pragma once

#include <cstdint>
#include <functional>

using JobHandle = std::uint64_t;

void JobSystem_Init(int workerCount);
void JobSystem_Shutdown();

JobHandle JobSystem_Submit(std::function<void()> job);

// Main thread only. Blocks until every job of the current batch finished.
void JobSystem_WaitAll();

bool JobSystem_UseParallel();
void JobSystem_SetUseParallel(bool enabled);

// Editor-only debug overlay (compiled out of GAME_RUNTIME / deploy).
void JobSystem_SetDebugWindowVisible(bool visible);
bool JobSystem_IsDebugWindowVisible();
void JobSystem_ToggleDebugWindow();
void JobSystem_DebugDraw();
