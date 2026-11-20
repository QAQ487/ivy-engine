// VertexShaderTerrain.hlsl
// Terrain chunk vertex shader (ADR 0011). Every chunk draws the same grid mesh; the
// per-instance data says which part of the heightmap it covers. Heights come from the
// R16 heightmap texture, so editing the map only updates the texture.
//
// Outputs match VertexShader3D, so the regular lit pixel shaders (Standard, PBR, Toon,
// ...) shade the terrain, including CSM shadows, fog and IBL. In the CSM depth pass the
// same shader runs with the light's view-projection in WorldViewProjection.
//
// Must match engine/terrain/terrain_heightmap.cpp: height = offset + sample * scale,
// vertex normal = central differences (one-sided at the map edge).

#include "ShaderCommon.hlsli"

cbuffer TerrainBuffer : register(b6)
{
    float2 TerrainCellSize;    // metres between samples along X / Z
    float  TerrainHeightScale; // metres for sample 65535
    float  TerrainHeightOffset;
    float  TerrainLastSample;  // resolution - 1
    float  TerrainTileSize;    // metres per texture repeat
    float2 TerrainPad;
}

Texture2D<float> g_TerrainHeight : register(t10); // R16_UNORM, 0..1 = sample / 65535

float TerrainHeightAt(int2 s)
{
    const int last = (int)TerrainLastSample;
    s = clamp(s, int2(0, 0), int2(last, last));
    return TerrainHeightOffset + g_TerrainHeight.Load(int3(s, 0)) * TerrainHeightScale;
}

void main(in float3 inGrid   : POSITION0,  // x, z = vertex in the chunk grid; y = 1 for skirt
          in float4 inChunk  : TEXCOORD5,  // per instance: origin x, origin z (samples), skirt depth
          out float4 outPosition : SV_POSITION,
          out float3 outNormal   : NORMAL0,
          out float2 outTexCoord : TEXCOORD0,
          out float4 outDiffuse  : COLOR0,
          out float3 outPosOS    : TEXCOORD2)
{
    const int last = (int)TerrainLastSample;
    const int2 s = int2(inChunk.xy + inGrid.xz + 0.5f);

    // Smooth normal from the neighbours (one-sided at the edges), as on the CPU.
    const int xl = max(s.x - 1, 0);
    const int xr = min(s.x + 1, last);
    const int zl = max(s.y - 1, 0);
    const int zr = min(s.y + 1, last);
    const float dhdx = (TerrainHeightAt(int2(xr, s.y)) - TerrainHeightAt(int2(xl, s.y)))
                       / (max(xr - xl, 1) * TerrainCellSize.x);
    const float dhdz = (TerrainHeightAt(int2(s.x, zr)) - TerrainHeightAt(int2(s.x, zl)))
                       / (max(zr - zl, 1) * TerrainCellSize.y);

    float3 posOS;
    posOS.x = s.x * TerrainCellSize.x;
    posOS.z = s.y * TerrainCellSize.y;
    posOS.y = TerrainHeightAt(s) - inGrid.y * inChunk.z; // skirt hangs below the edge

    outPosition = mul(float4(posOS, 1.0f), WorldViewProjection);
    outNormal   = normalize(float3(-dhdx, 1.0f, -dhdz));
    outTexCoord = posOS.xz / TerrainTileSize;
    outDiffuse  = float4(1.0f, 1.0f, 1.0f, 1.0f);
    outPosOS    = posOS;
}
