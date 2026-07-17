/**===========================================================================

 File   :  [debug_camera.cpp]

 Author : Hayato Otsuki
 Date   : 2025/11/4
 -----------------------------------------------------------------------------

 Maya-style camera controls implementation

 ===========================================================================**/
#include "debug_camera.h"
#include <DirectXMath.h>
#include "engine/input/keyboard.h"
#include "engine/input/key_logger.h"
#include "engine/input/mouse.h"
#include "camera.h"
#include "engine/core/engine_gui.h"
#ifndef GAME_RUNTIME
#include "imgui/imgui.h"
#endif

using namespace DirectX;

namespace {
	bool  s_enabled = false;
	DebugCameraControlMode s_controlMode = DebugCameraControlMode::Orbit;
	float s_yaw = 0.0f;
	float s_pitch = 0.0f;
	float s_distance = 10.0f;  // Distance from focal point
	XMFLOAT3 s_focalPoint = { 0.0f, 0.0f, 0.0f };  // Point to orbit around
	XMFLOAT3 s_flyPosition = { 0.0f, 3.0f, -6.0f };

	// Parameters
	float s_moveSpeed = 5.0f;       // units/sec
	float s_boostMult = 3.0f;       // Shift boost multiplier
	float s_sensDegPerPixel = 0.12f; // Mouse sensitivity (deg/px)
	float s_orbitSensitivity = 0.15f; // Orbit rotation sensitivity
	float s_panSensitivity = 0.00125f;  // Pan movement sensitivity
	float s_dollySensitivity = 0.05f; // Dolly/zoom sensitivity

	// Init flag
	bool s_inited = false;

	// Orbital state saved when the debug camera is disabled, so that camera
	// changes made by other systems (e.g. CameraComponent) while the debug
	// camera is off are not carried over when it is re-enabled.
	bool     s_hasSavedState   = false;
	float    s_savedYaw        = 0.0f;
	float    s_savedPitch      = 0.0f;
	float    s_savedDistance   = 10.0f;
	XMFLOAT3 s_savedFocalPoint = { 0.0f, 0.0f, 0.0f };

	// Initialize from current camera state
	void InitFromCameraOnce() {
		if (s_inited) return;
		s_inited = true;

		Camera& cam = GetCamera();
		XMFLOAT3 pos = cam.position;
		XMFLOAT3 at = cam.at;

		// Set focal point to camera's look-at target
		s_focalPoint = at;

		// Calculate distance from camera to focal point
		XMVECTOR vPos = XMLoadFloat3(&pos);
		XMVECTOR vAt = XMLoadFloat3(&at);
		XMVECTOR vDist = XMVectorSubtract(vPos, vAt);
		s_distance = XMVectorGetX(XMVector3Length(vDist));
		if (s_distance < 0.1f) s_distance = 10.0f; // Minimum distance

		// Calculate yaw and pitch from direction
		XMVECTOR vDir = XMVector3Normalize(XMVectorSubtract(vPos, vAt));
		XMFLOAT3 dir; XMStoreFloat3(&dir, vDir);

		// pitch
		float y = dir.y;
		if (y > 1.0f)       y = 1.0f;
		else if (y < -1.0f) y = -1.0f;
		s_pitch = asinf(y);

		float cp = cosf(s_pitch);
		if (fabsf(cp) < 1e-6f) cp = 1e-6f;

		// yaw
		float yaw = atan2f(dir.x / cp, dir.z / cp);
		s_yaw = yaw;
	}

	// Update camera position based on orbit parameters
	void UpdateCameraFromOrbit() {
		Camera& cam = GetCamera();

		// Calculate camera position based on yaw, pitch, and distance
		float cy = cosf(s_yaw), sy = sinf(s_yaw);
		float cp = cosf(s_pitch), sp = sinf(s_pitch);

		// Direction from focal point to camera (inverted orbit direction)
		XMFLOAT3 dir = { sy * cp, sp, cy * cp };

		// Camera position = focal point + direction * distance
		XMFLOAT3 pos;
		pos.x = s_focalPoint.x + dir.x * s_distance;
		pos.y = s_focalPoint.y + dir.y * s_distance;
		pos.z = s_focalPoint.z + dir.z * s_distance;

		cam.SetPosition(pos);
		cam.SetTarget(s_focalPoint);
		// Always restore the canonical world-up vector.  CameraComponent (and
		// other camera systems) may call cam.SetUp() with a tilted direction; if
		// we don't reset it here the view matrix ends up with unintended roll,
		// making orbit controls feel broken even when yaw/pitch are correct.
		cam.SetUp({ 0.0f, 1.0f, 0.0f });
	}

