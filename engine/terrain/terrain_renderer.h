// terrain_renderer.h
// Draws TerrainComponent heightmaps (ADR 0011): chunked grid mesh, heights read from an
// R16 texture in VertexShaderTerrain.hlsl, per-chunk frustum / cascade culling and LOD,
// one instanced draw per LOD level. Shading uses the scene's default lit pixel shader.

#pragma once

#include <DirectXMath.h>

class TerrainComponent;

// Draws one terrain in the current pass: the camera pass, or a CSM depth pass (skipped
// when settings.castShadows is false). Not in the stencil shadow-volume pass: terrain is
// an open surface, so it receives volume shadows but casts none. Restores the regular
// 3D shader state (Shader3D_Begin) afterwards.
void TerrainRenderer_Draw(TerrainComponent& terrain, const DirectX::XMFLOAT3& worldPosition);

// Releases the shared GPU objects (shader, chunk meshes, instance buffer). They are
// recreated on the next draw. Each component releases its own heightmap texture.
void TerrainRenderer_Finalize();
