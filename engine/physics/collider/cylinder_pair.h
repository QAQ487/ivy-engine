/**===========================================================================
 File   :  [cylinder_pair.h]
 Date   : 2026/11/01
 -----------------------------------------------------------------------------
 Convex contacts for Box/Cylinder, Cylinder/Cylinder, and Triangle/Cylinder.
 The normal points from B toward A so ResolveContact(A, B, normal) pushes A
 away from B. Penetration is the minimum translation depth. The contact point
 is the midpoint of the two witness points.
 Zero or negative cylinder radius or half-height is not a hit.
===========================================================================**/

#ifndef CYLINDER_PAIR_H
#define CYLINDER_PAIR_H

enum class CylinderPairShapeKind {
    Box,
    Cylinder,
    Triangle,
};

struct CylinderPairVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Box: center, orthonormal axisX/Y/Z, extents = half extents.
// Cylinder: center, unit axis in axisY, extents.x = radius, extents.y = half height.
// Triangle: v0 = center, v1 = tri1, v2 = tri2 (zero thickness).
struct CylinderPairShape {
    CylinderPairShapeKind kind = CylinderPairShapeKind::Cylinder;
    CylinderPairVec3 center{};
    CylinderPairVec3 axisX{ 1.0f, 0.0f, 0.0f };
    CylinderPairVec3 axisY{ 0.0f, 1.0f, 0.0f };
    CylinderPairVec3 axisZ{ 0.0f, 0.0f, 1.0f };
    CylinderPairVec3 extents{ 0.5f, 0.5f, 0.5f };
    CylinderPairVec3 tri1{};
    CylinderPairVec3 tri2{};
};

// Returns true when the convex shapes overlap or touch.
// outNormal is from b toward a (unit). outPoint may be null.
bool CylinderPair_Contact(const CylinderPairShape& a,
    const CylinderPairShape& b,
    CylinderPairVec3& outNormal,
    float& outPenetration,
    CylinderPairVec3* outPoint);

#endif
