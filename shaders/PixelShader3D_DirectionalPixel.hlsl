// PixelShader3D_DirectionalPixel.hlsl
// Per-pixel directional light pixel shader.

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

    float3 ambient = AmbientColor.rgb * base.rgb * 0.15f;

    float3 sumLighting = 0.0f;
    float overallShadow = 1.0f;
    uint loopCount = GetLightLoopCount();

    for (uint i = 0; i < loopCount; ++i)
    {
        if (Lights[i].Type < 0.5f)
        {
            float shadowFactor = EvalShadow(i, inPosOS, N);
            overallShadow = min(overallShadow, shadowFactor);

            float3 lit = EvalDirectionalLight(Lights[i], N, V, base.rgb, true);
            sumLighting += lit * shadowFactor;
        }
    }

    ambient *= ShadowAmbientScale(overallShadow);

    float3 color = ambient + sumLighting + Emission.rgb;

    color = ApplyHeightFog(color, inPosOS);
    outDiffuse = float4(color, base.alpha);
}
