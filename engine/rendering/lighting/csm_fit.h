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
// It also holds the cascade splits (perspective and orthographic cameras), the per-cascade
// receiver bias / normal offset / blend width, and the shadow-distance fade.
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

// Bounding sphere of an orthographic camera's slice [nearZ, farZ]: a box of half-height
// halfHeight and half-width halfHeight * aspect. Centred at mid-depth; like the
// perspective sphere it does not change when the camera turns.
inline SliceSphere FitSliceSphereOrtho(float nearZ, float farZ, float halfHeight, float aspect) {
    SliceSphere s;
    if (!std::isfinite(nearZ) || !std::isfinite(farZ) || !std::isfinite(halfHeight)
        || !std::isfinite(aspect) || farZ <= nearZ) {
        s.centerDist = std::isfinite(nearZ) ? nearZ : 0.0f;
        s.radius = kMinRadius;
        return s;
    }
    const float hz = 0.5f * (farZ - nearZ);
    const float hh = std::fabs(halfHeight);
    const float hw = hh * std::fabs(aspect);
    float r = std::sqrt(hz * hz + hh * hh + hw * hw);
    r = std::ceil(r / kRadiusStep) * kRadiusStep;
    if (r < kMinRadius) r = kMinRadius;
    s.centerDist = nearZ + hz;
    s.radius = r;
    return s;
}

// Far distance of cascade `index` (0-based) out of `count` between nearZ and farZ.
// Perspective cameras blend log and uniform splits (lambda 0.65: more resolution near the
// camera, where a texel covers more pixels). Orthographic cameras see every depth at the
// same scale, so their splits are uniform.
constexpr float kPerspectiveSplitLambda = 0.65f;

inline float CascadeSplit(int index, int count, float nearZ, float farZ, bool orthographic) {
    if (count <= 0) return farZ;
    if (index >= count - 1) return farZ;
    const float p = static_cast<float>(index + 1) / static_cast<float>(count);
    const float uni = nearZ + (farZ - nearZ) * p;
    if (orthographic || !(nearZ > 0.0f))
        return uni;
    const float logSplit = nearZ * std::pow(farZ / nearZ, p);
    return uni * (1.0f - kPerspectiveSplitLambda) + logSplit * kPerspectiveSplitLambda;
}

// Per-cascade receiver parameters (ShadowBuffer.ShadowParams[i] in LightingCommon.hlsli).
// Biases are expressed in shadow-map texels of the cascade, so a coarse far cascade gets a
// proportionally larger bias than a sharp near one instead of one value for all.
//   constDepthBias  x: depth bias in normalised light depth (constant part)
//   slopeDepthBias  y: depth bias per unit tan(angle between normal and light), same units;
//                      replaces the ddx/ddy slope estimate (undefined in the cascade branches)
//   normalOffset    z: world units the receiver is pushed along its normal before lookup
//   blendWidth      w: view-space depth over which this cascade fades into the next one
constexpr float kConstBiasTexels = 1.0f;
constexpr float kSlopeBiasTexels = 1.0f;
constexpr float kMaxSlopeTan = 4.0f;  // shader clamps tan(angle) to this
constexpr float kNormalOffsetTexels = 1.5f;
constexpr float kBlendFraction = 0.1f;   // of the cascade's depth span
constexpr float kMinBlendWidth = 0.25f;

struct ReceiverParams {
    float constDepthBias = 0.0f;
    float slopeDepthBias = 0.0f;
    float normalOffset = 0.0f;
    float blendWidth = kMinBlendWidth;
};

inline ReceiverParams CascadeReceiverParams(float texelWorld, float depthRange, float sliceNear,
                                            float sliceFar) {
    ReceiverParams r;
    if (!(texelWorld > 0.0f) || !std::isfinite(texelWorld))
        texelWorld = 0.0f;
    const float invRange = (depthRange > 1e-4f && std::isfinite(depthRange)) ? 1.0f / depthRange : 0.0f;
    r.constDepthBias = kConstBiasTexels * texelWorld * invRange;
    r.slopeDepthBias = kSlopeBiasTexels * texelWorld * invRange;
    r.normalOffset = kNormalOffsetTexels * texelWorld;
    const float span = sliceFar - sliceNear;
    float blend = (std::isfinite(span) && span > 0.0f) ? span * kBlendFraction : 0.0f;
    if (blend < kMinBlendWidth) blend = kMinBlendWidth;
    r.blendWidth = blend;
    return r;
}

// Shadows fade to lit over the last part of the shadow distance instead of ending in a hard
// line (kFadeFraction of the distance, at least kMinFadeLength).
constexpr float kFadeFraction = 0.1f;
constexpr float kMinFadeLength = 1.0f;

inline float ShadowFadeLength(float shadowFar) {
    if (!std::isfinite(shadowFar) || shadowFar <= 0.0f) return kMinFadeLength;
    const float len = shadowFar * kFadeFraction;
    return len < kMinFadeLength ? kMinFadeLength : len;
}

// CPU mirror of the shader's fade factor (0 = full shadow kept, 1 = fully faded to lit).
inline float ShadowFadeAmount(float viewZ, float shadowFar, float fadeLength) {
    const float len = fadeLength > 1e-3f ? fadeLength : 1e-3f;
    const float t = (viewZ - (shadowFar - len)) / len;
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

// CPU mirror of the shader's slope term: tan of the angle between normal and light,
// clamped (grazing angles would otherwise need unbounded bias).
inline float SlopeTan(float nDotL) {
    const float c = nDotL < 1e-3f ? 1e-3f : (nDotL > 1.0f ? 1.0f : nDotL);
    const float t = std::sqrt(1.0f - c * c) / c;
    return t > kMaxSlopeTan ? kMaxSlopeTan : t;
}

}  // namespace csm_fit
