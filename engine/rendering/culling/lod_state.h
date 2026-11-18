// lod_state.h
// Per-frame mesh LOD state: camera position for distance selection and
// main-pass counters. Shadow passes reuse the main camera position so shadow
// casters pick the same level as the visible mesh.

#pragma once

#include <DirectXMath.h>

constexpr int kLodMaxLevels = 8;

struct LodStats {
    int considered = 0;        // objects with LOD levels or a cull distance
    int culledByDistance = 0;  // skipped because of visual.cullDistance
    int drawn[kLodMaxLevels] = {};  // draws per level (0 = base mesh)
};

// Call at the start of the 3D object pass, before shadow cascade passes.
void Lod_BeginFrame(const DirectX::XMFLOAT3& cameraPosition);
void Lod_EndFrame();

// True between Lod_BeginFrame and Lod_EndFrame.
bool Lod_IsFrameActive();
const DirectX::XMFLOAT3& Lod_GetCameraPosition();

// Counters are recorded by the main camera pass only (see game_object.cpp).
void Lod_RecordConsidered();
void Lod_RecordCulledByDistance();
void Lod_RecordDrawn(int level);

LodStats Lod_GetLastStats();
