/**===========================================================================
 File   :  [third_person_camera.cpp]
 Brief  :  Follow camera writes PlayerFollowCam node; Director applies global.
           Pull/Push soft-face the magnet target; Orbit hard-locks look.
 ===========================================================================**/
#include "third_person_camera.h"
#include "camera_pose.h"
#include "engine/input/mouse.h"
#include "engine/input/controller.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/core/easing.h"
#include <cmath>
#include <cstdio>
#include <memory>

using namespace DirectX;

namespace {

constexpr float kBaseDistance = 8.0f;
constexpr float kBaseHeight = 3.0f;
constexpr float kBaseFov = 45.0f;
constexpr float kLookAtHeight = 1.5f;
constexpr float kPullDistanceScale = 0.95f;
constexpr float kOrbitDistance = 13.0f;
constexpr float kOrbitPitchBias = 12.0f;
constexpr float kPushFovBump = 5.0f;
constexpr float kPushLagDuration = 0.20f;
constexpr float kParamBlendSpeed = 6.0f;
constexpr float kNormalFollowSharpness = 14.0f;
constexpr float kPullFollowSharpness = 40.0f;
constexpr float kOrbitFollowSharpness = 10.0f;
constexpr float kPushFollowSharpnessStart = 2.5f;
constexpr float kPushFollowSharpnessEnd = 18.0f;
// Soft-turn yaw/pitch toward the pull/push target while held.
constexpr float kMagnetFaceSharpness = 10.0f;
constexpr float kMagnetFacePitchBias = 12.0f;

constexpr float kTraumaDecayRate = 1.4f;
constexpr float kTraumaPosAmp = 0.25f;
constexpr float kTraumaRollDeg = 3.0f;
constexpr float kSpeedFovBoost = 18.0f;
constexpr float kSpeedFovSmooth = 6.0f;
constexpr float kFovEpsilon = 0.01f;
constexpr float kTestCameraPitch = 15.0f;

XMFLOAT3* s_targetPos = nullptr;
float*    s_targetYaw = nullptr;
float s_distance = kBaseDistance;
float s_height = kBaseHeight;
float s_lookAtHeight = kLookAtHeight;
float s_pitch = 15.0f;
float s_mouseSensitivity = 0.2f;
float s_controllerSensitivity = 120.0f;
bool  s_cursorHidden = false;
float s_minPitch = -89.0f;
float s_maxPitch = 89.0f;

ThirdPersonMagnetMode s_magnetMode = ThirdPersonMagnetMode::Normal;
ThirdPersonMagnetMode s_prevMagnetMode = ThirdPersonMagnetMode::Normal;
XMFLOAT3 s_magnetTarget = { 0.0f, 0.0f, 0.0f };
bool s_hasMagnetTarget = false;

float s_currentDistance = kBaseDistance;
float s_currentFov = kBaseFov;
float s_pushBurstT = 1.0f;
bool  s_hasSmoothedEye = false;
XMFLOAT3 s_smoothedEye = { 0.0f, 0.0f, 0.0f };

bool s_orbitLocked = false;
float s_orbitLockYaw = 0.0f;
float s_orbitLockPitch = 15.0f;
float s_preOrbitPitch = 15.0f;

bool s_hasFollowPose = false;
CameraPose s_followPose;

float s_trauma = 0.0f;
float s_traumaShakeTime = 0.0f;
float s_speedFactor01 = 0.0f;
float s_speedFovCurrent = kBaseFov;

std::unique_ptr<GameObject> s_followCamNode;

#if defined(_DEBUG) && defined(GAME_RUNTIME)
static bool CrashTraceEnabled() {
	char val[4]{};
	return GetEnvironmentVariableA("IVY_RUNTIME_TEST_CRASH_TRACE", val, 4) > 0 && val[0] == '1';
}

struct CameraStaticDestructionMarker {
	~CameraStaticDestructionMarker() {
		if (CrashTraceEnabled()) {
			fprintf(stderr, "[IVY_CRASH_TRACE] third_person_camera.cpp: static destruction reached\n");
			fflush(stderr);
		}
	}
};
static CameraStaticDestructionMarker s_cameraStaticMarker;

bool FixedCameraRequested() {
	static const bool requested = [] {
		char value[8]{};
		const DWORD len = GetEnvironmentVariableA("IVY_RUNTIME_TEST_FIXED_CAMERA", value, 8);
		const bool req = len == 1 && value[0] == '1';
		if (CrashTraceEnabled()) {
			fprintf(stderr, "[IVY_CRASH_TRACE] third_person_camera: FixedCameraRequested=%d\n", req ? 1 : 0);
			fflush(stderr);
		}
		return req;
	}();
	return requested;
}
#endif
#if 0
bool FixedCameraRequested_Original() {
	static const bool requested = [] {
		char value[8]{};
		const DWORD len = GetEnvironmentVariableA("IVY_RUNTIME_TEST_FIXED_CAMERA", value, 8);
		return len == 1 && value[0] == '1';
	}();
	return requested;
}
#endif


float Clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

float LerpFloat(float a, float b, float t) {
	return a + (b - a) * t;
}

float ExpSmoothToward(float current, float target, float sharpness, float dt) {
	const float t = 1.0f - expf(-sharpness * dt);
	return LerpFloat(current, target, t);
}

XMFLOAT3 ExpSmoothToward(const XMFLOAT3& current, const XMFLOAT3& target, float sharpness, float dt) {
	const float t = 1.0f - expf(-sharpness * dt);
	return {
		LerpFloat(current.x, target.x, t),
		LerpFloat(current.y, target.y, t),
		LerpFloat(current.z, target.z, t),
	};
}

float NormalizeYawDeg(float yaw) {
	while (yaw >= 360.0f) yaw -= 360.0f;
	while (yaw < 0.0f) yaw += 360.0f;
	return yaw;
}

float ShortestAngleDeltaDeg(float from, float to) {
	float d = to - from;
	while (d > 180.0f) d -= 360.0f;
	while (d < -180.0f) d += 360.0f;
	return d;
}

float ExpSmoothAngleToward(float current, float target, float sharpness, float dt) {
	const float t = 1.0f - expf(-sharpness * dt);
	return NormalizeYawDeg(current + ShortestAngleDeltaDeg(current, target) * t);
}

bool IsPullOrPushMode(ThirdPersonMagnetMode mode) {
	return mode == ThirdPersonMagnetMode::Pull || mode == ThirdPersonMagnetMode::Push;
}

void DesiredFaceAnglesToTarget(const XMFLOAT3& player, const XMFLOAT3& target,
	float& outYawDeg, float& outPitchDeg) {
	const float dx = target.x - player.x;
	const float dy = target.y - (player.y + s_lookAtHeight);
	const float dz = target.z - player.z;
	const float horiz = sqrtf(dx * dx + dz * dz);

	outYawDeg = NormalizeYawDeg(XMConvertToDegrees(atan2f(dx, dz)));
	if (horiz > 0.05f) {
		const float elevDeg = XMConvertToDegrees(atan2f(dy, horiz));
		outPitchDeg = Clampf(-elevDeg + kMagnetFacePitchBias, s_minPitch, s_maxPitch);
	} else {
		outPitchDeg = Clampf(kMagnetFacePitchBias, s_minPitch, s_maxPitch);
	}
}

void EnsureFollowCamNode() {
	if (!s_followCamNode) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
		if (CrashTraceEnabled()) {
			fprintf(stderr, "[IVY_CRASH_TRACE] third_person_camera: creating PlayerFollowCam node\n");
			fflush(stderr);
		}
#endif
		s_followCamNode = std::make_unique<GameObject>("PlayerFollowCam");
		s_followCamNode->AddComponent<engine::Transform>();
		s_followCamNode->AddComponent<CameraComponent>();
	}
	if (!s_followCamNode->GetComponent<AudioListenerComponent>())
		s_followCamNode->AddComponent<AudioListenerComponent>();
}

