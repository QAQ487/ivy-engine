/**===========================================================================
 File   :  [collider.cpp]
 Author : Terry Yeung
 Date   : 2025/10/29
 -----------------------------------------------------------------------------
 Class Summary (BoxCollider detailed collision):
  - BoxCollider::ResolveCollision for Sphere / Capsule (other->self direction)
  - BoxCollider::ResolveCollision for Box vs Box collision (SAT overlap test)
  - Each collider calls ResolveContact logic
  - Collision response (impulse & position correction) logic implementation
===========================================================================**/

#include "collider.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "../core/rigidbody.h"
#include <DirectXMath.h>
#include <algorithm>
#include "engine/debug_draw/debug_drawer.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/core/debug_system.h"
#include "engine/physics/core/physics_system.h"
#include <cmath>
#include <cfloat>
#include <cstdio>

using namespace DirectX;

namespace {
void GetOwnerPose(const GameObject* owner, XMFLOAT3& pos, XMFLOAT3& rot, XMFLOAT3& scale) {
    if (!owner) {
        pos = rot = scale = {0.0f, 0.0f, 0.0f};
        return;
    }
    // Use physics pose (m_position), not GetWorldPose/Transform. Transform can lag
    // one frame behind rigidbody integration; CCD then rays the wrong segment.
    pos = owner->GetPosition();
    rot = owner->GetRotation();
    scale = owner->GetScale();
}

float AbsAxis(float v) {
    if (!std::isfinite(v))
        return 1.0f;
    return v < 0.0f ? -v : v;
}

XMFLOAT3 FiniteOwnerScale(const GameObject* owner) {
    XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
    if (!owner)
        return scale;
    XMFLOAT3 pos, rot;
    GetOwnerPose(owner, pos, rot, scale);
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z))
        return { 1.0f, 1.0f, 1.0f };
    return scale;
}

bool CollisionPairIgnored(const GameObject* a, const GameObject* b) {
    if (!a || !b) return true;
    return a->IgnoresCollisionWith(b) || b->IgnoresCollisionWith(a);
}

const char* ColliderTypeName(ColliderType type) {
    switch (type) {
    case ColliderType::BOX: return "Box";
    case ColliderType::SPHERE: return "Sphere";
    case ColliderType::CAPSULE: return "Capsule";
    case ColliderType::MESH: return "Mesh";
    case ColliderType::CYLINDER: return "Cylinder";
    default: return "None";
    }
}

// Once per unordered pair. Physics queries these every frame; a hit must not spam the log.
void LogUnsupportedCylinderPair(ColliderType a, ColliderType b) {
    constexpr int kTypeCount = static_cast<int>(ColliderType::CYLINDER) + 1;
    static bool logged[kTypeCount][kTypeCount] = {};
    const int ia = static_cast<int>(a);
    const int ib = static_cast<int>(b);
    if (ia < 0 || ia >= kTypeCount || ib < 0 || ib >= kTypeCount)
        return;
    if (logged[ia][ib] || logged[ib][ia])
        return;
    logged[ia][ib] = true;
    logged[ib][ia] = true;

    char msg[160];
    std::snprintf(msg, sizeof(msg),
        "[Collider] Unsupported cylinder pair: %s vs %s (no collision)\n",
        ColliderTypeName(a), ColliderTypeName(b));
    OutputDebugStringA(msg);
}

// A world-space sphere becomes an ellipsoid in mesh-local space under non-uniform scale.
// Use the smallest scale axis so the local test sphere conservatively encloses that ellipsoid.
float MeshLocalTestRadius(float worldRadius, const XMFLOAT3& effScale) {
    // Guard: clamp the *minimum axis* away from zero ? do NOT include epsilon in std::min,
    // or minScale is always 1e-6 and local radius becomes worldRadius*1e6 (player launched).
    const float minScale = (std::min)({ effScale.x, effScale.y, effScale.z });
    return worldRadius / (std::max)(minScale, 1e-6f);
}

float LocalPenetrationToWorld(float localPenetration,
    const XMFLOAT3& localNormal,
    const XMFLOAT3& effScale,
    FXMMATRIX rotM) {
    if (localPenetration <= 0.0f) return 0.0f;
    XMVECTOR localPush = XMVectorSet(
        localNormal.x * localPenetration,
        localNormal.y * localPenetration,
        localNormal.z * localPenetration,
        0.0f);
    XMVECTOR scaledPush = XMVectorSet(
        XMVectorGetX(localPush) * effScale.x,
        XMVectorGetY(localPush) * effScale.y,
        XMVectorGetZ(localPush) * effScale.z,
        0.0f);
    XMVECTOR worldPush = XMVector3TransformNormal(scaledPush, rotM);
    return XMVectorGetX(XMVector3Length(worldPush));
}

float MeshSizePaddingScale(float localBoundsRadius,
    float sizePadding,
    const XMFLOAT3& ownerScale) {
    if (sizePadding <= 0.0f) return 1.0f;
    const float maxOwnerScale = (std::max)({ ownerScale.x, ownerScale.y, ownerScale.z, 1e-6f });
    const float baseWorldRadius = localBoundsRadius * maxOwnerScale;
    if (baseWorldRadius <= 0.0f) return 1.0f;
    return (baseWorldRadius + sizePadding) / baseWorldRadius;
}

XMFLOAT3 StabilizeFloorContactNormal(const XMFLOAT3& worldNormal) {
    // Rim triangles on a flat disc can return tilted/edge normals and cause wobble.
    if (worldNormal.y > 0.5f)
        return {0.0f, 1.0f, 0.0f};
    return worldNormal;
}

struct CylinderWorldFrame {
    XMFLOAT3 center = {0, 0, 0};
    XMFLOAT3 axis = {0, 1, 0};
    XMFLOAT3 axisX = {1, 0, 0};
    XMFLOAT3 axisZ = {0, 0, 1};
    float radius = 0.0f;
    float halfHeight = 0.0f;
};

static CylinderWorldFrame BuildCylinderWorldFrame(const CylinderCollider* cyl) {
    CylinderWorldFrame frame;
    if (!cyl) return frame;

    frame.center = cyl->GetWorldCenter();
    frame.radius = cyl->GetRadius();
    frame.halfHeight = cyl->GetHeight() * 0.5f;

    GameObject* owner = cyl->GetOwner();
    if (!owner) return frame;

    XMFLOAT3 rot = owner->GetRotation();
    XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rot.x),
        XMConvertToRadians(rot.y),
        XMConvertToRadians(rot.z));

    XMVECTOR axisY = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotM));
    XMVECTOR axisX = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), rotM));
    XMVECTOR axisZ = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotM));
    XMStoreFloat3(&frame.axis, axisY);
    XMStoreFloat3(&frame.axisX, axisX);
    XMStoreFloat3(&frame.axisZ, axisZ);
    return frame;
}

static XMFLOAT3 WorldPointToCylinderLocal(const XMFLOAT3& worldPt, const CylinderWorldFrame& frame) {
    XMFLOAT3 rel = {
        worldPt.x - frame.center.x,
        worldPt.y - frame.center.y,
        worldPt.z - frame.center.z
    };
    return {
        rel.x * frame.axisX.x + rel.y * frame.axisX.y + rel.z * frame.axisX.z,
        rel.x * frame.axis.x + rel.y * frame.axis.y + rel.z * frame.axis.z,
        rel.x * frame.axisZ.x + rel.y * frame.axisZ.y + rel.z * frame.axisZ.z
    };
}

static XMFLOAT3 CylinderLocalToWorld(const XMFLOAT3& localPt, const CylinderWorldFrame& frame) {
    return {
        frame.center.x + localPt.x * frame.axisX.x + localPt.y * frame.axis.x + localPt.z * frame.axisZ.x,
        frame.center.y + localPt.x * frame.axisX.y + localPt.y * frame.axis.y + localPt.z * frame.axisZ.y,
        frame.center.z + localPt.x * frame.axisX.z + localPt.y * frame.axis.z + localPt.z * frame.axisZ.z
    };
}

static XMFLOAT3 ClosestPointOnCylinderLocal(const XMFLOAT3& localPt, float radius, float halfHeight) {
    const float y = (std::max)(-halfHeight, (std::min)(halfHeight, localPt.y));
    float x = localPt.x;
    float z = localPt.z;
    const float distSqXZ = x * x + z * z;
    if (distSqXZ > radius * radius && distSqXZ > 1e-12f) {
        const float s = radius / std::sqrt(distSqXZ);
        x *= s;
        z *= s;
    }
    return { x, y, z };
}

static bool SphereCylinder_Contact(const XMFLOAT3& sphereCenter,
    float sphereRadius,
    const CylinderWorldFrame& frame,
    XMFLOAT3& outNormal,
    float& outPenetration) {
    const XMFLOAT3 local = WorldPointToCylinderLocal(sphereCenter, frame);
    const XMFLOAT3 closestLocal = ClosestPointOnCylinderLocal(local, frame.radius, frame.halfHeight);
    const XMFLOAT3 closestWorld = CylinderLocalToWorld(closestLocal, frame);

    const float dx = sphereCenter.x - closestWorld.x;
    const float dy = sphereCenter.y - closestWorld.y;
    const float dz = sphereCenter.z - closestWorld.z;
    const float distSq = dx * dx + dy * dy + dz * dz;
    const float r = sphereRadius;

    if (distSq > r * r) return false;

    const float dist = std::sqrt((std::max)(distSq, 1e-12f));
    if (dist > 1e-6f) {
        outNormal = { dx / dist, dy / dist, dz / dist };
        outPenetration = r - dist;
    } else {
        outNormal = frame.axis;
        outPenetration = r;
    }

  // Stable floor response on the top cap.
    if (local.y >= 0.0f && local.y <= frame.halfHeight + 1e-3f) {
        const float distXZ = std::sqrt(local.x * local.x + local.z * local.z);
        if (distXZ <= frame.radius + 1e-3f)
            outNormal = frame.axis;
    }

    return true;
}
} // namespace

//-----------------------------------------------
// Helper functions
//-----------------------------------------------
static inline XMVECTOR Load(const XMFLOAT3& f) { return XMLoadFloat3(&f); }

static XMFLOAT3 ClosestPointOnOBB_World(const XMFLOAT3& pointWS,
    const XMFLOAT3& boxCenterWS,
    const XMFLOAT3& boxSize,
    const XMFLOAT3& boxRotDeg) {
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(boxRotDeg.x),
        XMConvertToRadians(boxRotDeg.y),
        XMConvertToRadians(boxRotDeg.z));
    XMMATRIX InvR = XMMatrixInverse(nullptr, R);

    XMVECTOR p = Load(pointWS);
    XMVECTOR c = Load(boxCenterWS);
    XMVECTOR local = XMVector3TransformCoord(p - c, InvR);

    XMFLOAT3 l; XMStoreFloat3(&l, local);
    XMFLOAT3 half{ boxSize.x * 0.5f, boxSize.y * 0.5f, boxSize.z * 0.5f };
    l.x = max(-half.x, min(half.x, l.x));
    l.y = max(-half.y, min(half.y, l.y));
    l.z = max(-half.z, min(half.z, l.z));

    XMVECTOR world = XMVector3TransformCoord(XMLoadFloat3(&l), R) + c;
    XMFLOAT3 out; XMStoreFloat3(&out, world);
    return out;
}

// Sphere vs OBB collision detection
// outNormal: other(BOX) -> self(SPHERE) direction
static bool SphereOBB_Contact(const XMFLOAT3& sphereCenter,
    float radius,
    const XMFLOAT3& boxCenter,
    const XMFLOAT3& boxSize,
    const XMFLOAT3& boxRotDeg,
    XMFLOAT3& outNormal,
    float& outPenetration,
    XMFLOAT3* outContact = nullptr) {
    XMFLOAT3 closest = ClosestPointOnOBB_World(sphereCenter, boxCenter, boxSize, boxRotDeg);
    float dx = sphereCenter.x - closest.x;
    float dy = sphereCenter.y - closest.y;
    float dz = sphereCenter.z - closest.z;
    float distSq = dx * dx + dy * dy + dz * dz;
    float r = radius;
    if (distSq <= r * r) {
        float dist = std::sqrt(max(distSq, 1e-12f));
        if (dist > 1e-6f) {
            outNormal = { dx / dist, dy / dist, dz / dist };
            outPenetration = r - dist;
        }
        else {
            // Centers match case
            outNormal = { sphereCenter.x - boxCenter.x,
                sphereCenter.y - boxCenter.y,
                sphereCenter.z - boxCenter.z };
            float nlen = std::sqrt(max(outNormal.x * outNormal.x + outNormal.y * outNormal.y + outNormal.z * outNormal.z, 1e-12f));
            outNormal.x /= nlen; outNormal.y /= nlen; outNormal.z /= nlen;
            outPenetration = r;
        }
        if (outContact) {
            *outContact = {
                sphereCenter.x - outNormal.x * r,
                sphereCenter.y - outNormal.y * r,
                sphereCenter.z - outNormal.z * r
            };
        }
        return true;
    }
    return false;
}

XMFLOAT3 Collider_ComputeLocalInverseInertia(const Collider* collider, float mass, bool kinematic) {
    if (kinematic || mass <= 0.0f)
        return {0.0f, 0.0f, 0.0f};
    if (!collider)
        return Rigidbody_InvInertiaUnitCube(mass);

    switch (collider->GetType()) {
    case ColliderType::BOX: {
        const auto* box = static_cast<const BoxCollider*>(collider);
        return Rigidbody_InvInertiaBox(mass, box->GetSize());
    }
    case ColliderType::SPHERE: {
        const auto* sphere = static_cast<const SphereCollider*>(collider);
        return Rigidbody_InvInertiaSphere(mass, sphere->GetRadius());
    }
    case ColliderType::CAPSULE: {
        const auto* cap = static_cast<const CapsuleCollider*>(collider);
        const float hh = 0.5f * cap->GetHeight();
        const float r = cap->GetRadius();
        return Rigidbody_InvInertiaSphere(mass, std::sqrt(r * r + hh * hh));
    }
    case ColliderType::CYLINDER: {
        const auto* cyl = static_cast<const CylinderCollider*>(collider);
        const float hh = 0.5f * cyl->GetHeight();
        const float r = cyl->GetRadius();
        return Rigidbody_InvInertiaSphere(mass, std::sqrt(r * r + hh * hh));
    }
    case ColliderType::MESH: {
        const auto* mesh = static_cast<const MeshCollider*>(collider);
        return Rigidbody_InvInertiaSphere(mass, mesh->GetWorldBoundingSphereRadius());
    }
    default:
        return Rigidbody_InvInertiaUnitCube(mass);
    }
}

//-----------------------------------------------
// Collider base functionality - world center from local offset
//-----------------------------------------------
XMFLOAT3 Collider::GetScaledLocalCenter() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    return {
        m_center.x * scale.x,
        m_center.y * scale.y,
        m_center.z * scale.z
    };
}

XMFLOAT3 Collider::GetWorldCenter() const {
    if (!m_owner) return m_center;
    XMFLOAT3 pos, rot, scale;
    GetOwnerPose(m_owner, pos, rot, scale);

    const XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rot.x),
        XMConvertToRadians(rot.y),
        XMConvertToRadians(rot.z));

    const XMFLOAT3 scaledCenter = GetScaledLocalCenter();
    XMVECTOR local = XMLoadFloat3(&scaledCenter);
    XMVECTOR world = XMVector3TransformCoord(local, rotMatrix) + XMLoadFloat3(&pos);
    XMFLOAT3 w;
    XMStoreFloat3(&w, world);
    return w;
}

