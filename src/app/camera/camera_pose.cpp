#include "camera_pose.h"
#include "camera.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/transform.h"
#include <cmath>

using namespace DirectX;

namespace {

constexpr float kSceneLookDistance = 10.0f;

float LerpFloat(float a, float b, float t) {
	return a + (b - a) * t;
}

XMFLOAT3 LerpVec(const XMFLOAT3& a, const XMFLOAT3& b, float t) {
	return {
		LerpFloat(a.x, b.x, t),
		LerpFloat(a.y, b.y, t),
		LerpFloat(a.z, b.z, t),
	};
}

XMFLOAT3 NormalizeSafe(const XMFLOAT3& v, const XMFLOAT3& fallback) {
	const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq < 1e-8f)
		return fallback;
	const float inv = 1.0f / sqrtf(lenSq);
	return { v.x * inv, v.y * inv, v.z * inv };
}

XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

XMFLOAT3 Scale(const XMFLOAT3& v, float s) {
	return { v.x * s, v.y * s, v.z * s };
}

float Length(const XMFLOAT3& v) {
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float Dot(const XMFLOAT3& a, const XMFLOAT3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

XMFLOAT3 SlerpDir(const XMFLOAT3& aIn, const XMFLOAT3& bIn, float t, const XMFLOAT3& fallback) {
	XMFLOAT3 a = NormalizeSafe(aIn, fallback);
	XMFLOAT3 b = NormalizeSafe(bIn, fallback);
	float d = Dot(a, b);
	if (d > 0.9995f)
		return NormalizeSafe(LerpVec(a, b, t), fallback);
	if (d < -0.9995f) {
		// Opposite dirs — pick perpendicular pivot, rotate 180 via two half-slerps.
		XMFLOAT3 pivot = { 0.0f, 1.0f, 0.0f };
		if (fabsf(Dot(a, pivot)) > 0.9f)
			pivot = { 1.0f, 0.0f, 0.0f };
		XMVECTOR av = XMLoadFloat3(&a);
		XMVECTOR pv = XMVector3Normalize(XMVector3Cross(av, XMLoadFloat3(&pivot)));
		XMFLOAT3 mid;
		XMStoreFloat3(&mid, pv);
		if (t < 0.5f)
			return SlerpDir(a, mid, t * 2.0f, fallback);
		return SlerpDir(mid, b, (t - 0.5f) * 2.0f, fallback);
	}

	d = (d < -1.0f) ? -1.0f : ((d > 1.0f) ? 1.0f : d);
	const float theta = acosf(d);
	const float sinTheta = sinf(theta);
	if (fabsf(sinTheta) < 1e-5f)
		return NormalizeSafe(LerpVec(a, b, t), fallback);
	const float w1 = sinf((1.0f - t) * theta) / sinTheta;
	const float w2 = sinf(t * theta) / sinTheta;
	return NormalizeSafe(Add(Scale(a, w1), Scale(b, w2)), fallback);
}

void ExtractLook(const CameraPose& pose, XMFLOAT3& outForward, float& outDist) {
	XMFLOAT3 delta = Sub(pose.lookAt, pose.eye);
	outDist = Length(delta);
	if (outDist < 1e-3f) {
		outForward = { 0.0f, 0.0f, 1.0f };
		outDist = kSceneLookDistance;
		return;
	}
	outForward = Scale(delta, 1.0f / outDist);
	// Unit-forward lookAt (old scene CameraComponent) → stable focus length.
	if (outDist < 1.5f)
		outDist = kSceneLookDistance;
}

} // namespace

CameraPose CameraPose_FromGlobal() {
	const Camera& cam = GetCamera();
	CameraPose pose;
	pose.eye = cam.position;
	pose.lookAt = cam.at;
	pose.up = cam.up;
	pose.fov = cam.fov;
	pose.nearClip = cam.nearClip;
	pose.farClip = cam.farClip;
	return pose;
}

void CameraPose_ApplyToGlobal(const CameraPose& pose) {
	Camera& cam = GetCamera();
	cam.SetPosition(pose.eye);
	cam.SetTarget(pose.lookAt);
	cam.SetUp(pose.up);
	cam.SetFov(pose.fov);
	cam.SetClip(pose.nearClip, pose.farClip);
}

CameraPose CameraPose_Lerp(const CameraPose& a, const CameraPose& b, float t) {
	t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

	XMFLOAT3 fwdA, fwdB;
	float distA = kSceneLookDistance, distB = kSceneLookDistance;
	ExtractLook(a, fwdA, distA);
	ExtractLook(b, fwdB, distB);

	CameraPose out;
	out.eye = LerpVec(a.eye, b.eye, t);
	const XMFLOAT3 forward = SlerpDir(fwdA, fwdB, t, fwdB);
	const float dist = LerpFloat(distA, distB, t);
	out.lookAt = Add(out.eye, Scale(forward, dist));
	out.up = NormalizeSafe(LerpVec(a.up, b.up, t), { 0.0f, 1.0f, 0.0f });
	out.fov = LerpFloat(a.fov, b.fov, t);
	out.nearClip = LerpFloat(a.nearClip, b.nearClip, t);
	out.farClip = LerpFloat(a.farClip, b.farClip, t);
	return out;
}

CameraPose CameraPose_FromComponent(const CameraComponent* cam, const engine::Transform* transform) {
	CameraPose pose;
	if (!cam || !transform)
		return pose;

	const XMFLOAT3 eye = CameraComponent::ComputeEyeWS(transform);
	const XMFLOAT3 forward = CameraComponent::ComputeForwardWS(transform);
	const XMFLOAT3 viewUp = CameraComponent::ComputeViewUpWS(forward);

	pose.eye = eye;
	pose.lookAt = {
		eye.x + forward.x * kSceneLookDistance,
		eye.y + forward.y * kSceneLookDistance,
		eye.z + forward.z * kSceneLookDistance,
	};
	pose.up = viewUp;
	pose.fov = cam->fov;
	pose.nearClip = cam->nearClip;
	pose.farClip = cam->farClip;
	return pose;
}

void CameraPose_WriteToNode(engine::Transform* transform, CameraComponent* cam, const CameraPose& pose) {
	if (!transform || !cam)
		return;

	XMVECTOR eye = XMLoadFloat3(&pose.eye);
	XMVECTOR at = XMLoadFloat3(&pose.lookAt);
	XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(at, eye));
	if (XMVector3Equal(forward, XMVectorZero()))
		forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	XMVECTOR worldUp = XMLoadFloat3(&pose.up);
	if (XMVector3Equal(worldUp, XMVectorZero()))
		worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR right = XMVector3Cross(worldUp, forward);
	if (XMVectorGetX(XMVector3LengthSq(right)) < 1e-6f) {
		worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		right = XMVector3Cross(worldUp, forward);
	}
	right = XMVector3Normalize(right);
	XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

	XMMATRIX world = XMMatrixIdentity();
	world.r[0] = XMVectorSetW(right, 0.0f);
	world.r[1] = XMVectorSetW(up, 0.0f);
	world.r[2] = XMVectorSetW(forward, 0.0f);
	world.r[3] = XMVectorSetW(eye, 1.0f);
	transform->SetFromLocalMatrix(world);

	cam->fov = pose.fov;
	cam->nearClip = pose.nearClip;
	cam->farClip = pose.farClip;
	cam->enabled = true;
}
