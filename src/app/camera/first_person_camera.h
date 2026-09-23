/**===========================================================================
 File   :  [first_person_camera.h]
 Brief  :  First-person eye camera; mirrors ThirdPerson attach/look seams.
 ===========================================================================**/
#ifndef FIRST_PERSON_CAMERA_H
#define FIRST_PERSON_CAMERA_H

#include "camera_pose.h"
#include <DirectXMath.h>

void FirstPersonCamera_Initialize();
void FirstPersonCamera_Update(double elapsed_time);

// Live pointers to the rig position and yaw (degrees). Pass nullptr / nullptr to detach.
void FirstPersonCamera_SetTarget(DirectX::XMFLOAT3* positionPtr, float* yawPtr);

// Eye Y = target.y + eyeHeight (default 0.7 → ~1.6 with capsule center at 0.9).
void FirstPersonCamera_SetEyeHeight(float eyeHeight);

void FirstPersonCamera_SetMouseSensitivity(float sensitivity);
void FirstPersonCamera_SetControllerSensitivity(float sensitivity);
void FirstPersonCamera_SetPitchLimits(float minPitch, float maxPitch);
void FirstPersonCamera_SetFov(float fovDegrees);

void FirstPersonCamera_ShowCursor(bool show);
bool FirstPersonCamera_IsCursorVisible();

bool FirstPersonCamera_TryGetPose(CameraPose& outPose);

#endif // !FIRST_PERSON_CAMERA_H
