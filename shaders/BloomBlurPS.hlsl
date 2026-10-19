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

Texture2D inputTex : register(t0);
SamplerState sampLinearClamp : register(s0);

struct PS_IN
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0; // 0..1 across render target
};

// 9-tap separable Gaussian blur kernel with spread support
static const float weights[5] = { 0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162 };

float4 main(PS_IN input) : SV_TARGET
{
	float2 step = Bloom_Direction * Bloom_TexelSize * Bloom_Spread;

	float3 result = inputTex.Sample(sampLinearClamp, input.uv).rgb * weights[0];

	[unroll]
	for (int i = 1; i < 5; ++i)
	{
		float2 offset = step * (float)i;
		result += inputTex.Sample(sampLinearClamp, input.uv + offset).rgb * weights[i];
		result += inputTex.Sample(sampLinearClamp, input.uv - offset).rgb * weights[i];
	}

	return float4(result, 1.0);
}
