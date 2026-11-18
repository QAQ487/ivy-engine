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

// Debug labels (render.lodDebugLabels): one entry per LOD-tracked Visual the main camera
// pass drew, plus one (culled = true) per Visual skipped by its cull distance, recorded by GameObject::DrawVisual. The list is reset by Lod_BeginFrame and
// stays valid after Lod_EndFrame so the overlay can draw it later in the frame.
struct LodDebugLabel {
    DirectX::XMFLOAT3 worldPosition{0.0f, 0.0f, 0.0f};  // above the object's world AABB
    int level = 0;
    float distance = 0.0f;       // camera distance used for the level choice
    float screenPercent = -1.0f; // projected height in % of the viewport, -1 = unknown
    // True when the Visual is NOT drawn because it is beyond its cull distance. `distance`
    // is then the camera distance and `cullDistance` the (scaled) limit it exceeded.
    bool culled = false;
    float cullDistance = 0.0f;
};
constexpr int kLodMaxDebugLabels = 256;
void Lod_AddDebugLabel(const LodDebugLabel& label);
const LodDebugLabel* Lod_GetDebugLabels(int& count);

// Debug options that live outside RenderSettings so they are never saved into a scene and
// survive scene reloads. forceLevel: -1 = automatic, otherwise every LOD-tracked mesh uses
// that level (limited to what it has). showRanges: draw the selected object's LOD switch
// distances as wire spheres in the Edit view.
void Lod_SetForceLevel(int level);
int Lod_GetForceLevel();
void Lod_SetShowRanges(bool show);
bool Lod_GetShowRanges();

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
