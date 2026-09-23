/**===========================================================================
 File   :  [first_person_camera.cpp]
 Brief  :  FPS eye pose from target pos/yaw; Director applies global camera.
 ===========================================================================**/
#include "first_person_camera.h"
#include "third_person_camera.h"
#include "camera_pose.h"
#include "engine/input/mouse.h"
#include "engine/input/controller.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/audio_listener_component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include <cmath>
#include <memory>

using namespace DirectX;

namespace {

constexpr float kDefaultEyeHeight = 0.7f;
constexpr float kDefaultFov = 70.0f;
constexpr float kLookDistance = 1.0f;

XMFLOAT3* s_targetPos = nullptr;
float*    s_targetYaw = nullptr;
float s_eyeHeight = kDefaultEyeHeight;
float s_pitch = 0.0f;
float s_mouseSensitivity = 0.15f;
float s_controllerSensitivity = 120.0f;
float s_minPitch = -89.0f;
float s_maxPitch = 89.0f;
float s_fov = kDefaultFov;

bool s_hasPose = false;
CameraPose s_pose;

std::unique_ptr<GameObject> s_fpCamNode;

float NormalizeYawDeg(float yaw) {
	while (yaw >= 360.0f) yaw -= 360.0f;
	while (yaw < 0.0f) yaw += 360.0f;
	return yaw;
}

void EnsureFpCamNode() {
	if (!s_fpCamNode) {
		s_fpCamNode = std::make_unique<GameObject>("PlayerFirstPersonCam");
		s_fpCamNode->AddComponent<engine::Transform>();
		s_fpCamNode->AddComponent<CameraComponent>();
	}
	if (!s_fpCamNode->GetComponent<AudioListenerComponent>())
		s_fpCamNode->AddComponent<AudioListenerComponent>();
}

void WritePose(const CameraPose& pose) {
	EnsureFpCamNode();
	auto* transform = s_fpCamNode->GetComponent<engine::Transform>();
	auto* cam = s_fpCamNode->GetComponent<CameraComponent>();
	CameraPose_WriteToNode(transform, cam, pose);
	s_pose = pose;
	s_hasPose = true;
}

} // namespace

void FirstPersonCamera_Initialize() {
	s_targetPos = nullptr;
	s_targetYaw = nullptr;
	s_pitch = 0.0f;
	s_minPitch = -89.0f;
	s_maxPitch = 89.0f;
	s_eyeHeight = kDefaultEyeHeight;
	s_fov = kDefaultFov;
	s_hasPose = false;
}

void FirstPersonCamera_Update(double elapsed_time) {
	if (!s_targetPos || !s_targetYaw)
		return;

	HWND hWnd = GetWindowHandle();
	const float dt = static_cast<float>(elapsed_time);
	if (dt <= 0.0f)
		return;

	int dx = 0, dy = 0;
	if (!ThirdPersonCamera_IsCursorVisible() && hWnd) {
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

	{ int unused_dx, unused_dy; Mouse_GetDelta(unused_dx, unused_dy); }

	float currentYaw = *s_targetYaw;
	currentYaw += dx * s_mouseSensitivity;
	*s_targetYaw = NormalizeYawDeg(currentYaw);

	s_pitch -= dy * s_mouseSensitivity;

	XMFLOAT2 rightStick = Controller_GetRightStick();
	*s_targetYaw = NormalizeYawDeg(*s_targetYaw + rightStick.x * s_controllerSensitivity * dt);
	s_pitch -= rightStick.y * s_controllerSensitivity * dt;

	if (s_pitch < s_minPitch) s_pitch = s_minPitch;
	if (s_pitch > s_maxPitch) s_pitch = s_maxPitch;

	const XMFLOAT3 playerPos = *s_targetPos;
	const float yawRad = XMConvertToRadians(*s_targetYaw);
	const float pitchRad = XMConvertToRadians(s_pitch);

	// Match TPS look basis: yaw 0 looks down -Z.
	const float dirX = -sinf(yawRad) * cosf(pitchRad);
	const float dirY = sinf(pitchRad);
	const float dirZ = -cosf(yawRad) * cosf(pitchRad);

	CameraPose pose;
	pose.eye = { playerPos.x, playerPos.y + s_eyeHeight, playerPos.z };
	pose.lookAt = {
		pose.eye.x + dirX * kLookDistance,
		pose.eye.y + dirY * kLookDistance,
		pose.eye.z + dirZ * kLookDistance
	};
	pose.up = { 0.0f, 1.0f, 0.0f };
	pose.fov = s_fov;
	pose.nearClip = 0.05f;
	pose.farClip = 1000.0f;

	WritePose(pose);
}

void FirstPersonCamera_SetTarget(XMFLOAT3* positionPtr, float* yawPtr) {
	s_targetPos = positionPtr;
	s_targetYaw = yawPtr;
	s_hasPose = false;
}

void FirstPersonCamera_SetEyeHeight(float eyeHeight) {
	s_eyeHeight = eyeHeight;
}

void FirstPersonCamera_SetMouseSensitivity(float sensitivity) {
	s_mouseSensitivity = sensitivity;
}

void FirstPersonCamera_SetControllerSensitivity(float sensitivity) {
	s_controllerSensitivity = sensitivity;
}

void FirstPersonCamera_SetPitchLimits(float minPitch, float maxPitch) {
	s_minPitch = minPitch;
	s_maxPitch = maxPitch;
}

void FirstPersonCamera_SetFov(float fovDegrees) {
	s_fov = (fovDegrees > 1.0f) ? fovDegrees : 1.0f;
}

void FirstPersonCamera_ShowCursor(bool show) {
	// Share Win32 cursor refcount with TPS so modes do not double ShowCursor.
	ThirdPersonCamera_ShowCursor(show);
}

bool FirstPersonCamera_IsCursorVisible() {
	return ThirdPersonCamera_IsCursorVisible();
}

bool FirstPersonCamera_TryGetPose(CameraPose& outPose) {
	if (!s_hasPose)
		return false;
	outPose = s_pose;
	return true;
}
