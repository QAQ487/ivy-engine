/**===========================================================================
 File   : debug_system.cpp
 Brief  : Global debug controls — collision viz, time scale, frame step.
 ===========================================================================**/
#include "debug_system.h"

#include "engine/physics/core/physics_system.h"
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
	return s_timeScale;
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

	if (s_collisionViz) {
		const PhysicsDebugStats st = PhysicsSystem_GetDebugStats();
		std::snprintf(s_overlayText, sizeof(s_overlayText),
			"COLLISION VIZ %s   TIME %s\n"
			"colliders:%d pairs:%d contacts:%d triggers:%d",
			viz, timeBuf, st.colliders, st.pairs, st.contacts, st.triggers);
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
	if (wasFrozen)
		ApplyFreezeCursor(false);
}

} // namespace DebugSystem
