// PixelShader2D.hlsl
// 2D sprite pixel shader.

cbuffer CB_Color : register(b0) {
    float4 g_Color;
};

Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN {
    float4 posH     : SV_POSITION;
    float4 color    : COLOR0;
    float2 texcoord : TEXCOORD0;
};

float4 main(PS_IN psin) : SV_TARGET
{
    float4 texcolor = g_Texture.Sample(g_Sampler, psin.texcoord);
    return psin.color * texcolor * g_Color;
}
