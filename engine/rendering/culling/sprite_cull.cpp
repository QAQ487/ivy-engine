// sprite_cull.cpp
// Per-frame 2D view-rect sprite cull counters.

#include "sprite_cull.h"

namespace {
bool g_passActive = false;
bool g_cullReady = false;
float g_margin = 0.0f;
sprite_cull::ViewRect g_view{};
SpriteCullStats g_stats{};
SpriteCullStats g_lastStats{};
}

void SpriteCull_BeginPass(
    float camX,
    float camY,
    float orthoHeight,
    float aspect,
    bool orthographic) {
    g_stats = {};
    g_passActive = true;
    g_cullReady = orthographic;
    g_margin = 0.0f;
    g_view = {};
    if (!g_cullReady)
        return;

    g_view = sprite_cull::MakeOrthoViewRect(camX, camY, orthoHeight, aspect);
    g_margin = sprite_cull::DefaultViewMargin(orthoHeight);
    sprite_cull::InflateViewRect(g_view, g_margin);
}

void SpriteCull_EndPass() {
    if (!g_passActive)
        return;
    g_lastStats = g_stats;
    g_passActive = false;
    g_cullReady = false;
}

bool SpriteCull_IsPassActive() {
    return g_passActive;
}

bool SpriteCull_IsCullReady() {
    return g_passActive && g_cullReady;
}

const sprite_cull::ViewRect& SpriteCull_GetViewRect() {
    return g_view;
}

float SpriteCull_GetMargin() {
    return g_margin;
}

void SpriteCull_RecordConsidered() { ++g_stats.considered; }
void SpriteCull_RecordCulled() { ++g_stats.culled; }
void SpriteCull_RecordDrawn() { ++g_stats.drawn; }

SpriteCullStats SpriteCull_GetStats() {
    return g_stats;
}

SpriteCullStats SpriteCull_GetLastStats() {
    return g_lastStats;
}