void WriteFollowPose(const CameraPose& pose) {
	EnsureFollowCamNode();
	auto* transform = s_followCamNode->GetComponent<engine::Transform>();
	auto* cam = s_followCamNode->GetComponent<CameraComponent>();
	CameraPose_WriteToNode(transform, cam, pose);
	s_followPose = pose;
	s_hasFollowPose = true;
}

float DesiredDistanceForMode(ThirdPersonMagnetMode mode) {
	switch (mode) {
	case ThirdPersonMagnetMode::Pull:  return kBaseDistance * kPullDistanceScale;
	case ThirdPersonMagnetMode::Orbit: return kOrbitDistance;
	case ThirdPersonMagnetMode::Push:
	case ThirdPersonMagnetMode::Normal:
	default:                           return kBaseDistance;
	}
}

float DesiredFovForMode(ThirdPersonMagnetMode mode) {
	if (mode == ThirdPersonMagnetMode::Push && s_pushBurstT < 1.0f)
		return kBaseFov + kPushFovBump * (1.0f - s_pushBurstT);
	return kBaseFov;
}

float FollowSharpnessForMode(ThirdPersonMagnetMode mode) {
	switch (mode) {
	case ThirdPersonMagnetMode::Pull:
		return kPullFollowSharpness;
	case ThirdPersonMagnetMode::Orbit:
		return kOrbitFollowSharpness;
	case ThirdPersonMagnetMode::Push: {
		const float t = Clampf(s_pushBurstT, 0.0f, 1.0f);
		if (t < 1.0f) {
			const float eased = static_cast<float>(easeOutCubic(t));
			return LerpFloat(kPushFollowSharpnessStart, kPushFollowSharpnessEnd, eased);
		}
		// Still slightly laggy while push held after burst recovers.
		return kPushFollowSharpnessEnd * 0.75f;
	}
	case ThirdPersonMagnetMode::Normal:
	default:
		return kNormalFollowSharpness;
	}
}

