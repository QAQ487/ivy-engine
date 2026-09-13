/**===========================================================================

 File   :  [physics_system.h]

 Author : Hayato Otsuki
 Date   : 2025/10/18
 -----------------------------------------------------------------------------

 ===========================================================================**/

#ifndef PHYSICS_SYSTEM_H
#define PHYSICS_SYSTEM_H

#include <DirectXMath.h>

class GameObject;
class Collider;

// Full physics tick: save previous positions, integrate velocity, detect and
// resolve collisions (swept AABB broad phase + sub-stepped narrow phase).
// The scene loop should call this only while playing; edit mode must not
// integrate gravity or resolve contacts.
void PhysicsSystem_Update(double elapsed_time);

// Snapshot current positions of all dynamic objects into m_previousPosition.
// Call this just before running scripts (e.g. orbit) so that the post-script
// swept collision pass can measure the full script-driven displacement.
void PhysicsSystem_SavePositions();

// Run collision detection and resolution without integrating velocity.
// Uses swept AABB: broad phase expands the bounding sphere along the path from
// m_previousPosition to m_position, and the narrow phase sub-steps over large
// displacements to prevent tunneling through thin objects.
void PhysicsSystem_ResolveCollisions();

// Clear trigger enter/exit pair tracking (no callbacks). Call on scene change.
void PhysicsSystem_ClearTriggerPairs();

// While obj is still alive: fire OnTriggerExit for pairs involving obj, then
// erase those pairs from tracking so the next pass cannot touch freed pointers.
void PhysicsSystem_NotifyObjectDestroyed(GameObject* obj);

struct PhysicsDebugStats {
	int colliders = 0; // broad-phase entries
	int pairs = 0;     // narrow/trigger CheckCollision true
	int contacts = 0;  // ResolveContact invocations
	int triggers = 0;  // InsertTriggerPair calls this pass
};

PhysicsDebugStats PhysicsSystem_GetDebugStats();
void PhysicsSystem_NotifyContactResolved();

// Deferred collider wireframes queued during Update; flush in Draw phase.
void PhysicsSystem_QueueDebugWire(Collider* collider, const DirectX::XMFLOAT4& color);
void PhysicsSystem_FlushDebugDraw();

struct RayHit {
	GameObject* object = nullptr;
	DirectX::XMFLOAT3 point{};
	DirectX::XMFLOAT3 normal{};
	float distance = 0.0f;
};

// Nearest-hit raycast against active scene colliders. skipTriggers defaults false
// (raycast hits triggers unless the caller opts out).
bool PhysicsSystem_Raycast(const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& dir, float maxDistance, RayHit& outHit,
	bool skipTriggers = false);

#endif // PHYSICS_SYSTEM_H
