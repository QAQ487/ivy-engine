// frustum_cull.h
// Per-frame 3D frustum cull state, counters, and main-pass helpers.

#pragma once

#include "engine/rendering/culling/frustum_cull_math.h"
#include <DirectXMath.h>

struct FrustumCullStats {
    int considered = 0;
    int culled = 0;
    int drawn = 0;
};

// Call once at the start of the main camera color pass (not shadow cascades).
void FrustumCull_BeginMainPass(DirectX::FXMMATRIX viewProj);
void FrustumCull_EndMainPass();

// True while the main camera pass is active and culling may run.
bool FrustumCull_IsMainPassActive();

const frustum_cull::FrustumPlanes& FrustumCull_GetPlanes();

void FrustumCull_RecordConsidered();
void FrustumCull_RecordCulled();
void FrustumCull_RecordDrawn();

FrustumCullStats FrustumCull_GetStats();
// Completed main-pass counters (stable across shadow work and UI reads).
FrustumCullStats FrustumCull_GetLastStats();