void OnMagnetModeChanged(ThirdPersonMagnetMode prev, ThirdPersonMagnetMode next) {
	if (next == ThirdPersonMagnetMode::Push && prev != ThirdPersonMagnetMode::Push)
		s_pushBurstT = 0.0f;

	if (next == ThirdPersonMagnetMode::Orbit && prev != ThirdPersonMagnetMode::Orbit) {
		s_orbitLocked = true;
		s_orbitLockYaw = s_targetYaw ? *s_targetYaw : 0.0f;
		s_preOrbitPitch = s_pitch;
		s_orbitLockPitch = Clampf(s_pitch + kOrbitPitchBias, s_minPitch, s_maxPitch);
	}

	if (prev == ThirdPersonMagnetMode::Orbit && next != ThirdPersonMagnetMode::Orbit) {
		if (s_orbitLocked) {
			if (s_targetYaw)
				*s_targetYaw = s_orbitLockYaw;
			s_pitch = s_preOrbitPitch;
		}
		s_orbitLocked = false;
	}
}

} // namespace

void ThirdPersonCamera_Initialize() {
	s_targetPos = nullptr;
	s_targetYaw = nullptr;
	s_pitch = 15.0f;
	s_minPitch = -89.0f;
	s_maxPitch = 89.0f;
	s_magnetMode = ThirdPersonMagnetMode::Normal;
	s_prevMagnetMode = ThirdPersonMagnetMode::Normal;
	s_hasMagnetTarget = false;
	s_currentDistance = kBaseDistance;
	s_currentFov = kBaseFov;
	s_pushBurstT = 1.0f;
	s_hasSmoothedEye = false;
	s_orbitLocked = false;
	s_hasFollowPose = false;
	s_distance = kBaseDistance;
	s_height = kBaseHeight;
	s_lookAtHeight = kLookAtHeight;
	s_trauma = 0.0f;
	s_traumaShakeTime = 0.0f;
	s_speedFactor01 = 0.0f;
	s_speedFovCurrent = kBaseFov;
}

