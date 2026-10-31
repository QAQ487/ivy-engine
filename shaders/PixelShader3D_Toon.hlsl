// PixelShader3D_Toon.hlsl
// Toon shading pixel shader.

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

    float3 lightRGB = 0.0f;
    float3 sumCelSpec = 0.0f;
    float overallShadow = 1.0f;

    uint loopCount = GetLightLoopCount();
    for (uint i = 0; i < loopCount; ++i)
    {
        float shadowFactor = EvalShadow(i, inPosOS, N);
        overallShadow = min(overallShadow, shadowFactor);

        const float type = Lights[i].Type;
        float3 diff = 0.0f;

        if (type < 0.5f)
            diff = EvalDirectionalDiffuse(Lights[i], N);
        else if (type < 1.5f)
            diff = EvalSpotDiffuse(Lights[i], N, inPosOS);
        else
            diff = EvalPointDiffuse(Lights[i], N, inPosOS);

        lightRGB += diff * shadowFactor;
        sumCelSpec += EvalCelSpecular(Lights[i], N, V, type, inPosOS) * shadowFactor;
    }

    float litFactor = saturate(dot(lightRGB, float3(0.3333f, 0.3333f, 0.3333f)));
    float band = QuantizeToon(litFactor, max(1.0f, ShadeSteps), max(0.0f, ShadeSmoothness));

    float3 shadedAlbedo;
    if (UseRamp > 0.0f)
    {
        float3 ramp = g_RampTexture.Sample(g_SamplerState, float2(band, 0.5f)).rgb;
        shadedAlbedo = base.rgb * ramp;
    }
    else
    {
        shadedAlbedo = lerp(base.rgb * ShadowTint.rgb, base.rgb, band);
    }

    float3 lit = 0.0f;
    if (litFactor > 1e-4f)
        lit = shadedAlbedo * (lightRGB / litFactor) * band;

    float rim = EvalLightFacingRim(N, V, inPosOS, RimStrength, 3.0f);
    float3 color = ambient + lit + sumCelSpec + Emission.rgb + base.rgb * Specular.rgb * rim;

    const float overlayStrength = 0.65f;
    float overlay = lerp(1.0f, overallShadow, overlayStrength);
    color *= overlay;

    color = ApplyHeightFog(color, inPosOS);
    outDiffuse = float4(color, base.alpha);
}