	XMFLOAT3 GetForward() {
		float cy = cosf(s_yaw), sy = sinf(s_yaw);
		float cp = cosf(s_pitch), sp = sinf(s_pitch);
		return XMFLOAT3(sy * cp, sp, cy * cp);
	}

	XMFLOAT3 GetRight() {
		XMFLOAT3 f = GetForward();
		XMVECTOR vf = XMVector3Normalize(XMVectorSet(f.x, f.y, f.z, 0));
		XMVECTOR vUp = XMVectorSet(0, 1, 0, 0);
		XMVECTOR vRight = XMVector3Normalize(XMVector3Cross(vUp, vf));
		XMFLOAT3 r; XMStoreFloat3(&r, vRight);
		return r;
	}

	void ApplyFlyViewToCamera() {
		Camera& cam = GetCamera();
		XMFLOAT3 forward = GetForward();
		XMFLOAT3 target = {
			s_flyPosition.x + forward.x,
			s_flyPosition.y + forward.y,
			s_flyPosition.z + forward.z
		};
		cam.SetPosition(s_flyPosition);
		cam.SetTarget(target);
		cam.SetUp({ 0.0f, 1.0f, 0.0f });
	}

	void UpdateFlyMode(double dt) {
		const float toRad = DirectX::XM_PI / 180.0f;
		const float limit = DirectX::XMConvertToRadians(89.0f);

		int dx = 0, dy = 0;
		Mouse_GetDelta(dx, dy);

		if (Mouse_IsRightDown()) {
			s_yaw += (dx * s_sensDegPerPixel) * toRad;
			s_pitch -= (dy * s_sensDegPerPixel) * toRad;
			if (s_pitch > limit) s_pitch = limit;
			if (s_pitch < -limit) s_pitch = -limit;
		}

		XMFLOAT3 forward = GetForward();
		XMFLOAT3 right = GetRight();
		const XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

		if (Mouse_IsRightDown()) {
			float speed = s_moveSpeed * static_cast<float>(dt);
			if (Keyboard_IsKeyDown(KK_LEFTSHIFT)) speed *= s_boostMult;

			if (Keyboard_IsKeyDown(KK_W)) {
				s_flyPosition.x += forward.x * speed;
				s_flyPosition.y += forward.y * speed;
				s_flyPosition.z += forward.z * speed;
			}
			if (Keyboard_IsKeyDown(KK_S)) {
				s_flyPosition.x -= forward.x * speed;
				s_flyPosition.y -= forward.y * speed;
				s_flyPosition.z -= forward.z * speed;
			}
			if (Keyboard_IsKeyDown(KK_A)) {
				s_flyPosition.x -= right.x * speed;
				s_flyPosition.z -= right.z * speed;
			}
			if (Keyboard_IsKeyDown(KK_D)) {
				s_flyPosition.x += right.x * speed;
				s_flyPosition.z += right.z * speed;
			}
			if (Keyboard_IsKeyDown(KK_SPACE)) {
				s_flyPosition.x += up.x * speed;
				s_flyPosition.y += up.y * speed;
				s_flyPosition.z += up.z * speed;
			}
			if (Keyboard_IsKeyDown(KK_LEFTCONTROL)) {
				s_flyPosition.x -= up.x * speed;
				s_flyPosition.y -= up.y * speed;
				s_flyPosition.z -= up.z * speed;
			}
		}

		ApplyFlyViewToCamera();
	}
}

bool DebugCamera_IsEnabled() { return s_enabled; }

void DebugCamera_SetControlMode(DebugCameraControlMode mode) {
	s_controlMode = mode;
	s_inited = true;
}

DebugCameraControlMode DebugCamera_GetControlMode() {
	return s_controlMode;
}

void DebugCamera_SetFlyView(XMFLOAT3 position, float yawDeg, float pitchDeg) {
	s_flyPosition = position;
	s_yaw = DirectX::XMConvertToRadians(yawDeg);
	s_pitch = DirectX::XMConvertToRadians(pitchDeg);
	s_controlMode = DebugCameraControlMode::Fly;
	s_inited = true;
	ApplyFlyViewToCamera();
}

