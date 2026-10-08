import { writeFileSync } from 'node:fs';

const file = (id, type, name, filePath, summary, tags, complexity, languageNotes) => {
  const n = { id, type, name, filePath, summary, tags, complexity };
  if (languageNotes) n.languageNotes = languageNotes;
  return n;
};

const fn = (path, name, lineRange, summary, tags, complexity) => ({
  id: `function:${path}:${name}`,
  type: 'function',
  name,
  filePath: path,
  lineRange,
  summary,
  tags,
  complexity,
});

const edge = (source, target, type, weight) => ({
  source,
  target,
  type,
  direction: 'forward',
  weight,
});

const contains = (fileId, fnId) => edge(fileId, fnId, 'contains', 1.0);
const exportsE = (fileId, fnId) => edge(fileId, fnId, 'exports', 0.8);
const depends = (src, tgt) => edge(src, tgt, 'depends_on', 0.6);
const calls = (src, tgt) => edge(src, tgt, 'calls', 0.8);

const lighting = 'shaders/LightingCommon.hlsli';
const common = 'shaders/ShaderCommon.hlsli';

const lightingFns = [
  fn(lighting, 'SampleBaseColor', [64, 72],
    'Samples the albedo texture when enabled and multiplies vertex plus material diffuse into RGB and alpha.',
    ['shader', 'sampling', 'material'], 'simple'),
  fn(lighting, 'CotangentFrame', [75, 89],
    'Builds a cotangent-frame TBN matrix from position and UV derivatives so normal maps work without mesh tangents.',
    ['shader', 'normal-mapping', 'utility'], 'simple'),
  fn(lighting, 'ApplyNormalMap', [91, 103],
    'Perturbs the interpolated object-space normal with the tangent-space normal map scaled by NormalStrength.',
    ['shader', 'normal-mapping', 'lighting'], 'simple'),
  fn(lighting, 'PCF3x3', [105, 119],
    'Averages a 3x3 percentage-closer filter using a comparison sampler for soft shadow edges.',
    ['shader', 'shadows', 'filtering'], 'simple'),
  fn(lighting, 'SampleCascadeShadow0', [121, 133],
    'Projects a receiver into cascade 0 light space and returns a biased PCF shadow factor.',
    ['shader', 'shadows', 'cascades'], 'simple'),
  fn(lighting, 'SampleCascadeShadow1', [134, 146],
    'Projects a receiver into cascade 1 light space and returns a biased PCF shadow factor.',
    ['shader', 'shadows', 'cascades'], 'simple'),
  fn(lighting, 'SampleCascadeShadow2', [147, 159],
    'Projects a receiver into cascade 2 light space and returns a biased PCF shadow factor.',
    ['shader', 'shadows', 'cascades'], 'simple'),
  fn(lighting, 'SampleCascadeShadow3', [160, 172],
    'Projects a receiver into cascade 3 light space and returns a biased PCF shadow factor.',
    ['shader', 'shadows', 'cascades'], 'simple'),
  fn(lighting, 'EvalShadow', [193, 240],
    'Selects a CSM cascade from view-space depth for directional lights and blends adjacent maps near split planes.',
    ['shader', 'shadows', 'cascades'], 'moderate'),
  fn(lighting, 'EvalHemisphereAmbient', [242, 255],
    'Lerps sky and ground ambient by normal Y and adds a downward ground-bounce term from Ambient.a.',
    ['shader', 'ambient', 'lighting'], 'simple'),
  fn(lighting, 'EvalDirectionalLight', [257, 272],
    'Computes Lambert diffuse plus optional Blinn-Phong specular for a directional light.',
    ['shader', 'lighting', 'directional'], 'simple'),
  fn(lighting, 'EvalPointLight', [274, 301],
    'Evaluates a point light with quadratic attenuation, optional range fade, and optional specular.',
    ['shader', 'lighting', 'point-light'], 'moderate'),
  fn(lighting, 'EvalSpotLight', [303, 334],
    'Evaluates a spot light with distance attenuation and an inner/outer cosine cone falloff.',
    ['shader', 'lighting', 'spot-light'], 'moderate'),
  fn(lighting, 'QuantizeToon', [342, 351],
    'Bands a 0-1 lighting factor into discrete toon steps with optional smoothstep edges.',
    ['shader', 'toon', 'quantization'], 'simple'),
  fn(lighting, 'EvalPointDiffuse', [361, 376],
    'Returns attenuated Lambert intensity for a point light without a specular term.',
    ['shader', 'lighting', 'toon'], 'simple'),
  fn(lighting, 'EvalSpotDiffuse', [378, 398],
    'Returns cone-attenuated Lambert intensity for a spot light without a specular term.',
    ['shader', 'lighting', 'toon'], 'moderate'),
  fn(lighting, 'EvalCelSpecular', [400, 441],
    'Hard-steps Blinn-Phong specular past SpecularThreshold for cel highlights across light types.',
    ['shader', 'toon', 'specular'], 'moderate'),
  fn(lighting, 'ApplyHeightFog', [443, 454],
    'Mixes lit color toward FogColor using camera distance and a world-height falloff when fog is enabled.',
    ['shader', 'fog', 'atmosphere'], 'simple'),
  fn(lighting, 'EvalLightFacingRim', [457, 493],
    'Computes a Fresnel rim weighted away from the key directional light for a backlit anime edge.',
    ['shader', 'rim', 'toon'], 'moderate'),
  fn(lighting, 'EvalCookTorrance', [523, 544],
    'Cook-Torrance BRDF using GGX NDF, Schlick-Smith geometry, and Schlick Fresnel with energy-conserving diffuse.',
    ['shader', 'pbr', 'brdf'], 'moderate'),
  fn(lighting, 'EvalPointPBR', [552, 566],
    'Feeds a range-attenuated point-light direction into the shared Cook-Torrance evaluator.',
    ['shader', 'pbr', 'point-light'], 'simple'),
  fn(lighting, 'EvalSpotPBR', [568, 585],
    'Feeds a cone-attenuated spot-light direction into the shared Cook-Torrance evaluator.',
    ['shader', 'pbr', 'spot-light'], 'simple'),
];

