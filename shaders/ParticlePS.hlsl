// ParticlePS.hlsl
// Particle system pixel shader.

Texture2D g_tex : register(t0);
SamplerState g_smp : register(s0);

struct PS_IN {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
	float4 color : COLOR0;
};

float4 main(PS_IN input) : SV_TARGET {
	float4 tex = g_tex.Sample(g_smp, input.texcoord);
	clip(tex.a * input.color.a - 0.01f);
	return tex * input.color;
}
