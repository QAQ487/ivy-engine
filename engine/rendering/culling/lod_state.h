// lod_state.h
// Per-frame mesh LOD state: camera position for distance selection and
// main-pass counters. Shadow passes reuse the main camera position so shadow
// casters pick the same level as the visible mesh.

#pragma once

#include <DirectXMath.h>
#include <cstdint>

constexpr int kLodMaxLevels = 8;

struct LodStats {
    int considered = 0;        // objects with LOD levels or a cull distance
    int culledByDistance = 0;  // skipped because of visual.cullDistance
    int drawn[kLodMaxLevels] = {};  // draws per level (0 = base mesh)
};

// Call at the start of the 3D object pass, before shadow cascade passes.
// verticalFovDeg feeds screen-size LOD (tan of half the vertical field of view); an
// invalid value (<= 0 or >= 180) disables screen-size conversion for the frame.
void Lod_BeginFrame(const DirectX::XMFLOAT3& cameraPosition, float verticalFovDeg = 0.0f);
void Lod_EndFrame();

// True between Lod_BeginFrame and Lod_EndFrame.
bool Lod_IsFrameActive();
const DirectX::XMFLOAT3& Lod_GetCameraPosition();
// tan(verticalFov / 2) for this frame; 0 when unknown.
float Lod_GetTanHalfFov();

// Counters are recorded by the main camera pass only (see game_object.cpp).
void Lod_RecordConsidered();
void Lod_RecordCulledByDistance();
void Lod_RecordDrawn(int level);

LodStats Lod_GetLastStats();

// Frame serial: incremented by every Lod_BeginFrame. Visuals stamp it when the 3D
// object pass considers / draws them so the next animation update can tell whether
// they were visible (see anim_cull_math.h). Starts at 0.
uint64_t Lod_GetFrameSerial();

// Off-screen animation counters. Recorded by VisualComponent::Update (which runs
// before the next draw), so the "last" totals are the previous update pass.
struct AnimCullStats {
    int updated = 0;  // animated Visuals whose pose was evaluated
    int skipped = 0;  // animated Visuals whose pose was skipped (not drawn)
};
void Lod_RecordAnimUpdated();
void Lod_RecordAnimSkipped();
AnimCullStats Lod_GetLastAnimStats();
