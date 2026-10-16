/**===========================================================================

 File   :  [rigidbody.h]

 Author : Terry Yeung
 Date   : 2025/12/2
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef RIGIDBODY_H
#define RIGIDBODY_H
#include <DirectXMath.h>

// Engine default gravity (m/s^2). Projects may override via Physics_SetGravity.
static const float GRAVITY = 9.81f;

float Physics_GetGravity();
void Physics_SetGravity(float gravity);
// Runtime host does not open the editor project; load physics.gravity from
// cwd or exe-adjacent project.json (same lookup as audio buses).
void Physics_TryLoadGravityFromNearbyProject();

enum class ForceMode {
	Force,          // Add a continuous force. Mass-dependent.
	Acceleration,   // Add a continuous acceleration. Mass-independent.
	Impulse,        // Add an instant force impulse. Mass-dependent.
	VelocityChange  // Add an instant velocity change. Mass-independent.
};

// Body-space inverse inertia diagonal (world tensor is R * diag * R^T).
DirectX::XMFLOAT3 Rigidbody_InvInertiaBox(float mass, const DirectX::XMFLOAT3& fullSize);
DirectX::XMFLOAT3 Rigidbody_InvInertiaSphere(float mass, float radius);
DirectX::XMFLOAT3 Rigidbody_InvInertiaUnitCube(float mass);

DirectX::XMFLOAT3 Rigidbody_ApplyInvInertiaWorld(
	const DirectX::XMFLOAT3& localInvInertia,
	const DirectX::XMFLOAT3& rotationDeg,
	const DirectX::XMFLOAT3& worldVec);

// Integrate a world-space finite rotation (radians) into a ZYX Euler pose.
void Rigidbody_ApplyWorldDeltaRotation(
	DirectX::XMFLOAT3& rotationDeg,
	const DirectX::XMFLOAT3& dThetaRad);

class Rigidbody {
private:
	bool m_useGravity = false;
	bool m_isKinematic = false;
	bool m_gravityOverridden = false;
	float m_mass = 1.0f;
	float m_drag = 0.05f;
	float m_angularDrag = 0.05f;
	float m_restitution = 0.0f;
	float m_friction = 0.0f;
	DirectX::XMFLOAT3 m_velocity = {0, 0, 0};
	DirectX::XMFLOAT3 m_angularVelocity = {0, 0, 0};
	DirectX::XMFLOAT3 m_forceAccum = {0, 0, 0};
	DirectX::XMFLOAT3 m_torqueAccum = {0, 0, 0};
	DirectX::XMFLOAT3 m_localInvInertia = {6.0f, 6.0f, 6.0f};
	DirectX::XMFLOAT3 m_lastRotation = {0, 0, 0};

public:
	void SetUseGravity(bool enable) { m_useGravity = enable; }
	bool UseGravity() const { return m_useGravity; }
	void OverrideGravity(bool shouldOverride) { m_gravityOverridden = shouldOverride; }

	void SetIsKinematic(bool kinematic) { m_isKinematic = kinematic; }
	bool IsKinematic() const { return m_isKinematic; }

	void SetMass(float mass) { m_mass = (mass > 0.0f) ? mass : 1.0f; }
	float GetMass() const { return m_mass; }

	void SetDrag(float drag) { m_drag = drag; }
	float GetDrag() const { return m_drag; }

	void SetAngularDrag(float drag) { m_angularDrag = drag < 0.0f ? 0.0f : drag; }
	float GetAngularDrag() const { return m_angularDrag; }

	// Bounce coefficient [0..1+]. Used as the contact fallback when the collider
	// has no Physics Material. Combined as max(self, other) in ResolveContact.
	void SetRestitution(float restitution) {
		m_restitution = restitution < 0.0f ? 0.0f : restitution;
	}
	float GetRestitution() const { return m_restitution; }

	// Surface friction [0..]. Used as both static and dynamic friction when the
	// collider has no Physics Material. Combined as min(self, other) so a body
	// with friction 0 (e.g. player) never gets tangential grip from the floor.
	void SetFriction(float friction) {
		m_friction = friction < 0.0f ? 0.0f : friction;
	}
	float GetFriction() const { return m_friction; }

	void SetVelocity(const DirectX::XMFLOAT3& v) { m_velocity = v; }
	DirectX::XMFLOAT3 GetVelocity() const { return m_velocity; }

	void SetAngularVelocity(const DirectX::XMFLOAT3& w) { m_angularVelocity = w; }
	DirectX::XMFLOAT3 GetAngularVelocity() const { return m_angularVelocity; }

	void SetLocalInverseInertia(const DirectX::XMFLOAT3& inv) { m_localInvInertia = inv; }
	DirectX::XMFLOAT3 GetLocalInverseInertia() const { return m_localInvInertia; }

	void AddForce(const DirectX::XMFLOAT3& force, ForceMode mode = ForceMode::Force);
	void AddTorque(const DirectX::XMFLOAT3& torque, ForceMode mode = ForceMode::Force);
	void AddForceAtPosition(const DirectX::XMFLOAT3& force,
		const DirectX::XMFLOAT3& worldPoint,
		const DirectX::XMFLOAT3& ownerPosition,
		const DirectX::XMFLOAT3& ownerRotation,
		ForceMode mode = ForceMode::Force);

	void ClearAllForces();
	void ClearAllVelocity() {
		m_velocity = {0, 0, 0};
		m_angularVelocity = {0, 0, 0};
	}
	void ClearVelocity(DirectX::XMFLOAT3 clear_flag);

	void Integrate(DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& rotation, double elapsed_time);

	void StopY() { m_velocity.y = 0.0f; }
};
#endif