const filesPart1 = [
  file('resource:rom/skybox/MySkybox.dds', 'resource', 'MySkybox.dds', 'rom/skybox/MySkybox.dds',
    'Assembled cubemap DDS used as the runtime skybox texture sampled by the skybox pixel shader.',
    ['texture', 'cubemap', 'skybox', 'asset'], 'moderate'),
  file('file:rom/skybox/run.bat', 'file', 'run.bat', 'rom/skybox/run.bat',
    'One-shot batch that runs texassemble to pack six PNG faces into MySkybox.dds.',
    ['script', 'skybox', 'asset-pipeline'], 'simple'),
  file('resource:rom/skybox/texassemble.exe', 'resource', 'texassemble.exe', 'rom/skybox/texassemble.exe',
    'Bundled DirectXTex texassemble executable that builds cube DDS files from six face images.',
    ['tool', 'texture', 'asset-pipeline'], 'simple'),
  file('config:rom/tags.json', 'config', 'tags.json', 'rom/tags.json',
    'Gameplay tag vocabulary listing collision and identity labels such as PLAYER, ENEMY, WALL, and SHADOW.',
    ['configuration', 'tags', 'gameplay'], 'simple'),
  file('file:shaders/FullscreenVS.hlsl', 'file', 'FullscreenVS.hlsl', 'shaders/FullscreenVS.hlsl',
    'Vertex shader that expands SV_VertexID into a fullscreen triangle covering NDC for post-process passes.',
    ['shader', 'vertex-shader', 'post-process'], 'simple',
    'Uses the classic bit-shift fullscreen-triangle UV construction.'),
  file('file:shaders/LightingCommon.hlsli', 'file', 'LightingCommon.hlsli', lighting,
    'Shared HLSL lighting library for shadows, hemisphere ambient, classic lights, toon bands, height fog, and Cook-Torrance PBR.',
    ['shader', 'lighting', 'include', 'pbr'], 'complex',
    'Include header with fixed cbuffer registers b2/b3 and comparison-sampler PCF cascades.'),
  file('file:shaders/ParticlePS.hlsl', 'file', 'ParticlePS.hlsl', 'shaders/ParticlePS.hlsl',
    'Particle pixel shader that multiplies atlas texture and instance color, then clips near-zero alpha.',
    ['shader', 'pixel-shader', 'particles'], 'simple'),
  file('file:shaders/ParticleVS.hlsl', 'file', 'ParticleVS.hlsl', 'shaders/ParticleVS.hlsl',
    'Instanced particle vertex shader that billboards quads in view space with per-instance size, UV rect, and rotation.',
    ['shader', 'vertex-shader', 'particles', 'instancing'], 'simple'),
  file('file:shaders/PixelShader2D.hlsl', 'file', 'PixelShader2D.hlsl', 'shaders/PixelShader2D.hlsl',
    '2D sprite pixel shader that multiplies sampled texture, vertex color, and a color constant buffer.',
    ['shader', 'pixel-shader', '2d'], 'simple'),
  file('file:shaders/PixelShader3D.hlsl', 'file', 'PixelShader3D.hlsl', 'shaders/PixelShader3D.hlsl',
    'Default lit 3D pixel shader that loops all lights, applies CSM shadows, hemisphere ambient, emission, and height fog.',
    ['shader', 'pixel-shader', 'lighting'], 'moderate'),
  file('file:shaders/PixelShader3D_DirectionalPixel.hlsl', 'file', 'PixelShader3D_DirectionalPixel.hlsl',
    'shaders/PixelShader3D_DirectionalPixel.hlsl',
    'Lit 3D pixel shader variant that evaluates only directional lights with shadows, ambient, and height fog.',
    ['shader', 'pixel-shader', 'directional'], 'simple'),
  file('file:shaders/PixelShader3D_Hemisphere.hlsl', 'file', 'PixelShader3D_Hemisphere.hlsl',
    'shaders/PixelShader3D_Hemisphere.hlsl',
    'Unshadowed 3D pixel shader that outputs hemisphere ambient plus emission and height fog.',
    ['shader', 'pixel-shader', 'ambient'], 'simple'),
  file('file:shaders/PixelShader3D_Outline.hlsl', 'file', 'PixelShader3D_Outline.hlsl',
    'shaders/PixelShader3D_Outline.hlsl',
    'Solid-color outline pixel shader that writes MaterialBuffer OutlineColor for inverted-hull passes.',
    ['shader', 'pixel-shader', 'outline', 'toon'], 'simple'),
];

