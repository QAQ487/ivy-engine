/**===========================================================================
 File   : debug_system.cpp
 Brief  : Global debug controls — collision viz, time scale, frame step.
 ===========================================================================**/
#include "debug_system.h"

#include "engine/physics/core/physics_system.h"
#include "engine/rendering/culling/frustum_cull.h"
#include "engine/rendering/culling/shadow_cascade_cull.h"
#include "engine/rendering/culling/sprite_cull.h"
#include "engine/rendering/culling/lod_state.h"
#include "third_person_camera.h"

#include <cstdio>
#include <cstring>

namespace {

float s_timeScale = 1.0f;
bool s_collisionViz = false;
bool s_frameStepRequest = false;

bool s_cursorBeforeFreeze = true;
bool s_holdingCursorForFreeze = false;

char s_overlayText[512] = {};
DirectX::XMFLOAT4 s_overlayColor = { 0.85f, 0.95f, 0.85f, 1.0f };

struct TimeScalePulseState {
	bool active = false;
	float scale = 1.0f;
	float remainingMs = 0.0f;
};
TimeScalePulseState s_pulse;

void ClearPulse() {
	s_pulse.active = false;
	s_pulse.scale = 1.0f;
	s_pulse.remainingMs = 0.0f;
}

void ApplyFreezeCursor(bool enteringFreeze) {
	if (enteringFreeze) {
		if (!s_holdingCursorForFreeze) {
			s_cursorBeforeFreeze = ThirdPersonCamera_IsCursorVisible();
			ThirdPersonCamera_ShowCursor(true);
			s_holdingCursorForFreeze = true;
		}
	} else if (s_holdingCursorForFreeze) {
		ThirdPersonCamera_ShowCursor(s_cursorBeforeFreeze);
		s_holdingCursorForFreeze = false;
	}
}

} // namespace

