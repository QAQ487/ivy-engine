// shadow_cascade_cull.h
// Per-cascade shadow caster cull state and counters (light-space open-near).

#pragma once

#include "engine/rendering/culling/frustum_cull_math.h"
#include <DirectXMath.h>

struct ShadowCascadeCullStats {
    int considered = 0;
    int culled = 0;
    int drawn = 0;
};

// Call once before the cascade depth loop; resets frame totals.
void ShadowCascadeCull_BeginFrame();
// Call at the start of each cascade depth pass with that cascade's lightView*lightProj.
void ShadowCascadeCull_BeginCascade(int cascadeIndex, DirectX::FXMMATRIX lightViewProj);
void ShadowCascadeCull_EndCascade();
// Call after the cascade loop; latches frame totals for UI / bridge reads.
void ShadowCascadeCull_EndFrame();

bool ShadowCascadeCull_IsCascadePassActive();
int ShadowCascadeCull_GetActiveCascadeIndex();

const frustum_cull::FrustumPlanes& ShadowCascadeCull_GetPlanes();

void ShadowCascadeCull_RecordConsidered();
void ShadowCascadeCull_RecordCulled();
void ShadowCascadeCull_RecordDrawn();

// Live totals for the current frame (sum across cascades so far).
ShadowCascadeCullStats ShadowCascadeCull_GetStats();
// Completed frame totals (stable for overlays / bridge).
ShadowCascadeCullStats ShadowCascadeCull_GetLastStats();
// Per-cascade last-frame counters (index clamped to [0, maxCascades)).
ShadowCascadeCullStats ShadowCascadeCull_GetLastCascadeStats(int cascadeIndex);
int ShadowCascadeCull_GetLastCascadeCount();
