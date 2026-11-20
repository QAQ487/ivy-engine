// PixelShaderTerrain_PBR.hlsl
// Terrain version of PixelShader3D_PBR.hlsl (ADR 0011 step 5): same lighting, base colour from the
// splat-blended layers (SampleBaseColor under TERRAIN_SPLAT in LightingCommon.hlsli).

#define TERRAIN_SPLAT
#include "PixelShader3D_PBR.hlsl"