XMFLOAT3 BoxCollider::GetSize() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    return {
        m_size.x * AbsAxis(scale.x),
        m_size.y * AbsAxis(scale.y),
        m_size.z * AbsAxis(scale.z)
    };
}

float SphereCollider::GetRadius() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    const float axis = (std::max)({ AbsAxis(scale.x), AbsAxis(scale.y), AbsAxis(scale.z) });
    return m_radius * axis;
}

float CapsuleCollider::GetRadius() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    const float axis = (std::max)(AbsAxis(scale.x), AbsAxis(scale.z));
    return m_radius * axis;
}

float CapsuleCollider::GetHeight() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    return m_height * AbsAxis(scale.y);
}

float CylinderCollider::GetRadius() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    const float axis = (std::max)(AbsAxis(scale.x), AbsAxis(scale.z));
    return m_radius * axis;
}

float CylinderCollider::GetHeight() const {
    const XMFLOAT3 scale = FiniteOwnerScale(m_owner);
    return m_height * AbsAxis(scale.y);
}

bool Collider::RayIntersect(const DirectX::XMFLOAT3& /*origin*/,
	const DirectX::XMFLOAT3& /*dir*/, float /*maxDistance*/,
	float& /*outT*/, DirectX::XMFLOAT3& /*outNormal*/) const {
	return false;
}

namespace {
bool RayIntersectSphereWorld(const XMFLOAT3& origin, const XMFLOAT3& dir,
	const XMFLOAT3& center, float radius, float maxDistance,
	float& outT, XMFLOAT3& outNormal) {
	if (radius <= 0.0f || maxDistance <= 0.0f) return false;
	const float ox = origin.x - center.x;
	const float oy = origin.y - center.y;
	const float oz = origin.z - center.z;
	const float a = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
	if (a < 1e-12f) return false;
	const float b = 2.0f * (ox * dir.x + oy * dir.y + oz * dir.z);
	const float c = ox * ox + oy * oy + oz * oz - radius * radius;
	const float disc = b * b - 4.0f * a * c;
	if (disc < 0.0f) return false;
	const float sqrtD = std::sqrt(disc);
	const float inv2a = 0.5f / a;
	float t0 = (-b - sqrtD) * inv2a;
	float t1 = (-b + sqrtD) * inv2a;
	float t = t0;
	if (t <= 1e-6f) t = t1;
	if (t <= 1e-6f || t > maxDistance) return false;
	outT = t;
	XMFLOAT3 hit = {
		origin.x + dir.x * t,
		origin.y + dir.y * t,
		origin.z + dir.z * t
	};
	XMVECTOR n = XMVector3Normalize(XMVectorSet(
		hit.x - center.x, hit.y - center.y, hit.z - center.z, 0.0f));
	XMStoreFloat3(&outNormal, n);
	return true;
}
} // namespace

bool SphereCollider::RayIntersect(const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& dir, float maxDistance,
	float& outT, DirectX::XMFLOAT3& outNormal) const {
	return RayIntersectSphereWorld(origin, dir, GetWorldCenter(), GetRadius(),
		maxDistance, outT, outNormal);
}

bool BoxCollider::RayIntersect(const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& dir, float maxDistance,
	float& outT, DirectX::XMFLOAT3& outNormal) const {
	if (!m_owner || maxDistance <= 0.0f) return false;

	const XMFLOAT3 center = GetWorldCenter();
	const XMFLOAT3 size = GetSize();
	const XMFLOAT3 half = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
	const XMFLOAT3 rot = m_owner->GetRotation();

	XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(rot.x),
		XMConvertToRadians(rot.y),
		XMConvertToRadians(rot.z));
	XMMATRIX InvR = XMMatrixTranspose(R);

	XMVECTOR vOrig = XMLoadFloat3(&origin);
	XMVECTOR vC = XMLoadFloat3(&center);
	XMVECTOR localO = XMVector3TransformCoord(XMVectorSubtract(vOrig, vC), InvR);
	XMVECTOR localD = XMVector3TransformNormal(XMLoadFloat3(&dir), InvR);
	XMFLOAT3 o, d;
	XMStoreFloat3(&o, localO);
	XMStoreFloat3(&d, localD);

	float tMin = 0.0f;
	float tMax = maxDistance;
	int hitAxis = -1;
	float hitSign = 1.0f;

	auto slab = [&](float oComp, float dComp, float h, int axis) -> bool {
		if (std::fabs(dComp) < 1e-8f) {
			return oComp >= -h && oComp <= h;
		}
		const float invD = 1.0f / dComp;
		float t1 = (-h - oComp) * invD;
		float t2 = ( h - oComp) * invD;
		float signNear = -1.0f;
		if (t1 > t2) {
			std::swap(t1, t2);
			signNear = 1.0f;
		} else {
			signNear = -1.0f;
		}
		if (t1 > tMin) {
			tMin = t1;
			hitAxis = axis;
			hitSign = signNear;
		}
		if (t2 < tMax) tMax = t2;
		return tMin <= tMax;
	};

	if (!slab(o.x, d.x, half.x, 0)) return false;
	if (!slab(o.y, d.y, half.y, 1)) return false;
	if (!slab(o.z, d.z, half.z, 2)) return false;
	if (tMin <= 1e-6f || tMin > maxDistance) return false;

	outT = tMin;
	XMFLOAT3 localN = { 0, 0, 0 };
	if (hitAxis == 0) localN.x = hitSign;
	else if (hitAxis == 1) localN.y = hitSign;
	else localN.z = hitSign;
	XMVECTOR worldN = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&localN), R));
	XMStoreFloat3(&outNormal, worldN);
	return true;
}

bool CapsuleCollider::RayIntersect(const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& dir, float maxDistance,
	float& outT, DirectX::XMFLOAT3& outNormal) const {
	if (!m_owner || maxDistance <= 0.0f || GetRadius() <= 0.0f) return false;

	const XMFLOAT3 center = GetWorldCenter();
	const float halfHeight = GetHeight() * 0.5f;
	const XMFLOAT3 rot = m_owner->GetRotation();
	XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(rot.x),
		XMConvertToRadians(rot.y),
		XMConvertToRadians(rot.z));
	XMVECTOR axis = XMVector3Normalize(
		XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotM));
	XMFLOAT3 axisF;
	XMStoreFloat3(&axisF, axis);

	XMFLOAT3 pa = {
		center.x - axisF.x * halfHeight,
		center.y - axisF.y * halfHeight,
		center.z - axisF.z * halfHeight
	};
	XMFLOAT3 pb = {
		center.x + axisF.x * halfHeight,
		center.y + axisF.y * halfHeight,
		center.z + axisF.z * halfHeight
	};

	// Inigo Quilez-style ray-capsule (body cylinder + end caps).
	XMVECTOR ba = XMVectorSubtract(XMLoadFloat3(&pb), XMLoadFloat3(&pa));
	XMVECTOR oa = XMVectorSubtract(XMLoadFloat3(&origin), XMLoadFloat3(&pa));
	XMVECTOR rd = XMLoadFloat3(&dir);
	const float baba = XMVectorGetX(XMVector3Dot(ba, ba));
	const float bard = XMVectorGetX(XMVector3Dot(ba, rd));
	const float baoa = XMVectorGetX(XMVector3Dot(ba, oa));
	const float rdoa = XMVectorGetX(XMVector3Dot(rd, oa));
	const float oaoa = XMVectorGetX(XMVector3Dot(oa, oa));
	const float ra = GetRadius();
	const float a = baba - bard * bard;
	float bestT = maxDistance + 1.0f;
	bool hit = false;
	XMFLOAT3 bestN{};

	auto considerT = [&](float t, const XMFLOAT3& n) {
		if (t > 1e-6f && t <= maxDistance && t < bestT) {
			bestT = t;
			bestN = n;
			hit = true;
		}
	};

	if (std::fabs(a) > 1e-8f) {
		const float b = baba * rdoa - baoa * bard;
		const float c = baba * oaoa - baoa * baoa - ra * ra * baba;
		const float h = b * b - a * c;
		if (h >= 0.0f) {
			const float t = (-b - std::sqrt(h)) / a;
			const float y = baoa + t * bard;
			if (y > 0.0f && y < baba && t > 1e-6f && t <= maxDistance) {
				XMFLOAT3 hitPt = {
					origin.x + dir.x * t,
					origin.y + dir.y * t,
					origin.z + dir.z * t
				};
				const float s = y / baba;
				XMFLOAT3 axisPt = {
					pa.x + (pb.x - pa.x) * s,
					pa.y + (pb.y - pa.y) * s,
					pa.z + (pb.z - pa.z) * s
				};
				XMVECTOR n = XMVector3Normalize(XMVectorSet(
					hitPt.x - axisPt.x, hitPt.y - axisPt.y, hitPt.z - axisPt.z, 0.0f));
				XMFLOAT3 nf;
				XMStoreFloat3(&nf, n);
				considerT(t, nf);
			}
		}
	}

	float tCap = 0.0f;
	XMFLOAT3 nCap{};
	if (RayIntersectSphereWorld(origin, dir, pa, ra, maxDistance, tCap, nCap))
		considerT(tCap, nCap);
	if (RayIntersectSphereWorld(origin, dir, pb, ra, maxDistance, tCap, nCap))
		considerT(tCap, nCap);

	if (!hit) return false;
	outT = bestT;
	outNormal = bestN;
	return true;
}

bool MeshCollider::RayIntersect(const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& dir, float maxDistance,
	float& outT, DirectX::XMFLOAT3& outNormal) const {
	XMFLOAT3 hitPt{}, hitN{};
	if (!Raycast(origin, dir, maxDistance, hitPt, hitN))
		return false;
	outT = std::sqrt(
		(hitPt.x - origin.x) * (hitPt.x - origin.x) +
		(hitPt.y - origin.y) * (hitPt.y - origin.y) +
		(hitPt.z - origin.z) * (hitPt.z - origin.z));
	outNormal = hitN;
	return outT > 1e-6f && outT <= maxDistance;
}

//-----------------------------------------------
// Collision resolution (normal: other -> self)
//-----------------------------------------------
enum class ContactSolve : int {
    Position = 1,
    Velocity = 2,
    All = 3
};

