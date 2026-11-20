// PixelShaderTerrain.hlsl
// Terrain version of PixelShader3D.hlsl (ADR 0011 step 5): same lighting, base colour from the
// splat-blended layers (SampleBaseColor under TERRAIN_SPLAT in LightingCommon.hlsli).

#define TERRAIN_SPLAT
#include "PixelShader3D.hlsl"
