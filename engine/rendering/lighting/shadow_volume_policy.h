// shadow_volume_policy.h
// Per-frame decisions of the stencil shadow-volume pass. Pure, header-only;
// Linux-tested by tests/shadow_volume_policy_test.cpp (together with anim_cull).
//
// The volume pass runs after the main camera pass of the same frame (same
// Lod_GetFrameSerial), so it reuses the main pass's decisions instead of
// recomputing them:
//   - a Visual the main pass hid by visual.cullDistance casts no volume either
//     (same rule as the CSM cascade passes);
//   - a Visual whose volume was drawn counts as drawn for off-screen animation
//     culling, so an off-screen character keeps animating while its shadow is
//     on screen (the CSM cascade passes do the same).
#pragma once

#include <cmath>
#include <cstdint>

namespace shadow_volume_policy {

constexpr uint64_t kNever = ~uint64_t{0};

// Should this Visual draw its shadow volume this frame?
//   optedIn               visual.shadowVolume
//   realModel             Play / runtime real-model pass (edit view draws proxies, no LOD)
//   distanceCulledSerial  frame serial when the main pass last hid it by cullDistance
//   frameSerial           current Lod_GetFrameSerial()
inline bool ShouldCast(bool optedIn, bool realModel, uint64_t distanceCulledSerial,
                       uint64_t frameSerial) {
    if (!optedIn)
        return false;
    if (realModel && distanceCulledSerial != kNever && distanceCulledSerial == frameSerial)
        return false;
    return true;
}

// Whether a drawn volume stamps VisualComponent::lastDrawnSerial (only the real-model
// pass takes part in animation culling, like the main and cascade passes).
inline bool StampsDrawn(bool realModel) { return realModel; }

// How far (world units) the vertex shader pushes corners that face away from the light.
// A finite length works for perspective and orthographic cameras alike (a w = 0 point at
// infinity is clipped away by an orthographic projection); depth clip is off for the
// volume pass, so anything past the far plane clamps onto it. 4x the far clip covers
// casters up to 3 far-clips away from the camera.
constexpr float kMinExtrusion = 100.0f;
constexpr float kMaxExtrusion = 1.0e6f;

inline float ExtrusionLength(float farClip) {
    if (!std::isfinite(farClip) || farClip <= 0.0f)
        return 1000.0f;
    const float len = farClip * 4.0f;
    if (len < kMinExtrusion) return kMinExtrusion;
    if (len > kMaxExtrusion) return kMaxExtrusion;
    return len;
}

}  // namespace shadow_volume_policy
