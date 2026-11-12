// frustum_cull_math.h
// Header-only 3D frustum / AABB helpers (D3D clip-space z in [0,1]).
// Uses DirectX::XMFLOAT3/4 for storage; matrix ops are plain floats so Linux
// offline checks compile with a minimal DirectXMath shim.

#pragma once

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace frustum_cull {

struct Aabb {
    DirectX::XMFLOAT3 min{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 max{0.0f, 0.0f, 0.0f};
    bool valid = false;
};

struct FrustumPlanes {
    // Inward-facing planes: ax + by + cz + d >= 0 is inside.
    DirectX::XMFLOAT4 planes[6]{};
};

inline void NormalizePlane(DirectX::XMFLOAT4& p) {
    const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (len > 1e-8f) {
        const float inv = 1.0f / len;
        p.x *= inv;
        p.y *= inv;
        p.z *= inv;
        p.w *= inv;
    }
}

// Row-major 4x4 view-projection (clip = position * M), Direct3D z in [0,1].
// Layout: m[0..3]=row0, m[4..7]=row1, m[8..11]=row2, m[12..15]=row3.
inline void ExtractFrustumPlanesD3D(const float m[16], FrustumPlanes& out) {
    out.planes[0] = {m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]}; // Left
    out.planes[1] = {m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]}; // Right
    out.planes[2] = {m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]}; // Bottom
    out.planes[3] = {m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]}; // Top
    // Near uses the z row alone for D3D [0,1] (not OpenGL's w+z).
    out.planes[4] = {m[2], m[6], m[10], m[14]};                              // Near
    out.planes[5] = {m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]}; // Far
    for (int i = 0; i < 6; ++i)
        NormalizePlane(out.planes[i]);
}

inline DirectX::XMFLOAT3 TransformPointRowMajor(const DirectX::XMFLOAT3& p, const float m[16]) {
    // Row-vector * matrix (DirectXMath convention).
    const float x = p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12];
    const float y = p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13];
    const float z = p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14];
    const float w = p.x * m[3] + p.y * m[7] + p.z * m[11] + m[15];
    if (std::fabs(w) > 1e-8f) {
        const float inv = 1.0f / w;
        return {x * inv, y * inv, z * inv};
    }
    return {x, y, z};
}

inline Aabb MakeAabbFromPoints(const DirectX::XMFLOAT3* points, size_t count) {
    Aabb box{};
    if (!points || count == 0)
        return box;
    box.min = points[0];
    box.max = points[0];
    for (size_t i = 1; i < count; ++i) {
        box.min.x = (std::min)(box.min.x, points[i].x);
        box.min.y = (std::min)(box.min.y, points[i].y);
        box.min.z = (std::min)(box.min.z, points[i].z);
        box.max.x = (std::max)(box.max.x, points[i].x);
        box.max.y = (std::max)(box.max.y, points[i].y);
        box.max.z = (std::max)(box.max.z, points[i].z);
    }
    box.valid = true;
    return box;
}

inline void InflateAabb(Aabb& box, float pad) {
    if (!box.valid || pad <= 0.0f)
        return;
    box.min.x -= pad;
    box.min.y -= pad;
    box.min.z -= pad;
    box.max.x += pad;
    box.max.y += pad;
    box.max.z += pad;
}

// Conservative OBB→AABB via the 8 local corners.
inline Aabb TransformAabb(const Aabb& local, const float world[16]) {
    Aabb out{};
    if (!local.valid || !world)
        return out;

    const DirectX::XMFLOAT3 corners[8] = {
        {local.min.x, local.min.y, local.min.z},
        {local.max.x, local.min.y, local.min.z},
        {local.min.x, local.max.y, local.min.z},
        {local.max.x, local.max.y, local.min.z},
        {local.min.x, local.min.y, local.max.z},
        {local.max.x, local.min.y, local.max.z},
        {local.min.x, local.max.y, local.max.z},
        {local.max.x, local.max.y, local.max.z},
    };

    DirectX::XMFLOAT3 worldCorners[8];
    for (int i = 0; i < 8; ++i)
        worldCorners[i] = TransformPointRowMajor(corners[i], world);
    return MakeAabbFromPoints(worldCorners, 8);
}