const mainsPart1 = [
  fn('shaders/FullscreenVS.hlsl', 'main', [7, 15],
    'Builds clip-space positions and UVs for a covering triangle from the vertex ID.',
    ['shader', 'entry-point', 'vertex-shader'], 'simple'),
  fn('shaders/ParticlePS.hlsl', 'main', [10, 14],
    'Samples the particle atlas, clips low alpha, and returns tinted premultiplied color.',
    ['shader', 'entry-point', 'pixel-shader'], 'simple'),
  fn('shaders/ParticleVS.hlsl', 'main', [22, 43],
    'Rotates the local quad, expands it along camera right/up, and applies the instance UV rectangle.',
    ['shader', 'entry-point', 'particles'], 'moderate'),
  fn('shaders/PixelShader2D.hlsl', 'main', [14, 18],
    'Returns texture * vertex color * g_Color for 2D sprites.',
    ['shader', 'entry-point', 'pixel-shader'], 'simple'),
  fn('shaders/PixelShader3D.hlsl', 'main', [3, 55],
    'Accumulates directional, spot, and point lighting with per-light shadows, then applies fog and a shadow overlay.',
    ['shader', 'entry-point', 'lighting'], 'moderate'),
  fn('shaders/PixelShader3D_DirectionalPixel.hlsl', 'main', [3, 41],
    'Loops lights but shades only directional entries, combining Lambert+specular, shadows, and fog.',
    ['shader', 'entry-point', 'directional'], 'moderate'),
  fn('shaders/PixelShader3D_Hemisphere.hlsl', 'main', [3, 16],
    'Samples base color, applies the normal map, and writes hemisphere ambient plus emission.',
    ['shader', 'entry-point', 'ambient'], 'simple'),
  fn('shaders/PixelShader3D_Outline.hlsl', 'main', [3, 6],
    'Writes the material outline color as the fragment output.',
    ['shader', 'entry-point', 'outline'], 'simple'),
];

