// light_select_math.h
// Header-only choice of which scene lights fill the fixed GPU light slots
// (Lighting::MAX_LIGHTS). No engine / D3D dependency.
//
// Rules:
//  - n <= maxOut: every light is used, in scene order (behaviour unchanged).
//  - Otherwise directional lights rank first (they affect everything), then
//    point / spot lights by influence at the camera:
//        luma * range^2 / (range^2 + distance^2)
//    where luma is the brightest channel of colour * intensity. A light that was
//    selected last frame gets a bonus so near-ties do not flicker.
//  - The chosen lights keep their scene order, so shadow-caster indices that are
//    handed out in light order stay stable.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace light_select {

constexpr float kStickyBonus = 1.25f;
constexpr float kDirectionalScore = 1.0e30f;

struct Candidate {
    bool directional = false;
    float x = 0.0f, y = 0.0f, z = 0.0f;  // world position (point / spot)
    float range = 0.0f;
    float luma = 0.0f;                   // max(r,g,b) of colour * intensity
    bool wasSelected = false;            // selected in the previous frame
};

inline float Score(const Candidate& c, float cx, float cy, float cz) {
    if (c.directional)
        return kDirectionalScore;
    const float luma = std::isfinite(c.luma) ? std::max(c.luma, 0.0f) : 0.0f;
    const float range = (std::isfinite(c.range) && c.range > 1e-3f) ? c.range : 1e-3f;
    const float dx = c.x - cx, dy = c.y - cy, dz = c.z - cz;
    float d2 = dx * dx + dy * dy + dz * dz;
    if (!std::isfinite(d2))
        d2 = 3.0e38f;
    const float r2 = range * range;
    float score = luma * (r2 / (r2 + d2));
    if (c.wasSelected)
        score *= kStickyBonus;
    return score;
}

// Writes up to maxOut indices (ascending) into outIdx and returns how many.
inline int Select(const Candidate* lights, int n,
                  float camX, float camY, float camZ,
                  int maxOut, int* outIdx) {
    if (!lights || n <= 0 || maxOut <= 0 || !outIdx)
        return 0;
    if (n <= maxOut) {
        for (int i = 0; i < n; ++i)
            outIdx[i] = i;
        return n;
    }

    std::vector<float> score(static_cast<size_t>(n));
    std::vector<int> order(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        score[static_cast<size_t>(i)] = Score(lights[i], camX, camY, camZ);
        order[static_cast<size_t>(i)] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return score[static_cast<size_t>(a)] > score[static_cast<size_t>(b)];
    });
    std::sort(order.begin(), order.begin() + maxOut);
    for (int i = 0; i < maxOut; ++i)
        outIdx[i] = order[static_cast<size_t>(i)];
    return maxOut;
}

} // namespace light_select
