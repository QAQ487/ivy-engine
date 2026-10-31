// SkyboxPS.hlsl
// Skybox pixel shader.

TextureCube g_skybox : register(t0);
SamplerState g_sampler : register(s0);

cbuffer CB_SkyboxParams : register(b0) {
    float4 g_tint;
    float g_exposure;
    float3 g_padding;
};

struct PS_IN {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};

float4 main(PS_IN psin) : SV_TARGET {
    float3 dir = normalize(psin.direction);
    float3 color = g_skybox.Sample(g_sampler, dir).rgb * g_tint.rgb * g_exposure;
    return float4(color, 1.0f);
}