static XMFLOAT3 Cross3(const XMFLOAT3& a, const XMFLOAT3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static XMFLOAT3 PointVelocity(const XMFLOAT3& v, const XMFLOAT3& w, const XMFLOAT3& r) {
    const XMFLOAT3 wxr = Cross3(w, r);
    return { v.x + wxr.x, v.y + wxr.y, v.z + wxr.z };
}

static float AngularDenom(const XMFLOAT3& r, const XMFLOAT3& dir,
    const XMFLOAT3& localInv, const XMFLOAT3& rotDeg)
{
    const XMFLOAT3 rxn = Cross3(r, dir);
    const XMFLOAT3 Iinv = Rigidbody_ApplyInvInertiaWorld(localInv, rotDeg, rxn);
    return rxn.x * Iinv.x + rxn.y * Iinv.y + rxn.z * Iinv.z;
}

static void ApplyBodyImpulse(Rigidbody& rb, bool kinematic, float invMass,
    const XMFLOAT3& localInv, const XMFLOAT3& rotDeg,
    const XMFLOAT3& r, const XMFLOAT3& jWorld,
    XMFLOAT3& vel, XMFLOAT3& omega)
{
    if (kinematic) return;
    vel.x += jWorld.x * invMass;
    vel.y += jWorld.y * invMass;
    vel.z += jWorld.z * invMass;
    const XMFLOAT3 dw = Rigidbody_ApplyInvInertiaWorld(localInv, rotDeg, Cross3(r, jWorld));
    omega.x += dw.x;
    omega.y += dw.y;
    omega.z += dw.z;
    rb.SetVelocity(vel);
    rb.SetAngularVelocity(omega);
}

static void ResolveContact(GameObject* self, GameObject* other,
                           const XMFLOAT3& normal,
                           float penetration,
                           float slope = 0.0005f,
                           const XMFLOAT3* contactPoint = nullptr,
                           ContactSolve solve = ContactSolve::All) {
    PhysicsSystem_NotifyContactResolved();

    // Track contact pairs; red flash markers only on NEW contacts.
    // Continuous green/yellow wires come from PhysicsSystem_FlushDebugDraw (F11).
    const bool knownContact = PhysicsSystem_WasContactPairKnown(self, other);
    if (DebugSystem::CollisionVizEnabled() && !knownContact) {
        // Place near self toward other ? midpoint of world centers is wrong
        // for large mesh colliders (field) and sits far from the real hit.
        XMFLOAT3 cA = self->GetPosition();
        if (contactPoint) {
            cA = *contactPoint;
        } else if (Collider* colA = self->GetCollider()) {
            cA = colA->GetWorldCenter();
        }
        constexpr float kAlong = 0.4f;
        constexpr float kNormalLen = 0.55f;
        const XMFLOAT3 point = contactPoint ? *contactPoint : XMFLOAT3{
            cA.x - normal.x * kAlong,
            cA.y - normal.y * kAlong,
            cA.z - normal.z * kAlong
        };
        const XMFLOAT3 tip{
            point.x + normal.x * kNormalLen,
            point.y + normal.y * kNormalLen,
            point.z + normal.z * kNormalLen
        };
        PhysicsSystem_QueueContactFlash(point, tip);
    }

    Rigidbody& rbA = self->GetRigidbody();
    Rigidbody& rbB = other->GetRigidbody();

    bool kinA = rbA.IsKinematic();
    bool kinB = rbB.IsKinematic();

    float mA = rbA.GetMass();
    float mB = rbB.GetMass();

    float invA = (!kinA && mA > 0.0f) ? 1.0f / mA : 0.0f;
    float invB = (!kinB && mB > 0.0f) ? 1.0f / mB : 0.0f;
    float invSum = invA + invB;

    const XMFLOAT3 posA = self->GetPosition();
    const XMFLOAT3 posB = other->GetPosition();
    XMFLOAT3 rotA = self->GetRotation();
    XMFLOAT3 rotB = other->GetRotation();
    const XMFLOAT3 invIA = kinA ? XMFLOAT3{0, 0, 0}
        : Collider_ComputeLocalInverseInertia(self->GetCollider(), mA, false);
    const XMFLOAT3 invIB = kinB ? XMFLOAT3{0, 0, 0}
        : Collider_ComputeLocalInverseInertia(other->GetCollider(), mB, false);
    if (!kinA) rbA.SetLocalInverseInertia(invIA);
    if (!kinB) rbB.SetLocalInverseInertia(invIB);

    const bool hasPoint = contactPoint != nullptr;
    const XMFLOAT3 rA = hasPoint
        ? XMFLOAT3{ contactPoint->x - posA.x, contactPoint->y - posA.y, contactPoint->z - posA.z }
        : XMFLOAT3{ 0, 0, 0 };
    const XMFLOAT3 rB = hasPoint
        ? XMFLOAT3{ contactPoint->x - posB.x, contactPoint->y - posB.y, contactPoint->z - posB.z }
        : XMFLOAT3{ 0, 0, 0 };

    auto effectiveMassInv = [&](const XMFLOAT3& dir) {
        float k = invA + invB;
        if (hasPoint) {
            k += AngularDenom(rA, dir, invIA, rotA);
            k += AngularDenom(rB, dir, invIB, rotB);
        }
        return k;
    };
    const float kNormal = effectiveMassInv(normal);

    ContactSurface surfA = PhysicsMaterial_FromRigidbody(rbA.GetFriction(), rbA.GetRestitution());
    ContactSurface surfB = PhysicsMaterial_FromRigidbody(rbB.GetFriction(), rbB.GetRestitution());
    bool assignedA = false;
    bool assignedB = false;
    if (Collider* colA = self->GetCollider()) {
        if (colA->GetContactSurface().assigned) {
            surfA = colA->GetContactSurface();
            assignedA = true;
        }
    }
    if (Collider* colB = other->GetCollider()) {
        if (colB->GetContactSurface().assigned) {
            surfB = colB->GetContactSurface();
            assignedB = true;
        }
    }
    const bool useMaterial = assignedA || assignedB;
    CombinedContactSurface combined{};
    if (useMaterial)
        combined = PhysicsMaterial_Combine(surfA, surfB);
    const float restitution = useMaterial
        ? combined.restitution
        : (std::max)(rbA.GetRestitution(), rbB.GetRestitution());

    // Position correction is linear-only. Angular pose at a long lever spends
    // the impulse on rotation and overwrites Integrate, which fights gravity tip.
    const bool doPosition = (static_cast<int>(solve) & static_cast<int>(ContactSolve::Position)) != 0;
    const bool doVelocity = (static_cast<int>(solve) & static_cast<int>(ContactSolve::Velocity)) != 0;
    float pen = max(0.0f, penetration - slope) * 0.95f;
    if (doPosition && pen > 0.0f && invSum > 0.0f) {
        constexpr float kPosBeta = 0.75f;
        float corr = kPosBeta * pen;
        constexpr float kMaxLin = 0.2f;
        if (corr > kMaxLin)
            corr = kMaxLin;
        const float moveA = invA / invSum;
        const float moveB = invB / invSum;
        if (!kinA) {
            XMFLOAT3 p = self->GetPosition();
            p.x += normal.x * corr * moveA;
            p.y += normal.y * corr * moveA;
            p.z += normal.z * corr * moveA;
            self->SetPosition(p);
        }
        if (!kinB) {
            XMFLOAT3 p = other->GetPosition();
            p.x -= normal.x * corr * moveB;
            p.y -= normal.y * corr * moveB;
            p.z -= normal.z * corr * moveB;
            other->SetPosition(p);
        }
    }

    if (!doVelocity)
        return;

    // Relative velocity at the contact (includes omega x r when a point is supplied).
    XMFLOAT3 vA = rbA.GetVelocity();
    XMFLOAT3 vB = rbB.GetVelocity();
    XMFLOAT3 wA = rbA.GetAngularVelocity();
    XMFLOAT3 wB = rbB.GetAngularVelocity();
    const XMFLOAT3 pVelA = PointVelocity(vA, wA, rA);
    const XMFLOAT3 pVelB = PointVelocity(vB, wB, rB);
    XMFLOAT3 vRel{ pVelA.x - pVelB.x, pVelA.y - pVelB.y, pVelA.z - pVelB.z };
    float vnRel = vRel.x * normal.x + vRel.y * normal.y + vRel.z * normal.z;
    // Apply the velocity impulse whenever the objects are approaching, even
    // at low speed.  A threshold of 0.01 left slow approaches uncancelled,
    // allowing micro-penetration that turns into position-correction
    // oscillation at low movement speeds (especially against angled walls).
    float jNormal = 0.0f;
    const float kImpulse = hasPoint ? kNormal : invSum;
    // Velocity impulses only cancel approach. Depenetration is pose-level so
    // Baumgarte cannot pump kinetic energy into spin (bias/dt * r).
    const float bias = 0.0f;
    if (kImpulse > 1e-8f && vnRel < -0.001f) {
        float vnTerm = vnRel;
        if (vnRel < -1.0f)
            vnTerm = (1.0f + restitution) * vnRel;
        const float jCandidate = -vnTerm / kImpulse;
        if (jCandidate > 0.0f) {
            jNormal = jCandidate;
            const XMFLOAT3 jn{ jNormal * normal.x, jNormal * normal.y, jNormal * normal.z };
            ApplyBodyImpulse(rbA, kinA, invA, invIA, rotA, rA, jn, vA, wA);
            const XMFLOAT3 jnB{ -jn.x, -jn.y, -jn.z };
            ApplyBodyImpulse(rbB, kinB, invB, invIB, rotB, rB, jnB, vB, wB);
        }
    }
    // Lever in the contact tangent plane: ~0 means the support is under COM along
    // the normal (face on a floor or ramp). xz-only missed YZ slopes.
    auto rPerp2 = [&](const XMFLOAT3& r) {
        const float rn = r.x * normal.x + r.y * normal.y + r.z * normal.z;
        const float r2 = r.x * r.x + r.y * r.y + r.z * r.z;
        return (std::max)(0.0f, r2 - rn * rn);
    };
    const float rPerp2A = rPerp2(rA);
    const float rPerp2B = rPerp2(rB);
    // Flush face contact is r_perp ~ 0. Keep this tight so a downhill edge
    // can still produce seating torque.
    constexpr float kFaceSupport = 0.02f;
    const bool faceA = hasPoint && rPerp2A < kFaceSupport;
    const bool faceB = hasPoint && rPerp2B < kFaceSupport;
    // Overhang (large tangent lever): keep tip spin, bleed corkscrew.
    // Face support: bleed all spin so a seated box does not rock or roll.
    auto dampTwist = [&](Rigidbody& rb, bool kin, bool face, const XMFLOAT3& r, XMFLOAT3& w) {
        if (kin) return;
        if (face) {
            w.x *= 0.7f;
            w.y *= 0.7f;
            w.z *= 0.7f;
            rb.SetAngularVelocity(w);
            return;
        }
        const XMFLOAT3 axis = Cross3(r, normal);
        const float a2 = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        if (a2 > 1e-6f) {
            const float inv = 1.0f / std::sqrt(a2);
            const float ax = axis.x * inv, ay = axis.y * inv, az = axis.z * inv;
            const float along = w.x * ax + w.y * ay + w.z * az;
            constexpr float kKeepPerp = 0.25f;
            w.x = ax * along + (w.x - ax * along) * kKeepPerp;
            w.y = ay * along + (w.y - ay * along) * kKeepPerp;
            w.z = az * along + (w.z - az * along) * kKeepPerp;
        } else {
            w.x *= 0.8f;
            w.y *= 0.8f;
            w.z *= 0.8f;
        }
        rb.SetAngularVelocity(w);
    };
    if (hasPoint) {
        dampTwist(rbA, kinA, faceA, rA, wA);
        dampTwist(rbB, kinB, faceB, rB, wB);
    }

    auto killTangentialVelocity = [&]() {
        if (!kinA) {
            const float vnA = vA.x * normal.x + vA.y * normal.y + vA.z * normal.z;
            vA.x = vnA * normal.x;
            vA.y = vnA * normal.y;
            vA.z = vnA * normal.z;
            rbA.SetVelocity(vA);
        }
        if (!kinB) {
            const float vnB = vB.x * normal.x + vB.y * normal.y + vB.z * normal.z;
            vB.x = vnB * normal.x;
            vB.y = vnB * normal.y;
            vB.z = vnB * normal.z;
            rbB.SetVelocity(vB);
        }
    };

    auto isBoxOwner = [](GameObject* go) {
        Collider* c = go ? go->GetCollider() : nullptr;
        return c && c->GetType() == ColliderType::BOX;
    };
    // Boxes: friction is linear only so a crate slides instead of rolling from
    // r x f at an edge. Spheres keep angular friction so balls still roll.
    const bool skipFricAngA = isBoxOwner(self);
    const bool skipFricAngB = isBoxOwner(other);
    auto applyFrictionImpulse = [&](float jFriction, const XMFLOAT3& tDir) {
        const XMFLOAT3 jF{ jFriction * tDir.x, jFriction * tDir.y, jFriction * tDir.z };
        const XMFLOAT3 rFricA = skipFricAngA ? XMFLOAT3{ 0, 0, 0 } : rA;
        const XMFLOAT3 rFricB = skipFricAngB ? XMFLOAT3{ 0, 0, 0 } : rB;
        ApplyBodyImpulse(rbA, kinA, invA, invIA, rotA, rFricA, jF, vA, wA);
        const XMFLOAT3 jFB{ -jF.x, -jF.y, -jF.z };
        ApplyBodyImpulse(rbB, kinB, invB, invIB, rotB, rFricB, jFB, vB, wB);
    };

    auto contactRelative = [&]() {
        vA = rbA.GetVelocity();
        vB = rbB.GetVelocity();
        wA = rbA.GetAngularVelocity();
        wB = rbB.GetAngularVelocity();
        const XMFLOAT3 pa = PointVelocity(vA, wA, rA);
        const XMFLOAT3 pb = PointVelocity(vB, wB, rB);
        return XMFLOAT3{ pa.x - pb.x, pa.y - pb.y, pa.z - pb.z };
    };

    auto maybeSleepAngular = [&]() {
        if (!hasPoint) return;
        auto trySleep = [](Rigidbody& rb, bool kin) {
            if (kin) return;
            const XMFLOAT3 v = rb.GetVelocity();
            const XMFLOAT3 w = rb.GetAngularVelocity();
            const float v2 = v.x * v.x + v.y * v.y + v.z * v.z;
            const float w2 = w.x * w.x + w.y * w.y + w.z * w.z;
            if (v2 < 1e-6f && w2 < 1e-5f) {
                rb.SetVelocity({0, 0, 0});
                rb.SetAngularVelocity({0, 0, 0});
            }
        };
        trySleep(rbA, kinA);
        trySleep(rbB, kinB);
    };

    if (useMaterial) {
        if (invSum > 0.0f) {
            vRel = contactRelative();
            const float vn = vRel.x * normal.x + vRel.y * normal.y + vRel.z * normal.z;
            XMFLOAT3 vT{
                vRel.x - vn * normal.x,
                vRel.y - vn * normal.y,
                vRel.z - vn * normal.z
            };
            const float vtLenSq = vT.x * vT.x + vT.y * vT.y + vT.z * vT.z;
            const float vtLen = std::sqrt(vtLenSq);
            XMFLOAT3 tDir{ 0, 0, 0 };
            float kTangent = hasPoint ? kNormal : invSum;
            if (vtLen > 1e-6f) {
                tDir = { -vT.x / vtLen, -vT.y / vtLen, -vT.z / vtLen };
                kTangent = effectiveMassInv(tDir);
                if (kTangent < 1e-8f)
                    kTangent = invSum;
            }
            // Resting contacts often have jn=0 this frame (already separated).
            // Without a normal impulse, Coulomb friction is zero and every pad
            // slides. Approximate the press-in impulse as mg |n.up| dt so that
            // stick requires mu_s >= tan(theta) on a slope (rubber holds, ice does not).
            float jNAbs = std::fabs(jNormal);
            if (jNAbs <= 1e-6f) {
                constexpr float kFixedDt = 1.0f / 60.0f;
                jNAbs = Physics_GetGravity() * std::fabs(normal.y) * kFixedDt / (std::max)(kTangent, 1e-8f);
            }
            const ContactFrictionDecision friction = PhysicsMaterial_DecideFriction(
                vtLen, kTangent, jNAbs, combined.muStatic, combined.muDynamic);
            if (friction.mode == ContactFrictionMode::ResidualStick ||
                friction.mode == ContactFrictionMode::StaticStick) {
                if (hasPoint && vtLen > 1e-6f && friction.jFriction > 0.0f) {
                    applyFrictionImpulse(friction.jFriction, tDir);
                } else {
                    killTangentialVelocity();
                }
                maybeSleepAngular();
                // Integrate-then-collide: gravity already moved the body this
                // frame. Killing vt leaves that downhill step in position, so
                // high-mu rubber still creeps. Undo this frame's tangent slide.
                auto rewindTangentStep = [&](GameObject* obj, bool kin) {
                    if (kin || !obj) return;
                    const XMFLOAT3 cur = obj->GetPosition();
                    const XMFLOAT3 prev = obj->m_previousPosition;
                    const XMFLOAT3 d{ cur.x - prev.x, cur.y - prev.y, cur.z - prev.z };
                    const float dn = d.x * normal.x + d.y * normal.y + d.z * normal.z;
                    obj->SetPosition({
                        cur.x - (d.x - dn * normal.x),
                        cur.y - (d.y - dn * normal.y),
                        cur.z - (d.z - dn * normal.z)
                    });
                };
                rewindTangentStep(self, kinA);
                rewindTangentStep(other, kinB);
            } else if (friction.mode == ContactFrictionMode::DynamicClamp &&
                       friction.jFriction > 0.0f && vtLen > 1e-6f) {
                applyFrictionImpulse(friction.jFriction, tDir);
            }
        }
    } else {
        // Coulomb friction along the contact tangent. Combine with min() so a body
        // with friction 0 (player dash) never gets tangential grip from the floor.
        const float mu = (std::min)(rbA.GetFriction(), rbB.GetFriction());
        if (mu > 0.0f && invSum > 0.0f) {
            vRel = contactRelative();
            const float vn = vRel.x * normal.x + vRel.y * normal.y + vRel.z * normal.z;
            XMFLOAT3 vT{
                vRel.x - vn * normal.x,
                vRel.y - vn * normal.y,
                vRel.z - vn * normal.z
            };
            const float vtLenSq = vT.x * vT.x + vT.y * vT.y + vT.z * vT.z;
            constexpr float kStickSpeed = 0.05f;
            constexpr float kStickSpeedSq = kStickSpeed * kStickSpeed;

            if (vtLenSq < kStickSpeedSq) {
                if (hasPoint) {
                    const float vtLen = std::sqrt((std::max)(vtLenSq, 0.0f));
                    if (vtLen > 1e-6f) {
                        const XMFLOAT3 tDir{ -vT.x / vtLen, -vT.y / vtLen, -vT.z / vtLen };
                        const float kTangent = effectiveMassInv(tDir);
                        if (kTangent > 1e-8f)
                            applyFrictionImpulse(vtLen / kTangent, tDir);
                    }
                    maybeSleepAngular();
                } else {
                    killTangentialVelocity();
                }
            } else {
                const float vtLen = std::sqrt(vtLenSq);
                const float jNAbs = std::fabs(jNormal);
                // Strict Coulomb: |jt| <= mu * |jn|. No floor "full-cancel" boost ?
                // that made mu=0.5 wipe horizontal speed in a few frames.
                if (jNAbs > 1e-6f) {
                    const XMFLOAT3 tDir{ -vT.x / vtLen, -vT.y / vtLen, -vT.z / vtLen };
                    const float kTangent = hasPoint ? effectiveMassInv(tDir) : invSum;
                    float jFriction = vtLen / (std::max)(kTangent, 1e-8f);
                    const float jMax = mu * jNAbs;
                    if (jFriction > jMax)
                        jFriction = jMax;
                    applyFrictionImpulse(jFriction, tDir);
                }
            }
        }
    }
}

//-----------------------------------------------
// BoxCollider collision
//-----------------------------------------------
bool BoxCollider::CheckCollision(const Collider* other) const {
    if (!other || !m_owner || !other->GetOwner()) return false;

    XMFLOAT3 rotA = m_owner->GetRotation();
    XMMATRIX rotMatrixA = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotA.x),
        XMConvertToRadians(rotA.y),
        XMConvertToRadians(rotA.z));
    XMFLOAT3 centerA = GetWorldCenter();
    XMFLOAT3 sizeA = GetSize(); // world-space size (m_size * owner scale)

    switch (other->GetType()) {
    case ColliderType::BOX:
    {
        const BoxCollider* o = static_cast<const BoxCollider*>(other);
        XMFLOAT3 rotB = o->GetOwner()->GetRotation();
        XMMATRIX rotMatrixB = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotB.x),
            XMConvertToRadians(rotB.y),
            XMConvertToRadians(rotB.z));
        XMFLOAT3 centerB = o->GetWorldCenter();
        XMFLOAT3 sizeB = o->GetSize();

        XMVECTOR axesA[3] = {
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), rotMatrixA)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMatrixA)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotMatrixA))
        };
        XMVECTOR axesB[3] = {
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), rotMatrixB)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMatrixB)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotMatrixB))
        };

        XMVECTOR centerDiff = Load(centerB) - Load(centerA);
        auto Dot = [&](FXMVECTOR a, FXMVECTOR b) {return XMVectorGetX(XMVector3Dot(a, b)); };

        // 15 axis test (SAT)
        for (int i = 0; i < 3; ++i) {
            float projA = 0.5f * sizeA.x * fabs(Dot(axesA[i], axesA[0])) +
                0.5f * sizeA.y * fabs(Dot(axesA[i], axesA[1])) +
                0.5f * sizeA.z * fabs(Dot(axesA[i], axesA[2]));
            float projB = 0.5f * sizeB.x * fabs(Dot(axesA[i], axesB[0])) +
                0.5f * sizeB.y * fabs(Dot(axesA[i], axesB[1])) +
                0.5f * sizeB.z * fabs(Dot(axesA[i], axesB[2]));
            float diff = fabs(Dot(centerDiff, axesA[i]));
            if (diff > projA + projB) return false;
        }
        for (int i = 0; i < 3; ++i) {
            float projA = 0.5f * sizeA.x * fabs(Dot(axesB[i], axesA[0])) +
                0.5f * sizeA.y * fabs(Dot(axesB[i], axesA[1])) +
                0.5f * sizeA.z * fabs(Dot(axesB[i], axesA[2]));
            float projB = 0.5f * sizeB.x * fabs(Dot(axesB[i], axesB[0])) +
                0.5f * sizeB.y * fabs(Dot(axesB[i], axesB[1])) +
                0.5f * sizeB.z * fabs(Dot(axesB[i], axesB[2]));
            float diff = fabs(Dot(centerDiff, axesB[i]));
            if (diff > projA + projB) return false;
        }
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
            XMVECTOR axis = XMVector3Cross(axesA[i], axesB[j]);
            if (XMVectorGetX(XMVector3Length(axis)) < 1e-6f) continue;
            axis = XMVector3Normalize(axis);
            float projA = 0.5f * sizeA.x * fabs(Dot(axis, axesA[0])) +
                0.5f * sizeA.y * fabs(Dot(axis, axesA[1])) +
                0.5f * sizeA.z * fabs(Dot(axis, axesA[2]));
            float projB = 0.5f * sizeB.x * fabs(Dot(axis, axesB[0])) +
                0.5f * sizeB.y * fabs(Dot(axis, axesB[1])) +
                0.5f * sizeB.z * fabs(Dot(axis, axesB[2]));
            float diff = fabs(Dot(centerDiff, axis));
            if (diff > projA + projB) return false;
        }
        return true;
    }
    case ColliderType::SPHERE:
    {
        const SphereCollider* s = static_cast<const SphereCollider*>(other);
        XMFLOAT3 sphereCenter = s->GetWorldCenter();
        float radius = s->GetRadius();

        XMVECTOR vSphereCenter = Load(sphereCenter);
        XMVECTOR vCenterA = Load(centerA);
        XMVECTOR vDiff = vSphereCenter - vCenterA;
        XMVECTOR vLocal = XMVector3TransformNormal(vDiff, XMMatrixTranspose(rotMatrixA));

        XMFLOAT3 local; XMStoreFloat3(&local, vLocal);
        XMFLOAT3 half{ sizeA.x * 0.5f, sizeA.y * 0.5f, sizeA.z * 0.5f };
        float cx = max(-half.x, min(half.x, local.x));
        float cy = max(-half.y, min(half.y, local.y));
        float cz = max(-half.z, min(half.z, local.z));

        XMVECTOR closestW = XMVector3TransformCoord(XMVectorSet(cx, cy, cz, 1.0f), rotMatrixA) + vCenterA;
        XMFLOAT3 cw; XMStoreFloat3(&cw, closestW);
        float dx = cw.x - sphereCenter.x;
        float dy = cw.y - sphereCenter.y;
        float dz = cw.z - sphereCenter.z;
        return dx * dx + dy * dy + dz * dz <= radius * radius;
    }
    case ColliderType::CAPSULE:
    {
        const CapsuleCollider* cap = static_cast<const CapsuleCollider*>(other);
        XMFLOAT3 capCenter = cap->GetWorldCenter();
        float r = cap->GetRadius();
        float hh = cap->GetHeight() * 0.5f;

        XMFLOAT3 rotCap = cap->GetOwner()->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotCap.x),
            XMConvertToRadians(rotCap.y),
            XMConvertToRadians(rotCap.z));
        XMVECTOR axis = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm);
        XMFLOAT3 af; XMStoreFloat3(&af, axis);

        XMFLOAT3 p1{ capCenter.x - af.x * hh, capCenter.y - af.y * hh, capCenter.z - af.z * hh };
        XMFLOAT3 p2{ capCenter.x + af.x * hh, capCenter.y + af.y * hh, capCenter.z + af.z * hh };

        XMFLOAT3 centerA2 = GetWorldCenter();
        XMVECTOR vP1 = Load(p1);
        XMVECTOR vP2 = Load(p2);
        XMVECTOR vDir = vP2 - vP1;
        float len = XMVectorGetX(XMVector3Length(vDir));
        XMFLOAT3 closest = p1;
        if (len >= 1e-4f) {
            XMVECTOR vT = XMVectorClamp(
                XMVectorDivide(
                    XMVector3Dot(Load(centerA2) - vP1, vDir),
                    XMVectorReplicate(len * len)),
                XMVectorZero(), XMVectorReplicate(1.0f));
            XMStoreFloat3(&closest, vP1 + XMVectorMultiply(vDir, vT));
        }
        XMFLOAT3 rotA2 = m_owner->GetRotation();
        XMFLOAT3 boxClosest = ClosestPointOnOBB_World(closest, centerA2, sizeA, rotA2);
        float dx = boxClosest.x - closest.x;
        float dy = boxClosest.y - closest.y;
        float dz = boxClosest.z - closest.z;
        return dx * dx + dy * dy + dz * dz <= r * r;
    }
    case ColliderType::MESH:
        return other->CheckCollision(this);
    case ColliderType::CYLINDER:
        LogUnsupportedCylinderPair(ColliderType::BOX, ColliderType::CYLINDER);
        return false;
    default: return false;
    }
}

