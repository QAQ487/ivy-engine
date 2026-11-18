// fx_cull.cpp
// Per-frame billboard / particle / effect cull counters.

#include "fx_cull.h"

namespace {
FxCullStats g_stats{};
FxCullStats g_lastStats{};
}

void FxCull_BeginFrame() {
    g_lastStats = g_stats;
    g_stats = {};
}

void FxCull_RecordBillboards(int considered, int culled) {
    g_stats.billboardConsidered += considered;
    g_stats.billboardCulled += culled;
}

void FxCull_RecordParticles(int considered, int culled) {
    g_stats.particleConsidered += considered;
    g_stats.particleCulled += culled;
}

void FxCull_RecordEffects(int considered, int culled) {
    g_stats.effectConsidered += considered;
    g_stats.effectCulled += culled;
}

FxCullStats FxCull_GetLastStats() { return g_lastStats; }
