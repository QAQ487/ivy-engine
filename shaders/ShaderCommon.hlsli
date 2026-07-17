//*****************************************************************************
// 定数バッファ（VS・PS 共有）
//*****************************************************************************
#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

#define MAX_LIGHTS 4

cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProjection;
}

cbuffer MaterialBuffer : register(b1)
{
    float4 Ambient;
    float4 Diffuse; // Diffuse.a はテクスチャ切り替えフラグ（1=使う, 0=使わない）として使用
    float4 Specular;
    float4 Emission;
    float Shininess;
    float Roughness;
    float Metallic;
    float NormalStrength; // 0 = no normal map
    // Toon / outline params (ignored by non-toon shaders)
    float ShadeSteps;
    float ShadeSmoothness;
    float SpecularThreshold;
    float OutlineWidth;
    float4 OutlineColor;
    float4 ShadowTint;
    float UseRamp;      // >0 = sample ramp texture at t6
    float RimStrength;
    float2 _padToon;
}

#endif // SHADER_COMMON_HLSLI
