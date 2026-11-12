// sprite_cull.h
// Per-frame 2D orthographic view-rect sprite cull state and counters.

#pragma once

#include "engine/rendering/culling/sprite_cull_math.h"

struct SpriteCullStats {
    int considered = 0;
    int culled = 0;
    int drawn = 0;
};

// Call once around DrawSceneSprites. Builds the world-XY view rect from an
// orthographic camera; non-ortho cameras leave the pass active but not ready
// (sprites draw unculled, counters stay 0).
void SpriteCull_BeginPass(
    float camX,
    float camY,
    float orthoHeight,
    float aspect,
    bool orthographic);
void SpriteCull_EndPass();

bool SpriteCull_IsPassActive();
// True when the pass is active and the camera is orthographic (cull may run).
bool SpriteCull_IsCullReady();

const sprite_cull::ViewRect& SpriteCull_GetViewRect();
float SpriteCull_GetMargin();

void SpriteCull_RecordConsidered();
void SpriteCull_RecordCulled();
void SpriteCull_RecordDrawn();

SpriteCullStats SpriteCull_GetStats();
// Completed pass counters (stable for overlays / bridge).
SpriteCullStats SpriteCull_GetLastStats();