static void BoxWorldCorners(const XMFLOAT3& center, const XMFLOAT3& size, const XMFLOAT3& rotDeg, XMFLOAT3 out[8]) {
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotDeg.x),
        XMConvertToRadians(rotDeg.y),
        XMConvertToRadians(rotDeg.z));
    const XMFLOAT3 h{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
    int i = 0;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                XMVECTOR local = XMVectorSet(sx * h.x, sy * h.y, sz * h.z, 0.0f);
                XMVECTOR world = XMVector3TransformNormal(local, R) + XMLoadFloat3(&center);
                XMStoreFloat3(&out[i++], world);
            }
        }
    }
}

static float BoxSupportAlong(const XMFLOAT3& size, FXMVECTOR ax, FXMVECTOR ay, FXMVECTOR az, FXMVECTOR n) {
    return 0.5f * size.x * fabsf(XMVectorGetX(XMVector3Dot(n, ax))) +
           0.5f * size.y * fabsf(XMVectorGetX(XMVector3Dot(n, ay))) +
           0.5f * size.z * fabsf(XMVectorGetX(XMVector3Dot(n, az)));
}

static int CollectBoxContactManifold(
    const XMFLOAT3& centerA, const XMFLOAT3& sizeA, const XMFLOAT3& rotA,
    const XMFLOAT3& centerB, const XMFLOAT3& sizeB, const XMFLOAT3& rotB,
    const XMFLOAT3& nBA,
    XMFLOAT3 outPts[4])
{
    XMMATRIX RA = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotA.x), XMConvertToRadians(rotA.y), XMConvertToRadians(rotA.z));
    XMMATRIX RB = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotB.x), XMConvertToRadians(rotB.y), XMConvertToRadians(rotB.z));
    XMVECTOR axA = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), RA));
    XMVECTOR ayA = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), RA));
    XMVECTOR azA = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), RA));
    XMVECTOR axB = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), RB));
    XMVECTOR ayB = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), RB));
    XMVECTOR azB = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), RB));
    XMVECTOR n = XMLoadFloat3(&nBA);
    const float projA = BoxSupportAlong(sizeA, axA, ayA, azA, n);
    const float projB = BoxSupportAlong(sizeB, axB, ayB, azB, n);

    XMFLOAT3 cornersA[8];
    XMFLOAT3 cornersB[8];
    BoxWorldCorners(centerA, sizeA, rotA, cornersA);
    BoxWorldCorners(centerB, sizeB, rotB, cornersB);

    XMFLOAT3 pts[8];
    int count = 0;
    auto addPt = [&](const XMFLOAT3& p) {
        for (int i = 0; i < count; ++i) {
            const float dx = pts[i].x - p.x;
            const float dy = pts[i].y - p.y;
            const float dz = pts[i].z - p.z;
            if (dx * dx + dy * dy + dz * dz < 1e-4f)
                return;
        }
        if (count < 8)
            pts[count++] = p;
    };

    auto considerIncident = [&](const XMFLOAT3* corners,
        const XMFLOAT3& refCenter, const XMFLOAT3& refSize, const XMFLOAT3& refRot,
        float projRef, const XMFLOAT3& nn)
    {
        for (int i = 0; i < 8; ++i) {
            const XMFLOAT3& c = corners[i];
            const float cN = c.x * nn.x + c.y * nn.y + c.z * nn.z;
            const float refN = refCenter.x * nn.x + refCenter.y * nn.y + refCenter.z * nn.z;
            const float pen = (refN + projRef) - cN;
            if (pen <= 1e-4f)
                continue;
            const XMFLOAT3 onPlane{
                c.x - nn.x * pen,
                c.y - nn.y * pen,
                c.z - nn.z * pen
            };
            const XMFLOAT3 clamped = ClosestPointOnOBB_World(onPlane, refCenter, refSize, refRot);
            const float dx = clamped.x - onPlane.x;
            const float dy = clamped.y - onPlane.y;
            const float dz = clamped.z - onPlane.z;
            if (dx * dx + dy * dy + dz * dz > 0.0625f)
                continue;
            addPt(clamped);
        }
    };

    considerIncident(cornersA, centerB, sizeB, rotB, projB, nBA);
    const XMFLOAT3 nAB{ -nBA.x, -nBA.y, -nBA.z };
    considerIncident(cornersB, centerA, sizeA, rotA, projA, nAB);

    if (count == 0) {
        const XMFLOAT3 mid{
            0.5f * (centerA.x + centerB.x),
            0.5f * (centerA.y + centerB.y),
            0.5f * (centerA.z + centerB.z)
        };
        outPts[0] = ClosestPointOnOBB_World(mid, centerB, sizeB, rotB);
        return 1;
    }
    if (count <= 4) {
        for (int i = 0; i < count; ++i)
            outPts[i] = pts[i];
        return count;
    }

    int idx[8];
    float pens[8];
    const float refN = centerB.x * nBA.x + centerB.y * nBA.y + centerB.z * nBA.z + projB;
    for (int i = 0; i < count; ++i) {
        idx[i] = i;
        pens[i] = refN - (pts[i].x * nBA.x + pts[i].y * nBA.y + pts[i].z * nBA.z);
    }
    for (int a = 0; a < count; ++a) {
        for (int b = a + 1; b < count; ++b) {
            if (pens[idx[b]] > pens[idx[a]]) {
                const int tmp = idx[a];
                idx[a] = idx[b];
                idx[b] = tmp;
            }
        }
    }
    for (int i = 0; i < 4; ++i)
        outPts[i] = pts[idx[i]];
    return 4;
}

