//*****************************************************************************
// 定数バッファ（VS・PS 共有）
//*****************************************************************************
#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

#define MAX_LIGHTS 4
#define SHADER_MAX_BONES 128

cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProjection;
}

cbuffer SkinBuffer : register(b5)
{
    float4x4 SkinBones[SHADER_MAX_BONES];
    int SkinEnabled;
    int SkinBoneCount;
    float2 SkinPad;
}

float3 SkinPositionOS(float3 p, uint4 indices, float4 weights)
{
    if (SkinEnabled == 0)
        return p;

    const float4 hp = float4(p, 1.0f);
    float4 acc = 0.0f;
    float wsum = 0.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        const float w = weights[i];
        if (w <= 0.0f)
            continue;
        const uint idx = indices[i];
        if (idx >= (uint)SkinBoneCount)
            continue;
        acc += w * mul(hp, SkinBones[idx]);
        wsum += w;
    }
    acc += (1.0f - wsum) * hp;
    return acc.xyz;
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
