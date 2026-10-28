/**===========================================================================
 File   :  [third_person_camera.h]
 Brief  :  Third-person follow camera + magnet Push/Pull/Orbit feel.
 ===========================================================================**/
#ifndef THIRD_PERSON_CAMERA_H
#define THIRD_PERSON_CAMERA_H

#include "camera_pose.h"
#include <DirectXMath.h>

class GameObject;

enum class ThirdPersonMagnetMode {
	Normal = 0,
	Push   = 1,
	Pull   = 2,
	Orbit  = 3,
};

void ThirdPersonCamera_Initialize();
void ThirdPersonCamera_Shutdown();
void ThirdPersonCamera_Update(double elapsed_time);

// Set the camera target via live pointers to the player's position and yaw
// (in degrees).  Pass nullptr / nullptr to detach the camera.
void ThirdPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr);

void ThirdPersonCamera_SetDistance(float distance);
void ThirdPersonCamera_SetHeight(float height);
void ThirdPersonCamera_SetLookAtHeight(float height);
void ThirdPersonCamera_SetMouseSensitivity(float sensitivity);
void ThirdPersonCamera_SetControllerSensitivity(float sensitivity);
void ThirdPersonCamera_SetPitchLimits(float minPitch, float maxPitch);

float ThirdPersonCamera_GetPitch();

void ThirdPersonCamera_ShowCursor(bool show);
bool ThirdPersonCamera_IsCursorVisible();

void ThirdPersonCamera_SetMagnetMode(ThirdPersonMagnetMode mode);
void ThirdPersonCamera_SetMagnetTarget(const DirectX::XMFLOAT3& targetPos);
bool ThirdPersonCamera_TryGetFollowPose(CameraPose& outPose);
GameObject* ThirdPersonCamera_GetFollowCamNode();

// main thread only. Clamp amount to [0,1]; typically 0.2–0.6 per hit.
void ThirdPersonCamera_AddTrauma(float amount01);
float ThirdPersonCamera_GetTrauma();
// main thread only. speed01 in [0,1]; smoothed internally into FOV boost.
void ThirdPersonCamera_SetSpeedFactor(float speed01);

#endif // !THIRD_PERSON_CAMERA_H