void BoxCollider::ResolveCollision(Collider* other) {
    if (!other || !m_owner || !other->GetOwner()) return;
    if (m_isTrigger || other->IsTrigger()) {
        return;
    }
    if (CollisionPairIgnored(m_owner, other->GetOwner())) return;

    XMFLOAT3 sizeA = GetSize(); // world-space size (m_size * owner scale)

    switch (other->GetType()) {
    case ColliderType::SPHERE:
    {
        // Sphere collision
        const SphereCollider* s = static_cast<const SphereCollider*>(other);
        XMFLOAT3 centerSphere = s->GetWorldCenter();
        float r = s->GetRadius();
        XMFLOAT3 centerBox = GetWorldCenter();
        XMFLOAT3 rotBox = m_owner->GetRotation();

        XMFLOAT3 n; float pen = 0.0f; XMFLOAT3 contact{};
        if (SphereOBB_Contact(centerSphere, r, centerBox, sizeA, rotBox, n, pen, &contact)) {
            // SphereOBB_Contact returns n as BOX -> SPHERE
            // ResolveContact(Box, Sphere, normal=other->self) so invert for sphere->box
            XMFLOAT3 nInv{ -n.x, -n.y, -n.z };
            ResolveContact(m_owner, other->GetOwner(), nInv, pen, 0.0005f, &contact);
        }
        break;
    }
    case ColliderType::CAPSULE:
    {
        // 3 point test + sphere collision
        const CapsuleCollider* cap = static_cast<const CapsuleCollider*>(other);
        XMFLOAT3 cC = cap->GetWorldCenter();
        float r = cap->GetRadius();
        float hh = cap->GetHeight() * 0.5f;
        XMFLOAT3 rotCap = cap->GetOwner()->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotCap.x),
            XMConvertToRadians(rotCap.y),
            XMConvertToRadians(rotCap.z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);

        XMFLOAT3 top{ cC.x + af.x * hh, cC.y + af.y * hh, cC.z + af.z * hh };
        XMFLOAT3 bottom{ cC.x - af.x * hh, cC.y - af.y * hh, cC.z - af.z * hh };
        XMFLOAT3 mid{ cC.x, cC.y, cC.z };

        XMFLOAT3 cB = GetWorldCenter();
        XMFLOAT3 rotB = m_owner->GetRotation();

        XMFLOAT3 bestN{ 0, 0, 0 };
        float bestPen = -FLT_MAX;
        auto testOne = [&](const XMFLOAT3& sc) {
            XMFLOAT3 n; float pen = 0.0f;
            if (SphereOBB_Contact(sc, r, cB, sizeA, rotB, n, pen)) {
                if (pen > bestPen) { bestPen = pen; bestN = n; }
            }
            };
        testOne(top); testOne(bottom); testOne(mid);

        if (bestPen > 0.0f) {
            XMFLOAT3 nInv{ -bestN.x, -bestN.y, -bestN.z }; // capsule->box
            ResolveContact(m_owner, other->GetOwner(), nInv, bestPen);
        }
        break;
    }
    case ColliderType::BOX:
    {
        // OBB vs OBB using SAT overlap test
        const BoxCollider* ob = static_cast<const BoxCollider*>(other);
        XMFLOAT3 centerA = GetWorldCenter();
        XMFLOAT3 centerB = ob->GetWorldCenter();
        XMFLOAT3 rotA = m_owner->GetRotation();
        XMFLOAT3 rotB = ob->GetOwner()->GetRotation();
        XMMATRIX RA = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotA.x),
            XMConvertToRadians(rotA.y),
            XMConvertToRadians(rotA.z));
        XMMATRIX RB = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotB.x),
            XMConvertToRadians(rotB.y),
            XMConvertToRadians(rotB.z));
        XMVECTOR axesA[3] = {
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), RA)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), RA)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), RA))
        };
        XMVECTOR axesB[3] = {
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), RB)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), RB)),
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), RB))
        };

        auto Dot = [&](FXMVECTOR a, FXMVECTOR b) { return XMVectorGetX(XMVector3Dot(a, b)); };
        XMVECTOR diff = Load(centerB) - Load(centerA);

        float minOverlap = FLT_MAX;
        XMVECTOR minAxis = XMVectorSet(0, 1, 0, 0);
        bool separated = false;

        // Track the best face-normal axis separately from edge-cross-product axes.
        // Face normals are more stable at angled-wall contacts; preferring them when
        // their overlap is close to the global minimum prevents the contact normal
        // from flipping between face and edge axes on successive frames (which is
        // the root cause of wobbling when pressing against an angled wall).
        float faceMinOverlap = FLT_MAX;
        XMVECTOR faceMinAxis = XMVectorSet(0, 1, 0, 0);

        auto TestAxis = [&](XMVECTOR axis, bool isFaceNormal)->bool {
            float projA =
                0.5f * sizeA.x * fabs(Dot(axis, axesA[0])) +
                0.5f * sizeA.y * fabs(Dot(axis, axesA[1])) +
                0.5f * sizeA.z * fabs(Dot(axis, axesA[2]));
            float projB =
                0.5f * ob->GetSize().x * fabs(Dot(axis, axesB[0])) +
                0.5f * ob->GetSize().y * fabs(Dot(axis, axesB[1])) +
                0.5f * ob->GetSize().z * fabs(Dot(axis, axesB[2]));
            float projDiff = fabs(Dot(diff, axis));
            float overlap = projA + projB - projDiff;
            if (overlap <= 0.0f) { separated = true; return false; }
            if (overlap < minOverlap) { minOverlap = overlap; minAxis = axis; }
            if (isFaceNormal && overlap < faceMinOverlap) { faceMinOverlap = overlap; faceMinAxis = axis; }
            return true;
            };

        for (int i = 0; i < 3; ++i) if (!TestAxis(axesA[i], true)) break;
        if (!separated) for (int i = 0; i < 3; ++i) if (!TestAxis(axesB[i], true)) break;
        if (!separated) {
            for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
                XMVECTOR axis = XMVector3Cross(axesA[i], axesB[j]);
                if (XMVectorGetX(XMVector3Length(axis)) < 1e-6f) continue;
                axis = XMVector3Normalize(axis);
                if (!TestAxis(axis, false)) break;
                if (separated) break;
            }
        }
        if (!separated && minOverlap < FLT_MAX) {
            // Prefer a face-axis normal over an edge-cross-product normal when their
            // overlaps are within kFaceBias.  This keeps the contact normal stable
            // (a face axis won't flip to an edge axis due to floating-point noise),
            // which eliminates the frame-to-frame wobble at angled walls.
            // kFaceBias is in world-space distance units (same as collision overlap):
            // an edge-cross axis must have overlap at least 0.01 units less than the
            // best face axis before it overrides the face normal.
            static constexpr float kFaceBias = 0.01f;
            bool useFace = (faceMinOverlap < FLT_MAX) && (faceMinOverlap <= minOverlap + kFaceBias);
            XMVECTOR resolveAxis = useFace ? faceMinAxis : minAxis;
            float resolveOverlap = useFace ? faceMinOverlap : minOverlap;

            // normal: other(Box B) -> self(Box A) direction. diff = B - A
            float signDot = Dot(diff, resolveAxis);
            if (signDot > 0.0f) resolveAxis = XMVectorNegate(resolveAxis);
            XMFLOAT3 n; XMStoreFloat3(&n, resolveAxis);
            XMFLOAT3 manifold[4];
            const int nPts = CollectBoxContactManifold(
                centerA, sizeA, rotA, centerB, ob->GetSize(), rotB, n, manifold);
            // One pose correction at the manifold centroid so +/-Z corners cannot
            // twist around the thin principal axis (Ixx >> Izz inverse).
            XMFLOAT3 centroid{ 0, 0, 0 };
            for (int i = 0; i < nPts; ++i) {
                centroid.x += manifold[i].x;
                centroid.y += manifold[i].y;
                centroid.z += manifold[i].z;
            }
            const float invN = 1.0f / static_cast<float>(nPts);
            centroid.x *= invN;
            centroid.y *= invN;
            centroid.z *= invN;
            ResolveContact(m_owner, other->GetOwner(), n, resolveOverlap, 0.0005f, &centroid, ContactSolve::Position);
            // Normal + friction at the centroid only: a single lever r.z?0 so
            // torque is the gravity-tip axis, not sequential +/-Z corner twist.
            for (int iter = 0; iter < 2; ++iter)
                ResolveContact(m_owner, other->GetOwner(), n, resolveOverlap, 0.0005f, &centroid, ContactSolve::Velocity);
        }
        break;
    }
    case ColliderType::MESH:
        other->ResolveCollision(this);
        return;
    case ColliderType::CYLINDER:
        LogUnsupportedCylinderPair(ColliderType::BOX, ColliderType::CYLINDER);
        return;
    default: break;
    }

    m_owner->OnCollision(other->GetOwner());
    other->GetOwner()->OnCollision(m_owner);
}

void BoxCollider::DebugDraw(const XMFLOAT4& color) {
    if (Shader_IsExternalVP()) return;
    static DebugDrawer drawer;
    if (!drawer.Initialize()) return;
    XMFLOAT3 pos, rot, scale;
    GetOwnerPose(m_owner, pos, rot, scale);
    (void)scale;
    drawer.DrawBox(pos, GetScaledLocalCenter(), GetSize(), rot, color);
}

//-----------------------------------------------
// SphereCollider collision
//-----------------------------------------------
bool SphereCollider::CheckCollision(const Collider* other) const {
    if (!other || !m_owner || !other->GetOwner()) return false;
    XMFLOAT3 cA = GetWorldCenter();
    float rA = GetRadius();
    switch (other->GetType()) {
    case ColliderType::SPHERE:
    {
        const SphereCollider* s = static_cast<const SphereCollider*>(other);
        XMFLOAT3 cB = s->GetWorldCenter();
        float rB = s->GetRadius();
        float dx = cB.x - cA.x;
        float dy = cB.y - cA.y;
        float dz = cB.z - cA.z;
        float rs = rA + rB;
        return dx * dx + dy * dy + dz * dz <= rs * rs;
    }
    case ColliderType::BOX:
    case ColliderType::CAPSULE:
    case ColliderType::MESH:
        return other->CheckCollision(this);
    case ColliderType::CYLINDER:
        return other->CheckCollision(this);
    default: return false;
    }
}

void SphereCollider::ResolveCollision(Collider* other) {
    if (!other || !m_owner || !other->GetOwner()) return;
    if (m_isTrigger || other->IsTrigger()) {
        return;
    }
    if (CollisionPairIgnored(m_owner, other->GetOwner())) return;

    switch (other->GetType()) {
    case ColliderType::SPHERE:
    {
        const SphereCollider* s = static_cast<const SphereCollider*>(other);
        XMFLOAT3 cA = GetWorldCenter();
        XMFLOAT3 cB = s->GetWorldCenter();
        float rA = GetRadius(), rB = s->GetRadius();
        XMFLOAT3 n{ cA.x - cB.x, cA.y - cB.y, cA.z - cB.z };
        float d2 = n.x * n.x + n.y * n.y + n.z * n.z;
        if (d2 < 1e-12f) { n = { 0, 1, 0 }; d2 = 1.0f; }
        float d = std::sqrt(d2);
        n.x /= d; n.y /= d; n.z /= d;
        float pen = (rA + rB) - d;
        if (pen > 0.0f) {
            const XMFLOAT3 contact{
                cA.x - n.x * rA,
                cA.y - n.y * rA,
                cA.z - n.z * rA
            };
            ResolveContact(m_owner, other->GetOwner(), n, pen, 0.0005f, &contact);
        }
        break;
    }
    case ColliderType::BOX:
    {
        const BoxCollider* b = static_cast<const BoxCollider*>(other);
        XMFLOAT3 cA = GetWorldCenter();
        float rA = GetRadius();
        XMFLOAT3 cB = b->GetWorldCenter();
        XMFLOAT3 rotB = b->GetOwner()->GetRotation();
        XMFLOAT3 n; float pen = 0.0f; XMFLOAT3 contact{};
        if (SphereOBB_Contact(cA, rA, cB, b->GetSize(), rotB, n, pen, &contact)) {
            ResolveContact(m_owner, other->GetOwner(), n, pen, 0.0005f, &contact);
        }
        break;
    }
    case ColliderType::CYLINDER:
        other->ResolveCollision(this);
        return;
    case ColliderType::MESH:
    {
        MeshCollider* mesh = static_cast<MeshCollider*>(other);

        // Continuous sweep: cast from previous center along this frame's displacement.
        // Casting forward from the *current* position misses walls already behind a
        // fast disc (thin perimeter mesh + high throw speed).
        // Prefer collider world centers so local center offsets stay consistent.
        const XMFLOAT3 curCenter = GetWorldCenter();
        const XMFLOAT3 centerOffset = {
            curCenter.x - m_owner->GetPosition().x,
            curCenter.y - m_owner->GetPosition().y,
            curCenter.z - m_owner->GetPosition().z
        };
        const XMFLOAT3 prevCenter = {
            m_owner->m_previousPosition.x + centerOffset.x,
            m_owner->m_previousPosition.y + centerOffset.y,
            m_owner->m_previousPosition.z + centerOffset.z
        };
        const XMFLOAT3 delta = {
            curCenter.x - prevCenter.x,
            curCenter.y - prevCenter.y,
            curCenter.z - prevCenter.z
        };
        const float moveLen = std::sqrt(
            delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

        if (moveLen > 0.001f) {
            const XMFLOAT3 rayDir = {
                delta.x / moveLen,
                delta.y / moveLen,
                delta.z / moveLen
            };
            // Start just behind the previous sphere so the surface is hit before center.
            const XMFLOAT3 rayOrigin = {
                prevCenter.x - rayDir.x * GetRadius(),
                prevCenter.y - rayDir.y * GetRadius(),
                prevCenter.z - rayDir.z * GetRadius()
            };
            const float sweepDistance = moveLen + GetRadius() * 2.0f;

            XMFLOAT3 hitPoint, hitNormal;
            const bool rayHit = mesh->Raycast(rayOrigin, rayDir, sweepDistance, hitPoint, hitNormal);
            if (rayHit) {
                // Face normals may point either way; flip so bounce pushes out of wall.
                const XMFLOAT3 toPrev = {
                    prevCenter.x - hitPoint.x,
                    prevCenter.y - hitPoint.y,
                    prevCenter.z - hitPoint.z
                };
                if (toPrev.x * hitNormal.x + toPrev.y * hitNormal.y + toPrev.z * hitNormal.z < 0.0f) {
                    hitNormal.x = -hitNormal.x;
                    hitNormal.y = -hitNormal.y;
                    hitNormal.z = -hitNormal.z;
                }

                XMVECTOR n = XMLoadFloat3(&hitNormal);
                XMVECTOR surfaceCenterV = XMLoadFloat3(&hitPoint) + (n * GetRadius());
                XMFLOAT3 surfaceCenter;
                XMStoreFloat3(&surfaceCenter, surfaceCenterV);
                m_owner->SetPosition({
                    surfaceCenter.x - centerOffset.x,
                    surfaceCenter.y - centerOffset.y,
                    surfaceCenter.z - centerOffset.z
                });

                // Small penetration keeps ResolveContact applying the bounce impulse.
                ResolveContact(m_owner, other->GetOwner(), hitNormal, 0.01f);
                m_owner->OnCollision(other->GetOwner());
                other->GetOwner()->OnCollision(m_owner);
                return;
            }
        }

        // Fallback to standard overlap check if raycast didn't catch a high-speed pass
        other->ResolveCollision(this);
        return;
    }
    default: break;
    }
    m_owner->OnCollision(other->GetOwner());
    other->GetOwner()->OnCollision(m_owner);
}

void SphereCollider::DebugDraw(const XMFLOAT4& color) {
    if (Shader_IsExternalVP()) return;
    static DebugDrawer drawer;
    if (!drawer.Initialize()) return;
    XMFLOAT3 pos, rot, scale;
    GetOwnerPose(m_owner, pos, rot, scale);
    (void)scale;
    drawer.DrawSphere(pos, GetScaledLocalCenter(), GetRadius(), rot, color);
}

//-----------------------------------------------
// CapsuleCollider collision
//-----------------------------------------------
bool CapsuleCollider::CheckCollision(const Collider* other) const {
    if (!other || !m_owner || !other->GetOwner()) return false;

    XMFLOAT3 centerA = GetWorldCenter();
    float radiusA = GetRadius();
    float halfHeightA = GetHeight() * 0.5f;
    XMFLOAT3 rotA = m_owner->GetRotation();
    XMMATRIX rotMatrixA = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotA.x),
        XMConvertToRadians(rotA.y),
        XMConvertToRadians(rotA.z));
    XMVECTOR axisA = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMatrixA);
    XMVECTOR axisAn = XMVector3Normalize(axisA);
    XMFLOAT3 axisAF; XMStoreFloat3(&axisAF, axisAn);

    XMFLOAT3 p1A{ centerA.x - axisAF.x * halfHeightA, centerA.y - axisAF.y * halfHeightA, centerA.z - axisAF.z * halfHeightA };
    XMFLOAT3 p2A{ centerA.x + axisAF.x * halfHeightA, centerA.y + axisAF.y * halfHeightA, centerA.z + axisAF.z * halfHeightA };

    switch (other->GetType()) {
    case ColliderType::CAPSULE:
    {
        const CapsuleCollider* oc = static_cast<const CapsuleCollider*>(other);
        XMFLOAT3 centerB = oc->GetWorldCenter();
        float radiusB = oc->GetRadius();
        float halfHeightB = oc->GetHeight() * 0.5f;
        XMFLOAT3 rotB = oc->GetOwner()->GetRotation();
        XMMATRIX rotMatrixB = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotB.x),
            XMConvertToRadians(rotB.y),
            XMConvertToRadians(rotB.z));
        XMVECTOR axisB = XMVector3TransformNormal(XMVECTOR{ 0, 1, 0, 0 }, rotMatrixB);
        axisB = XMVector3Normalize(axisB);
        XMFLOAT3 axisBF; XMStoreFloat3(&axisBF, axisB);

        XMFLOAT3 p1B{ centerB.x - axisBF.x * halfHeightB, centerB.y - axisBF.y * halfHeightB, centerB.z - axisBF.z * halfHeightB };
        XMFLOAT3 p2B{ centerB.x + axisBF.x * halfHeightB, centerB.y + axisBF.y * halfHeightB, centerB.z + axisBF.z * halfHeightB };

        XMVECTOR vP1A = Load(p1A), vP2A = Load(p2A);
        XMVECTOR vDirA = vP2A - vP1A;
        float lenA = XMVectorGetX(XMVector3Length(vDirA));
        XMVECTOR vP1B = Load(p1B), vP2B = Load(p2B);
        XMVECTOR vDirB = vP2B - vP1B;
        float lenB = XMVectorGetX(XMVector3Length(vDirB));

        if (lenA < 1e-4f && lenB < 1e-4f) {
            float dx = p1A.x - p1B.x;
            float dy = p1A.y - p1B.y;
            float dz = p1A.z - p1B.z;
            float rs = radiusA + radiusB;
            return dx * dx + dy * dy + dz * dz <= rs * rs;
        }

        XMVECTOR vD = vP1A - vP1B;
        float a = XMVectorGetX(XMVector3Dot(vDirA, vDirA));
        float b = XMVectorGetX(XMVector3Dot(vDirA, vDirB));
        float c = XMVectorGetX(XMVector3Dot(vDirB, vDirB));
        float d = XMVectorGetX(XMVector3Dot(vDirA, vD));
        float e = XMVectorGetX(XMVector3Dot(vDirB, vD));
        float denom = a * c - b * b;
        float s, t;
        if (denom < 1e-4f) { s = 0.0f; t = std::clamp(e / c, 0.0f, 1.0f); }
        else {
            s = std::clamp((b * e - c * d) / denom, 0.0f, 1.0f);
            t = std::clamp((a * e - b * d) / denom, 0.0f, 1.0f);
        }
        XMVECTOR closestA = vP1A + XMVectorMultiply(vDirA, XMVectorReplicate(s));
        XMVECTOR closestB = vP1B + XMVectorMultiply(vDirB, XMVectorReplicate(t));
        XMVECTOR diff = closestA - closestB;
        float distSq = XMVectorGetX(XMVector3Dot(diff, diff));
        float rs = radiusA + radiusB;
        return distSq <= rs * rs;
    }
    case ColliderType::SPHERE:
    {
        const SphereCollider* s = static_cast<const SphereCollider*>(other);
        XMFLOAT3 sc = s->GetWorldCenter();
        float rB = s->GetRadius();
        XMVECTOR vP1A = Load(p1A), vP2A = Load(p2A);
        XMVECTOR vDirA = vP2A - vP1A;
        float lenA = XMVectorGetX(XMVector3Length(vDirA));
        XMFLOAT3 closest = p1A;
        if (lenA >= 1e-4f) {
            XMVECTOR vT = XMVectorClamp(
                XMVectorDivide(
                    XMVector3Dot(Load(sc) - vP1A, vDirA),
                    XMVectorReplicate(lenA * lenA)),
                XMVectorZero(), XMVectorReplicate(1.0f));
            XMStoreFloat3(&closest, vP1A + XMVectorMultiply(vDirA, vT));
        }
        float dx = sc.x - closest.x;
        float dy = sc.y - closest.y;
        float dz = sc.z - closest.z;
        float rs = radiusA + rB;
        return dx * dx + dy * dy + dz * dz <= rs * rs;
    }
    case ColliderType::BOX:
    {
        const BoxCollider* box = static_cast<const BoxCollider*>(other);
        return box->CheckCollision(this); // Use BoxCollider reverse check
    }
    case ColliderType::MESH:
        return other->CheckCollision(this);
    case ColliderType::CYLINDER:
        return other->CheckCollision(this);
    default: return false;
    }
}

