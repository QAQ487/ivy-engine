/**===========================================================================

 File   : Billboard Vertex Shader [VertexShaderUnlit.hlsl]

 Author : Terry
 Date   : 2025/12/4
 -----------------------------------------------------------------------------

 ===========================================================================**/

cbuffer CB_ViewProj : register(b0) {
    float4x4 g_view;
    float4x4 g_proj;
};

cbuffer CB_Billboard : register(b1) {
    float3 g_position;
    float padding1;
    float2 g_size;
    float g_rotation;       // camera: in-plane rad; ground: yaw around Y (rad)
    float g_groundAligned;  // 0 = camera-facing, 1 = flat on XZ floor
    float4 g_color;
    float2 g_uvOffset;      // atlas: uv = offset + texcoord * scale
    float2 g_uvScale;       // full texture = (0,0) / (1,1)
};

struct VS_IN {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VS_OUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

VS_OUT main(VS_IN vsin) {
VS_OUT vsout;

    float2 localPos = vsin.position.xy;

    float3 worldPos;
    if (g_groundAligned > 0.5f) {
        // Ground decal: +X texture axis = aim forward; face +Y (visible from above)
        float3 forward = float3(sin(g_rotation), 0.0f, cos(g_rotation));
        float3 side = float3(cos(g_rotation), 0.0f, -sin(g_rotation));
        worldPos = g_position
                     + forward * localPos.x * g_size.x
                     - side * localPos.y * g_size.y;
    } else {
        // Camera-facing billboard with in-plane rotation
        float cosR = cos(g_rotation);
        float sinR = sin(g_rotation);
        localPos = float2(
            cosR * vsin.position.x - sinR * vsin.position.y,
            sinR * vsin.position.x + cosR * vsin.position.y
        );

        float3 right = float3(g_view[0][0], g_view[1][0], g_view[2][0]);
        float3 up = float3(g_view[0][1], g_view[1][1], g_view[2][1]);
        worldPos = g_position
                     + right * localPos.x * g_size.x
                     + up * localPos.y * g_size.y;
    }

float4 viewPos = mul(float4(worldPos, 1.0f), g_view);
    vsout.position = mul(viewPos, g_proj);
    float2 baseUv = (g_groundAligned > 0.5f)
        ? float2(vsin.texcoord.x, 1.0f - vsin.texcoord.y)
        : vsin.texcoord;
    vsout.texcoord = g_uvOffset + baseUv * g_uvScale;
    vsout.color = g_color;

    return vsout;
};