// VertexShader2D.hlsl
// 2D sprite vertex shader.

cbuffer VS_CONSTANT_BUFFER0 : register(b0) {
    float4x4 projection; // Combined View * Projection matrix; world transform applied separately via VS_CONSTANT_BUFFER1
};

cbuffer VS_CONSTANT_BUFFER1 : register(b1) {
    float4x4 world;
};

struct VS_IN {
    float3 posL     : POSITION0;
    float4 color    : COLOR0;
    float2 texcoord : TEXCOORD0;
};

struct VS_OUT {
    float4 posH     : SV_POSITION;
    float4 color    : COLOR0;
    float2 texcoord : TEXCOORD0;
};

VS_OUT main(VS_IN vsin)
{
    VS_OUT vsout;

    float4x4 mtx = mul(world, projection);
    vsout.posH = mul(float4(vsin.posL, 1.0f), mtx);

    vsout.color = vsin.color;
    vsout.texcoord = vsin.texcoord;
    return vsout;
}