void CapsuleCollider::ResolveCollision(Collider* other) {
    if (!other || !m_owner || !other->GetOwner()) return;
    if (m_isTrigger || other->IsTrigger()) {
        return;
    }
    if (CollisionPairIgnored(m_owner, other->GetOwner())) return;

    switch (other->GetType()) {
    case ColliderType::BOX:
    {
        const BoxCollider* b = static_cast<const BoxCollider*>(other);
        XMFLOAT3 cB = b->GetWorldCenter();
        XMFLOAT3 rotB = b->GetOwner()->GetRotation();
        XMFLOAT3 sizeB = b->GetSize();

        XMFLOAT3 cA = GetWorldCenter();
        float r = GetRadius();
        float hh = GetHeight() * 0.5f;

        XMFLOAT3 rotA = m_owner->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotA.x),
            XMConvertToRadians(rotA.y),
            XMConvertToRadians(rotA.z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);

        XMFLOAT3 top{ cA.x + af.x * hh, cA.y + af.y * hh, cA.z + af.z * hh };
        XMFLOAT3 bottom{ cA.x - af.x * hh, cA.y - af.y * hh, cA.z - af.z * hh };
        XMFLOAT3 mid{ cA.x, cA.y, cA.z };

        XMFLOAT3 bestN{ 0, 0, 0 };
        float bestPen = -FLT_MAX;

        auto testOne = [&](const XMFLOAT3& sc) {
            XMFLOAT3 n; float pen = 0.0f;
            if (SphereOBB_Contact(sc, r, cB, sizeB, rotB, n, pen)) {
                if (pen > bestPen) { bestPen = pen; bestN = n; }
            }
            };
        testOne(top); testOne(bottom); testOne(mid);

        if (bestPen > 0.0f) {
            // bestN is BOX -> CAPSULE (other -> self) so invert for collision response
            ResolveContact(m_owner, other->GetOwner(), bestN, bestPen);
        }
        break;
    }
    case ColliderType::SPHERE:
    {
        const SphereCollider* s = static_cast<const SphereCollider*>(other);
        XMFLOAT3 cA = GetWorldCenter();
        XMFLOAT3 cB = s->GetWorldCenter();
        float rA = GetRadius(), rB = s->GetRadius();

        // Match CheckCollision: closest point on capsule segment ? sphere center.
        // Center-only spheres miss head/feet hits (centerPen<=0 while segPen>0),
        // and swept narrow-phase returns after that failed resolve -> tunneling.
        float halfH = GetHeight() * 0.5f;
        XMFLOAT3 rotA = m_owner->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotA.x), XMConvertToRadians(rotA.y), XMConvertToRadians(rotA.z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);
        XMFLOAT3 p1{ cA.x - af.x * halfH, cA.y - af.y * halfH, cA.z - af.z * halfH };
        XMFLOAT3 p2{ cA.x + af.x * halfH, cA.y + af.y * halfH, cA.z + af.z * halfH };
        XMVECTOR vP1 = Load(p1), vP2 = Load(p2);
        XMVECTOR vDir = vP2 - vP1;
        float lenA = XMVectorGetX(XMVector3Length(vDir));
        XMFLOAT3 closest = p1;
        if (lenA >= 1e-4f) {
            XMVECTOR vT = XMVectorClamp(
                XMVectorDivide(XMVector3Dot(Load(cB) - vP1, vDir), XMVectorReplicate(lenA * lenA)),
                XMVectorZero(), XMVectorReplicate(1.0f));
            XMStoreFloat3(&closest, vP1 + XMVectorMultiply(vDir, vT));
        }

        XMFLOAT3 n{ closest.x - cB.x, closest.y - cB.y, closest.z - cB.z };
        float d2 = n.x * n.x + n.y * n.y + n.z * n.z;
        if (d2 < 1e-12f) { n = { 0, 1, 0 }; d2 = 1.0f; }
        float d = std::sqrt(d2);
        n.x /= d; n.y /= d; n.z /= d;
        float pen = (rA + rB) - d;

        if (pen > 0.0f) ResolveContact(m_owner, other->GetOwner(), n, pen);
        break;
    }
    case ColliderType::CAPSULE:
    {
        const CapsuleCollider* c = static_cast<const CapsuleCollider*>(other);
        XMFLOAT3 cA = GetWorldCenter();
        XMFLOAT3 cB = c->GetWorldCenter();
        float rA = GetRadius(), rB = c->GetRadius();
        XMFLOAT3 n{ cA.x - cB.x, cA.y - cB.y, cA.z - cB.z };
        float d2 = n.x * n.x + n.y * n.y + n.z * n.z;
        if (d2 < 1e-12f) { n = { 0, 1, 0 }; d2 = 1.0f; }
        float d = std::sqrt(d2);
        n.x /= d; n.y /= d; n.z /= d;
        float pen = (rA + rB) - d;
        if (pen > 0.0f) ResolveContact(m_owner, other->GetOwner(), n, pen);
        break;
    }
    case ColliderType::MESH:
    case ColliderType::CYLINDER:
        other->ResolveCollision(this);
        return;
    default: break;
    }

    m_owner->OnCollision(other->GetOwner());
    other->GetOwner()->OnCollision(m_owner);
}

void CapsuleCollider::DebugDraw(const XMFLOAT4& color) {
    if (Shader_IsExternalVP()) return;
    static DebugDrawer drawer;
    if (!drawer.Initialize()) return;
    XMFLOAT3 pos, rot, scale;
    GetOwnerPose(m_owner, pos, rot, scale);
    (void)scale;
    drawer.DrawCapsule(pos, GetScaledLocalCenter(), GetRadius(), GetHeight(), rot, color);
}

//-----------------------------------------------
// CylinderCollider collision
//-----------------------------------------------
bool CylinderCollider::CheckCollision(const Collider* other) const {
    if (!other || !m_owner || !other->GetOwner()) return false;

    const CylinderWorldFrame frame = BuildCylinderWorldFrame(this);

    switch (other->GetType()) {
    case ColliderType::SPHERE: {
        const SphereCollider* sphere = static_cast<const SphereCollider*>(other);
        XMFLOAT3 n; float pen = 0.0f;
        return SphereCylinder_Contact(sphere->GetWorldCenter(), sphere->GetRadius(), frame, n, pen);
    }
    case ColliderType::CAPSULE: {
        const CapsuleCollider* cap = static_cast<const CapsuleCollider*>(other);
        XMFLOAT3 cC = cap->GetWorldCenter();
        const float r = cap->GetRadius();
        const float hh = cap->GetHeight() * 0.5f;
        XMFLOAT3 rotCap = cap->GetOwner()->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotCap.x),
            XMConvertToRadians(rotCap.y),
            XMConvertToRadians(rotCap.z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);

        const XMFLOAT3 samples[3] = {
            { cC.x + af.x * hh, cC.y + af.y * hh, cC.z + af.z * hh },
            { cC.x, cC.y, cC.z },
            { cC.x - af.x * hh, cC.y - af.y * hh, cC.z - af.z * hh }
        };

        XMFLOAT3 n; float pen = 0.0f;
        for (const XMFLOAT3& sample : samples) {
            if (SphereCylinder_Contact(sample, r, frame, n, pen))
                return true;
        }
        return false;
    }
    case ColliderType::BOX:
    case ColliderType::MESH:
    case ColliderType::CYLINDER:
        LogUnsupportedCylinderPair(ColliderType::CYLINDER, other->GetType());
        return false;
    default:
        return false;
    }
}

void CylinderCollider::ResolveCollision(Collider* other) {
    if (!other || !m_owner || !other->GetOwner()) return;
    if (m_isTrigger || other->IsTrigger()) {
        return;
    }
    if (CollisionPairIgnored(m_owner, other->GetOwner())) return;

    const CylinderWorldFrame frame = BuildCylinderWorldFrame(this);

    switch (other->GetType()) {
    case ColliderType::SPHERE: {
        const SphereCollider* sphere = static_cast<const SphereCollider*>(other);
        XMFLOAT3 n; float pen = 0.0f;
        if (SphereCylinder_Contact(sphere->GetWorldCenter(), sphere->GetRadius(), frame, n, pen)) {
            ResolveContact(other->GetOwner(), m_owner, n, pen);
        }
        break;
    }
    case ColliderType::CAPSULE: {
        const CapsuleCollider* cap = static_cast<CapsuleCollider*>(other);
        XMFLOAT3 cC = cap->GetWorldCenter();
        const float r = cap->GetRadius();
        const float hh = cap->GetHeight() * 0.5f;
        XMFLOAT3 rotCap = other->GetOwner()->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotCap.x),
            XMConvertToRadians(rotCap.y),
            XMConvertToRadians(rotCap.z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);

        const XMFLOAT3 samples[3] = {
            { cC.x + af.x * hh, cC.y + af.y * hh, cC.z + af.z * hh },
            { cC.x - af.x * hh, cC.y - af.y * hh, cC.z - af.z * hh },
            cC
        };

        float bestPen = -FLT_MAX;
        XMFLOAT3 bestN = frame.axis;
        for (const XMFLOAT3& sample : samples) {
            XMFLOAT3 n; float pen = 0.0f;
            if (SphereCylinder_Contact(sample, r, frame, n, pen) && pen > bestPen) {
                bestPen = pen;
                bestN = n;
            }
        }
        if (bestPen > 0.0f)
            ResolveContact(other->GetOwner(), m_owner, bestN, bestPen);
        break;
    }
    case ColliderType::BOX:
    case ColliderType::MESH:
    case ColliderType::CYLINDER:
        LogUnsupportedCylinderPair(ColliderType::CYLINDER, other->GetType());
        return;
    default:
        break;
    }

    m_owner->OnCollision(other->GetOwner());
    other->GetOwner()->OnCollision(m_owner);
}

void CylinderCollider::DebugDraw(const XMFLOAT4& color) {
    if (Shader_IsExternalVP()) return;
    static DebugDrawer drawer;
    if (!drawer.Initialize()) return;
    XMFLOAT3 pos, rot, scale;
    GetOwnerPose(m_owner, pos, rot, scale);
    (void)scale;
    drawer.DrawCylinder(pos, GetScaledLocalCenter(), GetRadius(), GetHeight(), rot, color);
}

//=============================================================================
// MeshCollider collision
//=============================================================================

void MeshCollider::SetMeshData(const std::vector<XMFLOAT3>& vertices,
    const std::vector<unsigned int>& indices) {
    m_vertices = vertices;
    m_indices = indices;
    CalculateBoundingSphere();
}

void MeshCollider::BuildFromModel(const Model3D& model) {
    const std::vector<Model3D::VertexPNTC>& cpuVerts = model.GetVerticesCPU();

    m_vertices.clear();
    m_indices.clear();
    m_vertices.reserve(cpuVerts.size());

    for (const auto& v : cpuVerts) {
        m_vertices.push_back(v.Position);
    }

    if (m_indices.empty() && !m_vertices.empty()) {
        // Only emit complete triangles; a trailing partial triangle would otherwise
        // cause out-of-bounds reads (m_indices[i+1]/[i+2]) in the triangle-iteration loops below.
        const size_t triVertCount = (m_vertices.size() / 3) * 3;
        m_indices.reserve(triVertCount);
        for (size_t i = 0; i < triVertCount; ++i) {
            m_indices.push_back(static_cast<unsigned int>(i));
        }
    }

    CalculateBoundingSphere();
}

