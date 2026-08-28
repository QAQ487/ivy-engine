/**===========================================================================

 File   :  [rigidbody.cpp]

 Author : Hayato Otsuki
 Date   : 2025/12/2
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "rigidbody.h"

namespace {
	float g_worldGravity = GRAVITY;
}

float Physics_GetGravity() {
	return g_worldGravity;
}

void Physics_SetGravity(float gravity) {
	g_worldGravity = (gravity > 0.0f) ? gravity : GRAVITY;
}

void Rigidbody::AddForce(const DirectX::XMFLOAT3& force, ForceMode mode) {
	if (m_isKinematic) return;

	switch (mode) {
		case ForceMode::Force:
			// Add to the force accumulator for this frame.
			m_forceAccum.x += force.x;
			m_forceAccum.y += force.y;
			m_forceAccum.z += force.z;
			break;

		case ForceMode::Acceleration:
			// F = ma, so to get an acceleration, we apply a force of F = m*a
			m_forceAccum.x += force.x * m_mass;
			m_forceAccum.y += force.y * m_mass;
			m_forceAccum.z += force.z * m_mass;
			break;

		case ForceMode::Impulse:
			// Impulse is a change in momentum (p=mv), so ��v = impulse / mass
			m_velocity.x += force.x / m_mass;
			m_velocity.y += force.y / m_mass;
			m_velocity.z += force.z / m_mass;
			break;

		case ForceMode::VelocityChange:
			// Directly add to the velocity.
			m_velocity.x += force.x;
			m_velocity.y += force.y;
			m_velocity.z += force.z;
			break;
	}
}

void Rigidbody::ClearAllForces() {
	m_forceAccum.x = 0.0f;
	m_forceAccum.y = 0.0f;
	m_forceAccum.z = 0.0f;
}

void Rigidbody::ClearVelocity(DirectX::XMFLOAT3 clear_flag) {
	if (clear_flag.x) m_velocity.x = 0.0f;
	if (clear_flag.y) m_velocity.y = 0.0f;
	if (clear_flag.z) m_velocity.z = 0.0f;
}

void Rigidbody::Integrate(DirectX::XMFLOAT3& position, double elapsed_time) {
	if (m_isKinematic) return;

	float dt = static_cast<float>(elapsed_time);
	if (dt <= 0.0f) return;
	// Clamp to avoid huge integration steps after debugger breaks / hitches / tab-out.
	if (dt > (1.0f / 30.0f)) dt = 1.0f / 30.0f;

	if (m_useGravity && !m_gravityOverridden) {
		m_forceAccum.y -= Physics_GetGravity() * m_mass;
	}

	// Calculate acceleration from accumulated forces (a = F/m)
	DirectX::XMFLOAT3 acceleration;
	acceleration.x = m_forceAccum.x / m_mass;
	acceleration.y = m_forceAccum.y / m_mass;
	acceleration.z = m_forceAccum.z / m_mass;

	// Update velocity based on acceleration
	m_velocity.x += acceleration.x * dt;
	m_velocity.y += acceleration.y * dt;
	m_velocity.z += acceleration.z * dt;

	// Apply drag
	float dragFactor = 1.0f / (1.0f + m_drag * dt);
	m_velocity.x *= dragFactor;
	m_velocity.y *= dragFactor;
	m_velocity.z *= dragFactor;

	// Update position based on velocity
	position.x += m_velocity.x * dt;
	position.y += m_velocity.y * dt;
	position.z += m_velocity.z * dt;

	// Clear the forces for the next frame
	ClearAllForces();
}