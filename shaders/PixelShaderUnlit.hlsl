/**===========================================================================

 File   : �r���{�[�h�s�N�Z���V�F�[�_ [PixelShaderUnlit.hlsl]

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
    
    // �A���t�@��0�ɋ߂��ꍇ�͔j���i���������̏����j
    clip(tex.a - 0.01f);
    
    return tex * pin.color;
};