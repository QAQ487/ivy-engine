/**===========================================================================

 File   :  [physics_system.h]

 Author : Hayato Otsuki
 Date   : 2025/10/18
 -----------------------------------------------------------------------------

 ===========================================================================**/

#ifndef PHYSICS_SYSTEM_H
#define PHYSICS_SYSTEM_H

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

#endif // PHYSICS_SYSTEM_H

