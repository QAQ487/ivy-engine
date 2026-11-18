// lod_state.cpp
// Per-frame mesh LOD state and counters.

#include "lod_state.h"

#include <cmath>

namespace {
bool g_active = false;
DirectX::XMFLOAT3 g_cameraPos{0.0f, 0.0f, 0.0f};
float g_tanHalfFov = 0.0f;
LodStats g_stats{};
LodStats g_lastStats{};
uint64_t g_serial = 0;
AnimCullStats g_animStats{};
AnimCullStats g_lastAnimStats{};
}

void Lod_BeginFrame(const DirectX::XMFLOAT3& cameraPosition, float verticalFovDeg) {
    g_stats = {};
    ++g_serial;
    g_lastAnimStats = g_animStats;
    g_animStats = {};
    g_cameraPos = cameraPosition;
    g_tanHalfFov = 0.0f;
    if (verticalFovDeg > 0.0f && verticalFovDeg < 179.0f)
        g_tanHalfFov = std::tan(DirectX::XMConvertToRadians(verticalFovDeg) * 0.5f);
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
float Lod_GetTanHalfFov() { return g_tanHalfFov; }

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

uint64_t Lod_GetFrameSerial() { return g_serial; }
void Lod_RecordAnimUpdated() { ++g_animStats.updated; }
void Lod_RecordAnimSkipped() { ++g_animStats.skipped; }
AnimCullStats Lod_GetLastAnimStats() { return g_lastAnimStats; }
