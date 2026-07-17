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

    float3 ambient = AmbientColor.rgb * base.rgb * 0.25f;

    float3 sumLighting = 0.0f;
    uint loopCount = GetLightLoopCount();

    [loop]
    for (uint i = 0; i < loopCount; ++i)
    {
        if (Lights[i].Type >= 1.5f)
        {
            float3 lit = EvalPointLight(Lights[i], N, V, inPosOS, base.rgb, true);
            sumLighting += lit;
        }
    }

    float3 color = ambient + sumLighting + Emission.rgb;
    color = ApplyHeightFog(color, inPosOS);
    outDiffuse = float4(color, base.alpha);
}
