/**===========================================================================
 File   :  camera_pose.h
 Brief  :  Shared camera eye/lookAt/FOV pose for Director blends.
 ===========================================================================**/
#ifndef CAMERA_POSE_H
#define CAMERA_POSE_H

#include <DirectXMath.h>

struct CameraPose {
	DirectX::XMFLOAT3 eye    = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 lookAt = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 up     = { 0.0f, 1.0f, 0.0f };
	float fov      = 45.0f;
	float nearClip = 0.5f;
	float farClip  = 1000.0f;
};

CameraPose CameraPose_FromGlobal();
void       CameraPose_ApplyToGlobal(const CameraPose& pose);
CameraPose CameraPose_Lerp(const CameraPose& a, const CameraPose& b, float t);

namespace engine { class Transform; }
class CameraComponent;

CameraPose CameraPose_FromComponent(const CameraComponent* cam, const engine::Transform* transform);
void       CameraPose_WriteToNode(engine::Transform* transform, CameraComponent* cam, const CameraPose& pose);

#endif // !CAMERA_POSE_H