const filesPart2 = [
  file('file:shaders/PixelShader3D_PBR.hlsl', 'file', 'PixelShader3D_PBR.hlsl',
    'shaders/PixelShader3D_PBR.hlsl',
    'Cook-Torrance PBR pixel shader that lights all types with roughness/metallic and a Fresnel-boosted ambient term.',
    ['shader', 'pixel-shader', 'pbr'], 'moderate',
    'Stays in the engine gamma path and does not apply an extra tone-map or gamma encode.'),
  file('file:shaders/PixelShader3D_PointPixel.hlsl', 'file', 'PixelShader3D_PointPixel.hlsl',
    'shaders/PixelShader3D_PointPixel.hlsl',
    'Lit 3D pixel shader variant that evaluates only point lights plus a dim ambient and height fog.',
    ['shader', 'pixel-shader', 'point-light'], 'simple'),
  file('file:shaders/PixelShader3D_Rim.hlsl', 'file', 'PixelShader3D_Rim.hlsl',
    'shaders/PixelShader3D_Rim.hlsl',
    'Rim-light pixel shader that adds a light-facing Fresnel edge on top of dim hemisphere ambient.',
    ['shader', 'pixel-shader', 'rim'], 'simple'),
  file('file:shaders/PixelShader3D_SpotPixel.hlsl', 'file', 'PixelShader3D_SpotPixel.hlsl',
    'shaders/PixelShader3D_SpotPixel.hlsl',
    'Lit 3D pixel shader variant that evaluates only spot lights with shadows, overlay, and height fog.',
    ['shader', 'pixel-shader', 'spot-light'], 'simple'),
  file('file:shaders/PixelShader3D_SurfaceFX.hlsl', 'file', 'PixelShader3D_SurfaceFX.hlsl',
    'shaders/PixelShader3D_SurfaceFX.hlsl',
    'Parametric dual-scroll surface shader for water, lava, or energy with radial depth gradient and edge fade.',
    ['shader', 'pixel-shader', 'surface-fx', 'vfx'], 'moderate'),
  file('file:shaders/PixelShader3D_Toon.hlsl', 'file', 'PixelShader3D_Toon.hlsl',
    'shaders/PixelShader3D_Toon.hlsl',
    'Cel-shaded 3D pixel shader with quantized bands, optional ramp texture, cel specular, and a light-facing rim.',
    ['shader', 'pixel-shader', 'toon'], 'moderate'),
  file('file:shaders/PixelShaderUnlit.hlsl', 'file', 'PixelShaderUnlit.hlsl',
    'shaders/PixelShaderUnlit.hlsl',
    'Unlit textured pixel shader that clips near-zero alpha and multiplies sampled color by vertex color.',
    ['shader', 'pixel-shader', 'unlit'], 'simple'),
  file('file:shaders/PostProcessPS.hlsl', 'file', 'PostProcessPS.hlsl',
    'shaders/PostProcessPS.hlsl',
    'Fullscreen post-process pixel shader stacking mosaic, RGB split, contrast, posterize, grain, vignette, scanlines, and flicker.',
    ['shader', 'pixel-shader', 'post-process'], 'moderate'),
  file('file:shaders/ShaderCommon.hlsli', 'file', 'ShaderCommon.hlsli', common,
    'Shared HLSL constants for WVP, 128-bone skinning, and the material buffer used by 3D vertex and pixel shaders.',
    ['shader', 'include', 'material', 'skinning'], 'moderate',
    'Header-guarded include; Diffuse.a doubles as a texture-enable flag.'),
  file('file:shaders/SkyboxPS.hlsl', 'file', 'SkyboxPS.hlsl', 'shaders/SkyboxPS.hlsl',
    'Skybox pixel shader that samples a cubemap along the view direction and applies tint plus exposure.',
    ['shader', 'pixel-shader', 'skybox'], 'simple'),
  file('file:shaders/SkyboxVS.hlsl', 'file', 'SkyboxVS.hlsl', 'shaders/SkyboxVS.hlsl',
    'Skybox vertex shader that rotates the cube, projects with z=w so the sky stays at the far plane.',
    ['shader', 'vertex-shader', 'skybox'], 'simple'),
  file('file:shaders/VertexShader2D.hlsl', 'file', 'VertexShader2D.hlsl', 'shaders/VertexShader2D.hlsl',
    '2D vertex shader that transforms local positions by a world matrix and a combined view-projection matrix.',
    ['shader', 'vertex-shader', '2d'], 'simple'),
];

