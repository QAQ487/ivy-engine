# GPU vertex-shader skin, same formula as SkinFromScratch

Skinned Visuals are CPU-skinned every frame (`SkinFromScratch` + full VB upload). That cannot hold 32 Mixamo-scale bodies at 60 on a 3060-class card, and it already hurts FrisbeeArena 2v2 (4 Visuals). We will deform on the GPU in the vertex shader.

The last time this was “fixed,” body parts scattered and no longer formed the mesh. That is a Bind failure (wrong `bindOffset` order, dropped animated mesh `toLocal`, geo applied twice, or leftover weight without the bind-CP fallback)—not a mesh-density problem. The GPU palette must be the same formula as `SkinFromScratch`. `SkinFromScratch` stays as an oracle until Y Bot positions match; it is not the frame path. Positions only in this pass (CPU does not re-skin normals). First-clip bake hitch is out of scope.

Y Bot is two skinned meshes (`Alpha_Surface` + `Alpha_Joints`) sharing one skeleton. The palette is `meshCount * boneCount` slots (2×52=104), with vertex bone indices offset by `meshIndex * boneCount`. A single-mesh-only GPU path left 2v2 on CPU skin and under 20 FPS.

`FbxAMatrix::MultT` is row-vector × matrix, same as DirectXMath `XMVector3Transform`. Palette matrices are stored as `FbxToXM` and transposed only in `Shader_SetSkinPalette` (same as WVP). An extra CPU-side transpose applies Mᵀ and scatters the mesh.

**Considered Options**: Faster CPU skin (will not hit 32). Compute skin (no existing compute path). A “simpler” GPU bind (already produced scatter).