void DebugCamera_SetEnabled(bool enabled) {
	if (enabled == s_enabled) return;

	if (!enabled) {
		// Ensure the orbital state has been initialised from the current camera
		// before we snapshot it.  If the user never Alt-dragged (s_inited == false),
		// s_yaw/s_pitch are still factory defaults (0, 0) which don't reflect the
		// actual camera orientation; calling InitFromCameraOnce() here guarantees
		// the saved state always matches the real pre-play view.
		if (!s_inited) InitFromCameraOnce();

		// Snapshot the current orbital state before another camera system
		// takes ownership of the global Camera object.
		s_savedYaw        = s_yaw;
		s_savedPitch      = s_pitch;
		s_savedDistance   = s_distance;
		s_savedFocalPoint = s_focalPoint;
		s_hasSavedState   = true;
	} else {
		// Restore the snapshotted orbital state so that camera changes made
		// while the debug camera was off (e.g. from CameraComponent) are
		// not carried over, then immediately apply the position to the Camera.
		if (s_hasSavedState) {
			s_yaw        = s_savedYaw;
			s_pitch      = s_savedPitch;
			s_distance   = s_savedDistance;
			s_focalPoint = s_savedFocalPoint;
			s_inited     = true;
			UpdateCameraFromOrbit();
		}
		// If no saved state exists (very first enable) leave s_inited = false so
		// InitFromCameraOnce() picks up the initial camera position on first use.
	}

	s_enabled = enabled;
}

void DebugCamera_SetMoveSpeed(float speed) { s_moveSpeed = speed; }
void DebugCamera_SetBoostMultiplier(float mult) { s_boostMult = mult; }
void DebugCamera_SetMouseSensitivity(float degPerPixel) { s_sensDegPerPixel = degPerPixel; }
void DebugCamera_SetOrbitSensitivity(float sensitivity) { s_orbitSensitivity = sensitivity; }
void DebugCamera_SetPanSensitivity(float sensitivity) { s_panSensitivity = sensitivity; }
void DebugCamera_SetDollySensitivity(float sensitivity) { s_dollySensitivity = sensitivity; }

void DebugCamera_SetFocalPoint(DirectX::XMFLOAT3 point)
{
	s_focalPoint = point;
}

void DebugCamera_SyncOrbitFromCurrentView()
{
	Camera& cam = GetCamera();
	const XMFLOAT3 pos = cam.position;
	const XMFLOAT3 at = cam.at;

	s_focalPoint = at;

	const XMVECTOR vPos = XMLoadFloat3(&pos);
	const XMVECTOR vAt = XMLoadFloat3(&at);
	const XMVECTOR vDist = XMVectorSubtract(vPos, vAt);
	s_distance = XMVectorGetX(XMVector3Length(vDist));
	if (s_distance < 0.1f) s_distance = 10.0f;

	const XMVECTOR vDir = XMVector3Normalize(XMVectorSubtract(vPos, vAt));
	XMFLOAT3 dir;
	XMStoreFloat3(&dir, vDir);

	float y = dir.y;
	if (y > 1.0f)       y = 1.0f;
	else if (y < -1.0f) y = -1.0f;
	s_pitch = asinf(y);

	float cp = cosf(s_pitch);
	if (fabsf(cp) < 1e-6f) cp = 1e-6f;
	s_yaw = atan2f(dir.x / cp, dir.z / cp);

	s_inited = true;
}