const fnsPart2 = [
  fn('shaders/PixelShader3D_PBR.hlsl', 'main', [7, 58],
    'Samples material maps, boosts ambient by Schlick Fresnel, and accumulates shadowed Cook-Torrance lighting.',
    ['shader', 'entry-point', 'pbr'], 'moderate'),
  fn('shaders/PixelShader3D_PointPixel.hlsl', 'main', [3, 33],
    'Loops lights and adds only point-light contributions plus ambient, emission, and fog.',
    ['shader', 'entry-point', 'point-light'], 'moderate'),
  fn('shaders/PixelShader3D_Rim.hlsl', 'main', [3, 24],
    'Adds a specular-tinted light-facing rim on dim hemisphere ambient, then applies height fog.',
    ['shader', 'entry-point', 'rim'], 'simple'),
  fn('shaders/PixelShader3D_SpotPixel.hlsl', 'main', [3, 41],
    'Loops lights and shades only spot entries with shadows, overlay, emission, and fog.',
    ['shader', 'entry-point', 'spot-light'], 'moderate'),
  fn('shaders/PixelShader3D_SurfaceFX.hlsl', 'main', [25, 64],
    'Scrolls two world-XZ UVs, lerps shallow/deep colors by radius, and fades alpha at the mesh edge.',
    ['shader', 'entry-point', 'surface-fx'], 'moderate'),
  fn('shaders/PixelShader3D_Toon.hlsl', 'main', [3, 68],
    'Accumulates shadowed Lambert, quantizes to toon bands or a ramp, then adds cel specular and rim.',
    ['shader', 'entry-point', 'toon'], 'moderate'),
  fn('shaders/PixelShaderUnlit.hlsl', 'main', [20, 26],
    'Samples the bound texture, clips low alpha, and multiplies by interpolated vertex color.',
    ['shader', 'entry-point', 'unlit'], 'simple'),
  fn('shaders/PostProcessPS.hlsl', 'MosaicUV', [33, 42],
    'Snaps local UVs to a block grid when mosaic block count is greater than one.',
    ['shader', 'post-process', 'utility'], 'simple'),
  fn('shaders/PostProcessPS.hlsl', 'SampleRGBShift', [44, 53],
    'Samples red and blue with opposing radial offsets to produce a chromatic-aberration split.',
    ['shader', 'post-process', 'utility'], 'simple'),
  fn('shaders/PostProcessPS.hlsl', 'main', [115, 144],
    'Applies the configured post stack to the scene texture, including optional flicker from a time hash.',
    ['shader', 'entry-point', 'post-process'], 'moderate'),
  fn(common, 'SkinPositionOS', [23, 45],
    'Linear-blend skins an object-space position by up to four bone matrices when skinning is enabled.',
    ['shader', 'skinning', 'vertex-shader'], 'moderate'),
  fn('shaders/SkyboxPS.hlsl', 'main', [15, 19],
    'Normalizes the interpolated direction and samples the cubemap with tint and exposure.',
    ['shader', 'entry-point', 'skybox'], 'simple'),
  fn('shaders/SkyboxVS.hlsl', 'main', [16, 24],
    'Rotates the skybox vertex, projects it, and writes clip.xyww so depth stays at the far plane.',
    ['shader', 'entry-point', 'skybox'], 'simple'),
  fn('shaders/VertexShader2D.hlsl', 'main', [21, 31],
    'Multiplies local position by world * projection and forwards color plus texcoord.',
    ['shader', 'entry-point', 'vertex-shader'], 'simple'),
];

const L = (name) => `function:${lighting}:${name}`;
const mainOf = (path) => `function:${path}:main`;

const nodes1 = [...filesPart1, ...lightingFns, ...mainsPart1];
const nodes2 = [...filesPart2, ...fnsPart2];

