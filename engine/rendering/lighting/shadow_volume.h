/**===========================================================================

 File   :  [shadow_volume.h]
 -----------------------------------------------------------------------------
 Stencil shadow volumes (settings.render.shadowMode = "volume").

 Frame flow (src/app/scene/game_scene.cpp, after the opaque camera pass):
   BeginPass(light)  -> stencil-only state, VertexShaderShadowVolume, depth clip off
   GameObject::DrawVisual for every root; Visuals with shadowVolume = true call
     Model3D::DrawShadowVolume -> DrawVolume (z-fail count: back faces +1, front -1
     where the depth test fails)
   EndPass(strength) -> fullscreen multiply where stencil != 0, previous state restored

 One directional light (the first DirectionalLight with castShadows). Volume topology
 comes from shadow_volume_mesh.h (closed meshes only).

 ===========================================================================**/
#ifndef SHADOW_VOLUME_H
#define SHADOW_VOLUME_H

#include <d3d11.h>
#include <DirectXMath.h>

namespace ShadowVolume {

    struct Stats {
        int casters = 0;        // volume draws submitted
        int openMeshes = 0;     // shadowVolume Visuals skipped: mesh not closed
        int triangles = 0;      // volume triangles submitted (caps + edge quads)
    };

    // Loads rom/shader/VertexShaderShadowVolume.cso, FullscreenVS.cso and
    // PixelShaderShadowVolumeDarken.cso and creates the states. Returns false (and the
    // mode draws nothing) when a shader is missing.
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void Finalize();
    bool IsReady();

    // shineDirWS: direction the light travels (need not be normalised).
    // extrusionLength: world units the far side is pushed along the light
    // (shadow_volume_policy::ExtrusionLength(camera far clip)).
    bool BeginPass(const DirectX::XMFLOAT3& shineDirWS, float extrusionLength);
    bool IsPassActive();

    // Called by Model3D::DrawShadowVolume after it bound the volume VB/IB, set the world
    // matrix (Shader_SetWorldMatrix) and the skin palette.
    void DrawVolume(const DirectX::XMMATRIX& world, UINT indexCount);
    void RecordOpenMesh();

    // Darkens stencil != 0 pixels by strength (0..1) and restores the saved state.
    void EndPass(float strength);

    Stats GetLastStats();
}

#endif // SHADOW_VOLUME_H