void ThirdPersonCamera_Shutdown() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
	if (CrashTraceEnabled()) {
		fprintf(stderr, "[IVY_CRASH_TRACE] third_person_camera: ThirdPersonCamera_Shutdown called (s_followCamNode=%s)\n",
			s_followCamNode ? "active" : "null");
		fflush(stderr);
	}
#endif
	s_targetPos = nullptr;
	s_targetYaw = nullptr;
	s_followCamNode.reset();
	s_hasFollowPose = false;
	s_hasSmoothedEye = false;
	s_smoothedEye = { 0.0f, 0.0f, 0.0f };
	s_orbitLocked = false;
	s_orbitLockYaw = 0.0f;
	s_orbitLockPitch = 15.0f;
	s_preOrbitPitch = 15.0f;
	s_trauma = 0.0f;
	s_traumaShakeTime = 0.0f;
	s_speedFactor01 = 0.0f;
	s_speedFovCurrent = kBaseFov;
	s_pitch = 15.0f;
	s_currentDistance = kBaseDistance;
	s_currentFov = kBaseFov;
	s_pushBurstT = 1.0f;
	s_hasMagnetTarget = false;
	s_magnetTarget = { 0.0f, 0.0f, 0.0f };
	s_magnetMode = ThirdPersonMagnetMode::Normal;
	s_prevMagnetMode = ThirdPersonMagnetMode::Normal;
}

