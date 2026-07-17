#pragma once

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace MagnetHelixPath {

inline constexpr float kPi = 3.14159265358979323846f;

struct MagnetHelixSettings {
	float ampMin = 0.15f;
	float ampMax = 0.6f;
	float turnsMin = 2.0f;
	float turnsMax = 6.5f;
	float scaleRefDistance = 12.0f;
	float rotateSpeed = 1.75f;
	float travelTime = 1.2f;
	float heightBias = 1.0f;
	float spawnT = 0.005f;
	float spawnTSpread = 0.005f;
	int spawnsPerStrand = 3;
	float skeletonRefreshInterval = 0.0f;
	int skeletonPointCount = 64;
	float particleSize = 0.3f;
	float skeletonParticleSize = 0.38f;
	float skeletonLifetime = 0.14f;
};

struct LinkFrame {
	DirectX::XMFLOAT3 from = {};
	DirectX::XMFLOAT3 to = {};
	DirectX::XMFLOAT3 axis = {};
	DirectX::XMFLOAT3 right = {};
	DirectX::XMFLOAT3 up = {};
	float linkLen = 0.0f;
};

struct HelixParams {
	float amplitude = 0.0f;
	float turns = 0.0f;
};

inline float SmoothStep01(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

inline DirectX::XMFLOAT3 Sub(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline DirectX::XMFLOAT3 Mul(const DirectX::XMFLOAT3& v, float s) {
	return { v.x * s, v.y * s, v.z * s };
}

inline float LengthSq(const DirectX::XMFLOAT3& v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline DirectX::XMFLOAT3 NormalizeOrUp(const DirectX::XMFLOAT3& v) {
	const float lenSq = LengthSq(v);
	if (lenSq <= 0.000001f)
		return { 0.0f, 1.0f, 0.0f };
	const float invLen = 1.0f / std::sqrt(lenSq);
	return { v.x * invLen, v.y * invLen, v.z * invLen };
}

inline DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

inline DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t
	};
}

inline bool BuildLinkFrame(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, LinkFrame& out) {
	const DirectX::XMFLOAT3 linkDelta = Sub(to, from);
	const float linkLen = std::sqrt(LengthSq(linkDelta));
	if (linkLen < 0.05f)
		return false;

	out.from = from;
	out.to = to;
	out.linkLen = linkLen;
	out.axis = Mul(linkDelta, 1.0f / linkLen);

	DirectX::XMFLOAT3 right = Cross({ 0.0f, 1.0f, 0.0f }, out.axis);
	if (LengthSq(right) <= 0.000001f)
		right = Cross({ 1.0f, 0.0f, 0.0f }, out.axis);
	out.right = NormalizeOrUp(right);
	out.up = Cross(out.axis, out.right);
	return true;
}

inline HelixParams ComputeHelixParams(float linkLen, const MagnetHelixSettings& settings) {
	const float u = SmoothStep01(linkLen / settings.scaleRefDistance);
	HelixParams params;
	params.amplitude = settings.ampMin + (settings.ampMax - settings.ampMin) * u;
	params.turns = settings.turnsMin + (settings.turnsMax - settings.turnsMin) * u;
	return params;
}

inline float HelixStrandPhaseOffset(int strand) {
	return (strand == 0) ? 0.0f : kPi;
}

inline DirectX::XMFLOAT3 EvalHelixRadialDir(const LinkFrame& frame,
	float t,
	int strand,
	float phase,
	float turns) {
	const float angle = (t * (2.0f * kPi) * turns) + phase + HelixStrandPhaseOffset(strand);
	const float ca = std::cos(angle);
	const float sa = std::sin(angle);
	return NormalizeOrUp(Add(Mul(frame.right, ca), Mul(frame.up, sa)));
}

inline DirectX::XMFLOAT3 EvalHelixPoint(const LinkFrame& frame,
	float t,
	int strand,
	float phase,
	float amplitude,
	float turns) {
	const DirectX::XMFLOAT3 center = Lerp(frame.from, frame.to, t);
	const DirectX::XMFLOAT3 radialDir = EvalHelixRadialDir(frame, t, strand, phase, turns);
	return Add(center, Mul(radialDir, amplitude));
}

inline DirectX::XMFLOAT3 EvalAxialVelocity(const LinkFrame& frame, float axialSpeed) {
	return Mul(frame.axis, axialSpeed);
}

inline DirectX::XMFLOAT3 EvalHelixVelocity(const LinkFrame& frame,
	float t,
	int strand,
	float phase,
	float amplitude,
	float turns,
	float axialSpeed,
	float rotateSpeed) {
	const DirectX::XMFLOAT3 radialDir = EvalHelixRadialDir(frame, t, strand, phase, turns);
	const DirectX::XMFLOAT3 tangent = NormalizeOrUp(Cross(frame.axis, radialDir));
	const DirectX::XMFLOAT3 tangentialVel = Mul(tangent, amplitude * rotateSpeed);
	const DirectX::XMFLOAT3 axialVel = Mul(frame.axis, axialSpeed);
	return Add(tangentialVel, axialVel);
}

inline float HelixEndIntensity(float t) {
	return std::sin(t * kPi);
}

} // namespace MagnetHelixPath
