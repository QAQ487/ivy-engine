#include "ShaderCommon.hlsli"

void main(in float3 inPosition : POSITION0,
          in float3 inNormal   : NORMAL0,
          in float4 inDiffuse  : COLOR0,
          in float2 inTexCoord : TEXCOORD0,
          in uint4  inBlendIndices : BLENDINDICES0,
          in float4 inBlendWeights : BLENDWEIGHT0,

          out float4 outPosition : SV_POSITION,
          out float3 outNormal   : NORMAL0,
          out float2 outTexCoord : TEXCOORD0,
          out float4 outDiffuse  : COLOR0,
          out float3 outPosOS    : TEXCOORD2)
{
    const float3 skinned = SkinPositionOS(inPosition, inBlendIndices, inBlendWeights);
    outPosition = mul(float4(skinned, 1.0f), WorldViewProjection);
    outNormal   = inNormal;
    outTexCoord = inTexCoord;
    outDiffuse  = inDiffuse;
    outPosOS    = skinned;
}
