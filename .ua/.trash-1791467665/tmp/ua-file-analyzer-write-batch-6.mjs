import { writeFileSync, mkdirSync } from 'node:fs';

const file = (path, name, summary, tags, complexity, extra = {}) => ({
  id: `file:${path}`,
  type: 'file',
  name,
  filePath: path,
  summary,
  tags,
  complexity,
  ...extra,
});

const fn = (path, name, lineRange, summary, tags, complexity, extra = {}) => ({
  id: `function:${path}:${name}`,
  type: 'function',
  name,
  filePath: path,
  lineRange,
  summary,
  tags,
  complexity,
  ...extra,
});

const cls = (path, name, lineRange, summary, tags, complexity, extra = {}) => ({
  id: `class:${path}:${name}`,
  type: 'class',
  name,
  filePath: path,
  lineRange,
  summary,
  tags,
  complexity,
  ...extra,
});

const edge = (source, target, type, weight) => ({
  source, target, type, direction: 'forward', weight,
});

const contains = (filePath, nodeId) => edge(`file:${filePath}`, nodeId, 'contains', 1.0);
const exportsE = (filePath, nodeId) => edge(`file:${filePath}`, nodeId, 'exports', 0.8);
const importsE = (from, to) => edge(`file:${from}`, `file:${to}`, 'imports', 0.7);
const callsE = (source, target) => edge(source, target, 'calls', 0.8);

const importData = {
  'engine/rendering/environment/skybox.cpp': [
    'engine/ecs/string_utils.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/environment/skybox.h',
    'engine/rendering/sprite/texture.h',
    'engine/scene/render_settings.h',
  ],
  'engine/rendering/model/model_util.cpp': [
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/model/model_util.h',
  ],
  'engine/rendering/particle/particle_system.cpp': [
    'engine/ecs/string_utils.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/particle/particle_system.h',
    'engine/rendering/sprite/texture.h',
  ],
  'engine/rendering/sprite/line_sprite.cpp': [
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/d3d11/shader.h',
    'engine/rendering/sprite/line_sprite.h',
    'engine/rendering/sprite/texture.h',
  ],
  'engine/rendering/sprite/line_sprite.h': [],
  'engine/rendering/sprite/sprite.cpp': [
    'engine/debug_draw/debug_ofstream.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/d3d11/shader.h',
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
  ],
  'engine/rendering/sprite/sprite.h': [],
  'engine/rendering/sprite/texture.cpp': [
    'engine/debug_draw/debug_ofstream.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/sprite/texture.h',
    'engine/ui/ui_font.h',
  ],
  'engine/rendering/sprite/texture.h': [],
  'engine/ui/scene_canvas.cpp': [
    'engine/core/engine_gui.h',
    'engine/ecs/string_utils.h',
    'engine/input/key_logger.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'engine/ui/scene_canvas.h',
  ],
  'engine/ui/ui_bar.cpp': [
    'engine/ecs/string_utils.h',
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'engine/ui/ui_bar.h',
  ],
  'engine/ui/ui_button.cpp': [
    'engine/audio/audio_manager.h',
    'engine/core/engine_gui.h',
    'engine/ecs/string_utils.h',
    'engine/input/mouse.h',
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'engine/ui/ui_button.h',
  ],
  'engine/ui/ui_font.cpp': [
    'engine/rendering/sprite/texture.h',
    'engine/ui/ui_font.h',
  ],
  'engine/ui/ui_font.h': [],
  'engine/ui/ui_sprite.cpp': [
    'engine/ecs/string_utils.h',
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'engine/ui/ui_sprite.h',
  ],
  'engine/ui/ui_text_renderer.cpp': [
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'engine/ui/ui_font.h',
    'engine/ui/ui_text_renderer.h',
  ],
  'engine/ui/ui_text_renderer.h': [],
  'src/app/main.cpp': [
    'engine/audio/audio_manager.h',
    'engine/core/debug_system.h',
    'engine/core/engine_gui.h',
    'engine/core/job_system.h',
    'engine/core/system_timer.h',
    'engine/debug_draw/debug_ofstream.h',
    'engine/debug_draw/debug_text.h',
    'engine/input/controller.h',
    'engine/input/key_logger.h',
    'engine/input/keyboard.h',
    'engine/input/mouse.h',
    'engine/rendering/d3d11/post_process.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/rendering/d3d11/shader.h',
    'engine/rendering/effect/effect_system.h',
    'engine/rendering/model/model_util.h',
    'engine/rendering/sprite/line_sprite.h',
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'src/app/main.h',
  ],
  'src/app/main.h': [],
  'src/app/rendering/fade.cpp': [
    'engine/rendering/sprite/sprite.h',
    'engine/rendering/sprite/texture.h',
    'src/app/rendering/fade.h',
  ],
  'src/app/rendering/fade.h': [],
};

const nodes = [];

