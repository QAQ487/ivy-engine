/**===========================================================================

 File   :  [rigidbody.h]

 Author : Hayato Otsuki
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

enum class ForceMode {
	Force,          // Add a continuous force. Mass-dependent.
	Acceleration,   // Add a continuous acceleration. Mass-independent.
	Impulse,        // Add an instant force impulse. Mass-dependent.
	VelocityChange  // Add an instant velocity change. Mass-independent.
};

class Rigidbody {
private:
	bool m_useGravity = false;
	bool m_isKinematic = false;
	bool m_gravityOverridden = false;
	float m_mass = 1.0f;
	float m_drag = 0.05f;
	float m_restitution = 0.0f;
	DirectX::XMFLOAT3 m_velocity = {0, 0, 0};
	DirectX::XMFLOAT3 m_forceAccum = {0, 0, 0};

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

	// Bounce coefficient [0..1+]. Combined as max(self, other) in ResolveContact.
	void SetRestitution(float restitution) {
		m_restitution = restitution < 0.0f ? 0.0f : restitution;
	}
	float GetRestitution() const { return m_restitution; }

	void SetVelocity(const DirectX::XMFLOAT3& v) { m_velocity = v; }
	DirectX::XMFLOAT3 GetVelocity() const { return m_velocity; }

	void AddForce(const DirectX::XMFLOAT3& force, ForceMode mode = ForceMode::Force);

	void ClearAllForces();
	void ClearAllVelocity() { m_velocity = {0, 0, 0}; }
	void ClearVelocity(DirectX::XMFLOAT3 clear_flag);

	void Integrate(DirectX::XMFLOAT3& position, double elapsed_time);

	void StopY() { m_velocity.y = 0.0f; }
};
#endif