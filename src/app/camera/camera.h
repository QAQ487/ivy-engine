/**===========================================================================

 File   :  [camera.h]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>
using namespace DirectX;

class Camera {
public:
	Camera();

	void SetPosition(const XMFLOAT3& pos);
	void SetTarget(const XMFLOAT3& at);
	void SetUp(const XMFLOAT3& up);
	void SetFov(float fov);
	void SetClip(float nearClip, float farClip);

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjMatrix(float screenWidth, float screenHeight) const;
	XMFLOAT3 GetForward() const;

	XMFLOAT3 position;
	XMFLOAT3 at;
	XMFLOAT3 up;
	float fov;
    bool orthographic = false;
    float orthographicHeight = 10.0f;
	float nearClip;
	float farClip;
};

// Accessor for the global camera instance
Camera& GetCamera();
void SetCamera(const Camera& cam);

#endif // !CAMERA_H