const edges1 = [
  depends('file:rom/skybox/run.bat', 'resource:rom/skybox/texassemble.exe'),
  depends(`file:${lighting}`, `file:${common}`),
  depends('file:shaders/PixelShader3D.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_DirectionalPixel.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_Hemisphere.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_Outline.hlsl', `file:${common}`),
  ...lightingFns.map((f) => contains(`file:${lighting}`, f.id)),
  ...mainsPart1.map((f) => contains(`file:${f.filePath}`, f.id)),
  ...mainsPart1.map((f) => exportsE(`file:${f.filePath}`, f.id)),
  // Same-part calls into LightingCommon
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('SampleBaseColor')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('ApplyNormalMap')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('EvalHemisphereAmbient')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('EvalShadow')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('EvalDirectionalLight')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('EvalSpotLight')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('EvalPointLight')),
  calls(mainOf('shaders/PixelShader3D.hlsl'), L('ApplyHeightFog')),
  calls(mainOf('shaders/PixelShader3D_DirectionalPixel.hlsl'), L('SampleBaseColor')),
  calls(mainOf('shaders/PixelShader3D_DirectionalPixel.hlsl'), L('ApplyNormalMap')),
  calls(mainOf('shaders/PixelShader3D_DirectionalPixel.hlsl'), L('EvalShadow')),
  calls(mainOf('shaders/PixelShader3D_DirectionalPixel.hlsl'), L('EvalDirectionalLight')),
  calls(mainOf('shaders/PixelShader3D_DirectionalPixel.hlsl'), L('ApplyHeightFog')),
  calls(mainOf('shaders/PixelShader3D_Hemisphere.hlsl'), L('SampleBaseColor')),
  calls(mainOf('shaders/PixelShader3D_Hemisphere.hlsl'), L('ApplyNormalMap')),
  calls(mainOf('shaders/PixelShader3D_Hemisphere.hlsl'), L('EvalHemisphereAmbient')),
  calls(mainOf('shaders/PixelShader3D_Hemisphere.hlsl'), L('ApplyHeightFog')),
];

const batchImportPaths = new Set([
  'rom/skybox/MySkybox.dds',
  'rom/skybox/run.bat',
  'rom/skybox/texassemble.exe',
  'rom/tags.json',
  'shaders/FullscreenVS.hlsl',
  lighting,
  'shaders/ParticlePS.hlsl',
  'shaders/ParticleVS.hlsl',
  'shaders/PixelShader2D.hlsl',
  'shaders/PixelShader3D.hlsl',
  'shaders/PixelShader3D_DirectionalPixel.hlsl',
  'shaders/PixelShader3D_Hemisphere.hlsl',
  'shaders/PixelShader3D_Outline.hlsl',
  'shaders/PixelShader3D_PBR.hlsl',
  'shaders/PixelShader3D_PointPixel.hlsl',
  'shaders/PixelShader3D_Rim.hlsl',
  'shaders/PixelShader3D_SpotPixel.hlsl',
  'shaders/PixelShader3D_SurfaceFX.hlsl',
  'shaders/PixelShader3D_Toon.hlsl',
  'shaders/PixelShaderUnlit.hlsl',
  'shaders/PostProcessPS.hlsl',
  common,
  'shaders/SkyboxPS.hlsl',
  'shaders/SkyboxVS.hlsl',
  'shaders/VertexShader2D.hlsl',
]);

const edges2 = [
  depends('file:shaders/PixelShader3D_PBR.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_PointPixel.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_Rim.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_SpotPixel.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_SurfaceFX.hlsl', `file:${lighting}`),
  depends('file:shaders/PixelShader3D_Toon.hlsl', `file:${lighting}`),
  ...fnsPart2.map((f) => contains(`file:${f.filePath}`, f.id)),
  ...fnsPart2.filter((f) => f.name === 'main').map((f) => exportsE(`file:${f.filePath}`, f.id)),
];

function validate(part, nodes, edges) {
  const ids = new Set(nodes.map((n) => n.id));
  const failures = [];
  for (const e of edges) {
    const okSource = ids.has(e.source);
    if (!okSource) {
      failures.push(`source missing: ${e.source} -> ${e.target}`);
      continue;
    }
    if (ids.has(e.target)) continue;
    if (e.target.startsWith('file:')) {
      const p = e.target.slice('file:'.length);
      if (batchImportPaths.has(p)) continue;
    }
    failures.push(`target invalid: ${e.source} -> ${e.target} (${e.type})`);
  }
  if (failures.length) {
    console.error(`Part ${part} validation failed:`);
    for (const f of failures) console.error('  ' + f);
    process.exit(1);
  }
}

validate(1, nodes1, edges1);
validate(2, nodes2, edges2);

const outDir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';
writeFileSync(`${outDir}/batch-20-part-1.json`, JSON.stringify({ nodes: nodes1, edges: edges1 }, null, 2) + '\n');
writeFileSync(`${outDir}/batch-20-part-2.json`, JSON.stringify({ nodes: nodes2, edges: edges2 }, null, 2) + '\n');

console.log(JSON.stringify({
  part1: { nodes: nodes1.length, edges: edges1.length },
  part2: { nodes: nodes2.length, edges: edges2.length },
  total: { nodes: nodes1.length + nodes2.length, edges: edges1.length + edges2.length },
}, null, 2));
