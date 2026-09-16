/**===========================================================================
 File   : debug_system.h
 Brief  : Main-thread global debug controls (collision viz, time scale, frame
          step, overlay text). GAME_RUNTIME-safe — no ImGui.
 ===========================================================================**/
#pragma once

#include <DirectXMath.h>

namespace DebugSystem {

void Init();
void Shutdown();

void ToggleCollisionViz();
bool CollisionVizEnabled();

float GetTimeScale();
void CycleTimeScale(); // 1.0 → 0.5 → 0.25 → 0.0 (freeze) → 1.0
bool IsFrozen();       // timeScale == 0

// main thread only. Freezes the world briefly for game-feel moments.
// scale: 0.0–1.0 (typical 0.05). durationMs: real-time duration.
// Ignored when F7 debug time-scale is not 1.0 (debug controls win).
void PulseTimeScale(float scale, unsigned durationMs);
// main thread only. Tick with unscaled wall-clock delta in milliseconds.
void UpdateTimeScalePulse(float realDeltaMs);

void StepOneFrame();   // request one Update while frozen
bool FrameStepRequested();
float StepAdvance();   // 1/60 if step pending (clears request), else 0
bool ConsumeFrameStep(); // true once if step pending, then clears

void SetOverlayText(const char* text);
const char* GetOverlayText();
DirectX::XMFLOAT4 GetOverlayColor(); // red when frozen

// Rebuild status / stats lines (call once per frame from main Update).
void RefreshOverlay();

// Clear freeze / time-scale when play ends or a scene load tears down play,
// so deferred EngineGUI scene flushes are not blocked by a frozen Update skip.
void ResetTimeScale();

} // namespace DebugSystem
