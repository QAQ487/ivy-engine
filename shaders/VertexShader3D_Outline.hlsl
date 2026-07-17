#include "ShaderCommon.hlsli"

void main(
    in float3 inPosition : POSITION0,
    in float3 inNormal   : NORMAL0,
    in float4 inDiffuse  : COLOR0,
    in float2 inTexCoord : TEXCOORD0,

    out float4 outPosition : SV_POSITION)
{
    float4 posCS = mul(float4(inPosition, 1.0f), WorldViewProjection);
    // Clip-space normal approximation — screen-consistent outline width
    float4 nCS = mul(float4(inNormal, 0.0f), WorldViewProjection);
    float2 nXY = nCS.xy;
    float nLen = length(nXY);
    if (nLen > 1e-6f)
        nXY /= nLen;
    posCS.xy += nXY * OutlineWidth * posCS.w;
    outPosition = posCS;
}
