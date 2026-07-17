/**===========================================================================
 File   : LockOnUI.h
 Brief  : Billboard lock-on reticle at the magnet target.
============================================================================**/
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

class GameObject;

enum class LockOnTargetState {
    None,
    Available,
    OnCooldown,
    TooFar
};

class LockOnUI {
public:
    LockOnUI() = delete;
    ~LockOnUI() = delete;

    static bool Initialize();
    static void Finalize();
    static void Update(double elapsed_time);
    static void Draw();

    static void SetTarget(GameObject* target, LockOnTargetState state = LockOnTargetState::None);
    static GameObject* GetTarget() { return s_target; }
    static bool IsActive() { return s_target != nullptr && s_is_active; }

    static void TriggerLockOnEffect();
    static void TriggerReleaseEffect();

private:
    static void ResetIdleState();

    static GameObject* s_target;
    static LockOnTargetState s_target_state;
    static bool s_is_active;

    static float s_animation_timer;
    static float s_rotation_angle;
    static float s_pulse_scale;
    static float s_lock_on_progress;
    static bool s_is_locking_on;
    static bool s_is_releasing;

    static float s_outer_ring_rotation;
    static float s_outer_ring_scale;
    static float s_inner_ring_rotation;
    static float s_inner_ring_scale;
    static float s_corner_offset;
    static float s_corner_scale;
    static float s_pulse_timer;
    static float s_pulse_alpha;
    static float s_diamond_rotation;
    static float s_diamond_scale;

    static ID3D11VertexShader* s_ptr_vs;
    static ID3D11PixelShader* s_ptr_ps;
    static ID3D11InputLayout* s_ptr_il;
    static ID3D11Buffer* s_ptr_vertex_buffer;
    static ID3D11Buffer* s_ptr_index_buffer;
    static ID3D11Buffer* s_ptr_cb_transform;
    static ID3D11Buffer* s_ptr_cb_color;
    static ID3D11BlendState* s_ptr_blend_state;
    static ID3D11DepthStencilState* s_ptr_depth_state;
    static ID3D11RasterizerState* s_ptr_raster_state;

    static ID3D11Device* s_ptr_device;
    static ID3D11DeviceContext* s_ptr_context;

    static DirectX::XMFLOAT3 s_last_target_pos;

    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };

    struct CB_Transform {
        DirectX::XMFLOAT4X4 world_view_proj;
    };

    struct CB_Color {
        DirectX::XMFLOAT4 color;
        float time;
        float padding[3];
    };

    static void DrawOuterRing(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj,
                              const DirectX::XMFLOAT3& position, float base_size);
    static void DrawInnerRing(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj,
                              const DirectX::XMFLOAT3& position, float base_size);
    static void DrawCornerMarkers(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj,
                                  const DirectX::XMFLOAT3& position, float base_size);
    static void DrawDiamond(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj,
                            const DirectX::XMFLOAT3& position, float base_size);
    static void DrawCrosshair(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj,
                              const DirectX::XMFLOAT3& position, float base_size);

    static void DrawQuad(const DirectX::XMMATRIX& world_view_proj, const DirectX::XMFLOAT4& color);

    static float EaseOutBack(float t);
    static float EaseOutElastic(float t);

    static DirectX::XMFLOAT4 PrimaryColor();
    static DirectX::XMFLOAT4 GlowColor();
};