void ThirdPersonCamera_Update(double elapsed_time) {
	if (!s_targetPos || !s_targetYaw)
		return;

	HWND hWnd = GetWindowHandle();
	const float dt = static_cast<float>(elapsed_time);
	if (dt <= 0.0f)
		return;

	if (s_magnetMode != s_prevMagnetMode) {
		OnMagnetModeChanged(s_prevMagnetMode, s_magnetMode);
		s_prevMagnetMode = s_magnetMode;
	}

	const bool orbitLookLocked = (s_magnetMode == ThirdPersonMagnetMode::Orbit && s_orbitLocked);
	const bool magnetFaceLocked = IsPullOrPushMode(s_magnetMode) && s_hasMagnetTarget;

	const bool fixedCamera =
#if defined(_DEBUG) && defined(GAME_RUNTIME)
		FixedCameraRequested();
#else
		false;
#endif

	int dx = 0, dy = 0;
	if (fixedCamera) {
		s_pitch = kTestCameraPitch;
#if defined(_DEBUG) && defined(GAME_RUNTIME)
		if (Mouse_IsInjectionActive()) {
			Mouse_GetDelta(dx, dy);
		}
#endif
	} else if (s_cursorHidden && hWnd) {
		const int centerX = (int)(GetClientWidth() * 0.5f);
		const int centerY = (int)(GetClientHeight() * 0.5f);

		int clientX = 0;
		int clientY = 0;
		Mouse_GetClientPosition(clientX, clientY);

		dx = clientX - centerX;
		dy = clientY - centerY;

		POINT screenCenter = { centerX, centerY };
		ClientToScreen(hWnd, &screenCenter);
		SetCursorPos(screenCenter.x, screenCenter.y);
	}

	if (!fixedCamera) { int unused_dx, unused_dy; Mouse_GetDelta(unused_dx, unused_dy); }

	if (magnetFaceLocked) {
		float desiredYaw = 0.0f;
		float desiredPitch = s_pitch;
		DesiredFaceAnglesToTarget(*s_targetPos, s_magnetTarget, desiredYaw, desiredPitch);
		*s_targetYaw = ExpSmoothAngleToward(*s_targetYaw, desiredYaw, kMagnetFaceSharpness, dt);
		s_pitch = ExpSmoothToward(s_pitch, desiredPitch, kMagnetFaceSharpness, dt);
		(void)Controller_GetRightStick();
	} else if (!orbitLookLocked) {
		float currentYaw = *s_targetYaw;
		currentYaw += dx * s_mouseSensitivity;
		*s_targetYaw = NormalizeYawDeg(currentYaw);

		s_pitch += dy * s_mouseSensitivity;

		XMFLOAT2 rightStick = Controller_GetRightStick();
		if (!fixedCamera) {
			*s_targetYaw = NormalizeYawDeg(*s_targetYaw + rightStick.x * s_controllerSensitivity * dt);
			s_pitch -= rightStick.y * s_controllerSensitivity * dt;
		}
		s_pitch -= rightStick.y * s_controllerSensitivity * dt;

		if (s_pitch < s_minPitch) s_pitch = s_minPitch;
		if (s_pitch > s_maxPitch) s_pitch = s_maxPitch;
	} else {
		// Drain stick/mouse so they don't accumulate into other systems oddly.
		(void)Controller_GetRightStick();
	}

	s_pushBurstT = Clampf(s_pushBurstT + dt / kPushLagDuration, 0.0f, 1.0f);

	const float desiredDistance = DesiredDistanceForMode(s_magnetMode);
	const float desiredFov = DesiredFovForMode(s_magnetMode);
	s_currentDistance = ExpSmoothToward(s_currentDistance, desiredDistance, kParamBlendSpeed, dt);
	s_currentFov = ExpSmoothToward(s_currentFov, desiredFov, kParamBlendSpeed, dt);

	XMFLOAT3 playerPos = *s_targetPos;
	float yawDeg = orbitLookLocked ? s_orbitLockYaw : *s_targetYaw;
	float pitchDeg = orbitLookLocked ? s_orbitLockPitch : s_pitch;

	float yawRad = XMConvertToRadians(yawDeg);
	float pitchRad = XMConvertToRadians(pitchDeg);

	float dirX = -sinf(yawRad) * cosf(pitchRad);
	float dirY = sinf(pitchRad);
	float dirZ = -cosf(yawRad) * cosf(pitchRad);

	float safeDistance = fmaxf(0.5f, s_currentDistance - 0.3f);
	XMFLOAT3 headPos = { playerPos.x, playerPos.y + s_height, playerPos.z };

	XMFLOAT3 idealEye;
	idealEye.x = headPos.x + dirX * safeDistance;
	idealEye.y = headPos.y + dirY * safeDistance;
	idealEye.z = headPos.z + dirZ * safeDistance;

	XMFLOAT3 lookAt = { playerPos.x, playerPos.y + s_lookAtHeight, playerPos.z };

	if (!s_hasSmoothedEye) {
		s_smoothedEye = idealEye;
		s_hasSmoothedEye = true;
	} else {
		const float sharpness = FollowSharpnessForMode(s_magnetMode);
		s_smoothedEye = ExpSmoothToward(s_smoothedEye, idealEye, sharpness, dt);
	}

	CameraPose pose;
	pose.eye = s_smoothedEye;
	pose.lookAt = lookAt;
	pose.up = { 0.0f, 1.0f, 0.0f };
	pose.fov = s_currentFov;
	pose.nearClip = 0.5f;
	pose.farClip = 1000.0f;

	// Trauma shake — final local offset only; never feeds back into follow state.
	s_traumaShakeTime += dt;
	if (s_trauma > 0.0f) {
		s_trauma = fmaxf(0.0f, s_trauma - kTraumaDecayRate * dt);
		const float shake = s_trauma * s_trauma;
		const float t = s_traumaShakeTime;

		XMVECTOR eye = XMLoadFloat3(&pose.eye);
		XMVECTOR at = XMLoadFloat3(&pose.lookAt);
		XMVECTOR up = XMLoadFloat3(&pose.up);
		XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(at, eye));
		XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
		XMVECTOR camUp = XMVector3Normalize(XMVector3Cross(forward, right));

		const float ox = sinf(t * 37.1f) * (kTraumaPosAmp * shake);
		const float oy = sinf(t * 23.7f + 1.3f) * (kTraumaPosAmp * shake);
		eye = XMVectorAdd(eye, XMVectorScale(right, ox));
		eye = XMVectorAdd(eye, XMVectorScale(camUp, oy));
		XMStoreFloat3(&pose.eye, eye);

		const float rollRad = XMConvertToRadians(kTraumaRollDeg * shake * sinf(t * 31.3f));
		XMMATRIX roll = XMMatrixRotationAxis(forward, rollRad);
		camUp = XMVector3TransformNormal(camUp, roll);
		XMStoreFloat3(&pose.up, XMVector3Normalize(camUp));
	}

	// Speed FOV — additive boost on top of push/base FOV; skip write if unchanged.
	const float targetFov = s_currentFov + kSpeedFovBoost * s_speedFactor01;
	s_speedFovCurrent = ExpSmoothToward(s_speedFovCurrent, targetFov, kSpeedFovSmooth, dt);
	if (fabsf(pose.fov - s_speedFovCurrent) > kFovEpsilon)
		pose.fov = s_speedFovCurrent;

	WriteFollowPose(pose);
}

