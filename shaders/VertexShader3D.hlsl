#include "ShaderCommon.hlsli"

//=============================================================================
// VS
//=============================================================================
void main(in float3 inPosition : POSITION0,
          in float3 inNormal   : NORMAL0,
          in float4 inDiffuse  : COLOR0,
          in float2 inTexCoord : TEXCOORD0,

          out float4 outPosition : SV_POSITION,
          out float3 outNormal   : NORMAL0,
          out float2 outTexCoord : TEXCOORD0,
          out float4 outDiffuse  : COLOR0,
          out float3 outPosOS    : TEXCOORD2)
{
    outPosition = mul(float4(inPosition, 1.0f), WorldViewProjection);
    outNormal   = inNormal;
    outTexCoord = inTexCoord;
    outDiffuse  = inDiffuse;
    outPosOS    = inPosition;
}
