// frustum_cull.cpp
// Per-frame frustum cull counters for the main camera pass.

#include "frustum_cull.h"

namespace {
bool g_mainPassActive = false;
frustum_cull::FrustumPlanes g_planes{};
FrustumCullStats g_stats{};
FrustumCullStats g_lastStats{};
}

void FrustumCull_BeginMainPass(DirectX::FXMMATRIX viewProj) {
    g_stats = {};
    g_mainPassActive = true;
    frustum_cull::ExtractFrustumPlanesD3D(viewProj, g_planes);
}

void FrustumCull_EndMainPass() {
    if (!g_mainPassActive)
        return;
    g_lastStats = g_stats;
    g_mainPassActive = false;
}

bool FrustumCull_IsMainPassActive() {
    return g_mainPassActive;
}

const frustum_cull::FrustumPlanes& FrustumCull_GetPlanes() {
    return g_planes;
}

void FrustumCull_RecordConsidered() { ++g_stats.considered; }
void FrustumCull_RecordCulled() { ++g_stats.culled; }
void FrustumCull_RecordDrawn() { ++g_stats.drawn; }

FrustumCullStats FrustumCull_GetStats() {
    return g_stats;
}

FrustumCullStats FrustumCull_GetLastStats() {
    return g_lastStats;
}
