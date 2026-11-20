// VertexShaderShadowVolume.hlsl
// Stencil shadow volume extrusion (settings.render.shadowMode = "volume").
//
// Input is the volume mesh built by engine/rendering/lighting/shadow_volume_mesh.h:
// unshared triangle corners carrying their triangle's outward face normal, plus
// zero-area quads on every edge. Corners whose face does not face the light are pushed
// ExtrudeParams.x world units along the light direction (4x the camera far clip; see
// shadow_volume_policy.h); the rest stay put (nudged slightly along the light so the
// near cap sits just inside the caster). Quads on silhouette edges stretch into the
// volume's side walls.
//
// A finite push (not a w = 0 point at infinity) keeps orthographic cameras working: an
// orthographic projection keeps w = 0, so such points would be clipped away. The
// rasterizer runs with depth clip off, so the far cap clamps onto the far plane instead
// of being clipped; with the z-fail stencil count that keeps the result right when the
// camera is inside a shadow.
//
// Works for static and GPU-skinned meshes: SkinPositionOS / SkinDirectionOS are the
// identity when SkinEnabled == 0.

#include "ShaderCommon.hlsli"

cbuffer ShadowVolumeObject : register(b7)
{
    float4 ShineDirOS; // xyz: direction the light travels, in object space (not normalised)
    float4 CapBiasOS;  // xyz: near-cap offset along the light, object space
    float4 ExtrudeParams; // x: extrusion length in world units (ShineDirOS maps world units to object space)
}

float3 SkinDirectionOS(float3 d, uint4 indices, float4 weights)
{
    float3 result = d;
    if (SkinEnabled != 0)
    {
        const float4 hd = float4(d, 0.0f);
        float3 acc = 0.0f;
        float wsum = 0.0f;
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            const float w = weights[i];
            if (w <= 0.0f)
                continue;
            const uint idx = indices[i];
            if (idx >= (uint)SkinBoneCount)
                continue;
            acc += w * mul(hd, SkinBones[idx]).xyz;
            wsum += w;
        }
        acc += (1.0f - wsum) * d;
        result = acc;
    }
    return result;
}

float4 main(in float3 inPosition : POSITION0,
            in float3 inNormal   : NORMAL0,
            in float4 inDiffuse  : COLOR0,
            in float2 inTexCoord : TEXCOORD0,
            in uint4  inBlendIndices : BLENDINDICES0,
            in float4 inBlendWeights : BLENDWEIGHT0) : SV_POSITION
{
    const float3 posOS = SkinPositionOS(inPosition, inBlendIndices, inBlendWeights);
    // Face normals are geometric (outward); skinning them with the bone rotation keeps
    // the facing test right for rigid and near-rigid bones.
    const float3 faceN = SkinDirectionOS(inNormal, inBlendIndices, inBlendWeights);

    // Lit when the face points toward the light (against the travel direction).
    const float facing = -dot(faceN, ShineDirOS.xyz);
    if (facing > 0.0f)
        return mul(float4(posOS + CapBiasOS.xyz, 1.0f), WorldViewProjection);
    return mul(float4(posOS + ShineDirOS.xyz * ExtrudeParams.x, 1.0f), WorldViewProjection);
}
