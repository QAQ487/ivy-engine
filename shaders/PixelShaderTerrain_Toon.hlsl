// PixelShaderTerrain_Toon.hlsl
// Terrain version of PixelShader3D_Toon.hlsl (ADR 0011 step 5): same lighting, base colour from the
// splat-blended layers (SampleBaseColor under TERRAIN_SPLAT in LightingCommon.hlsli).

#define TERRAIN_SPLAT
#include "PixelShader3D_Toon.hlsl"
