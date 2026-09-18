cbuffer PostProcessBuffer : register(b10)
{
	float PP_RGBShift;     // shift amount in local (viewport) uv units
	float PP_MosaicBlocks; // blocks across viewport width
	float PP_Posterize;
	float PP_Noise;
	float PP_Grayscale;
	float PP_Contrast;
	float PP_Vignette;
	float PP_Scanline;
	float PP_Time;
	float PP_Flicker;
	float2 PP_UVOffset;    // PostRT region that holds the game viewport
	float2 PP_UVScale;
	float PP_Pad0;
	float PP_Pad1;
};

Texture2D sceneTex : register(t0);
SamplerState sampLinearClamp : register(s0);

struct PS_IN
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0; // local 0..1 across the current D3D viewport
};

float2 ToTexUV(float2 localUv)
{
	return PP_UVOffset + localUv * PP_UVScale;
}

float2 MosaicUV(float2 uv, float blocks)
{
	float2 result = uv;
	if (blocks > 1.0)
	{
		float2 cell = float2(blocks, blocks);
		result = (floor(uv * cell) + 0.5) / cell;
	}
	return result;
}

float4 SampleRGBShift(float2 localUv, float shift)
{
	float2 dir = localUv - 0.5;
	float2 offset = dir * shift;
	float r = sceneTex.Sample(sampLinearClamp, ToTexUV(localUv + offset)).r;
	float g = sceneTex.Sample(sampLinearClamp, ToTexUV(localUv)).g;
	float b = sceneTex.Sample(sampLinearClamp, ToTexUV(localUv - offset)).b;
	float a = sceneTex.Sample(sampLinearClamp, ToTexUV(localUv)).a;
	return float4(r, g, b, a);
}

float Hash12(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.x + p3.y) * p3.z);
}

float Hash11(float n)
{
	return frac(sin(n) * 43758.5453123);
}

float3 ApplyContrast(float3 c, float k)
{
	if (k <= 0.0)
		return c;
	return (c - 0.5) * k + 0.5;
}

float3 Posterize(float3 c, float steps)
{
	if (steps <= 0.0)
		return c;
	return floor(saturate(c) * steps) / steps;
}

float3 ApplyGrayscale(float3 c, float amount)
{
	if (amount <= 0.0)
		return c;
	float lum = dot(c, float3(0.299, 0.587, 0.114));
	return lerp(c, lum.xxx, amount);
}

float3 ApplyNoise(float3 c, float2 pixelPos, float amount, float time)
{
	if (amount <= 0.0)
		return c;
	float n = Hash12(pixelPos + time * 60.0);
	return c + (n - 0.5) * amount;
}

float3 ApplyVignette(float3 c, float2 uv, float strength)
{
	if (strength <= 0.0)
		return c;
	float2 d = uv - 0.5;
	float r = length(d) * 1.41421356; // normalize so corners ~1
	float darken = saturate(r * r * strength);
	return c * (1.0 - darken);
}

float3 ApplyScanline(float3 c, float2 pixelPos, float strength)
{
	if (strength <= 0.0)
		return c;
	float rowMod = (frac(pixelPos.y * 0.5) < 0.5) ? 1.0 : (1.0 - strength * 0.35);
	return c * rowMod;
}

float4 main(PS_IN input) : SV_TARGET
{
	float2 localUv = input.uv;
	float2 muv = MosaicUV(localUv, PP_MosaicBlocks);

	float4 sampled;
	if (PP_RGBShift > 0.0)
		sampled = SampleRGBShift(muv, PP_RGBShift);
	else
		sampled = sceneTex.Sample(sampLinearClamp, ToTexUV(muv));

	float3 c = sampled.rgb;
	float a = sampled.a;

	c = ApplyContrast(c, PP_Contrast);
	c = Posterize(c, PP_Posterize);
	c = ApplyGrayscale(c, PP_Grayscale);
	c = ApplyNoise(c, input.pos.xy, PP_Noise, PP_Time);
	c = ApplyVignette(c, localUv, PP_Vignette);
	c = ApplyScanline(c, input.pos.xy, PP_Scanline);

	if (PP_Flicker > 0.0)
	{
		float h = Hash11(PP_Time * 61.0);
		float dip = (h < 0.3) ? h / 0.3 : 0.0;
		c *= 1.0 - PP_Flicker * dip;
	}

	return float4(c, a);
}