void DebugCamera_Update(double dt) {
	// F1 to toggle
	if (KeyLogger_IsTrigger(KK_F1)) {
		DebugCamera_SetEnabled(!s_enabled);
	}
	if (!s_enabled) return;

	// If the mouse is over an ImGui window (like your Hierarchy or Properties),
	// do not process camera rotation or zooming.
#ifndef GAME_RUNTIME
	const bool ignoreImGuiCapture = EngineGUI_IsDebugCameraPlayInputActive();
	if (!ignoreImGuiCapture && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	if (!ignoreImGuiCapture && ImGui::GetIO().WantCaptureKeyboard) {
		return; // Don't move camera if typing in a textbox
	}
#endif

	if (s_controlMode == DebugCameraControlMode::Fly) {
		UpdateFlyMode(dt);
		return;
	}

	Camera& cam = GetCamera();
	bool altPressed = Keyboard_IsKeyDown(KK_LEFTALT) || Keyboard_IsKeyDown(KK_RIGHTALT);

	int dx = 0, dy = 0;
	Mouse_GetDelta(dx, dy);

	// Maya-style controls with Alt key
	if (altPressed) {
		// Initialize camera state on first Alt interaction
		if (!s_inited && (Mouse_IsLeftDown() || Mouse_IsMiddleDown() || Mouse_IsRightDown())) {
			InitFromCameraOnce();
		}

		// Alt + Left Mouse: Orbit/Tumble
		if (Mouse_IsLeftDown()) {
			const float toRad = DirectX::XM_PI / 180.0f;
			s_yaw += (dx * s_orbitSensitivity) * toRad;
			s_pitch -= (dy * s_orbitSensitivity) * toRad * -1.0f;

			// Clamp pitch
			const float limit = DirectX::XMConvertToRadians(89.0f);
			if (s_pitch > limit) s_pitch = limit;
			if (s_pitch < -limit) s_pitch = -limit;

			UpdateCameraFromOrbit();
		}
		// Alt + Middle Mouse: Pan/Track
		else if (Mouse_IsMiddleDown()) {
			XMFLOAT3 right = GetRight();
			XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

			float panSpeed = s_panSensitivity * s_distance;
			s_focalPoint.x += right.x * dx * panSpeed;
			s_focalPoint.z += right.z * dx * panSpeed;
			s_focalPoint.y += dy * panSpeed;

			UpdateCameraFromOrbit();
		}
		// Alt + Right Mouse: Dolly/Zoom
		else if (Mouse_IsRightDown()) {
			float dollyAmount = dy * s_dollySensitivity;
			s_distance += dollyAmount;
			if (s_distance < 0.1f) s_distance = 0.1f;
			if (s_distance > 1000.0f) s_distance = 1000.0f;

			UpdateCameraFromOrbit();
		}
	}

	// Mouse wheel: Dolly/Zoom (works without Alt)
	float wheelDelta = Mouse_GetWheelDelta();
#ifndef GAME_RUNTIME
	if (wheelDelta != 0 && !ImGui::GetIO().WantCaptureMouse) {
#else
	if (wheelDelta != 0) {
#endif
		float dollyAmount = -(wheelDelta / 120.0f) * s_distance * 0.1f;
		s_distance += dollyAmount;
		if (s_distance < 0.1f) s_distance = 0.1f;
		if (s_distance > 1000.0f) s_distance = 1000.0f;

		UpdateCameraFromOrbit();
	}

	// Unity-style fly: RMB + WASD (+ Shift boost, Space/Ctrl up/down)
	if (Mouse_IsRightDown()) {
		XMFLOAT3 pos = cam.position;
		XMFLOAT3 f = GetForward();
		XMFLOAT3 r = GetRight();

		XMVECTOR vf_flat = XMVector3Normalize(XMVectorSet(f.x, 0, f.z, 0));
		XMFLOAT3 fFlat; XMStoreFloat3(&fFlat, vf_flat);

		float speed = s_moveSpeed * static_cast<float>(dt);
		if (Keyboard_IsKeyDown(KK_LEFTSHIFT)) speed *= s_boostMult;

		bool moved = false;
		if (Keyboard_IsKeyDown(KK_W)) { pos.x += fFlat.x * speed; pos.z += fFlat.z * speed; moved = true; }
		if (Keyboard_IsKeyDown(KK_S)) { pos.x -= fFlat.x * speed; pos.z -= fFlat.z * speed; moved = true; }
		if (Keyboard_IsKeyDown(KK_A)) { pos.x -= r.x * speed; pos.z -= r.z * speed; moved = true; }
		if (Keyboard_IsKeyDown(KK_D)) { pos.x += r.x * speed; pos.z += r.z * speed; moved = true; }
		if (Keyboard_IsKeyDown(KK_SPACE)) { pos.y += speed; moved = true; }
		if (Keyboard_IsKeyDown(KK_LEFTCONTROL)) { pos.y -= speed; moved = true; }

		if (moved) {
			XMVECTOR vOldPos = XMLoadFloat3(&cam.position);
			XMVECTOR vNewPos = XMLoadFloat3(&pos);
			XMVECTOR vDelta = XMVectorSubtract(vNewPos, vOldPos);

			XMVECTOR vFocal = XMLoadFloat3(&s_focalPoint);
			vFocal = XMVectorAdd(vFocal, vDelta);
			XMStoreFloat3(&s_focalPoint, vFocal);

			cam.SetPosition(pos);

			XMFLOAT3 fw = GetForward();
			XMFLOAT3 at = { pos.x + fw.x, pos.y + fw.y, pos.z + fw.z };
			cam.SetTarget(at);
		}
	}
}
