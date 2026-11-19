// PixelShaderShadowVolumeDarken.hlsl
// Fullscreen pass after the stencil count: pixels with a non-zero stencil are inside a
// shadow volume. The blend state multiplies the scene colour by this output
// (dest * src), so DarkenColor.rgb = 1 - strength.

cbuffer ShadowVolumeDarken : register(b8)
{
    float4 DarkenColor;
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return DarkenColor;
}
