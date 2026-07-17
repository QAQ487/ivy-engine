/**===========================================================================

 File   :  [camera.cpp]

 Author : Hayato Otsuki
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "camera.h"

 // グローバルなカメラインスタンスは静的変数としてこのファイル内で管理
static Camera s_Camera;

Camera& GetCamera() {
	return s_Camera;
}

void SetCamera(const Camera& cam) {
	s_Camera = cam;
}

Camera::Camera()
	: position(0.0f, 0.0f, 0.0f)
	, at(0.0f, 0.0f, 1.0f)
	, up(0.0f, 1.0f, 0.0f)
	, fov(45.0f)
	, nearClip(0.5f)
	, farClip(1000.0f)
{
	// brief
}

void Camera::SetPosition(const XMFLOAT3& pos) { position = pos; }
void Camera::SetTarget(const XMFLOAT3& target) { at = target; }
void Camera::SetUp(const XMFLOAT3& u) { up = u; }
void Camera::SetFov(float f) { fov = f; }
void Camera::SetClip(float nearC, float farC) { nearClip = nearC; farClip = farC; }

XMMATRIX Camera::GetViewMatrix() const {
	XMVECTOR vPos = XMLoadFloat3(&position);
	XMVECTOR vAt = XMLoadFloat3(&at);
	XMVECTOR vUp = XMLoadFloat3(&up);
	return XMMatrixLookAtLH(vPos, vAt, vUp);
}

XMMATRIX Camera::GetProjMatrix(float screenWidth, float screenHeight) const {
	return XMMatrixPerspectiveFovLH(
		XMConvertToRadians(fov),
		screenWidth / screenHeight,
		nearClip, farClip
	);
}

XMFLOAT3 Camera::GetForward() const {
	XMFLOAT3 forward;
	forward.x = at.x - position.x;
	forward.y = at.y - position.y;
	forward.z = at.z - position.z;
	float len = sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len > 0.0001f) {
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	}
	return forward;
}