void MeshCollider::GetWorldAABB(XMFLOAT3& outCenter, XMFLOAT3& outSize) const {
    outCenter = {0.0f, 0.0f, 0.0f};
    outSize = {0.0f, 0.0f, 0.0f};
    if (m_vertices.empty() || !m_owner) {
        outCenter = GetWorldCenter();
        return;
    }

    XMFLOAT3 ownerPos, ownerRot, ownerScale;
    GetOwnerPose(m_owner, ownerPos, ownerRot, ownerScale);

    float paddingScale = MeshSizePaddingScale(m_boundingSphereRadius, m_sizePadding, ownerScale);

    XMFLOAT3 effScale = {
        ownerScale.x * paddingScale,
        ownerScale.y * paddingScale,
        ownerScale.z * paddingScale
    };

    XMMATRIX worldM =
        XMMatrixScaling(effScale.x, effScale.y, effScale.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(ownerRot.x),
            XMConvertToRadians(ownerRot.y),
            XMConvertToRadians(ownerRot.z)) *
        XMMatrixTranslation(ownerPos.x, ownerPos.y, ownerPos.z);

    XMFLOAT3 worldMin = {FLT_MAX, FLT_MAX, FLT_MAX};
    XMFLOAT3 worldMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (const XMFLOAT3& localVert : m_vertices) {
        XMVECTOR worldVert = XMVector3TransformCoord(XMLoadFloat3(&localVert), worldM);
        XMFLOAT3 w;
        XMStoreFloat3(&w, worldVert);
        worldMin.x = (std::min)(worldMin.x, w.x);
        worldMin.y = (std::min)(worldMin.y, w.y);
        worldMin.z = (std::min)(worldMin.z, w.z);
        worldMax.x = (std::max)(worldMax.x, w.x);
        worldMax.y = (std::max)(worldMax.y, w.y);
        worldMax.z = (std::max)(worldMax.z, w.z);
    }

    outCenter = {
        (worldMin.x + worldMax.x) * 0.5f,
        (worldMin.y + worldMax.y) * 0.5f,
        (worldMin.z + worldMax.z) * 0.5f
    };
    outSize = {
        worldMax.x - worldMin.x,
        worldMax.y - worldMin.y,
        worldMax.z - worldMin.z
    };
}

void MeshCollider::CalculateBoundingSphere() {
    if (m_vertices.empty()) {
        m_boundingSphereRadius = 0.0f;
        m_boundingSphereCenter = { 0, 0, 0 };
        return;
    }

    // Calculate bounding sphere in local space
    XMFLOAT3 center = { 0, 0, 0 };
    for (const auto& v : m_vertices) {
        center.x += v.x; center.y += v.y; center.z += v.z;
    }
    float invCount = 1.0f / static_cast<float>(m_vertices.size());
    center.x *= invCount; center.y *= invCount; center.z *= invCount;
    m_boundingSphereCenter = center;

    float maxDistSq = 0.0f;
    for (const auto& v : m_vertices) {
        float dx = v.x - center.x;
        float dy = v.y - center.y;
        float dz = v.z - center.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > maxDistSq) maxDistSq = distSq;
    }
    m_boundingSphereRadius = std::sqrt(maxDistSq);

    // Keep the radius in mesh-local space; apply the owner's current scale during queries.
    // (Baking scale here breaks when the transform changes, e.g. RoundManager shrink.)
}

float MeshCollider::GetWorldBoundingSphereRadius() const {
    if (m_vertices.empty()) return m_sizePadding;

    XMFLOAT3 ownerScale = {1.0f, 1.0f, 1.0f};
    if (m_owner)
        ownerScale = m_owner->GetScale();

    float paddingScale = MeshSizePaddingScale(m_boundingSphereRadius, m_sizePadding, ownerScale);

    const XMFLOAT3 effScale = {
        ownerScale.x * paddingScale,
        ownerScale.y * paddingScale,
        ownerScale.z * paddingScale
    };
    const float maxEffScale = (std::max)({ effScale.x, effScale.y, effScale.z, 1e-6f });
    return m_boundingSphereRadius * maxEffScale + m_sizePadding;
}

bool MeshCollider::CheckCollision(const Collider* other) const {
    if (!other || m_vertices.empty() || m_indices.empty()) return false;

    // Broad phase: Bounding sphere test
    XMFLOAT3 ownerPos, ownerRot, ownerScale;
    GetOwnerPose(m_owner, ownerPos, ownerRot, ownerScale);

    float paddingScale = MeshSizePaddingScale(m_boundingSphereRadius, m_sizePadding, ownerScale);

    XMFLOAT3 effScale = {
        ownerScale.x * paddingScale,
        ownerScale.y * paddingScale,
        ownerScale.z * paddingScale
    };

    XMMATRIX worldM =
        XMMatrixScaling(effScale.x, effScale.y, effScale.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(ownerRot.x),
            XMConvertToRadians(ownerRot.y),
            XMConvertToRadians(ownerRot.z)) *
        XMMatrixTranslation(ownerPos.x, ownerPos.y, ownerPos.z);

    XMFLOAT3 myBSCenter;
    {
        XMVECTOR worldBSC = XMVector3TransformCoord(XMLoadFloat3(&m_boundingSphereCenter), worldM);
        XMStoreFloat3(&myBSCenter, worldBSC);
    }

    XMFLOAT3 otherCenter = other->GetWorldCenter();
    float dx = otherCenter.x - myBSCenter.x;
    float dy = otherCenter.y - myBSCenter.y;
    float dz = otherCenter.z - myBSCenter.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    float broadRadius = GetWorldBoundingSphereRadius();
    if (other->GetType() == ColliderType::SPHERE) {
        broadRadius += static_cast<const SphereCollider*>(other)->GetRadius();
    }
    else if (other->GetType() == ColliderType::BOX) {
        XMFLOAT3 sz = static_cast<const BoxCollider*>(other)->GetSize();
        broadRadius += 0.5f * std::sqrt(sz.x * sz.x + sz.y * sz.y + sz.z * sz.z);
    }
    else if (other->GetType() == ColliderType::CAPSULE) {
        auto* cap = static_cast<const CapsuleCollider*>(other);
        broadRadius += cap->GetRadius() + 0.5f * cap->GetHeight();
    }
    else if (other->GetType() == ColliderType::CYLINDER) {
        auto* cy = static_cast<const CylinderCollider*>(other);
        const float hh = 0.5f * cy->GetHeight();
        broadRadius += std::sqrt(cy->GetRadius() * cy->GetRadius() + hh * hh);
    }

    if (distSq > broadRadius * broadRadius) return false;

    // Narrow phase: transform the test shape into mesh-local space once, then test
    // triangles using the stored local-vertex arrays directly.
  // This avoids O(N) per-triangle XMVector3Transform calls per collision pair.
    XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(ownerRot.x),
        XMConvertToRadians(ownerRot.y),
        XMConvertToRadians(ownerRot.z));
    XMMATRIX invRot = XMMatrixTranspose(rotM);

  // Inverse effective scale (guard against divide by zero)
    XMFLOAT3 invEffScale = {
        effScale.x > 1e-6f ? 1.0f / effScale.x : 0.0f,
        effScale.y > 1e-6f ? 1.0f / effScale.y : 0.0f,
        effScale.z > 1e-6f ? 1.0f / effScale.z : 0.0f
    };

    // Transform a world-space point into mesh-local space:
    // localPt = invEffScale * invRot * (worldPt - ownerPos)
    auto worldToLocal = [&](const XMFLOAT3& worldPt) -> XMFLOAT3 {
        XMVECTOR w = XMVectorSet(worldPt.x - ownerPos.x, worldPt.y - ownerPos.y, worldPt.z - ownerPos.z, 0.0f);
        XMVECTOR r = XMVector3TransformNormal(w, invRot);
        XMFLOAT3 lr; XMStoreFloat3(&lr, r);
        return { lr.x * invEffScale.x, lr.y * invEffScale.y, lr.z * invEffScale.z };
    };

    // Test all triangles in local space against a local-space sphere; early-exit on first hit.
    auto testTrianglesLocal = [&](const XMFLOAT3& localCenter, float worldRadius) -> bool {
        const float localRadius = MeshLocalTestRadius(worldRadius, effScale);
        for (size_t i = 0; i < m_indices.size(); i += 3) {
            const XMFLOAT3& v0 = m_vertices[m_indices[i]];
            const XMFLOAT3& v1 = m_vertices[m_indices[i + 1]];
            const XMFLOAT3& v2 = m_vertices[m_indices[i + 2]];
            XMFLOAT3 n; float p;
            if (TriangleIntersectsSphere(v0, v1, v2, localCenter, localRadius, n, p)) return true;
        }
        return false;
    };

    if (other->GetType() == ColliderType::SPHERE) {
        auto* sphere = static_cast<const SphereCollider*>(other);
        return testTrianglesLocal(worldToLocal(sphere->GetWorldCenter()),
                                  sphere->GetRadius());
    }
    else if (other->GetType() == ColliderType::BOX) {
        auto* box = static_cast<const BoxCollider*>(other);
        XMFLOAT3 sz = box->GetSize();
        float worldRadius = 0.5f * std::sqrt(sz.x * sz.x + sz.y * sz.y + sz.z * sz.z);
        return testTrianglesLocal(worldToLocal(box->GetWorldCenter()), worldRadius);
    }
    else if (other->GetType() == ColliderType::CAPSULE) {
        auto* cap = static_cast<const CapsuleCollider*>(other);
        XMFLOAT3 cC = cap->GetWorldCenter();
        float r   = cap->GetRadius();
        float hh  = cap->GetHeight() * 0.5f;

        XMFLOAT3 rotCap = cap->GetOwner()->GetRotation();
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotCap.x),
            XMConvertToRadians(rotCap.y),
            XMConvertToRadians(rotCap.z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);

        XMFLOAT3 worldPoints[3] = {
            { cC.x + af.x * hh, cC.y + af.y * hh, cC.z + af.z * hh }, // Top
            { cC.x,             cC.y,             cC.z             }, // Mid
            { cC.x - af.x * hh, cC.y - af.y * hh, cC.z - af.z * hh }  // Bottom
        };

        for (const auto& wp : worldPoints) {
            if (testTrianglesLocal(worldToLocal(wp), r)) return true;
        }
    }
    else if (other->GetType() == ColliderType::CYLINDER) {
        LogUnsupportedCylinderPair(ColliderType::MESH, ColliderType::CYLINDER);
        return false;
    }
    return false;
}

void MeshCollider::ResolveCollision(Collider* other) {
    if (!other || !m_owner || m_vertices.empty() || m_indices.empty()) return;

    GameObject* otherObj = other->GetOwner();
    if (!otherObj || m_isTrigger || other->IsTrigger()) {
        return;
    }

    if (CollisionPairIgnored(m_owner, otherObj)) return;

    // Mesh is typically static object, only resolve with dynamic objects
    Rigidbody& otherRB = otherObj->GetRigidbody();
    if (otherRB.IsKinematic()) return;

    // Build inverse transform once to run triangle tests in mesh-local space,
    // avoiding O(N) per-triangle world transforms per collision pair.
    XMFLOAT3 ownerPos   = m_owner->GetPosition();
    XMFLOAT3 ownerRot   = m_owner->GetRotation();
    XMFLOAT3 ownerScale = m_owner->GetScale();

    // Apply size padding by scaling up uniformly
    float paddingScale = MeshSizePaddingScale(m_boundingSphereRadius, m_sizePadding, ownerScale);

    // Effective scale = owner scale * padding
    XMFLOAT3 effScale = {
        ownerScale.x * paddingScale,
        ownerScale.y * paddingScale,
        ownerScale.z * paddingScale
    };

    // Rotation matrix and its inverse (transpose for orthonormal rotation)
    XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(ownerRot.x),
        XMConvertToRadians(ownerRot.y),
        XMConvertToRadians(ownerRot.z));
    XMMATRIX invRot = XMMatrixTranspose(rotM);

    // Inverse effective scale (guard against divide by zero)
    XMFLOAT3 invEffScale = {
        effScale.x > 1e-6f ? 1.0f / effScale.x : 0.0f,
        effScale.y > 1e-6f ? 1.0f / effScale.y : 0.0f,
        effScale.z > 1e-6f ? 1.0f / effScale.z : 0.0f
    };

    // World-space point -> mesh-local space
    auto worldToLocal = [&](const XMFLOAT3& worldPt) -> XMFLOAT3 {
        XMVECTOR w = XMVectorSet(worldPt.x - ownerPos.x, worldPt.y - ownerPos.y, worldPt.z - ownerPos.z, 0.0f);
        XMVECTOR r = XMVector3TransformNormal(w, invRot);
        XMFLOAT3 lr; XMStoreFloat3(&lr, r);
        return { lr.x * invEffScale.x, lr.y * invEffScale.y, lr.z * invEffScale.z };
    };

    // Local-space normal -> world-space normal: N_world = normalize(R * S^-1 * N_local)
    auto localNormalToWorld = [&](const XMFLOAT3& localNormal) -> XMFLOAT3 {
        XMVECTOR n = XMVectorSet(
            localNormal.x * invEffScale.x,
            localNormal.y * invEffScale.y,
            localNormal.z * invEffScale.z,
            0.0f);
        XMVECTOR worldN = XMVector3Normalize(XMVector3TransformNormal(n, rotM));
        XMFLOAT3 result; XMStoreFloat3(&result, worldN);
        return result;
    };

    // Keep deepest penetration per distinct world-normal cluster, then resolve each
    // cluster once. Iterating the same contact repeatedly over-pushes and shakes;
    // keeping only the single deepest contact flips between corner walls.
    struct ContactCluster {
        XMFLOAT3 localNormal = {0, 1, 0};
        XMFLOAT3 worldNormal = {0, 1, 0};
        float localPen = -FLT_MAX;
    };
    ContactCluster clusters[4];
    int clusterCount = 0;

    auto considerContact = [&](const XMFLOAT3& localN, float localPen) {
        const XMFLOAT3 worldN = localNormalToWorld(localN);
        for (int s = 0; s < clusterCount; ++s) {
            const float d =
                clusters[s].worldNormal.x * worldN.x +
                clusters[s].worldNormal.y * worldN.y +
                clusters[s].worldNormal.z * worldN.z;
            if (d > 0.7f) {
                if (localPen > clusters[s].localPen) {
                    clusters[s].localPen = localPen;
                    clusters[s].localNormal = localN;
                    clusters[s].worldNormal = worldN;
                }
                return;
            }
        }
        if (clusterCount < 4) {
            clusters[clusterCount].localNormal = localN;
            clusters[clusterCount].worldNormal = worldN;
            clusters[clusterCount].localPen = localPen;
            ++clusterCount;
        }
    };

    auto testTrianglesLocal = [&](const XMFLOAT3& localCenter, float worldRadius) {
        const float localRadius = MeshLocalTestRadius(worldRadius, effScale);
        for (size_t i = 0; i < m_indices.size(); i += 3) {
            const XMFLOAT3& v0 = m_vertices[m_indices[i]];
            const XMFLOAT3& v1 = m_vertices[m_indices[i + 1]];
            const XMFLOAT3& v2 = m_vertices[m_indices[i + 2]];
            XMFLOAT3 n; float p;
            if (TriangleIntersectsSphere(v0, v1, v2, localCenter, localRadius, n, p)) {
                considerContact(n, p);
            }
        }
    };

    switch (other->GetType()) {
    case ColliderType::SPHERE:
        testTrianglesLocal(worldToLocal(static_cast<SphereCollider*>(other)->GetWorldCenter()),
                           static_cast<SphereCollider*>(other)->GetRadius());
        break;
    case ColliderType::BOX: {
        auto* box = static_cast<BoxCollider*>(other);
        XMFLOAT3 sz = box->GetSize();
        float worldRadius = 0.5f * std::sqrt(sz.x * sz.x + sz.y * sz.y + sz.z * sz.z);
        testTrianglesLocal(worldToLocal(box->GetWorldCenter()), worldRadius);
        break;
    }
    case ColliderType::CAPSULE: {
        auto* cap = static_cast<CapsuleCollider*>(other);
        XMFLOAT3 cC = cap->GetWorldCenter();
        float r = cap->GetRadius(), hh = cap->GetHeight() * 0.5f;
        XMMATRIX rm = XMMatrixRotationRollPitchYaw(XMConvertToRadians(otherObj->GetRotation().x),
            XMConvertToRadians(otherObj->GetRotation().y),
            XMConvertToRadians(otherObj->GetRotation().z));
        XMVECTOR axis = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rm));
        XMFLOAT3 af; XMStoreFloat3(&af, axis);
        testTrianglesLocal(worldToLocal({ cC.x + af.x * hh, cC.y + af.y * hh, cC.z + af.z * hh }), r);
        testTrianglesLocal(worldToLocal({ cC.x - af.x * hh, cC.y - af.y * hh, cC.z - af.z * hh }), r);
        testTrianglesLocal(worldToLocal(cC), r);
        break;
    }
    case ColliderType::CYLINDER:
        LogUnsupportedCylinderPair(ColliderType::MESH, ColliderType::CYLINDER);
        return;
    default:
        break;
    }

    // Deepest cluster first so the primary wall wins position before secondary.
    int order[4] = {0, 1, 2, 3};
    for (int a = 0; a < clusterCount; ++a) {
        for (int b = a + 1; b < clusterCount; ++b) {
            if (clusters[order[b]].localPen > clusters[order[a]].localPen) {
                int tmp = order[a]; order[a] = order[b]; order[b] = tmp;
            }
        }
    }

    for (int oi = 0; oi < clusterCount; ++oi) {
        const ContactCluster& c = clusters[order[oi]];
        const float worldPenetration = LocalPenetrationToWorld(
            c.localPen, c.localNormal, effScale, rotM);
        if (worldPenetration <= 0.0005f) continue;
        XMFLOAT3 worldNormal = StabilizeFloorContactNormal(c.worldNormal);
        // Note: normal is triangle -> other direction
        ResolveContact(otherObj, m_owner, worldNormal, worldPenetration, 0.001f);
    }

    m_owner->OnCollision(otherObj);
    otherObj->OnCollision(m_owner);
}

