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

    float3 ambient = EvalHemisphereAmbient(N, base.rgb);

    float3 sumLighting = 0.0f;
    float overallShadow = 1.0f;

    uint loopCount = GetLightLoopCount();

    for (uint i = 0; i < loopCount; ++i)
    {
        float shadowFactor = EvalShadow(i, inPosOS, N);
        overallShadow = min(overallShadow, shadowFactor);

        const float type = Lights[i].Type;
        float3 lit = 0.0f;

        if (type < 0.5f)
        {
            lit = EvalDirectionalLight(Lights[i], N, V, base.rgb, true);
        }
        else if (type < 1.5f)
        {
            lit = EvalSpotLight(Lights[i], N, V, inPosOS, base.rgb, true);
        }
        else
        {
            lit = EvalPointLight(Lights[i], N, V, inPosOS, base.rgb, true);
        }

        sumLighting += lit * shadowFactor;
    }

    float3 color = ambient + sumLighting + Emission.rgb;

    const float overlayStrength = 0.65f;
    float overlay = lerp(1.0f, overallShadow, overlayStrength);
    color *= overlay;

    color = ApplyHeightFog(color, inPosOS);
    outDiffuse = float4(color, base.alpha);
}
