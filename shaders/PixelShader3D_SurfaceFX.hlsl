// PixelShader3D_SurfaceFX.hlsl
// Surface FX pixel shader.

#include "LightingCommon.hlsli"

// Parametric dual-scroll surface (water / void / lava / energy).
// 1 texture, 2 samples. Straight alpha (matches engine blend).
// Gradient/edge params are 0-1 fractions of MeshRadius (object-space XZ).
cbuffer SurfaceFXBuffer : register(b4)
{
    float4 SFX_Color1;          // shallow / center
    float4 SFX_Color2;          // deep / edge
    float2 SFX_ScrollSpeed1;
    float2 SFX_ScrollSpeed2;
    float  SFX_Time;
    float  SFX_UvScale;
    float  SFX_FresnelPower;
    float  SFX_EmissionStrength;
    float  SFX_GradientStart;
    float  SFX_GradientEnd;
    float  SFX_EdgeFadeStart;
    float  SFX_EdgeFadeEnd;
    float  SFX_MeshRadius;
    float3 SFX_PadMesh;
    matrix SFX_World;
}

void main(
    in float4 inPosition : SV_POSITION,
    in float3 inNormal   : NORMAL0,
    in float2 inTexCoord : TEXCOORD0,
    in float4 inDiffuse  : COLOR0,
    in float3 inPosOS    : TEXCOORD2,
    out float4 outDiffuse : SV_Target)
{
    float3 worldPos = mul(float4(inPosOS, 1.0f), SFX_World).xyz;
    float2 uvBase = worldPos.xz * SFX_UvScale;

    float2 uv1 = uvBase + SFX_ScrollSpeed1 * SFX_Time;
    float2 uv2 = uvBase + SFX_ScrollSpeed2 * SFX_Time;

    float3 sample1 = g_Texture.Sample(g_SamplerState, uv1).rgb;
    float3 sample2 = g_Texture.Sample(g_SamplerState, uv2).rgb;
    float detail = saturate((sample1.r + sample2.g) * 0.5f);

    float radial = length(inPosOS.xz) / max(SFX_MeshRadius, 1e-4f);
    float deepT = saturate(
        (radial - SFX_GradientStart) / max(SFX_GradientEnd - SFX_GradientStart, 1e-4f));

    float3 baseCol = lerp(SFX_Color1.rgb, SFX_Color2.rgb, deepT);
    baseCol *= lerp(0.75f, 1.15f, detail);

    float3 N = normalize(inNormal);
    float3 V = normalize(ViewDirOS);
    float fresnel = pow(1.0f - saturate(dot(N, V)), max(1.0f, SFX_FresnelPower));

    float3 glow = Emission.rgb * SFX_EmissionStrength;
    float3 rim = SFX_Color1.rgb * fresnel * 0.55f;

    float3 color = baseCol + glow + rim;

    float edgeFade = 1.0f - saturate(
        (radial - SFX_EdgeFadeStart) / max(SFX_EdgeFadeEnd - SFX_EdgeFadeStart, 1e-4f));
    float alpha = saturate(lerp(SFX_Color1.a, SFX_Color2.a, deepT) * edgeFade);

    outDiffuse = float4(color, alpha);
}
