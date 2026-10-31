/**===========================================================================

 File   : Billboard pixel shader [PixelShaderUnlit.hlsl]

 Author : Hayato Otsuki
 Date   : 2025/12/4
 -----------------------------------------------------------------------------

 ===========================================================================**/

Texture2D g_tex : register(t0);
SamplerState g_smp : register(s0);

struct PS_IN {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_IN pin) : SV_TARGET {
    float4 tex = g_tex.Sample(g_smp, pin.uv);
    
    // Discard when alpha is near 0 (alpha test)
    clip(tex.a - 0.01f);
    
    return tex * pin.color;
};
