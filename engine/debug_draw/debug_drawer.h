/**===========================================================================

 File   :  [debug_drawer.h]

 Author : Hayato Otsuki
 Date   : 2025/10/24
 -----------------------------------------------------------------------------

 ===========================================================================**/

#ifndef DEBUG_DRAWER_H
#define DEBUG_DRAWER_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/shader.h"
#include "camera.h"

class DebugDrawer {
public:
    // Request queued for deferred rendering during the Draw phase.
    struct SphereDrawRequest {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 center;
        float radius;
        DirectX::XMFLOAT3 rotation;
        DirectX::XMFLOAT4 color;
    };

    DebugDrawer() = default;
    ~DebugDrawer();

    // Initialize debug resources (vertex/index buffers, texture)
    bool Initialize();

    // Release debug resources
    void Release();

    // Draw a wireframe box at the specified position, center, and size.
    // color defaults to green {0,1,0,1}; pass a different value for custom colors (e.g. yellow for selection).
    void DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT3& rotation,
                 const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

	// Draw a wireframe sphere at the specified position, center, and radius.
    // When enableDepth is false the sphere is drawn without depth testing so it
    // always appears on top of scene geometry (useful for the deferred queue).
    void DrawSphere(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT3& rotation,
                    const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f },
                    bool enableDepth = true);

    // --- Deferred sphere-wire queue (drawn during the Draw phase) ---
    // Queue a sphere draw request; call FlushSphereQueue() from the Draw phase.
    static void QueueSphere(const DirectX::XMFLOAT3& position,
                            const DirectX::XMFLOAT3& center,
                            float radius,
                            const DirectX::XMFLOAT3& rotation,
                            const DirectX::XMFLOAT4& color);
    // Execute all queued sphere draws without depth testing and clear the queue.
    static void FlushSphereQueue();

    // Draw a wireframe capsule at the specified position, center, radius and height
    void DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, float height, const DirectX::XMFLOAT3& rotation,
                     const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

    // Draw a wireframe cylinder (flat disc/slab) aligned to local Y after rotation
    void DrawCylinder(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& center, float radius, float height, const DirectX::XMFLOAT3& rotation,
                      const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

    // Draw a line between two points
    void DrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
                  const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

    // Draw many line segments in a single batched GPU draw call.
    // lineVerts must contain an even number of world-space positions;
    // each consecutive pair (lineVerts[i], lineVerts[i+1]) defines one segment.
    void DrawLines(const std::vector<DirectX::XMFLOAT3>& lineVerts,
                   const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

    // Like DrawLines but reuses a persistent GPU buffer across frames.
    // The buffer is only reallocated when lineVerts grows beyond the previous
    // capacity; otherwise data is uploaded via Map/Unmap (zero CreateBuffer calls).
    // Prefer this over DrawLines for geometry that is drawn every frame (e.g.
    // mesh collider wireframes and selection highlights).
    void DrawLinesCached(const std::vector<DirectX::XMFLOAT3>& lineVerts,
                         const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

private:
    static VERTEX_3D m_boxVerts[8];
    static UINT m_boxIndices[24];
    static ID3D11Buffer* m_boxVB;
    static ID3D11Buffer* m_boxIB;

    static VERTEX_3D m_sphereVerts[60];
    static UINT m_sphereIndices[120];
    static ID3D11Buffer* m_sphereVB;
    static ID3D11Buffer* m_sphereIB;

    static VERTEX_3D m_capsuleVerts[8]; // 4 lines * 2 vertices
    static UINT m_capsuleIndices[8]; // 4 lines * 2 indices
    static ID3D11Buffer* m_capsuleVB;
    static ID3D11Buffer* m_capsuleIB;

    static ID3D11ShaderResourceView* m_debugTexture;
    static bool m_initialized;

    // Depth stencil state that disables depth testing (used for the deferred queue).
    static ID3D11DepthStencilState* m_depthNoTest;

    // Queue of sphere draw requests accumulated during Update; flushed in Draw.
    static std::vector<SphereDrawRequest> s_sphereQueue;

    // Persistent dynamic VB used by DrawLinesCached.
    // Grown (reallocated) only when capacity is exceeded; otherwise updated
    // each frame via Map/Unmap to avoid per-frame CreateBuffer overhead.
    static ID3D11Buffer* m_linesVB;
    static UINT          m_linesVBCapacity;  // capacity in VERTEX_3D elements
};

#endif // DEBUG_DRAWER_H