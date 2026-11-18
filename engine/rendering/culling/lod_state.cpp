// lod_state.cpp
// Per-frame mesh LOD state and counters.

#include "lod_state.h"

namespace {
bool g_active = false;
DirectX::XMFLOAT3 g_cameraPos{0.0f, 0.0f, 0.0f};
LodStats g_stats{};
LodStats g_lastStats{};
}

void Lod_BeginFrame(const DirectX::XMFLOAT3& cameraPosition) {
    g_stats = {};
    g_cameraPos = cameraPosition;
    g_active = true;
}

void Lod_EndFrame() {
    if (!g_active)
        return;
    g_lastStats = g_stats;
    g_active = false;
}

bool Lod_IsFrameActive() { return g_active; }
const DirectX::XMFLOAT3& Lod_GetCameraPosition() { return g_cameraPos; }

void Lod_RecordConsidered() { ++g_stats.considered; }
void Lod_RecordCulledByDistance() { ++g_stats.culledByDistance; }
void Lod_RecordDrawn(int level) {
    if (level < 0)
        level = 0;
    if (level >= kLodMaxLevels)
        level = kLodMaxLevels - 1;
    ++g_stats.drawn[level];
}

LodStats Lod_GetLastStats() { return g_lastStats; }