void MeshCollider::DebugDraw(const XMFLOAT4& color) {
    if (Shader_IsExternalVP()) return;
    if (m_vertices.empty() || m_indices.empty()) return;

    static DebugDrawer drawer;
    if (!drawer.Initialize()) return;

    XMFLOAT3 ownerPos, ownerRot, ownerScale;
    GetOwnerPose(m_owner, ownerPos, ownerRot, ownerScale);

    // Apply size padding by scaling up uniformly (same as collision detection)
    float paddingScale = MeshSizePaddingScale(m_boundingSphereRadius, m_sizePadding, ownerScale);
    
    XMMATRIX worldTransform = XMMatrixScaling(ownerScale.x * paddingScale, ownerScale.y * paddingScale, ownerScale.z * paddingScale) *
                              XMMatrixRotationRollPitchYaw(
                                  XMConvertToRadians(ownerRot.x),
                                  XMConvertToRadians(ownerRot.y),
                                  XMConvertToRadians(ownerRot.z)) *
                              XMMatrixTranslation(ownerPos.x, ownerPos.y, ownerPos.z);

    // Pre-transform all unique vertices to world space once.
    std::vector<XMFLOAT3> worldVerts(m_vertices.size());
    for (size_t i = 0; i < m_vertices.size(); ++i) {
        XMVECTOR v = XMVector3Transform(XMLoadFloat3(&m_vertices[i]), worldTransform);
        XMStoreFloat3(&worldVerts[i], v);
    }

    // Build flat edge-vertex list for a single batched draw call:
    // 3 edges x 2 endpoints per triangle.
    std::vector<XMFLOAT3> lineVerts;
    lineVerts.reserve(m_indices.size() * 2);
    for (size_t i = 0; i < m_indices.size(); i += 3) {
        const XMFLOAT3& tv0 = worldVerts[m_indices[i]];
        const XMFLOAT3& tv1 = worldVerts[m_indices[i + 1]];
        const XMFLOAT3& tv2 = worldVerts[m_indices[i + 2]];
        lineVerts.push_back(tv0); lineVerts.push_back(tv1);
        lineVerts.push_back(tv1); lineVerts.push_back(tv2);
        lineVerts.push_back(tv2); lineVerts.push_back(tv0);
    }

    drawer.DrawLinesCached(lineVerts, color);
}

bool MeshCollider::Raycast(const DirectX::XMFLOAT3& rayOrigin,
    const DirectX::XMFLOAT3& rayDirection,
    float maxDistance,
    DirectX::XMFLOAT3& outHitPoint,
    DirectX::XMFLOAT3& outHitNormal) const {
    if (m_vertices.empty() || m_indices.empty() || !m_owner) {
        return false;
    }

    // Transform the ray into mesh-local space once, then test all triangles using
    // stored local-vertex arrays (no per-triangle XMVector3Transform).
    // The parametric t value is invariant under the linear transform, so the same
    // t can be used to compute the world-space hit point from the original world ray.
    XMFLOAT3 ownerPos   = m_owner->GetPosition();
    XMFLOAT3 ownerRot   = m_owner->GetRotation();
    XMFLOAT3 ownerScale = m_owner->GetScale();

    // Apply size padding by scaling up uniformly
    float paddingScale = MeshSizePaddingScale(m_boundingSphereRadius, m_sizePadding, ownerScale);

    // Effective scale = owner scale * padding
    XMFLOAT3 effScale = {
        ownerScale.x * paddingScale,
        ownerScale.y * paddingScale,
        ownerScale.z * paddingScale
    };

    // Rotation matrix and its inverse (transpose for orthonormal rotation)
    XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(ownerRot.x),
        XMConvertToRadians(ownerRot.y),
        XMConvertToRadians(ownerRot.z));
    XMMATRIX invRot = XMMatrixTranspose(rotM);

    // Inverse effective scale (guard against divide by zero)
    XMFLOAT3 invEffScale = {
        effScale.x > 1e-6f ? 1.0f / effScale.x : 0.0f,
        effScale.y > 1e-6f ? 1.0f / effScale.y : 0.0f,
        effScale.z > 1e-6f ? 1.0f / effScale.z : 0.0f
    };

    // World-space point -> mesh-local space
    auto worldToLocal = [&](const XMFLOAT3& worldPt) -> XMFLOAT3 {
        XMVECTOR w = XMVectorSet(worldPt.x - ownerPos.x, worldPt.y - ownerPos.y, worldPt.z - ownerPos.z, 0.0f);
        XMVECTOR r = XMVector3TransformNormal(w, invRot);
        XMFLOAT3 lr; XMStoreFloat3(&lr, r);
        return { lr.x * invEffScale.x, lr.y * invEffScale.y, lr.z * invEffScale.z };
    };

    // Transform ray origin into local space; transform direction without translation.
    // The direction is not normalized in local space (non-uniform scale), but the
    // parametric t returned by RayIntersectsTriangle is identical in both spaces.
    XMFLOAT3 localOrigin = worldToLocal(rayOrigin);
    XMVECTOR d = XMVector3TransformNormal(XMLoadFloat3(&rayDirection), invRot);
    XMFLOAT3 dr; XMStoreFloat3(&dr, d);
    XMFLOAT3 localDir = { dr.x * invEffScale.x, dr.y * invEffScale.y, dr.z * invEffScale.z };

    float closestT = maxDistance;
    bool hitFound = false;
    XMFLOAT3 closestWorldNormal = { 0, 1, 0 };

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        const XMFLOAT3& v0 = m_vertices[m_indices[i]];
        const XMFLOAT3& v1 = m_vertices[m_indices[i + 1]];
        const XMFLOAT3& v2 = m_vertices[m_indices[i + 2]];

        float t;
        if (RayIntersectsTriangle(localOrigin, localDir, v0, v1, v2, t)) {
            if (t < closestT && t >= 0.0f) {
                closestT = t;
                hitFound = true;

                // Compute face normal in local space, then convert to world space:
                // N_world = normalize(R * S^-1 * N_local)
                XMVECTOR lv0 = XMLoadFloat3(&v0);
                XMVECTOR lv1 = XMLoadFloat3(&v1);
                XMVECTOR lv2 = XMLoadFloat3(&v2);
                XMVECTOR localN = XMVector3Normalize(XMVector3Cross(lv1 - lv0, lv2 - lv0));
                XMFLOAT3 lnf; XMStoreFloat3(&lnf, localN);
                XMVECTOR scaledN = XMVectorSet(lnf.x * invEffScale.x, lnf.y * invEffScale.y, lnf.z * invEffScale.z, 0.0f);
                XMVECTOR worldN = XMVector3Normalize(XMVector3TransformNormal(scaledN, rotM));
                XMStoreFloat3(&closestWorldNormal, worldN);
            }
        }
    }

    if (hitFound) {
        XMVECTOR hitPointV = XMLoadFloat3(&rayOrigin) + XMLoadFloat3(&rayDirection) * closestT;
        XMStoreFloat3(&outHitPoint, hitPointV);
        outHitNormal = closestWorldNormal;
        return true;
    }
    return false;
}

// Moller-Trumbore ray-triangle intersection algorithm
bool MeshCollider::RayIntersectsTriangle(const XMFLOAT3& rayOrigin,
    const XMFLOAT3& rayDir,
    const XMFLOAT3& v0,
    const XMFLOAT3& v1,
    const XMFLOAT3& v2,
    float& t) const {
    const float EPSILON = 0.0000001f;

    XMVECTOR vertex0 = XMLoadFloat3(&v0);
    XMVECTOR vertex1 = XMLoadFloat3(&v1);
    XMVECTOR vertex2 = XMLoadFloat3(&v2);
    XMVECTOR origin = XMLoadFloat3(&rayOrigin);
    XMVECTOR direction = XMLoadFloat3(&rayDir);

    XMVECTOR edge1 = vertex1 - vertex0;
    XMVECTOR edge2 = vertex2 - vertex0;
    XMVECTOR h = XMVector3Cross(direction, edge2);
    float a = XMVectorGetX(XMVector3Dot(edge1, h));

    if (a > -EPSILON && a < EPSILON)
        return false;    // Ray parallel to triangle

    float f = 1.0f / a;
    XMVECTOR s = origin - vertex0;
    float u = f * XMVectorGetX(XMVector3Dot(s, h));

    if (u < 0.0f || u > 1.0f)
        return false;

    XMVECTOR q = XMVector3Cross(s, edge1);
    float v = f * XMVectorGetX(XMVector3Dot(direction, q));

    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * XMVectorGetX(XMVector3Dot(edge2, q));
    return t > EPSILON;
}

// Triangle-sphere collision detection
bool MeshCollider::TriangleIntersectsSphere(const XMFLOAT3& v0,
    const XMFLOAT3& v1,
    const XMFLOAT3& v2,
    const XMFLOAT3& sphereCenter,
    float sphereRadius,
    XMFLOAT3& outNormal,
    float& outPenetration) const {
    XMVECTOR p0 = XMLoadFloat3(&v0);
    XMVECTOR p1 = XMLoadFloat3(&v1);
    XMVECTOR p2 = XMLoadFloat3(&v2);
    XMVECTOR center = XMLoadFloat3(&sphereCenter);

    XMVECTOR edge1 = p1 - p0;
    XMVECTOR edge2 = p2 - p0;
    XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));
    XMFLOAT3 triangleNormal;
    XMStoreFloat3(&triangleNormal, normal);

    XMVECTOR closestPoint;
    float dist = XMVectorGetX(XMVector3Dot(center - p0, normal));
    XMVECTOR projectedCenter = center - normal * dist;

    XMVECTOR edge0_v = p1 - p0;
    XMVECTOR edge1_v = p2 - p1;
    XMVECTOR edge2_v = p0 - p2;

    XMVECTOR c0 = projectedCenter - p0;
    XMVECTOR c1 = projectedCenter - p1;
    XMVECTOR c2 = projectedCenter - p2;

    bool inside =
        XMVectorGetX(XMVector3Dot(normal, XMVector3Cross(edge0_v, c0))) >= 0 &&
        XMVectorGetX(XMVector3Dot(normal, XMVector3Cross(edge1_v, c1))) >= 0 &&
        XMVectorGetX(XMVector3Dot(normal, XMVector3Cross(edge2_v, c2))) >= 0;

    if (inside) {
        closestPoint = projectedCenter;
    }
    else {
        float minDistSq = FLT_MAX;
        auto closestPointOnSegment = [](XMVECTOR a, XMVECTOR b, XMVECTOR p) {
            XMVECTOR ab = b - a;
            float t = XMVectorGetX(XMVector3Dot(p - a, ab)) / XMVectorGetX(XMVector3Dot(ab, ab));
            t = max(0.0f, min(1.0f, t));
            return a + ab * t;
            };

        XMVECTOR candidates[3] = {
            closestPointOnSegment(p0, p1, center),
            closestPointOnSegment(p1, p2, center),
            closestPointOnSegment(p2, p0, center)
        };

        for (int i = 0; i < 3; i++) {
            float dSq = XMVectorGetX(XMVector3LengthSq(candidates[i] - center));
            if (dSq < minDistSq) {
                minDistSq = dSq;
                closestPoint = candidates[i];
            }
        }
    }

    XMVECTOR diff = center - closestPoint;
    float distSq = XMVectorGetX(XMVector3LengthSq(diff));

    if (distSq <= sphereRadius * sphereRadius) {
        float dist = std::sqrt(max(distSq, 1e-12f));
        XMVECTOR n;

        auto orientNormalTowardSphere = [&]() {
            float signedDist = XMVectorGetX(XMVector3Dot(center - p0, normal));
            return (signedDist >= 0.0f) ? normal : XMVectorNegate(normal);
        };

        if (dist < 1e-6f) {
            // Coplanar contact: use the face normal, but always push toward the sphere.
            n = orientNormalTowardSphere();
            outPenetration = sphereRadius;
        }
        else {
            n = diff / dist;
            // Edge/face contact: keep the normal pointing from the surface toward the sphere.
            if (XMVectorGetX(XMVector3Dot(n, diff)) < 0.0f)
                n = XMVectorNegate(n);

            // On steep edges prefer the face normal when it still points toward the sphere.
            float dot = XMVectorGetX(XMVector3Dot(n, normal));
            if (dot < 0.9f) {
                XMVECTOR faceTowardSphere = orientNormalTowardSphere();
                if (XMVectorGetX(XMVector3Dot(faceTowardSphere, diff)) > 0.0f)
                    n = faceTowardSphere;
            }
            outPenetration = sphereRadius - dist;
        }
        XMStoreFloat3(&outNormal, n);
        return true;
    }
    return false;
}