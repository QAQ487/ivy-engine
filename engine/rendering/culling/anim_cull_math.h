// anim_cull_math.h
// Header-only decision for skipping skeletal animation evaluation of Visuals
// that were not drawn. No engine / D3D dependency.
//
// A Visual records two frame serials while the 3D object pass runs:
//   consideredSerial - the last frame DrawVisual looked at it (real model, Play only)
//   drawnSerial      - the last frame any pass (main or shadow cascade) submitted it
// The animation update runs before the next frame's draw, so `currentSerial` is
// still the frame that was last drawn.

#pragma once

#include <cstdint>

namespace anim_cull {

constexpr uint64_t kNever = ~uint64_t{0};

// True when pose evaluation can be skipped this update: the feature is on, the
// Visual was considered by the last completed draw frame, and no pass drew it.
// A Visual never considered (new, hidden by an inactive parent, editor) keeps
// animating so it can never freeze before its first draw.
inline bool ShouldSkipPose(bool enabled,
                           uint64_t consideredSerial,
                           uint64_t drawnSerial,
                           uint64_t currentSerial) {
    if (!enabled)
        return false;
    if (consideredSerial == kNever || consideredSerial != currentSerial)
        return false;
    return drawnSerial != currentSerial;
}

} // namespace anim_cull
