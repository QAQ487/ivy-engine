// shadow_cascade_cull.cpp
// Per-cascade light-ortho caster cull counters.

#include "shadow_cascade_cull.h"

namespace {
constexpr int kMaxCascades = 4;

bool g_frameActive = false;
bool g_cascadeActive = false;
int g_activeCascade = -1;
int g_cascadeCount = 0;

frustum_cull::FrustumPlanes g_planes{};
ShadowCascadeCullStats g_frameStats{};
ShadowCascadeCullStats g_cascadeStats{};
ShadowCascadeCullStats g_perCascade[kMaxCascades]{};

ShadowCascadeCullStats g_lastStats{};
ShadowCascadeCullStats g_lastPerCascade[kMaxCascades]{};
int g_lastCascadeCount = 0;
}

void ShadowCascadeCull_BeginFrame() {
    g_frameActive = true;
    g_cascadeActive = false;
    g_activeCascade = -1;
    g_cascadeCount = 0;
    g_frameStats = {};
    g_cascadeStats = {};
    for (int i = 0; i < kMaxCascades; ++i)
        g_perCascade[i] = {};
}

void ShadowCascadeCull_BeginCascade(int cascadeIndex, DirectX::FXMMATRIX lightViewProj) {
    if (!g_frameActive)
        ShadowCascadeCull_BeginFrame();

    if (cascadeIndex < 0)
        cascadeIndex = 0;
    if (cascadeIndex >= kMaxCascades)
        cascadeIndex = kMaxCascades - 1;

    g_cascadeActive = true;
    g_activeCascade = cascadeIndex;
    g_cascadeStats = {};
    if (cascadeIndex + 1 > g_cascadeCount)
        g_cascadeCount = cascadeIndex + 1;
    frustum_cull::ExtractFrustumPlanesD3D(lightViewProj, g_planes);
}

void ShadowCascadeCull_EndCascade() {
    if (!g_cascadeActive)
        return;
    if (g_activeCascade >= 0 && g_activeCascade < kMaxCascades)
        g_perCascade[g_activeCascade] = g_cascadeStats;
    g_cascadeActive = false;
    g_activeCascade = -1;
}

void ShadowCascadeCull_EndFrame() {
    if (g_cascadeActive)
        ShadowCascadeCull_EndCascade();
    g_lastStats = g_frameStats;
    g_lastCascadeCount = g_cascadeCount;
    for (int i = 0; i < kMaxCascades; ++i)
        g_lastPerCascade[i] = g_perCascade[i];
    g_frameActive = false;
}

bool ShadowCascadeCull_IsCascadePassActive() {
    return g_cascadeActive;
}

int ShadowCascadeCull_GetActiveCascadeIndex() {
    return g_activeCascade;
}

const frustum_cull::FrustumPlanes& ShadowCascadeCull_GetPlanes() {
    return g_planes;
}

void ShadowCascadeCull_RecordConsidered() {
    ++g_frameStats.considered;
    ++g_cascadeStats.considered;
}

void ShadowCascadeCull_RecordCulled() {
    ++g_frameStats.culled;
    ++g_cascadeStats.culled;
}

void ShadowCascadeCull_RecordDrawn() {
    ++g_frameStats.drawn;
    ++g_cascadeStats.drawn;
}

ShadowCascadeCullStats ShadowCascadeCull_GetStats() {
    return g_frameStats;
}

ShadowCascadeCullStats ShadowCascadeCull_GetLastStats() {
    return g_lastStats;
}

ShadowCascadeCullStats ShadowCascadeCull_GetLastCascadeStats(int cascadeIndex) {
    if (cascadeIndex < 0 || cascadeIndex >= kMaxCascades)
        return {};
    return g_lastPerCascade[cascadeIndex];
}

int ShadowCascadeCull_GetLastCascadeCount() {
    return g_lastCascadeCount;
}