// ---- files ----
nodes.push(file(
  'engine/rendering/environment/skybox.cpp',
  'skybox.cpp',
  'Implements cubemap skybox rendering: loads compiled shaders, builds a unit cube, and draws a camera-locked environment with tint and exposure from render settings.',
  ['rendering', 'skybox', 'environment', 'd3d11'],
  'complex',
  { languageNotes: 'Strips view translation so the cube stays camera-centered; uses a front-face cull and read-only depth so the skybox sits behind the scene.' },
));
nodes.push(file(
  'engine/rendering/model/model_util.cpp',
  'model_util.cpp',
  'Shared model import helpers: WIC texture loading with an SRV cache, MTL/FBX diffuse texture resolution, and FBX skinned-mesh evaluation via a process-wide FbxManager.',
  ['rendering', 'model', 'texture-cache', 'fbx', 'utility'],
  'complex',
  { languageNotes: 'Caches WIC-decoded SRVs by wide path; FbxShared owns the SDK manager/IO settings for the process lifetime.' },
));
nodes.push(file(
  'engine/rendering/particle/particle_system.cpp',
  'particle_system.cpp',
  'Implements the CPU particle pool, emitter simulation (shapes, lifetime color/size, gravity), and GPU billboard instancing with additive or alpha blending.',
  ['rendering', 'particles', 'gpu-instancing', 'service'],
  'complex',
  { languageNotes: 'ParticleSystem is a singleton; ParticleRenderer_* builds instanced billboard meshes and binds blend states per emitter.' },
));
nodes.push(file(
  'engine/rendering/sprite/line_sprite.cpp',
  'line_sprite.cpp',
  'Draws a camera-facing world-space quad stretched between two points, used for ropes and trails with depth testing enabled.',
  ['rendering', 'sprite', 'billboard', 'debug-draw'],
  'moderate',
));
nodes.push(file(
  'engine/rendering/sprite/line_sprite.h',
  'line_sprite.h',
  'Declares LineSprite initialize/finalize/draw for a depth-tested, camera-facing quad between two world points.',
  ['rendering', 'sprite', 'type-definition', 'api-handler'],
  'simple',
));
nodes.push(file(
  'engine/rendering/sprite/sprite.cpp',
  'sprite.cpp',
  '2D sprite rasterizer that uploads quads (and a radial-fill fan) through the shared 2D shaders, with rotation, atlas UVs, and color tint.',
  ['rendering', 'sprite', '2d', 'd3d11'],
  'complex',
  { languageNotes: 'Sprite_Draw overloads funnel into one textured-quad path; Sprite_DrawRadialFill builds a triangle fan for circular progress.' },
));
nodes.push(file(
  'engine/rendering/sprite/sprite.h',
  'sprite.h',
  'Public 2D sprite API: initialize/finalize plus draw overloads and radial-fill drawing for circular progress indicators.',
  ['rendering', 'sprite', 'type-definition', 'api-handler'],
  'simple',
));
nodes.push(file(
  'engine/rendering/sprite/texture.cpp',
  'texture.cpp',
  'Texture slot manager that loads WIC/DirectXTex images, creates RGBA atlases, caches a 1x1 white texture, and loads DDS cubemaps.',
  ['rendering', 'texture', 'resource-cache', 'd3d11'],
  'complex',
  { languageNotes: 'Fixed arrays of 1024 2D textures and 64 cubemaps; TextureAllRelease also clears the UI font cache.' },
));
nodes.push(file(
  'engine/rendering/sprite/texture.h',
  'texture.h',
  'Declares texture and cubemap load/bind/release APIs used by sprites, UI, particles, and the skybox.',
  ['rendering', 'texture', 'type-definition', 'api-handler'],
  'simple',
));
nodes.push(file(
  'engine/ui/scene_canvas.cpp',
  'scene_canvas.cpp',
  'Owns the shared in-game UI canvas: JSON load/save of sprites, text, buttons, and bars, plus HUD bindings, button actions, and respawn overlay/timer helpers.',
  ['ui', 'canvas', 'serialization', 'api-handler'],
  'complex',
  { languageNotes: 'Anonymous-namespace JSON helpers plus a button-action registry that defaults LoadScene to SceneCanvas_RequestSceneFile.' },
));
nodes.push(file(
  'engine/ui/ui_bar.cpp',
  'ui_bar.cpp',
  'Implements UIBar, a progress-bar element that draws background/fill sprites, optional label/value text, and loads bar textures from paths.',
  ['ui', 'component', 'progress-bar', 'sprite'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_button.cpp',
  'ui_button.cpp',
  'Implements UIButton hover/press hit-testing, state textures, shared bitmap/TTF label rendering, click actions, and UI click audio.',
  ['ui', 'component', 'button', 'event-handler'],
  'complex',
));
nodes.push(file(
  'engine/ui/ui_font.cpp',
  'ui_font.cpp',
  'Bakes TrueType/OpenType fonts into a 1024 atlas via stb_truetype, uploads the atlas as an RGBA texture, and caches UIFont metrics by path.',
  ['ui', 'font', 'texture-atlas', 'cache'],
  'moderate',
  { languageNotes: 'STB_TRUETYPE_IMPLEMENTATION is compiled here; glyphs 32-126 are baked at 64px into a 1024 atlas.' },
));
nodes.push(file(
  'engine/ui/ui_font.h',
  'ui_font.h',
  'Defines UIGlyph/UIFont atlas metrics and the TTF path/cache API used by UI text rendering.',
  ['ui', 'font', 'type-definition', 'data-model'],
  'simple',
));
nodes.push(file(
  'engine/ui/ui_sprite.cpp',
  'ui_sprite.cpp',
  'Implements UISprite drawing with optional atlas source rects and Unity-style Radial360 fill for circular progress.',
  ['ui', 'component', 'sprite', 'radial-fill'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_text_renderer.cpp',
  'ui_text_renderer.cpp',
  'Draws UI strings either from a baked TTF atlas or a legacy monospaced font texture, measuring width and line height for layout.',
  ['ui', 'text', 'font', 'rendering'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_text_renderer.h',
  'ui_text_renderer.h',
  'Declares UITextRenderer, which can render either a TTF-backed UIFont atlas or a grid font texture.',
  ['ui', 'text', 'type-definition', 'component'],
  'simple',
));
nodes.push(file(
  'src/app/main.cpp',
  'main.cpp',
  'Windows application entry: creates the DPI-aware window, initializes renderer/input/audio/jobs, and runs the 60 Hz update/draw loop with fade and scene management.',
  ['entry-point', 'application', 'game-loop', 'windows'],
  'complex',
  { languageNotes: 'WinMain hosts a PeekMessage loop capped at ~60 FPS; editor-only F7/F9/F10/F11/F12 hotkeys are compiled out of GAME_RUNTIME.' },
));
nodes.push(file(
  'src/app/main.h',
  'main.h',
  'Application header that pulls in D3D11/DirectInput headers and defines the 1920x1080 design-resolution screen macros.',
  ['entry-point', 'type-definition', 'configuration'],
  'simple',
));
nodes.push(file(
  'src/app/rendering/fade.cpp',
  'fade.cpp',
  'Full-screen color fade that interpolates alpha, optionally swaps SceneManager scenes at fade-out, then fades back in.',
  ['rendering', 'fade', 'scene-transition', 'sprite'],
  'moderate',
));
nodes.push(file(
  'src/app/rendering/fade.h',
  'fade.h',
  'Declares fade modes, the FADE state struct, and the public Fade_* API for scene transitions.',
  ['rendering', 'fade', 'type-definition', 'api-handler'],
  'simple',
));

const fnNodes = [];
const classNodes = [];

function F(path, name, range, summary, tags, complexity, isExport) {
  const node = fn(path, name, range, summary, tags, complexity);
  fnNodes.push({ node, isExport: !!isExport });
}

function C(path, name, range, summary, tags, complexity, isExport) {
  const node = cls(path, name, range, summary, tags, complexity);
  classNodes.push({ node, isExport: !!isExport });
}

const sky = 'engine/rendering/environment/skybox.cpp';
F(sky, 'LoadCompiledShader', [43, 63], 'Reads a compiled shader blob from disk and creates the skybox vertex and pixel shaders.', ['shader-loader', 'd3d11', 'utility'], 'moderate');
F(sky, 'Skybox_Initialize', [70, 159], 'Creates skybox shaders, cube geometry, constant buffers, sampler, and raster/depth states from the D3D11 device.', ['initialization', 'skybox', 'd3d11'], 'complex', true);
F(sky, 'Skybox_Finalize', [161, 177], 'Releases skybox GPU objects and clears the cached cubemap path.', ['shutdown', 'skybox', 'resource-lifetime'], 'simple', true);
F(sky, 'Skybox_ReloadFromSettings', [179, 197], 'Loads or clears the skybox cubemap from RenderSettings, converting the path with ToWString.', ['skybox', 'settings', 'cubemap'], 'simple', true);
F(sky, 'Skybox_Draw', [199, 291], 'Draws the camera-locked cube with current view/projection, tint, and exposure, restoring prior D3D state afterward.', ['skybox', 'draw', 'd3d11'], 'complex', true);

const mu = 'engine/rendering/model/model_util.cpp';
F(mu, 'ModelUtil_ReleaseTextureCache', [37, 42], 'Releases every cached model texture SRV; must run before the D3D11 device is destroyed.', ['texture-cache', 'shutdown', 'utility'], 'simple', true);
F(mu, 'Trim', [45, 49], 'Trims leading and trailing whitespace from a std::string in place.', ['utility', 'string'], 'simple', true);
F(mu, 'Split', [50, 56], 'Splits a string on a separator character into a vector of tokens.', ['utility', 'string'], 'simple', true);
F(mu, 'WideToUTF8', [57, 63], 'Converts a wide Windows path to a UTF-8 std::string.', ['utility', 'encoding'], 'simple', true);
F(mu, 'Utf8ToWide', [64, 71], 'Converts a UTF-8 C string to a std::wstring.', ['utility', 'encoding'], 'simple', true);
F(mu, 'DirnameW', [72, 76], 'Returns the parent directory of a wide path.', ['utility', 'path'], 'simple', true);
F(mu, 'JoinPathW', [77, 83], 'Joins a wide directory and file name, inserting a separator when needed.', ['utility', 'path'], 'simple', true);
F(mu, 'IsAbsolutePathW', [84, 88], 'Reports whether a wide path is absolute (drive or UNC).', ['utility', 'path'], 'simple', true);
F(mu, 'LoadTextureWIC_Internal', [109, 240], 'Decodes an image with WIC, creates a mipmapped D3D11 SRV, and inserts it into the path cache.', ['texture-loader', 'wic', 'd3d11'], 'complex');
F(mu, 'LoadTextureWIC_SRV', [242, 245], 'Public wrapper that loads or returns a cached WIC texture SRV.', ['texture-loader', 'cache', 'api-handler'], 'simple', true);
F(mu, 'TryLoadMTLTexture_SRV', [317, 378], 'Parses an MTL file for map_Kd and loads the diffuse texture plus UV scale/offset/clamp flags.', ['mtl', 'texture-loader', 'model'], 'moderate', true);
F(mu, 'FindFirstDiffuseFileTexture', [381, 492], 'Walks an FBX node material graph to find the first diffuse file texture and its UV set.', ['fbx', 'texture-loader', 'model'], 'complex', true);
F(mu, 'TransformNormal_IT', [495, 502], 'Transforms a normal by an inverse-transpose FBX matrix.', ['math', 'fbx', 'utility'], 'simple', true);
F(mu, 'HasNegativeDeterminant', [503, 503], 'Returns whether an FBX matrix has a negative determinant (winding flip).', ['math', 'fbx', 'utility'], 'simple', true);
F(mu, 'FbxShared', [506, 510], 'Constructs the process-wide FBX manager and IO settings.', ['fbx', 'singleton', 'initialization'], 'simple', true);
F(mu, '~FbxShared', [511, 514], 'Destroys the shared FBX manager on process teardown.', ['fbx', 'shutdown'], 'simple', true);
F(mu, 'EnumerateSkinnedMeshes', [518, 527], 'Recursively collects FBX meshes that have skin deformers.', ['fbx', 'skinning', 'traversal'], 'simple', true);
F(mu, 'ComputeSkinnedCP_ForMesh', [530, 649], 'Evaluates an FBX skin at a time and writes deformed control-point positions and normals.', ['fbx', 'skinning', 'animation'], 'complex', true);

const ps = 'engine/rendering/particle/particle_system.cpp';
F(ps, 'LoadCompiledShader', [51, 60], 'Loads a compiled particle shader blob from disk for the billboard renderer.', ['shader-loader', 'particles', 'utility'], 'simple', true);
F(ps, 'CreateBlendState', [96, 107], 'Creates additive or alpha D3D11 blend states used by particle billboards.', ['d3d11', 'blend', 'particles'], 'simple', true);
F(ps, 'Reset', [110, 114], 'Clears a ParticlePool or the ParticleSystem emitter list so simulation can restart.', ['particles', 'lifecycle'], 'simple', true);
F(ps, 'Spawn', [116, 142], 'Allocates a live particle in the pool with position, velocity, size, color, and lifetime.', ['particles', 'spawn'], 'moderate', true);
F(ps, 'Tick', [144, 169], 'Advances particle or emitter simulation: aging, gravity, emission, and compacting dead particles.', ['particles', 'simulation', 'update'], 'moderate', true);
F(ps, 'Compact', [171, 184], 'Packs live particles to the front of the pool after deaths.', ['particles', 'memory'], 'simple', true);
F(ps, 'CopyParticle', [186, 200], 'Copies all per-particle attributes from one pool slot to another.', ['particles', 'utility'], 'simple', true);
F(ps, 'ParticleEmitter', [202, 205], 'Constructs an emitter, assigns its id, and applies the initial ParticleEmitterConfig.', ['particles', 'factory', 'lifecycle'], 'simple', true);
F(ps, 'SetConfig', [207, 218], 'Applies emitter configuration and resizes/resets the underlying particle pool.', ['particles', 'configuration'], 'simple', true);
F(ps, 'Render', [244, 246], 'Forwards alive emitters to the GPU billboard renderer.', ['particles', 'draw'], 'simple', true);
F(ps, 'Stop', [248, 251], 'Stops emission and optionally marks the emitter to destroy when empty.', ['particles', 'lifecycle'], 'simple', true);
F(ps, 'Emit', [253, 286], 'Spawns particles from the emitter shape using random position, direction, and lifetime ranges.', ['particles', 'spawn', 'emission'], 'moderate', true);
F(ps, 'RandomShapePosition', [288, 307], 'Samples a spawn position inside the configured sphere, cone, box, or edge shape.', ['particles', 'math', 'random'], 'moderate', true);
F(ps, 'RandomDirection', [309, 322], 'Samples an emission direction for the current shape, falling back to up if degenerate.', ['particles', 'math', 'random'], 'simple', true);
F(ps, 'Initialize', [358, 360], 'Initializes the ParticleSystem singleton by creating the GPU billboard renderer.', ['particles', 'initialization', 'singleton'], 'simple', true);
F(ps, 'Finalize', [362, 365], 'Resets emitters and releases particle renderer GPU resources.', ['particles', 'shutdown'], 'simple', true);
F(ps, 'CreateEmitter', [393, 397], 'Allocates a new ParticleEmitter and returns its id.', ['particles', 'factory', 'api-handler'], 'simple', true);
F(ps, 'SetEmitterPosition', [399, 402], 'Moves a live emitter to a world position.', ['particles', 'api-handler'], 'simple', true);
F(ps, 'SetEmitterEmitting', [404, 407], 'Toggles whether a named emitter is currently emitting.', ['particles', 'api-handler'], 'simple', true);
F(ps, 'StopEmitter', [409, 412], 'Stops a named emitter, optionally destroying it when empty.', ['particles', 'api-handler'], 'simple', true);
F(ps, 'DestroyEmitter', [414, 421], 'Removes an emitter from the system immediately.', ['particles', 'api-handler'], 'simple', true);
F(ps, 'SetEmitterConfig', [423, 426], 'Replaces the configuration of an existing emitter.', ['particles', 'configuration', 'api-handler'], 'simple', true);
F(ps, 'SpawnParticle', [428, 452], 'Manually injects one particle into a named emitter pool.', ['particles', 'spawn', 'api-handler'], 'moderate', true);
F(ps, 'ParticleRenderer_Initialize', [462, 555], 'Creates particle billboard shaders, input layout, instance buffers, and blend states.', ['particles', 'initialization', 'd3d11'], 'complex', true);
F(ps, 'ParticleRenderer_Finalize', [557, 572], 'Releases particle renderer shaders, buffers, and blend states.', ['particles', 'shutdown', 'd3d11'], 'simple', true);
F(ps, 'ParticleRenderer_RenderEmitter', [574, 708], 'Uploads instance data for an emitter and draws camera-facing billboards with the emitter texture.', ['particles', 'draw', 'gpu-instancing'], 'complex', true);

const ls = 'engine/rendering/sprite/line_sprite.cpp';
F(ls, 'LineSprite_Initialize', [36, 68], 'Creates the line-sprite vertex/index buffers and a no-cull rasterizer state.', ['initialization', 'sprite', 'd3d11'], 'moderate', true);
F(ls, 'LineSprite_Finalize', [70, 85], 'Releases line-sprite GPU buffers and rasterizer state.', ['shutdown', 'sprite'], 'simple', true);
F(ls, 'LineSprite_Draw', [87, 182], 'Builds a camera-facing quad between two world points and draws it with the white texture and tint.', ['sprite', 'billboard', 'draw'], 'complex', true);

const sp = 'engine/rendering/sprite/sprite.cpp';
F(sp, 'Sprite_Initialize', [46, 72], 'Creates the 2D sprite vertex buffer and a larger radial-fill vertex/index pair.', ['initialization', 'sprite', 'd3d11'], 'moderate', true);
F(sp, 'Sprite_Finalize', [74, 79], 'Releases sprite and radial-fill GPU buffers.', ['shutdown', 'sprite'], 'simple', true);
F(sp, 'Sprite_Draw', [92, 169], 'Draws a textured 2D quad with optional atlas UVs, rotation, and color; overloads forward here.', ['sprite', 'draw', '2d'], 'complex', true);
F(sp, 'Sprite_DrawRadialFill', [171, 383], 'Draws a circular-progress sprite by uploading a triangle fan clipped to fill amount and origin.', ['sprite', 'radial-fill', 'draw'], 'complex', true);

const tx = 'engine/rendering/sprite/texture.cpp';
F(tx, 'Texture_Initialize', [35, 39], 'Stores the D3D11 device and context used by later texture loads.', ['initialization', 'texture'], 'simple', true);
F(tx, 'Texture_Finalize', [50, 53], 'Releases all 2D textures and cubemaps held by the manager.', ['shutdown', 'texture'], 'simple', true);
F(tx, 'TextureLoad', [55, 107], 'Loads or reuses a texture by wide path via DirectXTex and returns a slot id.', ['texture-loader', 'cache', 'd3d11'], 'moderate', true);
F(tx, 'TextureCreateFromRGBA', [109, 151], 'Creates a texture slot from raw RGBA bytes, used for baked font atlases.', ['texture-loader', 'font-atlas'], 'moderate', true);
F(tx, 'TextureGetWhiteTexture', [153, 191], 'Lazily creates and returns the cached 1x1 white texture id.', ['texture', 'fallback', 'cache'], 'moderate', true);
F(tx, 'TextureAllRelease', [193, 199], 'Releases every 2D texture slot and clears the UI font cache.', ['texture', 'shutdown'], 'simple', true);
F(tx, 'TextureSetTexture', [201, 208], 'Binds a texture slot SRV to pixel-shader slot 0, or unbinds if invalid.', ['texture', 'bind', 'd3d11'], 'simple', true);
F(tx, 'TextureGetSize', [210, 213], 'Returns the width and height of a texture slot.', ['texture', 'query'], 'simple', true);
F(tx, 'CubemapLoad', [235, 267], 'Loads or returns a cached DDS cubemap SRV for skybox rendering.', ['cubemap', 'texture-loader', 'dds'], 'moderate', true);
F(tx, 'CubemapReleaseAll', [268, 273], 'Releases every cached cubemap SRV.', ['cubemap', 'shutdown'], 'simple', true);

const sc = 'engine/ui/scene_canvas.cpp';
F(sc, 'EnsureDefaultButtonActions', [35, 57], 'Registers the built-in LoadScene button action that requests a scene file from the canvas.', ['ui', 'button-action', 'registry'], 'moderate', true);
F(sc, 'AnchorFromString', [59, 69], 'Parses a JSON anchor name into a UIAnchor enum.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'AnchorToString', [71, 84], 'Serializes a UIAnchor enum back to its JSON name.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'TextAlignFromString', [86, 90], 'Parses a text-align string used by UIText elements.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'TextAlignToString', [92, 99], 'Serializes a text-align value for canvas JSON.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'ReadFloat2', [101, 106], 'Reads an XMFLOAT2 from a JSON array.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'WriteFloat2', [108, 110], 'Writes an XMFLOAT2 as a JSON array.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'ReadColor', [112, 120], 'Reads an RGBA color from JSON with defaults.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'WriteColor', [122, 124], 'Writes an RGBA color as a JSON array.', ['ui', 'serialization', 'utility'], 'simple', true);
F(sc, 'NormalizeSceneFile', [126, 140], 'Normalizes a requested scene path so canvas buttons can load it consistently.', ['ui', 'path', 'utility'], 'simple', true);
F(sc, 'ApplyCommon', [142, 145], 'Applies shared JSON fields such as visibility to a freshly loaded UI element.', ['ui', 'deserialization'], 'simple', true);
F(sc, 'LoadElementScript', [147, 152], 'Attaches a canvas script to an element from JSON.', ['ui', 'scripting', 'deserialization'], 'simple', true);
F(sc, 'AppendElementScript', [154, 158], 'Writes an element script block back into serialized JSON.', ['ui', 'scripting', 'serialization'], 'simple', true);
F(sc, 'ReadOnClickScript', [160, 169], 'Reads a button onClick script payload from JSON.', ['ui', 'button', 'scripting'], 'simple', true);
F(sc, 'BindSceneHudElements', [171, 180], 'Rebinds well-known HUD pointers (score, timer, red overlay) after canvas reload.', ['ui', 'hud', 'binding'], 'simple', true);
F(sc, 'LoadSpriteElement', [182, 198], 'Deserializes a UISprite from canvas JSON including texture path and color.', ['ui', 'sprite', 'deserialization'], 'simple', true);
F(sc, 'LoadTextElement', [200, 221], 'Deserializes a UIText element including font, align, and color.', ['ui', 'text', 'deserialization'], 'moderate', true);
F(sc, 'LoadButtonElement', [223, 267], 'Deserializes a UIButton with state textures, action, and onClick script.', ['ui', 'button', 'deserialization'], 'moderate', true);
F(sc, 'LoadBarElement', [269, 296], 'Deserializes a UIBar including fill/background textures and value range.', ['ui', 'progress-bar', 'deserialization'], 'moderate', true);
F(sc, 'SerializeCommon', [298, 306], 'Writes shared element fields (name, anchor, position) into JSON.', ['ui', 'serialization'], 'simple', true);
F(sc, 'SerializeElementJson', [308, 387], 'Serializes a live UIElement (sprite/text/button/bar) to a JSON object for save/undo.', ['ui', 'serialization', 'undo'], 'complex', true);
F(sc, 'SceneCanvas_Initialize', [390, 397], 'Creates the shared UICanvas, registers default button actions, and binds HUD elements.', ['ui', 'initialization', 'canvas'], 'simple', true);
F(sc, 'SceneCanvas_Finalize', [399, 410], 'Destroys the shared canvas and clears HUD element pointers.', ['ui', 'shutdown', 'canvas'], 'simple', true);
F(sc, 'SceneCanvas_Update', [412, 416], 'Ticks the shared scene canvas.', ['ui', 'update', 'canvas'], 'simple', true);
F(sc, 'SceneCanvas_Draw', [418, 422], 'Draws the shared scene canvas.', ['ui', 'draw', 'canvas'], 'simple', true);
F(sc, 'SceneCanvas_ClearSceneCanvas', [429, 435], 'Removes all canvas elements and rebinds HUD pointers.', ['ui', 'canvas', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_LoadSceneCanvas', [437, 472], 'Rebuilds the canvas from a JSON document by dispatching per-type loaders.', ['ui', 'deserialization', 'api-handler'], 'moderate', true);
F(sc, 'SceneCanvas_SaveSceneCanvas', [474, 488], 'Serializes the current canvas element list to JSON.', ['ui', 'serialization', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_SerializeElement', [490, 493], 'Serializes one element to a JSON string for editor undo.', ['ui', 'serialization', 'undo'], 'simple', true);
F(sc, 'SceneCanvas_GetElementIndex', [534, 538], 'Returns the index of an element pointer in the canvas list, or -1.', ['ui', 'query', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_RemoveElementInternal', [540, 545], 'Removes an element and refreshes HUD bindings.', ['ui', 'canvas', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_BindButtonAction', [547, 558], 'Applies a registered action binder to a button by its action name.', ['ui', 'button-action', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_RegisterButtonAction', [560, 564], 'Registers or replaces a named button-action binder.', ['ui', 'button-action', 'registry'], 'simple', true);
F(sc, 'SceneCanvas_RequestSceneFile', [566, 570], 'Queues a scene file (and optional play-on-load) for the next consume.', ['ui', 'scene-load', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_ConsumeRequestedSceneFile', [572, 580], 'Returns and clears the pending scene-file request.', ['ui', 'scene-load', 'api-handler'], 'simple', true);
F(sc, 'SceneCanvas_ShowRedOverlay', [582, 587], 'Shows the full-screen red respawn overlay sprite.', ['ui', 'hud', 'overlay'], 'simple', true);
F(sc, 'SceneCanvas_HideRedOverlay', [589, 594], 'Hides the red respawn overlay.', ['ui', 'hud', 'overlay'], 'simple', true);
F(sc, 'SceneCanvas_SetRedOverlayAlpha', [596, 601], 'Sets the red overlay sprite alpha.', ['ui', 'hud', 'overlay'], 'simple', true);
F(sc, 'SceneCanvas_SetTimerSeconds', [603, 614], 'Updates the top-middle scene timer text from a seconds value.', ['ui', 'hud', 'timer'], 'simple', true);

const bar = 'engine/ui/ui_bar.cpp';
F(bar, 'UIBar', [15, 36], 'Constructs a UIBar with default colors, size, and optional label.', ['ui', 'constructor', 'progress-bar'], 'moderate', true);
F(bar, 'Initialize', [38, 48], 'Creates the bar text renderer and loads any configured textures.', ['ui', 'initialization'], 'simple', true);
F(bar, 'Update', [50, 55], 'Invokes the optional onUpdate callback each tick.', ['ui', 'update'], 'simple', true);
F(bar, 'Draw', [57, 122], 'Draws background/fill sprites and optional label/value text at the anchored screen rect.', ['ui', 'draw', 'progress-bar'], 'moderate', true);
F(bar, 'GetScreenRect', [124, 145], 'Computes the bar hit/layout rectangle from anchor and design size.', ['ui', 'layout'], 'moderate', true);
F(bar, 'Finalize', [147, 153], 'Releases the bar text renderer.', ['ui', 'shutdown'], 'simple', true);
F(bar, 'SetBackgroundTexturePath', [155, 164], 'Loads a background texture from a UTF-8 path via TextureLoad.', ['ui', 'texture', 'api-handler'], 'simple', true);
F(bar, 'SetFillTexturePath', [166, 175], 'Loads a fill texture from a UTF-8 path via TextureLoad.', ['ui', 'texture', 'api-handler'], 'simple', true);

const btn = 'engine/ui/ui_button.cpp';
F(btn, 'UIButton', [24, 51], 'Constructs a UIButton with default colors, navigation links, and unset textures.', ['ui', 'constructor', 'button'], 'moderate', true);
F(btn, 'Initialize', [53, 55], 'No-op initialize hook required by UIElement.', ['ui', 'initialization'], 'simple', true);
F(btn, 'Update', [57, 79], 'Updates hover/press from the mouse and activates the button on click.', ['ui', 'update', 'event-handler'], 'moderate', true);
F(btn, 'SetVisible', [81, 88], 'Shows or hides the button and clears hover/press when hidden.', ['ui', 'visibility'], 'simple', true);
F(btn, 'Draw', [90, 143], 'Draws the state texture (or white fallback) and centered label text.', ['ui', 'draw', 'button'], 'moderate', true);
F(btn, 'Finalize', [145, 151], 'Releases button-owned resources.', ['ui', 'shutdown'], 'simple', true);
F(btn, 'SetTexture', [153, 155], 'Assigns the normal-state texture id or loads it from a path.', ['ui', 'texture'], 'simple', true);
F(btn, 'SetTexturePath', [157, 166], 'Stores and loads the normal-state texture from a UTF-8 path.', ['ui', 'texture', 'api-handler'], 'simple', true);
F(btn, 'SetHoverTexture', [168, 170], 'Assigns the hover-state texture id.', ['ui', 'texture'], 'simple', true);
F(btn, 'SetHoverTexturePath', [172, 181], 'Loads the hover-state texture from a UTF-8 path.', ['ui', 'texture', 'api-handler'], 'simple', true);
F(btn, 'SetPressedTexture', [183, 185], 'Assigns the pressed-state texture id.', ['ui', 'texture'], 'simple', true);
F(btn, 'SetPressedTexturePath', [187, 196], 'Loads the pressed-state texture from a UTF-8 path.', ['ui', 'texture', 'api-handler'], 'simple', true);
F(btn, 'SetAction', [198, 202], 'Stores the button action name, target scene, and play-on-load flag.', ['ui', 'button-action'], 'simple', true);
F(btn, 'Activate', [204, 221], 'Fires the click callback, plays the UI click sound, and runs the bound action.', ['ui', 'event-handler', 'audio'], 'simple', true);
F(btn, 'GetScreenRect', [223, 237], 'Computes the button rectangle from anchor and size.', ['ui', 'layout'], 'simple', true);
F(btn, 'ContainsPoint', [239, 241], 'Hit-tests a point against the button screen rect.', ['ui', 'hit-test'], 'simple', true);
F(btn, 'SetFont', [243, 249], 'Configures the shared button UITextRenderer from a font texture or TTF path.', ['ui', 'font'], 'simple', true);
F(btn, 'SetFontPath', [251, 256], 'UTF-8 wrapper that forwards to SetFont.', ['ui', 'font', 'api-handler'], 'simple', true);
F(btn, 'ShutdownSharedFont', [258, 262], 'Finalizes the process-wide button text renderer.', ['ui', 'font', 'shutdown'], 'simple', true);

const fontCpp = 'engine/ui/ui_font.cpp';
F(fontCpp, 'EndsWithIgnoreCase', [24, 35], 'Case-insensitive suffix test used to detect .ttf/.otf paths (wide and UTF-8 overloads).', ['utility', 'string', 'font'], 'simple', true);
F(fontCpp, 'LoadFile', [51, 68], 'Reads an entire font file into a byte buffer for stb_truetype.', ['io', 'font'], 'simple', true);
F(fontCpp, 'BakeFont', [70, 124], 'Bakes ASCII glyphs into a 1024 atlas and uploads it with TextureCreateFromRGBA.', ['font', 'texture-atlas', 'stb-truetype'], 'moderate', true);
F(fontCpp, 'UIFont_IsTrueTypePath', [127, 132], 'Returns true when a path ends with .ttf or .otf.', ['font', 'validation', 'api-handler'], 'simple', true);
F(fontCpp, 'UIFont_Get', [138, 159], 'Returns a cached UIFont, baking the TTF on first use or showing an error dialog on failure.', ['font', 'cache', 'api-handler'], 'moderate', true);
F(fontCpp, 'UIFont_ClearCache', [161, 163], 'Clears all baked UIFont entries.', ['font', 'cache', 'shutdown'], 'simple', true);

C('engine/ui/ui_font.h', 'UIGlyph', [5, 13], 'Per-glyph atlas rectangle and advance metrics for baked UI fonts.', ['font', 'data-model', 'glyph'], 'simple', true);
C('engine/ui/ui_font.h', 'UIFont', [15, 24], 'Baked font atlas handle: texture id, metrics, and a 95-glyph ASCII table.', ['font', 'data-model', 'atlas'], 'simple', true);

const us = 'engine/ui/ui_sprite.cpp';
F(us, 'UISprite', [13, 34], 'Constructs a UISprite with default tint, rotation, and no source-rect/fill.', ['ui', 'constructor', 'sprite'], 'moderate', true);
F(us, 'Initialize', [36, 38], 'No-op initialize; texture is assigned later via SetTexture.', ['ui', 'initialization'], 'simple', true);
F(us, 'Update', [40, 42], 'No-op per-frame update for static sprites.', ['ui', 'update'], 'simple', true);
F(us, 'Draw', [44, 106], 'Draws the sprite with optional atlas rect or Radial360 fill using Sprite_Draw*.', ['ui', 'draw', 'radial-fill'], 'moderate', true);
F(us, 'Finalize', [108, 110], 'No-op finalize hook.', ['ui', 'shutdown'], 'simple', true);
F(us, 'GetScreenRect', [112, 126], 'Computes the sprite rectangle from anchor and size.', ['ui', 'layout'], 'simple', true);
F(us, 'SetTexture', [128, 130], 'Assigns a texture id or loads one from a path.', ['ui', 'texture'], 'simple', true);
F(us, 'SetTexturePath', [132, 141], 'Stores and loads the sprite texture from a UTF-8 path.', ['ui', 'texture', 'api-handler'], 'simple', true);
F(us, 'SetSourceRect', [143, 149], 'Enables an atlas source rectangle in texture pixels.', ['ui', 'atlas'], 'simple', true);
F(us, 'ClearSourceRect', [151, 157], 'Clears the atlas rect so the full texture is drawn.', ['ui', 'atlas'], 'simple', true);

const tr = 'engine/ui/ui_text_renderer.cpp';
F(tr, 'UITextRenderer', [11, 20], 'Constructs a UITextRenderer with legacy grid-font defaults and no TTF bound.', ['ui', 'constructor', 'text'], 'simple', true);
F(tr, 'DrawScale', [22, 26], 'Scales TTF draw size so baked 64px glyphs match the legacy 32px cell.', ['ui', 'font', 'layout'], 'simple', true);
F(tr, 'Initialize', [34, 58], 'Loads either a TTF via UIFont_Get or a grid font texture via TextureLoad.', ['ui', 'initialization', 'font'], 'moderate', true);
F(tr, 'Draw', [60, 135], 'Draws a string with TTF glyph quads or a monospaced grid font via Sprite_Draw.', ['ui', 'text', 'draw'], 'complex', true);
F(tr, 'GetTextWidth', [137, 177], 'Measures string width for TTF advances or grid-cell spacing.', ['ui', 'layout', 'text'], 'moderate', true);
F(tr, 'GetLineHeight', [179, 183], 'Returns scaled line height from the TTF or grid cell height.', ['ui', 'layout', 'text'], 'simple', true);
F(tr, 'Finalize', [185, 189], 'Clears the renderer font/texture binding.', ['ui', 'shutdown'], 'simple', true);
C('engine/ui/ui_text_renderer.h', 'UITextRenderer', [7, 41], 'Text renderer that can draw baked TTF atlases or a legacy monospaced font texture.', ['ui', 'text', 'component', 'type-definition'], 'moderate', true);

const main = 'src/app/main.cpp';
F(main, 'WinMain', [99, 247], 'Windows entry point: DPI-aware window creation, subsystem init, and the 60 Hz message/game loop.', ['entry-point', 'windows', 'game-loop'], 'complex', true);
F(main, 'Initialize', [252, 292], 'Boots renderer, shaders, sprites, input, jobs, audio, fade, post-process, effects, and the first GameScene.', ['initialization', 'bootstrap'], 'moderate', true);
F(main, 'Finalize', [297, 318], 'Tears down the scene and all subsystems in reverse dependency order before the D3D device dies.', ['shutdown', 'lifecycle'], 'moderate', true);
F(main, 'Update', [323, 388], 'Ticks input, debug time-scale, effects, fade, post-process, scene, and audio at a scaled fixed timestep.', ['update', 'game-loop'], 'moderate', true);
F(main, 'Draw', [393, 463], 'Clears, draws the scene, fade overlay, debug text, and presents the back buffer.', ['draw', 'game-loop'], 'complex', true);
F(main, 'WndProc', [468, 576], 'Window procedure handling resize/move, DPI, ImGui input, and quit.', ['windows', 'event-handler'], 'complex', true);

const fade = 'src/app/rendering/fade.cpp';
F(fade, 'Fade_Initialize', [21, 31], 'Resets fade state and loads the full-screen fade texture.', ['fade', 'initialization'], 'simple', true);
F(fade, 'Fade_Finalize', [33, 35], 'Unbinds the fade texture slot.', ['fade', 'shutdown'], 'simple', true);
F(fade, 'Fade_Update', [37, 64], 'Advances fade-out/in alpha and changes scene when fade-out completes.', ['fade', 'update', 'scene-transition'], 'moderate', true);
F(fade, 'Fade_Draw', [66, 72], 'Draws the full-screen fade sprite at the current alpha.', ['fade', 'draw'], 'simple', true);
F(fade, 'Fade_SetOut', [75, 85], 'Starts a fade-out with color, speed, and optional scene change.', ['fade', 'api-handler'], 'simple', true);
F(fade, 'Fade_SetIn', [88, 95], 'Starts a fade-in from full alpha.', ['fade', 'api-handler'], 'simple', true);
F(fade, 'Fade_SetNextScene', [98, 100], 'Stores the SceneBase to switch to at the end of fade-out.', ['fade', 'scene-transition'], 'simple', true);
F(fade, 'Fade_IsFadeEnd', [103, 103], 'True when no fade is active.', ['fade', 'query'], 'simple', true);
F(fade, 'Fade_IsOutEnd', [104, 104], 'True when fade-out has reached full alpha and fade-in is about to start.', ['fade', 'query'], 'simple', true);
F(fade, 'Fade_IsInEnd', [105, 105], 'True when fade-in has finished.', ['fade', 'query'], 'simple', true);
F(fade, 'Fade_IsFading', [106, 106], 'True while a fade is in progress.', ['fade', 'query'], 'simple', true);

for (const { node } of fnNodes) nodes.push(node);
for (const { node } of classNodes) nodes.push(node);

const edges = [];

// contains + exports
for (const { node, isExport } of [...fnNodes, ...classNodes]) {
  edges.push(contains(node.filePath, node.id));
  if (isExport) edges.push(exportsE(node.filePath, node.id));
}

// imports 1:1
let expectedImports = 0;
for (const [from, tos] of Object.entries(importData)) {
  expectedImports += tos.length;
  for (const to of tos) edges.push(importsE(from, to));
}

const f = (path, name) => `function:${path}:${name}`;

// cross-file calls
edges.push(callsE(f(sky, 'Skybox_ReloadFromSettings'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(sky, 'Skybox_ReloadFromSettings'), f(tx, 'CubemapLoad')));
edges.push(callsE(f(ps, 'ParticleRenderer_RenderEmitter'), f(tx, 'TextureGetWhiteTexture')));
edges.push(callsE(f(ls, 'LineSprite_Draw'), f(tx, 'TextureGetWhiteTexture')));
edges.push(callsE(f(ls, 'LineSprite_Draw'), f(tx, 'TextureSetTexture')));
edges.push(callsE(f(sp, 'Sprite_Draw'), f(tx, 'TextureGetSize')));
edges.push(callsE(f(sp, 'Sprite_Draw'), f(tx, 'TextureSetTexture')));
edges.push(callsE(f(sp, 'Sprite_DrawRadialFill'), f(tx, 'TextureSetTexture')));
edges.push(callsE(f(tx, 'TextureAllRelease'), f(fontCpp, 'UIFont_ClearCache')));

edges.push(callsE(f(sc, 'LoadSpriteElement'), f(us, 'SetTexturePath')));
edges.push(callsE(f(sc, 'LoadButtonElement'), f(btn, 'SetTexturePath')));
edges.push(callsE(f(sc, 'LoadButtonElement'), f(btn, 'SetHoverTexturePath')));
edges.push(callsE(f(sc, 'LoadButtonElement'), f(btn, 'SetPressedTexturePath')));
edges.push(callsE(f(sc, 'LoadButtonElement'), f(btn, 'SetAction')));
edges.push(callsE(f(sc, 'LoadBarElement'), f(bar, 'SetFillTexturePath')));
edges.push(callsE(f(sc, 'LoadBarElement'), f(bar, 'SetBackgroundTexturePath')));

edges.push(callsE(f(bar, 'Draw'), f(tx, 'TextureGetWhiteTexture')));
edges.push(callsE(f(bar, 'Draw'), f(sp, 'Sprite_Draw')));
edges.push(callsE(f(bar, 'SetBackgroundTexturePath'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(bar, 'SetBackgroundTexturePath'), f(tx, 'TextureLoad')));
edges.push(callsE(f(bar, 'SetFillTexturePath'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(bar, 'SetFillTexturePath'), f(tx, 'TextureLoad')));

edges.push(callsE(f(btn, 'Draw'), f(tx, 'TextureGetWhiteTexture')));
edges.push(callsE(f(btn, 'Draw'), f(tx, 'TextureGetSize')));
edges.push(callsE(f(btn, 'Draw'), f(sp, 'Sprite_Draw')));
edges.push(callsE(f(btn, 'Draw'), f(tr, 'GetTextWidth')));
edges.push(callsE(f(btn, 'Draw'), f(tr, 'GetLineHeight')));
edges.push(callsE(f(btn, 'SetTexture'), f(tx, 'TextureLoad')));
edges.push(callsE(f(btn, 'SetTexturePath'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(btn, 'SetHoverTexture'), f(tx, 'TextureLoad')));
edges.push(callsE(f(btn, 'SetHoverTexturePath'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(btn, 'SetPressedTexture'), f(tx, 'TextureLoad')));
edges.push(callsE(f(btn, 'SetPressedTexturePath'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(btn, 'SetFont'), f(tr, 'Initialize')));
edges.push(callsE(f(btn, 'SetFontPath'), 'function:engine/ecs/string_utils.h:ToWString'));
edges.push(callsE(f(btn, 'ShutdownSharedFont'), f(tr, 'Finalize')));

edges.push(callsE(f(fontCpp, 'BakeFont'), f(tx, 'TextureCreateFromRGBA')));

edges.push(callsE(f(us, 'Draw'), f(tx, 'TextureGetWhiteTexture')));
edges.push(callsE(f(us, 'Draw'), f(tx, 'TextureGetSize')));
edges.push(callsE(f(us, 'Draw'), f(sp, 'Sprite_DrawRadialFill')));
edges.push(callsE(f(us, 'Draw'), f(sp, 'Sprite_Draw')));
edges.push(callsE(f(us, 'SetTexture'), f(tx, 'TextureLoad')));
edges.push(callsE(f(us, 'SetTexturePath'), 'function:engine/ecs/string_utils.h:ToWString'));

edges.push(callsE(f(tr, 'Initialize'), f(fontCpp, 'UIFont_IsTrueTypePath')));
edges.push(callsE(f(tr, 'Initialize'), f(fontCpp, 'UIFont_Get')));
edges.push(callsE(f(tr, 'Initialize'), f(tx, 'TextureLoad')));
edges.push(callsE(f(tr, 'Draw'), f(sp, 'Sprite_Draw')));

edges.push(callsE(f(main, 'Initialize'), f(tx, 'Texture_Initialize')));
edges.push(callsE(f(main, 'Initialize'), f(sp, 'Sprite_Initialize')));
edges.push(callsE(f(main, 'Initialize'), f(ls, 'LineSprite_Initialize')));
edges.push(callsE(f(main, 'Initialize'), f(fade, 'Fade_Initialize')));
edges.push(callsE(f(main, 'Finalize'), f(fade, 'Fade_Finalize')));
edges.push(callsE(f(main, 'Finalize'), f(ls, 'LineSprite_Finalize')));
edges.push(callsE(f(main, 'Finalize'), f(sp, 'Sprite_Finalize')));
edges.push(callsE(f(main, 'Finalize'), f(tx, 'Texture_Finalize')));
edges.push(callsE(f(main, 'Finalize'), f(mu, 'ModelUtil_ReleaseTextureCache')));
edges.push(callsE(f(main, 'Update'), f(fade, 'Fade_Update')));
edges.push(callsE(f(main, 'Update'), f(fade, 'Fade_IsFadeEnd')));
edges.push(callsE(f(main, 'Draw'), f(fade, 'Fade_Draw')));

edges.push(callsE(f(fade, 'Fade_Initialize'), f(tx, 'TextureLoad')));
edges.push(callsE(f(fade, 'Fade_Finalize'), f(tx, 'TextureSetTexture')));
edges.push(callsE(f(fade, 'Fade_Draw'), f(sp, 'Sprite_Draw')));

// validate
const ids = new Set();
for (const n of nodes) {
  if (!n || !n.id) throw new Error('bad node ' + JSON.stringify(n));
  if (ids.has(n.id)) throw new Error('duplicate node ' + n.id);
  ids.add(n.id);
}
for (const e of edges) {
  if (e.source === e.target) throw new Error('self edge ' + e.source);
}

const importCount = edges.filter((e) => e.type === 'imports').length;
if (importCount !== expectedImports) throw new Error(`expected ${expectedImports} imports, got ${importCount}`);

const filesAlpha = [...new Set(nodes.map((n) => n.filePath))].sort();
console.log('files', filesAlpha.length);
console.log('nodes', nodes.length, 'edges', edges.length, 'imports', importCount);

const parts = Math.max(1, Math.ceil(Math.max(nodes.length / 60, edges.length / 120)));
const chunk = Math.ceil(filesAlpha.length / parts);
const groups = [];
for (let i = 0; i < parts; i++) {
  const g = filesAlpha.slice(i * chunk, (i + 1) * chunk);
  if (g.length) groups.push(g);
}

const neighborOrImport = new Set();
for (const [from, tos] of Object.entries(importData)) {
  neighborOrImport.add(from);
  for (const to of tos) neighborOrImport.add(to);
}

const neighborSymbols = new Set([
  'function:engine/ecs/string_utils.h:ToWString',
]);

const outDir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';
mkdirSync(outDir, { recursive: true });

const sameBatchIds = new Set(nodes.map((n) => n.id));
const sameBatchFiles = new Set(filesAlpha);

let totalNodes = 0;
let totalEdges = 0;
const actualParts = groups.length;
groups.forEach((group, idx) => {
  const groupSet = new Set(group);
  const partNodes = nodes.filter((n) => groupSet.has(n.filePath));
  const partIds = new Set(partNodes.map((n) => n.id));
  const partEdges = edges.filter((e) => partIds.has(e.source));

  for (const e of partEdges) {
    const fileTarget = e.target.startsWith('file:') ? e.target.slice(5) : null;
    const ok =
      partIds.has(e.target) ||
      sameBatchIds.has(e.target) ||
      (fileTarget && (neighborOrImport.has(fileTarget) || sameBatchFiles.has(fileTarget))) ||
      neighborSymbols.has(e.target);
    if (!ok) throw new Error(`part ${idx + 1} invalid target ${e.target} from ${e.source}`);
  }

  const part = { nodes: partNodes, edges: partEdges };
  const name = actualParts === 1 ? 'batch-6.json' : `batch-6-part-${idx + 1}.json`;
  writeFileSync(`${outDir}/${name}`, JSON.stringify(part, null, 2));
  totalNodes += partNodes.length;
  totalEdges += partEdges.length;
  console.log(name, 'nodes', partNodes.length, 'edges', partEdges.length, 'files', group.length);
});

console.log('TOTAL nodes', totalNodes, 'edges', totalEdges, 'parts', actualParts);
