#include "ShaderCommon.hlsli"

void main(out float4 outDiffuse : SV_Target)
{
    outDiffuse = OutlineColor;
}
