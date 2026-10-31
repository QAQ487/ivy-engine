// PixelShader3D_Rim.hlsl
// Rim-light pixel shader.

#include "LightingCommon.hlsli"

void main(
    in float4 inPosition : SV_POSITION,
    in float3 inNormal   : NORMAL0,
    in float2 inTexCoord : TEXCOORD0,
    in float4 inDiffuse  : COLOR0,
    in float3 inPosOS    : TEXCOORD2,
    out float4 outDiffuse : SV_Target)
{
    BaseColorResult base = SampleBaseColor(inDiffuse, inTexCoord);
    float3 N = ApplyNormalMap(inNormal, inPosOS, inTexCoord);
    float3 V = normalize(ViewDirOS);

    float3 ambient = EvalHemisphereAmbient(N, base.rgb) * 0.25f;

    float rimPower = max(1.0f, Shininess);
    float3 rimColor = Specular.rgb;
    float rim = EvalLightFacingRim(N, V, inPosOS, 1.0f, rimPower);
    float3 color = ambient + base.rgb * rimColor * rim + Emission.rgb;

    color = ApplyHeightFog(color, inPosOS);
    outDiffuse = float4(color, base.alpha);
}
