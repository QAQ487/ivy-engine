cbuffer CB_Skybox : register(b0) {
    float4x4 g_viewRot;
    float4x4 g_proj;
    float4x4 g_rotationY;
};

struct VS_IN {
    float3 position : POSITION;
};

struct VS_OUT {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};

VS_OUT main(VS_IN vsin) {
    VS_OUT vsout;
    float4 dir = mul(float4(vsin.position, 0.0f), g_rotationY);
    float4 viewPos = mul(float4(dir.xyz, 1.0f), g_viewRot);
    float4 clipPos = mul(viewPos, g_proj);
    vsout.position = clipPos.xyww;
    vsout.direction = dir.xyz;
    return vsout;
}
