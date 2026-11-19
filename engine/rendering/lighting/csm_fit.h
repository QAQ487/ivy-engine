// csm_fit.h
// Cascade fitting math for the cascaded shadow maps (engine/rendering/lighting/shadow.cpp).
// Pure, header-only, no DirectXMath; Linux-tested by tests/csm_fit_test.cpp.
//
// Two rules keep the shadow edges from shimmering while the camera moves:
//   - Rotation: each cascade covers a bounding sphere of its view-frustum slice. The sphere
//     depends only on the slice's near/far distances, the field of view and the aspect, so
//     turning the camera does not change the cascade's size (texel size stays constant).
//   - Translation: the sphere centre is snapped to whole shadow-map texels in a light-space
//     frame whose origin is the world origin (not the moving camera), so moving the camera
//     slides the cascade by whole texels and the rasterised shadow edges do not crawl.
#pragma once

#include <cmath>

namespace csm_fit {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline Vec3 Add(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 Sub(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 Scale(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline Vec3 Normalize(Vec3 a, Vec3 fallback) {
    const float len = Length(a);
    if (!(len > 1e-6f) || !std::isfinite(len))
        return fallback;
    return Scale(a, 1.0f / len);
}

// Bounding sphere of a view-frustum slice [nearZ, farZ] along the view axis.
// centerDist: distance from the eye to the sphere centre along the view direction.
struct SliceSphere {
    float centerDist = 0.0f;
    float radius = 0.0f;
};

// Radius granularity: radii are rounded up to 1/16 world unit so float noise in the
// slice distances cannot change the texel size from frame to frame.
constexpr float kRadiusStep = 1.0f / 16.0f;
constexpr float kMinRadius = 0.5f;

// Smallest sphere centred on the view axis that contains the slice's 8 corners.
// k2 = tan(fovY/2)^2 * (1 + aspect^2) is the squared corner offset per unit depth.
// Equal distance to the near and far corners gives z = (n + f)(1 + k2) / 2; when that
// lies past the far plane (wide, shallow slices), the far rectangle's circumcircle wins.
inline SliceSphere FitSliceSphere(float nearZ, float farZ, float tanHalfFovY, float aspect) {
    SliceSphere s;
    if (!std::isfinite(nearZ) || !std::isfinite(farZ) || !std::isfinite(tanHalfFovY)
        || !std::isfinite(aspect) || nearZ < 0.0f || farZ <= nearZ) {
        s.centerDist = std::isfinite(nearZ) ? nearZ : 0.0f;
        s.radius = kMinRadius;
        return s;
    }
    const float k2 = tanHalfFovY * tanHalfFovY * (1.0f + aspect * aspect);
    float z = 0.5f * (nearZ + farZ) * (1.0f + k2);
    if (z > farZ) z = farZ;
    const float dz = farZ - z;
    float r = std::sqrt(dz * dz + farZ * farZ * k2);
    r = std::ceil(r / kRadiusStep) * kRadiusStep;
    if (r < kMinRadius) r = kMinRadius;
    s.centerDist = z;
    s.radius = r;
    return s;
}

// Light-space axes matching XMMatrixLookAtLH(eye, eye + shineDir, up) with the up rule of
// shadow.cpp BuildLightView (world +Y, or +Z when the light is within ~18 deg of vertical).
struct LightBasis {
    Vec3 right;    // light-space +X
    Vec3 up;       // light-space +Y
    Vec3 forward;  // light-space +Z (= shine direction)
};

inline LightBasis MakeLightBasis(Vec3 shineDir) {
    LightBasis b;
    b.forward = Normalize(shineDir, Vec3{ 0.0f, -1.0f, 0.0f });
    Vec3 up{ 0.0f, 1.0f, 0.0f };
    if (std::fabs(Dot(b.forward, up)) > 0.95f)
        up = Vec3{ 0.0f, 0.0f, 1.0f };
    b.right = Normalize(Cross(up, b.forward), Vec3{ 1.0f, 0.0f, 0.0f });
    b.up = Cross(b.forward, b.right);
    return b;
}

// World size of one shadow-map texel for an ortho projection of half-width halfExtent.
inline float TexelWorldSize(float halfExtent, unsigned mapSize) {
    return (halfExtent * 2.0f) / static_cast<float>(mapSize == 0 ? 1u : mapSize);
}

// Half-width of the cascade's ortho projection for a slice sphere of the given radius:
// one spare texel on every side, because snapping moves the centre by up to one texel on
// each axis. Like the radius, it does not change when the camera turns.
inline float CascadeHalfExtent(float radius, unsigned mapSize) {
    if (mapSize <= 2u)
        return radius * 2.0f;
    return radius * static_cast<float>(mapSize) / static_cast<float>(mapSize - 2u);
}

// Moves centerWS within the light's image plane so its light-space X/Y (measured from the
// world origin) are whole multiples of the texel size. Depth along the light is kept.
inline Vec3 SnapCenterToTexels(Vec3 centerWS, const LightBasis& basis, float texelWorld) {
    if (!(texelWorld > 0.0f) || !std::isfinite(texelWorld))
        return centerWS;
    const float x = Dot(centerWS, basis.right);
    const float y = Dot(centerWS, basis.up);
    const float sx = std::floor(x / texelWorld) * texelWorld;
    const float sy = std::floor(y / texelWorld) * texelWorld;
    return Add(centerWS, Add(Scale(basis.right, sx - x), Scale(basis.up, sy - y)));
}

}  // namespace csm_fit
