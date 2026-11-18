// fx_cull.h
// Per-frame counters for billboard, particle and Effekseer culling.

#pragma once

struct FxCullStats {
    int billboardConsidered = 0;
    int billboardCulled = 0;
    int particleConsidered = 0;   // individual particles
    int particleCulled = 0;
    int effectConsidered = 0;     // live Effekseer handles
    int effectCulled = 0;
};

// Call once per frame before the billboard pass; the previous frame's totals
// become "last" (billboards, particles and effects are drawn at different points).
void FxCull_BeginFrame();

void FxCull_RecordBillboards(int considered, int culled);
void FxCull_RecordParticles(int considered, int culled);
void FxCull_RecordEffects(int considered, int culled);

FxCullStats FxCull_GetLastStats();
