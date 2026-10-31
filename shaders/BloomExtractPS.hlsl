// BloomExtractPS.hlsl
// Bloom bright-pass extract pixel shader.

cbuffer BloomBuffer : register(b10)
{
	float Bloom_Threshold;
	float Bloom_Intensity;
	float Bloom_Spread;
	float Bloom_Pad0;
	float2 Bloom_UVOffset;
	float2 Bloom_UVScale;
	float2 Bloom_TexelSize;
	float2 Bloom_Direction;
};

Texture2D sceneTex : register(t0);
SamplerState sampLinearClamp : register(s0);

struct PS_IN
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0; // local 0..1 across viewport
};

float4 main(PS_IN input) : SV_TARGET
{
	float2 texUV = Bloom_UVOffset + input.uv * Bloom_UVScale;
	float4 color = sceneTex.Sample(sampLinearClamp, texUV);

	// Perceptual luminance calculation
	float lum = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));

	// Soft knee thresholding
	float threshold = Bloom_Threshold;
	float knee = max(threshold * 0.5, 1e-4);
	float soft = lum - threshold + knee;
	soft = clamp(soft, 0.0, 2.0 * knee);
	soft = (soft * soft) / (4.0 * knee + 1e-5);
	float weight = max(soft, lum - threshold) / max(lum, 1e-4);
	weight = saturate(weight);

	float3 bright = color.rgb * weight;
	return float4(bright, 1.0);
}
