/**===========================================================================

 File   :  [rigidbody.cpp]

 Author : Terry Yeung
 Date   : 2025/12/2
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "rigidbody.h"

#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace {
	float g_worldGravity = GRAVITY;

	bool LoadGravityFromProjectFile(const std::string& path) {
		try {
			std::ifstream file(path);
			if (!file.is_open())
				return false;
			nlohmann::json j;
			file >> j;
			if (!j.contains("physics") || !j["physics"].is_object())
				return false;
			const float gravity = j["physics"].value("gravity", GRAVITY);
			Physics_SetGravity(gravity);
			return true;
		}
		catch (...) {
			return false;
		}
	}

	DirectX::XMFLOAT3 EulerZyxDegreesFromRotationMatrix(DirectX::XMMATRIX rot) {
		// DirectX XMMATRIX is row-major. R = Rz*Ry*Rx so this must be the
		// inverse of RotationZyxFromEulerDeg (r01/r12, not column-major r10/r21).
		const float r00 = rot.r[0].m128_f32[0];
		const float r01 = rot.r[0].m128_f32[1];
		const float r02 = rot.r[0].m128_f32[2];
		const float r12 = rot.r[1].m128_f32[2];
		const float r22 = rot.r[2].m128_f32[2];
		const float r11 = rot.r[1].m128_f32[1];
		const float r21 = rot.r[2].m128_f32[1];

		const float sy = sqrtf(r00 * r00 + r01 * r01);
		float rotX, rotY, rotZ;
		if (sy > 1e-6f) {
			rotX = atan2f(r12, r22);
			rotY = atan2f(-r02, sy);
			rotZ = atan2f(r01, r00);
		} else {
			rotX = atan2f(-r21, r11);
			rotY = atan2f(-r02, sy);
			rotZ = 0.0f;
		}

		return {
			DirectX::XMConvertToDegrees(rotX),
			DirectX::XMConvertToDegrees(rotY),
			DirectX::XMConvertToDegrees(rotZ)
		};
	}

	DirectX::XMMATRIX RotationZyxFromEulerDeg(const DirectX::XMFLOAT3& rotationDeg) {
		using namespace DirectX;
		XMMATRIX Rx = XMMatrixRotationX(XMConvertToRadians(rotationDeg.x));
		XMMATRIX Ry = XMMatrixRotationY(XMConvertToRadians(rotationDeg.y));
		XMMATRIX Rz = XMMatrixRotationZ(XMConvertToRadians(rotationDeg.z));
		return Rz * Ry * Rx;
	}

	DirectX::XMFLOAT3 InvInertiaFromMoments(float ix, float iy, float iz) {
		constexpr float kMinI = 1e-6f;
		return {
			1.0f / (std::max)(ix, kMinI),
			1.0f / (std::max)(iy, kMinI),
			1.0f / (std::max)(iz, kMinI)
		};
	}

	DirectX::XMFLOAT3 Cross3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}
}

float Physics_GetGravity() {
	return g_worldGravity;
}

void Physics_SetGravity(float gravity) {
	g_worldGravity = (gravity > 0.0f) ? gravity : GRAVITY;
}

void Physics_TryLoadGravityFromNearbyProject() {
	namespace fs = std::filesystem;
	if (LoadGravityFromProjectFile("project.json"))
		return;

	wchar_t exePath[MAX_PATH] = {};
	if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
		return;
	const fs::path nearby = fs::path(exePath).parent_path() / "project.json";
	LoadGravityFromProjectFile(nearby.string());
}

DirectX::XMFLOAT3 Rigidbody_InvInertiaBox(float mass, const DirectX::XMFLOAT3& fullSize) {
	const float m = (mass > 0.0f) ? mass : 1.0f;
	const float sx = fullSize.x;
	const float sy = fullSize.y;
	const float sz = fullSize.z;
	const float ix = (m / 12.0f) * (sy * sy + sz * sz);
	const float iy = (m / 12.0f) * (sx * sx + sz * sz);
	const float iz = (m / 12.0f) * (sx * sx + sy * sy);
	return InvInertiaFromMoments(ix, iy, iz);
}

DirectX::XMFLOAT3 Rigidbody_InvInertiaSphere(float mass, float radius) {
	const float m = (mass > 0.0f) ? mass : 1.0f;
	const float r = (radius > 1e-4f) ? radius : 0.5f;
	const float i = 0.4f * m * r * r;
	return InvInertiaFromMoments(i, i, i);
}

DirectX::XMFLOAT3 Rigidbody_InvInertiaUnitCube(float mass) {
	return Rigidbody_InvInertiaBox(mass, {1.0f, 1.0f, 1.0f});
}

DirectX::XMFLOAT3 Rigidbody_ApplyInvInertiaWorld(
	const DirectX::XMFLOAT3& localInvInertia,
	const DirectX::XMFLOAT3& rotationDeg,
	const DirectX::XMFLOAT3& worldVec)
{
	using namespace DirectX;
	if (localInvInertia.x == 0.0f && localInvInertia.y == 0.0f && localInvInertia.z == 0.0f)
		return {0, 0, 0};

	const XMMATRIX R = RotationZyxFromEulerDeg(rotationDeg);
	const XMMATRIX Rt = XMMatrixTranspose(R);
	XMVECTOR local = XMVector3TransformNormal(XMLoadFloat3(&worldVec), Rt);
	XMFLOAT3 l;
	XMStoreFloat3(&l, local);
	l.x *= localInvInertia.x;
	l.y *= localInvInertia.y;
	l.z *= localInvInertia.z;
	XMFLOAT3 world;
	XMStoreFloat3(&world, XMVector3TransformNormal(XMLoadFloat3(&l), R));
	return world;
}

void Rigidbody::AddForce(const DirectX::XMFLOAT3& force, ForceMode mode) {
	if (m_isKinematic) return;

	switch (mode) {
		case ForceMode::Force:
			m_forceAccum.x += force.x;
			m_forceAccum.y += force.y;
			m_forceAccum.z += force.z;
			break;

		case ForceMode::Acceleration:
			m_forceAccum.x += force.x * m_mass;
			m_forceAccum.y += force.y * m_mass;
			m_forceAccum.z += force.z * m_mass;
			break;

		case ForceMode::Impulse:
			m_velocity.x += force.x / m_mass;
			m_velocity.y += force.y / m_mass;
			m_velocity.z += force.z / m_mass;
			break;

		case ForceMode::VelocityChange:
			m_velocity.x += force.x;
			m_velocity.y += force.y;
			m_velocity.z += force.z;
			break;
	}
}

void Rigidbody::AddTorque(const DirectX::XMFLOAT3& torque, ForceMode mode) {
	if (m_isKinematic) return;

	switch (mode) {
		case ForceMode::Force:
			m_torqueAccum.x += torque.x;
			m_torqueAccum.y += torque.y;
			m_torqueAccum.z += torque.z;
			break;

		case ForceMode::Acceleration:
			m_torqueAccum.x += torque.x * m_mass;
			m_torqueAccum.y += torque.y * m_mass;
			m_torqueAccum.z += torque.z * m_mass;
			break;

		case ForceMode::Impulse: {
			const DirectX::XMFLOAT3 dw = Rigidbody_ApplyInvInertiaWorld(
				m_localInvInertia, m_lastRotation, torque);
			m_angularVelocity.x += dw.x;
			m_angularVelocity.y += dw.y;
			m_angularVelocity.z += dw.z;
			break;
		}

		case ForceMode::VelocityChange:
			m_angularVelocity.x += torque.x;
			m_angularVelocity.y += torque.y;
			m_angularVelocity.z += torque.z;
			break;
	}
}

void Rigidbody::AddForceAtPosition(const DirectX::XMFLOAT3& force,
	const DirectX::XMFLOAT3& worldPoint,
	const DirectX::XMFLOAT3& ownerPosition,
	const DirectX::XMFLOAT3& ownerRotation,
	ForceMode mode)
{
	if (m_isKinematic) return;

	AddForce(force, mode);

	const DirectX::XMFLOAT3 r{
		worldPoint.x - ownerPosition.x,
		worldPoint.y - ownerPosition.y,
		worldPoint.z - ownerPosition.z
	};

	switch (mode) {
		case ForceMode::Force:
			AddTorque(Cross3(r, force), ForceMode::Force);
			break;
		case ForceMode::Acceleration: {
			const DirectX::XMFLOAT3 f{
				force.x * m_mass, force.y * m_mass, force.z * m_mass
			};
			AddTorque(Cross3(r, f), ForceMode::Force);
			break;
		}
		case ForceMode::Impulse: {
			const DirectX::XMFLOAT3 tau = Cross3(r, force);
			const DirectX::XMFLOAT3 dw = Rigidbody_ApplyInvInertiaWorld(
				m_localInvInertia, ownerRotation, tau);
			m_angularVelocity.x += dw.x;
			m_angularVelocity.y += dw.y;
			m_angularVelocity.z += dw.z;
			break;
		}
		case ForceMode::VelocityChange: {
			const DirectX::XMFLOAT3 impulse{
				force.x * m_mass, force.y * m_mass, force.z * m_mass
			};
			const DirectX::XMFLOAT3 dw = Rigidbody_ApplyInvInertiaWorld(
				m_localInvInertia, ownerRotation, Cross3(r, impulse));
			m_angularVelocity.x += dw.x;
			m_angularVelocity.y += dw.y;
			m_angularVelocity.z += dw.z;
			break;
		}
	}
}

void Rigidbody::ClearAllForces() {
	m_forceAccum.x = 0.0f;
	m_forceAccum.y = 0.0f;
	m_forceAccum.z = 0.0f;
	m_torqueAccum.x = 0.0f;
	m_torqueAccum.y = 0.0f;
	m_torqueAccum.z = 0.0f;
}

void Rigidbody::ClearVelocity(DirectX::XMFLOAT3 clear_flag) {
	if (clear_flag.x) m_velocity.x = 0.0f;
	if (clear_flag.y) m_velocity.y = 0.0f;
	if (clear_flag.z) m_velocity.z = 0.0f;
}

void Rigidbody::Integrate(DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& rotation, double elapsed_time) {
	if (m_isKinematic) return;

	m_lastRotation = rotation;

	float dt = static_cast<float>(elapsed_time);
	if (dt <= 0.0f) return;
	// Clamp to avoid huge integration steps after debugger breaks / hitches / tab-out.
	if (dt > (1.0f / 30.0f)) dt = 1.0f / 30.0f;

	if (m_useGravity && !m_gravityOverridden) {
		m_forceAccum.y -= Physics_GetGravity() * m_mass;
	}

	DirectX::XMFLOAT3 acceleration;
	acceleration.x = m_forceAccum.x / m_mass;
	acceleration.y = m_forceAccum.y / m_mass;
	acceleration.z = m_forceAccum.z / m_mass;

	m_velocity.x += acceleration.x * dt;
	m_velocity.y += acceleration.y * dt;
	m_velocity.z += acceleration.z * dt;

	const DirectX::XMFLOAT3 alpha = Rigidbody_ApplyInvInertiaWorld(
		m_localInvInertia, rotation, m_torqueAccum);
	m_angularVelocity.x += alpha.x * dt;
	m_angularVelocity.y += alpha.y * dt;
	m_angularVelocity.z += alpha.z * dt;

	const float dragFactor = 1.0f / (1.0f + m_drag * dt);
	m_velocity.x *= dragFactor;
	m_velocity.y *= dragFactor;
	m_velocity.z *= dragFactor;

	const float angDragFactor = 1.0f / (1.0f + m_angularDrag * dt);
	m_angularVelocity.x *= angDragFactor;
	m_angularVelocity.y *= angDragFactor;
	m_angularVelocity.z *= angDragFactor;

	position.x += m_velocity.x * dt;
	position.y += m_velocity.y * dt;
	position.z += m_velocity.z * dt;

	using namespace DirectX;
	const XMMATRIX R0 = RotationZyxFromEulerDeg(rotation);
	XMVECTOR q = XMQuaternionRotationMatrix(R0);
	XMVECTOR omegaQ = XMVectorSet(
		m_angularVelocity.x, m_angularVelocity.y, m_angularVelocity.z, 0.0f);
	XMVECTOR dq = XMVectorScale(XMQuaternionMultiply(omegaQ, q), 0.5f * dt);
	q = XMQuaternionNormalize(XMVectorAdd(q, dq));
	rotation = EulerZyxDegreesFromRotationMatrix(XMMatrixRotationQuaternion(q));
	m_lastRotation = rotation;

	ClearAllForces();
}

void Rigidbody_ApplyWorldDeltaRotation(
	DirectX::XMFLOAT3& rotationDeg,
	const DirectX::XMFLOAT3& dThetaRad)
{
	using namespace DirectX;
	const float mag2 = dThetaRad.x * dThetaRad.x + dThetaRad.y * dThetaRad.y + dThetaRad.z * dThetaRad.z;
	if (mag2 < 1e-16f)
		return;
	const XMMATRIX R0 = RotationZyxFromEulerDeg(rotationDeg);
	XMVECTOR q = XMQuaternionRotationMatrix(R0);
	XMVECTOR omegaQ = XMVectorSet(dThetaRad.x, dThetaRad.y, dThetaRad.z, 0.0f);
	XMVECTOR dq = XMVectorScale(XMQuaternionMultiply(omegaQ, q), 0.5f);
	q = XMQuaternionNormalize(XMVectorAdd(q, dq));
	rotationDeg = EulerZyxDegreesFromRotationMatrix(XMMatrixRotationQuaternion(q));
}
