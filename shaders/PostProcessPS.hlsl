cbuffer PostProcessBuffer : register(b10)
{
	float PP_Mode;         // 0=passthrough, 1=rgb shift, 2=mosaic, 3=combined
	float PP_RGBShift;     // shift amount in local (viewport) uv units
	float PP_MosaicBlocks; // blocks across viewport width
	float PP_Padding;
	float2 PP_UVOffset;    // PostRT region that holds the game viewport
	float2 PP_UVScale;
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

float4 main(PS_IN input) : SV_TARGET
{
	float2 localUv = input.uv;
	int mode = (int)PP_Mode;

	if (mode == 0)
		return sceneTex.Sample(sampLinearClamp, ToTexUV(localUv));

	if (mode == 1)
		return SampleRGBShift(localUv, PP_RGBShift);

	if (mode == 2)
	{
		float2 muv = MosaicUV(localUv, PP_MosaicBlocks);
		return sceneTex.Sample(sampLinearClamp, ToTexUV(muv));
	}

	// mode 3: mosaic first, then rgb shift on mosaicked coords
	{
		float2 muv = MosaicUV(localUv, PP_MosaicBlocks);
		return SampleRGBShift(muv, PP_RGBShift);
	}
}