namespace DebugSystem {

void Init() {
	s_timeScale = 1.0f;
	s_collisionViz = false;
	s_frameStepRequest = false;
	s_holdingCursorForFreeze = false;
	s_overlayText[0] = '\0';
	s_overlayColor = { 0.85f, 0.95f, 0.85f, 1.0f };
	ClearPulse();
}

void Shutdown() {
	if (s_holdingCursorForFreeze) {
		ThirdPersonCamera_ShowCursor(s_cursorBeforeFreeze);
		s_holdingCursorForFreeze = false;
	}
	s_frameStepRequest = false;
	s_overlayText[0] = '\0';
}

void ToggleCollisionViz() {
	s_collisionViz = !s_collisionViz;
}

bool CollisionVizEnabled() {
	return s_collisionViz;
}

float GetTimeScale() {
	if (s_pulse.active && s_timeScale >= 0.999f)
		return s_pulse.scale;
	return s_timeScale;
}

void PulseTimeScale(float scale, unsigned durationMs) {
	// Debug F7 time-scale always wins — ignore pulse when not at normal 1.0.
	if (s_timeScale < 0.999f)
		return;
	if (durationMs == 0)
		return;

	if (scale < 0.0f) scale = 0.0f;
	if (scale > 1.0f) scale = 1.0f;

	s_pulse.active = true;
	s_pulse.scale = scale;
	s_pulse.remainingMs = static_cast<float>(durationMs);
}

void UpdateTimeScalePulse(float realDeltaMs) {
	if (!s_pulse.active)
		return;
	if (realDeltaMs < 0.0f)
		realDeltaMs = 0.0f;
	s_pulse.remainingMs -= realDeltaMs;
	if (s_pulse.remainingMs <= 0.0f)
		ClearPulse();
}

void CycleTimeScale() {
	const bool wasFrozen = (s_timeScale == 0.0f);
	if (s_timeScale >= 0.999f)
		s_timeScale = 0.5f;
	else if (s_timeScale >= 0.49f)
		s_timeScale = 0.25f;
	else if (s_timeScale >= 0.24f)
		s_timeScale = 0.0f;
	else
		s_timeScale = 1.0f;

	const bool nowFrozen = (s_timeScale == 0.0f);
	if (nowFrozen && !wasFrozen)
		ApplyFreezeCursor(true);
	else if (!nowFrozen && wasFrozen)
		ApplyFreezeCursor(false);
}

bool IsFrozen() {
	return s_timeScale == 0.0f;
}

void StepOneFrame() {
	if (IsFrozen())
		s_frameStepRequest = true;
}

bool FrameStepRequested() {
	return s_frameStepRequest;
}

float StepAdvance() {
	if (!s_frameStepRequest)
		return 0.0f;
	s_frameStepRequest = false;
	return 1.0f / 60.0f;
}

bool ConsumeFrameStep() {
	if (!s_frameStepRequest)
		return false;
	s_frameStepRequest = false;
	return true;
}

void SetOverlayText(const char* text) {
	if (!text) {
		s_overlayText[0] = '\0';
		return;
	}
	std::snprintf(s_overlayText, sizeof(s_overlayText), "%s", text);
}

const char* GetOverlayText() {
	return s_overlayText;
}

DirectX::XMFLOAT4 GetOverlayColor() {
	return s_overlayColor;
}

void RefreshOverlay() {
	// Keep the viewport clean by default. Only show status when debug state is
	// non-default (viz on or time scale changed from 1.0x).
	if (!s_collisionViz && s_timeScale >= 0.999f) {
		s_overlayText[0] = '\0';
		s_overlayColor = { 0.85f, 0.95f, 0.85f, 1.0f };
		return;
	}

	const char* viz = s_collisionViz ? "ON" : "OFF";
	char timeBuf[32];
	if (s_timeScale == 0.0f)
		std::snprintf(timeBuf, sizeof(timeBuf), "0.0x");
	else if (s_timeScale >= 0.999f)
		std::snprintf(timeBuf, sizeof(timeBuf), "1.0x");
	else if (s_timeScale >= 0.49f)
		std::snprintf(timeBuf, sizeof(timeBuf), "0.5x");
	else
		std::snprintf(timeBuf, sizeof(timeBuf), "0.25x");

	const FrustumCullStats cull = FrustumCull_GetLastStats();
	const ShadowCascadeCullStats shadowCull = ShadowCascadeCull_GetLastStats();
	const SpriteCullStats spriteCull = SpriteCull_GetLastStats();
	const LodStats lodStats = Lod_GetLastStats();
	int lodFar = 0;
	for (int level = 2; level < kLodMaxLevels; ++level)
		lodFar += lodStats.drawn[level];
	if (s_collisionViz) {
		const PhysicsDebugStats st = PhysicsSystem_GetDebugStats();
		std::snprintf(s_overlayText, sizeof(s_overlayText),
			"COLLISION VIZ %s   TIME %s\n"
			"colliders:%d pairs:%d contacts:%d triggers:%d\n"
			"frustum considered:%d culled:%d drawn:%d\n"
			"shadow considered:%d culled:%d drawn:%d\n"
			"sprite considered:%d culled:%d drawn:%d\n"
			"lod considered:%d culled:%d l0:%d l1:%d l2+:%d",
			viz, timeBuf, st.colliders, st.pairs, st.contacts, st.triggers,
			cull.considered, cull.culled, cull.drawn,
			shadowCull.considered, shadowCull.culled, shadowCull.drawn,
			spriteCull.considered, spriteCull.culled, spriteCull.drawn,
			lodStats.considered, lodStats.culledByDistance,
			lodStats.drawn[0], lodStats.drawn[1], lodFar);
	} else {
		std::snprintf(s_overlayText, sizeof(s_overlayText),
			"COLLISION VIZ %s   TIME %s",
			viz, timeBuf);
	}

	if (IsFrozen())
		s_overlayColor = { 1.0f, 0.2f, 0.2f, 1.0f };
	else
		s_overlayColor = { 0.85f, 0.95f, 0.85f, 1.0f };
}

void ResetTimeScale() {
	const bool wasFrozen = (s_timeScale == 0.0f);
	s_timeScale = 1.0f;
	s_frameStepRequest = false;
	ClearPulse();
	if (wasFrozen)
		ApplyFreezeCursor(false);
}

} // namespace DebugSystem