void ThirdPersonCamera_SetTarget(XMFLOAT3* positionPtr, float* yawPtr) {
	s_targetPos = positionPtr;
	s_targetYaw = yawPtr;
	s_hasSmoothedEye = false;
	s_hasFollowPose = false;
}

void ThirdPersonCamera_SetDistance(float distance) {
	s_distance = distance;
}

void ThirdPersonCamera_SetHeight(float height) {
	s_height = height;
}

void ThirdPersonCamera_SetLookAtHeight(float height) {
	s_lookAtHeight = height;
}

void ThirdPersonCamera_SetMouseSensitivity(float sensitivity) {
	s_mouseSensitivity = sensitivity;
}

void ThirdPersonCamera_SetControllerSensitivity(float sensitivity) {
	s_controllerSensitivity = sensitivity;
}

void ThirdPersonCamera_SetPitchLimits(float minPitch, float maxPitch) {
	s_minPitch = minPitch;
	s_maxPitch = maxPitch;
}

float ThirdPersonCamera_GetPitch() {
	return s_pitch;
}

bool ThirdPersonCamera_IsCursorVisible() {
	return !s_cursorHidden;
}

void ThirdPersonCamera_ShowCursor(bool show) {
	if (show && s_cursorHidden) {
		while (ShowCursor(TRUE) < 0) {}
		s_cursorHidden = false;
	} else if (!show && !s_cursorHidden) {
		while (ShowCursor(FALSE) >= 0) {}
		s_cursorHidden = true;
	}
}

void ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode mode) {
	s_magnetMode = mode;
}

void ThirdPersonCamera_SetMagnetTarget(const XMFLOAT3& targetPos) {
	s_magnetTarget = targetPos;
	s_hasMagnetTarget = true;
}

bool ThirdPersonCamera_TryGetFollowPose(CameraPose& outPose) {
	if (!s_hasFollowPose)
		return false;
	outPose = s_followPose;
	return true;
}

GameObject* ThirdPersonCamera_GetFollowCamNode() {
	return s_followCamNode.get();
}

void ThirdPersonCamera_AddTrauma(float amount01) {
	s_trauma = Clampf(s_trauma + Clampf(amount01, 0.0f, 1.0f), 0.0f, 1.0f);
}

float ThirdPersonCamera_GetTrauma() {
	return s_trauma;
}

void ThirdPersonCamera_SetSpeedFactor(float speed01) {
	s_speedFactor01 = Clampf(speed01, 0.0f, 1.0f);
}
