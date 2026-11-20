// terrain_editor.h
// Editor sculpting tools for the Terrain component (ADR 0011, step 4): brush settings
// in the Terrain inspector section, left-drag strokes in the 3D viewport, a brush
// circle on the surface, one undo step per stroke, and writing edited heightmaps when
// the scene is saved. Editor build only.

#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>

class GameObject;
class TerrainComponent;

struct TerrainViewportInput {
    bool enabled = false;        // edit mode, 3D scene view, not playing, no scene load
    bool hasRay = false;         // mouse inside the game viewport
    DirectX::XMFLOAT3 rayOrigin{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 rayDir{0.0f, 0.0f, 1.0f};
    bool mouseDown = false;      // left button held
    bool mouseBlocked = false;   // ImGui window / gizmo / Alt camera owns the mouse
    float dt = 1.0f / 60.0f;     // seconds since the last call
};

// Sculpt tools for the selected terrain (drawn inside its inspector section).
void TerrainEditor_DrawInspector(GameObject* node, TerrainComponent& terrain);

// Called every editor Update. Draws the brush circle and applies strokes. Returns true
// while the brush owns the mouse (the caller then skips click-to-select).
bool TerrainEditor_HandleViewport(GameObject* selected, const TerrainViewportInput& input);

// True while a sculpt tool is active on `node` (the transform gizmo is hidden then).
bool TerrainEditor_IsSculpting(const GameObject* node);

// Writes every terrain in the open scene with unsaved sculpt edits. Terrains without a
// heightmap file get rom/terrain/<scene>_<object>.r16. Called before the scene JSON is
// written; false (with a message) stops the save.
bool TerrainEditor_SaveEditedTerrains(std::string* error);

// True when the file at the rom/ path holds exactly the bytes the editor last saved
// there (so the content monitor does not reload the scene for the editor's own write).
bool TerrainEditor_IsOwnWrite(const std::string& romPath);

// GameObject -> 3D Object -> Terrain: 256 x 256 m flat terrain centred on the origin,
// heights -20 .. 80 m (ground at 0, so it can be dug down and raised up).
std::unique_ptr<GameObject> TerrainEditor_CreateTerrainObject();