// Build Scale * RotationYawPitchRoll * Translation as row-major float[16].
// Matches DirectXMath: XMMatrixScaling * XMMatrixRotationRollPitchYaw * XMMatrixTranslation
// (RotationRollPitchYaw = pitch(X), yaw(Y), roll(Z)).
inline void BuildWorldMatrixTRS(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& rotationDeg,
    const DirectX::XMFLOAT3& scale,
    float out[16]) {
    const float degToRad = 0.017453292519943295f;
    const float pitch = rotationDeg.x * degToRad;
    const float yaw = rotationDeg.y * degToRad;
    const float roll = rotationDeg.z * degToRad;

    const float cx = std::cos(pitch), sx = std::sin(pitch);
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cz = std::cos(roll), sz = std::sin(roll);

    // R = Rz * Ry * Rx (same composition as XMMatrixRotationRollPitchYaw)
    const float r00 = cy * cz;
    const float r01 = cy * sz;
    const float r02 = -sy;
    const float r10 = sx * sy * cz - cx * sz;
    const float r11 = sx * sy * sz + cx * cz;
    const float r12 = sx * cy;
    const float r20 = cx * sy * cz + sx * sz;
    const float r21 = cx * sy * sz - sx * cz;
    const float r22 = cx * cy;

    const float sx_ = scale.x, sy_ = scale.y, sz_ = scale.z;
    out[0] = r00 * sx_;
    out[1] = r01 * sx_;
    out[2] = r02 * sx_;
    out[3] = 0.0f;
    out[4] = r10 * sy_;
    out[5] = r11 * sy_;
    out[6] = r12 * sy_;
    out[7] = 0.0f;
    out[8] = r20 * sz_;
    out[9] = r21 * sz_;
    out[10] = r22 * sz_;
    out[11] = 0.0f;
    out[12] = position.x;
    out[13] = position.y;
    out[14] = position.z;
    out[15] = 1.0f;
}

inline Aabb TransformAabbTRS(
    const Aabb& local,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& rotationDeg,
    const DirectX::XMFLOAT3& scale) {
    float world[16];
    BuildWorldMatrixTRS(position, rotationDeg, scale, world);
    return TransformAabb(local, world);
}

#if defined(_WIN32)
inline void ExtractFrustumPlanesD3D(DirectX::FXMMATRIX viewProj, FrustumPlanes& out) {
    DirectX::XMFLOAT4X4 m;
    DirectX::XMStoreFloat4x4(&m, viewProj);
    const float rows[16] = {
        m._11, m._12, m._13, m._14,
        m._21, m._22, m._23, m._24,
        m._31, m._32, m._33, m._34,
        m._41, m._42, m._43, m._44
    };
    ExtractFrustumPlanesD3D(rows, out);
}

inline Aabb TransformAabb(const Aabb& local, DirectX::FXMMATRIX world) {
    DirectX::XMFLOAT4X4 m;
    DirectX::XMStoreFloat4x4(&m, world);
    const float rows[16] = {
        m._11, m._12, m._13, m._14,
        m._21, m._22, m._23, m._24,
        m._31, m._32, m._33, m._34,
        m._41, m._42, m._43, m._44
    };
    return TransformAabb(local, rows);
}
#endif

// Returns false when the AABB is fully outside the frustum.
inline bool AabbIntersectsFrustum(const Aabb& box, const FrustumPlanes& frustum) {
    if (!box.valid)
        return true; // caller should not cull invalid bounds

    for (int i = 0; i < 6; ++i) {
        const DirectX::XMFLOAT4& p = frustum.planes[i];
        const float px = (p.x >= 0.0f) ? box.max.x : box.min.x;
        const float py = (p.y >= 0.0f) ? box.max.y : box.min.y;
        const float pz = (p.z >= 0.0f) ? box.max.z : box.min.z;
        if (p.x * px + p.y * py + p.z * pz + p.w < 0.0f)
            return false;
    }
    return true;
}

// Same as AabbIntersectsFrustum but skips the near plane (index 4).
// Directional shadow cascade caster cull: casters between the light and the
// cascade near plane must still draw (pancaking / open near toward the light).
// Far + lateral planes still cull. Invalid bounds never cull.
inline bool AabbIntersectsFrustumOpenNear(const Aabb& box, const FrustumPlanes& frustum) {
    if (!box.valid)
        return true;

    for (int i = 0; i < 6; ++i) {
        if (i == 4)
            continue; // skip near
        const DirectX::XMFLOAT4& p = frustum.planes[i];
        const float px = (p.x >= 0.0f) ? box.max.x : box.min.x;
        const float py = (p.y >= 0.0f) ? box.max.y : box.min.y;
        const float pz = (p.z >= 0.0f) ? box.max.z : box.min.z;
        if (p.x * px + p.y * py + p.z * pz + p.w < 0.0f)
            return false;
    }
    return true;
}

} // namespace frustum_cull
