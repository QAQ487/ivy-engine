const fs = require('fs');
const outDir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';

function n(id, type, name, filePath, summary, tags, complexity, extra = {}) {
  return { id, type, name, filePath, summary, tags, complexity, ...extra };
}
function fn(path, name, lineRange, summary, tags, complexity) {
  return n('function:' + path + ':' + name, 'function', name, path, summary, tags, complexity, { lineRange });
}
function e(source, target, type, weight) {
  return { source, target, type, direction: 'forward', weight };
}

const files = [
  n('file:engine/core/job_system.cpp', 'file', 'job_system.cpp', 'engine/core/job_system.cpp',
    'Implements a frame-scoped worker-pool scheduler with a dependency graph, Win32 wait objects, sequential fallback, and an ImGui timeline overlay.',
    ['job-system', 'worker-pool', 'concurrency', 'runtime'], 'complex',
    { languageNotes: 'Worker threads wait on a semaphore plus a shutdown event; JobHandles are invalidated by JobSystem_WaitAll at the frame fence.' }),
  n('file:engine/core/job_system.h', 'file', 'job_system.h', 'engine/core/job_system.h',
    'Public API for the frame-level job scheduler: submit with dependencies, wait fences, parallel toggle, and editor-only debug overlay hooks.',
    ['job-system', 'api-header', 'concurrency', 'type-definition'], 'simple'),
  n('file:engine/core/system_timer.h', 'file', 'system_timer.h', 'engine/core/system_timer.h',
    'Declares high-resolution system timer controls for start, stop, advance, elapsed time, and thread-affinity pinning used by the main loop and shaders.',
    ['timer', 'runtime', 'utility', 'api-header'], 'simple'),
  n('file:engine/debug_draw/debug_ofstream.cpp', 'file', 'debug_ofstream.cpp', 'engine/debug_draw/debug_ofstream.cpp',
    'Defines the process-wide hal::dout debug stream instance that forwards formatted text to the Windows debugger.',
    ['debug', 'logging', 'utility'], 'simple'),
  n('file:engine/debug_draw/debug_ofstream.h', 'file', 'debug_ofstream.h', 'engine/debug_draw/debug_ofstream.h',
    'Custom std::ostream adapter whose streambuf syncs to OutputDebugStringA so engine code can log with stream syntax.',
    ['debug', 'logging', 'utility', 'api-header'], 'simple',
    { languageNotes: 'debugbuf overrides basic_stringbuf::sync to flush into OutputDebugStringA and clear the buffer.' }),
  n('file:engine/debug_draw/debug_text.cpp', 'file', 'debug_text.cpp', 'engine/debug_draw/debug_text.cpp',
    'Implements bitmap-font debug text: WIC texture load, runtime-compiled shaders, wrapping layout, and batched quad drawing.',
    ['debug', 'text-rendering', 'd3d11', 'ui-overlay'], 'complex'),
  n('file:engine/debug_draw/debug_text.h', 'file', 'debug_text.h', 'engine/debug_draw/debug_text.h',
    'Declares hal::DebugText, a D3D11 on-screen bitmap font renderer with shared static pipeline state and per-instance vertex buffers.',
    ['debug', 'text-rendering', 'd3d11', 'api-header'], 'moderate'),
  n('file:engine/ecs/billboard_component.cpp', 'file', 'billboard_component.cpp', 'engine/ecs/billboard_component.cpp',
    'Serializes BillboardComponent fields and walks the scene graph to submit camera-facing sprites into the billboard draw list.',
    ['ecs', 'component', 'billboard', 'serialization'], 'moderate'),
  n('file:engine/ecs/string_utils.h', 'file', 'string_utils.h', 'engine/ecs/string_utils.h',
    'Header-only UTF-8 to wide-string helper shared by asset loaders and Win32/DirectX path APIs.',
    ['utility', 'string', 'encoding', 'header-only'], 'simple'),
  n('file:engine/input/key_logger.cpp', 'file', 'key_logger.cpp', 'engine/input/key_logger.cpp',
    'Tracks per-key edge, hold, repeat, double-tap, and long-press state from the keyboard snapshot using real frame delta.',
    ['input', 'keyboard', 'event-handler', 'runtime'], 'moderate'),
  n('file:engine/input/key_logger.h', 'file', 'key_logger.h', 'engine/input/key_logger.h',
    'Public keyboard-query API for pressed, trigger, release, double-tap, repeat, long-press, and hold-time checks.',
    ['input', 'keyboard', 'api-header'], 'simple'),
  n('file:engine/input/mouse.cpp', 'file', 'mouse.cpp', 'engine/input/mouse.cpp',
    'Processes Win32 mouse messages into button, wheel, client, and backbuffer coordinates plus per-frame movement deltas.',
    ['input', 'mouse', 'event-handler', 'runtime'], 'moderate'),
  n('file:engine/rendering/billboard/billboard.cpp', 'file', 'billboard.cpp', 'engine/rendering/billboard/billboard.cpp',
    'D3D11 camera-facing and ground-aligned quad batcher: loads CSO shaders, sorts by depth, and draws the registered billboard list.',
    ['rendering', 'billboard', 'd3d11', 'batching'], 'complex'),
  n('file:engine/rendering/billboard/billboard.h', 'file', 'billboard.h', 'engine/rendering/billboard/billboard.h',
    'Static Billboard API and BillboardData layout for queued world-space sprites with UV, rotation, and ground-align flags.',
    ['rendering', 'billboard', 'data-model', 'api-header'], 'moderate'),
  n('file:engine/rendering/d3d11/post_process.cpp', 'file', 'post_process.cpp', 'engine/rendering/d3d11/post_process.cpp',
    'Fullscreen post-process pass that composites stackable effects from the offscreen PostRT onto the back buffer, including timed RGB-shift bursts.',
    ['post-process', 'd3d11', 'rendering', 'fullscreen'], 'complex'),
  n('file:engine/rendering/d3d11/post_process.h', 'file', 'post_process.h', 'engine/rendering/d3d11/post_process.h',
    'Declares PostFXParams and the initialize, update, burst, and draw API for the fullscreen post-process stack.',
    ['post-process', 'api-header', 'rendering'], 'simple'),
  n('file:engine/rendering/d3d11/renderer.cpp', 'file', 'renderer.cpp', 'engine/rendering/d3d11/renderer.cpp',
    'Owns the D3D11 device, swap chain, depth buffer, and offscreen PostRT, plus resize, window-mode, viewport, and present.',
    ['renderer', 'd3d11', 'swapchain', 'runtime'], 'complex',
    { languageNotes: 'PostRT is an offscreen color target matching the swap chain so post-process can sample the scene before presenting.' }),
  n('file:engine/rendering/d3d11/renderer.h', 'file', 'renderer.h', 'engine/rendering/d3d11/renderer.h',
    'Public D3D11 renderer surface: device accessors, MATERIAL/VERTEX_3D layouts, viewport helpers, and PostRT bind/resize entry points.',
    ['renderer', 'd3d11', 'api-header', 'data-model'], 'moderate'),
  n('file:engine/rendering/d3d11/shader.cpp', 'file', 'shader.cpp', 'engine/rendering/d3d11/shader.cpp',
    'Loads compiled 2D/3D shaders and uploads WVP, skin palette, material, surface-FX, and cascade-shadow constant buffers.',
    ['shader', 'd3d11', 'gpu-skinning', 'shadows'], 'complex',
    { languageNotes: 'Shader3D variants are selected by id and bind different pixel CSOs; skin and surface-FX use dedicated constant-buffer slots.' }),
  n('file:engine/rendering/effect/effect_system.cpp', 'file', 'effect_system.cpp', 'engine/rendering/effect/effect_system.cpp',
    'Wraps Effekseer DX11: manager setup, cached effect loading, playback handles, and 3D draw using the current camera matrices.',
    ['vfx', 'effekseer', 'rendering', 'service'], 'moderate'),
  n('file:engine/rendering/effect/effect_system.h', 'file', 'effect_system.h', 'engine/rendering/effect/effect_system.h',
    'EffectSystem namespace API for loading, playing, transforming, and drawing Effekseer particle effects.',
    ['vfx', 'effekseer', 'api-header'], 'simple'),
];

const fns = [
  n('class:engine/core/job_system.cpp:JobNode', 'class', 'JobNode', 'engine/core/job_system.cpp',
    'Internal dependency-graph node holding the job functor, pending-dep count, dependents, completion event, and debug index.',
    ['job-system', 'data-model', 'concurrency'], 'simple', { lineRange: [21, 36] }),
  fn('engine/core/job_system.cpp', 'CompleteNode', [88, 109],
    'Marks a finished job complete, signals waiters, decrements dependents, and pushes newly ready nodes onto the worker queue.',
    ['job-system', 'concurrency', 'event-handler'], 'moderate'),
  fn('engine/core/job_system.cpp', 'WorkerMain', [120, 149],
    'Worker-thread loop that waits on the ready semaphore or shutdown event, runs one job, then completes it with QPC timing.',
    ['job-system', 'worker-pool', 'concurrency'], 'moderate'),
  fn('engine/core/job_system.cpp', 'SubmitInternal', [151, 184],
    'Allocates a JobNode, wires dependency edges, and either runs inline in sequential mode or enqueues when all deps are satisfied.',
    ['job-system', 'factory', 'concurrency'], 'moderate'),
  fn('engine/core/job_system.cpp', 'JobSystem_Init', [188, 211],
    'Starts the worker pool, ready semaphore, and shutdown/frame-done events for the requested worker count.',
    ['job-system', 'initialization', 'runtime'], 'moderate'),
  fn('engine/core/job_system.cpp', 'JobSystem_Shutdown', [213, 236],
    'Signals workers to exit, joins threads, recycles frame nodes, and closes Win32 synchronization handles.',
    ['job-system', 'shutdown', 'runtime'], 'moderate'),
  fn('engine/core/job_system.cpp', 'JobSystem_Submit', [238, 240],
    'Public zero-dependency submit that forwards the job to SubmitInternal.',
    ['job-system', 'api-handler', 'entry-point'], 'simple'),
  fn('engine/core/job_system.cpp', 'JobSystem_SubmitDep', [242, 244],
    'Public submit that records explicit same-frame dependency handles before enqueueing.',
    ['job-system', 'api-handler', 'concurrency'], 'simple'),
  fn('engine/core/job_system.cpp', 'JobSystem_SubmitAfter', [246, 251],
    'Convenience wrapper that submits a job depending on a single prior handle.',
    ['job-system', 'api-handler', 'concurrency'], 'simple'),
  fn('engine/core/job_system.cpp', 'JobSystem_WaitFor', [253, 262],
    'Main-thread wait on one node completion event; no-op when sequential or the handle is already done.',
    ['job-system', 'synchronization', 'runtime'], 'simple'),
  fn('engine/core/job_system.cpp', 'JobSystem_WaitAll', [264, 273],
    'Frame fence that blocks until every submitted node finishes, then recycles storage and invalidates handles.',
    ['job-system', 'synchronization', 'runtime'], 'simple'),
  fn('engine/core/job_system.cpp', 'JobSystem_DebugDraw', [311, 413],
    'ImGui overlay showing worker count, outstanding jobs, and a QPC-based per-worker timeline of the last frame.',
    ['job-system', 'debug', 'imgui'], 'complex'),

  fn('engine/debug_draw/debug_text.cpp', 'DebugText', [32, 199],
    'Constructs a DebugText instance: loads or shares the font texture, compiles VS/PS, and creates blend, depth, and raster states.',
    ['debug', 'initialization', 'd3d11'], 'complex'),
  fn('engine/debug_draw/debug_text.cpp', '~DebugText', [201, 212],
    'Releases instance texture references and GPU buffers while leaving shared static pipeline objects alive.',
    ['debug', 'shutdown', 'd3d11'], 'simple'),
  fn('engine/debug_draw/debug_text.cpp', 'SetText', [214, 265],
    'Appends colored text into wrapped line buckets, splitting on newlines and the per-line character cap.',
    ['debug', 'text-layout', 'ui-overlay'], 'moderate'),
  fn('engine/debug_draw/debug_text.cpp', 'Draw', [267, 406],
    'Builds colored glyph quads from queued lines and issues a D3D11 draw with the shared font atlas.',
    ['debug', 'text-rendering', 'd3d11'], 'complex'),
  fn('engine/debug_draw/debug_text.cpp', 'MeasureText', [415, 457],
    'Measures wrapped pixel width and height of a string using the current spacing and max-line rules.',
    ['debug', 'text-layout', 'utility'], 'moderate'),
  fn('engine/debug_draw/debug_text.cpp', 'createBuffer', [459, 477],
    'Allocates or resizes the dynamic vertex and index buffers for the requested glyph capacity.',
    ['debug', 'd3d11', 'buffer'], 'moderate'),

  fn('engine/ecs/billboard_component.cpp', 'Serialize', [30, 45],
    'Writes BillboardComponent texture, size, color, rotation, and flags to a JSON string.',
    ['serialization', 'ecs', 'billboard'], 'simple'),
  fn('engine/ecs/billboard_component.cpp', 'Deserialize', [47, 67],
    'Restores BillboardComponent fields from JSON, including nested color channels, and logs parse failures.',
    ['serialization', 'ecs', 'billboard'], 'moderate'),
  fn('engine/ecs/billboard_component.cpp', 'SubmitNodeBillboards', [69, 88],
    'If the node is active, queues its BillboardComponent via Billboard::Add and recursively visits children.',
    ['billboard', 'ecs', 'scene-graph'], 'moderate'),
  fn('engine/ecs/billboard_component.cpp', 'SubmitSceneBillboards', [90, 94],
    'Entry point that submits billboards for every root game object in the scene.',
    ['billboard', 'ecs', 'entry-point'], 'simple'),

  fn('engine/ecs/string_utils.h', 'ToWString', [13, 22],
    'Converts a UTF-8/ANSI narrow string to std::wstring with MultiByteToWideChar and strips the appended null.',
    ['utility', 'string', 'encoding'], 'simple'),

  fn('engine/input/key_logger.cpp', 'KeyLogger_Initialize', [25, 32],
    'Resets hold and repeat timers and initializes the underlying keyboard device snapshot.',
    ['input', 'keyboard', 'initialization'], 'simple'),
  fn('engine/input/key_logger.cpp', 'KeyLogger_Update', [34, 114],
    'Diffs the current keyboard snapshot against last frame to update trigger, release, hold, repeat, and double-tap bits.',
    ['input', 'keyboard', 'event-handler'], 'complex'),

  fn('engine/input/mouse.cpp', 'Mouse_Initialize', [60, 68],
    'Stores the window handle and seeds client coordinates from the current cursor position.',
    ['input', 'mouse', 'initialization'], 'simple'),
  fn('engine/input/mouse.cpp', 'Mouse_ProcessMessage', [74, 117],
    'Handles WM mouse move, button, and wheel messages, accumulating deltas and updating button/wheel state.',
    ['input', 'mouse', 'event-handler'], 'moderate'),
  fn('engine/input/mouse.cpp', 'Mouse_GetDelta', [119, 129],
    'Returns this frame movement as normalized backbuffer-space deltas using client-to-buffer scale.',
    ['input', 'mouse', 'utility'], 'simple'),

  fn('engine/rendering/billboard/billboard.cpp', 'Initialize', [36, 137],
    'Loads billboard VS/PS CSO files and creates the unit-quad VB, constant buffers, sampler, and raster/depth states.',
    ['billboard', 'initialization', 'd3d11'], 'complex'),
  fn('engine/rendering/billboard/billboard.cpp', 'Finalize', [139, 154],
    'Releases static D3D11 billboard resources and clears the pending draw list.',
    ['billboard', 'shutdown', 'd3d11'], 'simple'),
  fn('engine/rendering/billboard/billboard.cpp', 'Update', [156, 174],
    'Extracts camera right/up from the view matrix and depth-sorts queued billboards back-to-front.',
    ['billboard', 'sorting', 'camera'], 'moderate'),
  fn('engine/rendering/billboard/billboard.cpp', 'Draw', [176, 291],
    'Binds the billboard pipeline and draws each queued sprite with per-instance position, UV, and alignment constants.',
    ['billboard', 'rendering', 'd3d11'], 'complex'),
  fn('engine/rendering/billboard/billboard.cpp', 'Add', [297, 308],
    'Queues a billboard with explicit UV offset/scale, or the simpler overloads that default those fields.',
    ['billboard', 'factory', 'api-handler'], 'simple'),

  fn('engine/rendering/d3d11/post_process.cpp', 'LoadShaderFile', [60, 72],
    'Reads a compiled shader CSO from disk into a byte vector for CreateVertexShader/CreatePixelShader.',
    ['shader', 'io', 'utility'], 'simple'),
  fn('engine/rendering/d3d11/post_process.cpp', 'PostProcess_Initialize', [76, 118],
    'Creates the fullscreen post-process VS/PS, constant buffer, and linear sampler from compiled CSO files.',
    ['post-process', 'initialization', 'd3d11'], 'moderate'),
  fn('engine/rendering/d3d11/post_process.cpp', 'PostProcess_Shutdown', [120, 138],
    'Releases post-process shaders, constant buffer, and sampler state.',
    ['post-process', 'shutdown', 'd3d11'], 'simple'),
  fn('engine/rendering/d3d11/post_process.cpp', 'PostProcess_SetEffects', [145, 155],
    'Stores the full PostFXParams stack (shift, mosaic, posterize, noise, and related knobs) as the base effect state.',
    ['post-process', 'api-handler'], 'simple'),
  fn('engine/rendering/d3d11/post_process.cpp', 'PostProcess_GetEffects', [162, 172],
    'Copies the current base PostFXParams out for editor or gameplay inspection.',
    ['post-process', 'api-handler'], 'simple'),
  fn('engine/rendering/d3d11/post_process.cpp', 'PostProcess_Update', [180, 189],
    'Advances the timed RGB-shift burst and decays it back toward the base effect values.',
    ['post-process', 'animation', 'runtime'], 'simple'),
  fn('engine/rendering/d3d11/post_process.cpp', 'PostProcess_Draw', [191, 254],
    'Samples PostRT and draws a viewport-fitted fullscreen triangle onto the back buffer with the current effect CB.',
    ['post-process', 'rendering', 'd3d11'], 'complex'),

  fn('engine/rendering/d3d11/renderer.cpp', 'CreateDepthBuffer', [50, 81],
    'Creates the D24S8 depth-stencil texture and view for the current back-buffer size.',
    ['renderer', 'd3d11', 'depth'], 'moderate'),
  fn('engine/rendering/d3d11/renderer.cpp', 'CreateRenderTargetFromSwapChain', [83, 99],
    'Obtains the swap-chain back buffer and creates its render-target view.',
    ['renderer', 'd3d11', 'swapchain'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'PostRT_Resize', [107, 150],
    'Recreates the offscreen post-process color target, RTV, and SRV to match the requested size.',
    ['renderer', 'post-process', 'd3d11'], 'moderate'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_BindPostTarget', [168, 179],
    'Binds PostRT plus depth as the OM target and clears both using current render settings.',
    ['renderer', 'post-process', 'd3d11'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'ReadClientSize', [187, 201],
    'Reads the HWND client rectangle into cached width/height, rejecting degenerate sizes.',
    ['renderer', 'windowing', 'utility'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_ClientToBackbuffer', [243, 252],
    'Maps client-pixel coordinates onto back-buffer space using the client-to-buffer scale.',
    ['renderer', 'coordinate-mapping', 'utility'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_BackbufferToClient', [254, 263],
    'Inverse-maps back-buffer coordinates back into client-pixel space.',
    ['renderer', 'coordinate-mapping', 'utility'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_UpdateClientSize', [265, 274],
    'Refreshes cached client dimensions from the window and reports whether they changed.',
    ['renderer', 'windowing', 'runtime'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_ResizeBuffers', [276, 331],
    'Resizes the swap chain, rebuilds RTV/depth/PostRT, and restores the default viewport.',
    ['renderer', 'swapchain', 'resize'], 'complex'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_ResizeToClient', [333, 351],
    'Resizes GPU buffers when the client size no longer matches the back buffer.',
    ['renderer', 'resize', 'windowing'], 'moderate'),
  fn('engine/rendering/d3d11/renderer.cpp', 'Renderer_SetWindowMode', [357, 416],
    'Toggles windowed versus borderless/fullscreen by updating styles, placement, and swap-chain buffers.',
    ['renderer', 'windowing', 'swapchain'], 'complex'),
  fn('engine/rendering/d3d11/renderer.cpp', 'SetRenderViewport', [418, 433],
    'Applies a sub-rectangle RS viewport used by the game view inside the editor chrome.',
    ['renderer', 'viewport', 'd3d11'], 'simple'),
  fn('engine/rendering/d3d11/renderer.cpp', 'InitRenderer', [451, 605],
    'Creates the D3D11 device, swap chain, rasterizer/depth/blend states, and initial back-buffer targets.',
    ['renderer', 'initialization', 'd3d11'], 'complex'),
  fn('engine/rendering/d3d11/renderer.cpp', 'UninitRenderer', [607, 625],
    'Releases device, swap chain, state objects, and offscreen PostRT resources.',
    ['renderer', 'shutdown', 'd3d11'], 'moderate'),

  fn('engine/rendering/d3d11/shader.cpp', 'UpdateShadowCB', [162, 180],
    'Uploads cascade light-view-projection matrices, bias, and split distances to the shadow constant buffer.',
    ['shader', 'shadows', 'constant-buffer'], 'moderate'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_Initialize', [223, 439],
    'Loads 2D/3D vertex and pixel CSOs, creates input layouts, and allocates WVP, skin, material, FX, and shadow CBs.',
    ['shader', 'initialization', 'd3d11'], 'complex'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_Finalize', [441, 474],
    'Releases all shader objects and constant buffers created during Shader_Initialize.',
    ['shader', 'shutdown', 'd3d11'], 'moderate'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_SetProjectionMatrix', [491, 501],
    'Caches the projection matrix and dirties 2D/3D VP constant buffers when the value changes.',
    ['shader', 'camera', 'constant-buffer'], 'simple'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_SetViewMatrix', [502, 512],
    'Caches the view matrix and dirties VP constant buffers when the camera view changes.',
    ['shader', 'camera', 'constant-buffer'], 'simple'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_SetSkinPalette', [524, 544],
    'Uploads the GPU skin-bone palette and enabled/count flags used by the skinned 3D vertex shader.',
    ['shader', 'gpu-skinning', 'constant-buffer'], 'moderate'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_CommitSurfaceFX', [560, 582],
    'Packs scrolling/fresnel surface-FX parameters plus SystemTimer time into PS slot 4.',
    ['shader', 'vfx', 'constant-buffer'], 'moderate'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader_BindShadowMaps', [632, 658],
    'Binds an array of cascade shadow SRVs to the 3D pixel-shader shadow slots.',
    ['shader', 'shadows', 'd3d11'], 'moderate'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader3D_Begin', [712, 761],
    'Binds the requested 3D shader variant, updates WVP/material/shadow CBs, and sets IA topology.',
    ['shader', 'rendering', 'd3d11'], 'moderate'),
  fn('engine/rendering/d3d11/shader.cpp', 'Shader3D_BeginOutline', [771, 780],
    'Binds the outline 3D shader path used for toon/silhouette passes.',
    ['shader', 'outline', 'd3d11'], 'simple'),

  fn('engine/rendering/effect/effect_system.cpp', 'Initialize', [49, 86],
    'Creates the Effekseer DX11 graphics device, renderer, and manager, then wires sprite/model/material loaders.',
    ['vfx', 'effekseer', 'initialization'], 'moderate'),
  fn('engine/rendering/effect/effect_system.cpp', 'Draw3D', [104, 135],
    'Builds Effekseer view/projection from the active camera and draws all playing 3D effects.',
    ['vfx', 'effekseer', 'rendering'], 'moderate'),
  fn('engine/rendering/effect/effect_system.cpp', 'LoadCached', [152, 177],
    'Loads an .efk effect from a wide path, caching the manager effect id for reuse.',
    ['vfx', 'effekseer', 'asset-loading'], 'moderate'),
];

const importPairs = [
  ['engine/core/job_system.cpp', 'engine/core/job_system.h'],
  ['engine/debug_draw/debug_ofstream.cpp', 'engine/debug_draw/debug_ofstream.h'],
  ['engine/debug_draw/debug_text.cpp', 'engine/debug_draw/debug_text.h'],
  ['engine/ecs/billboard_component.cpp', 'engine/ecs/billboard_component.h'],
  ['engine/ecs/billboard_component.cpp', 'engine/ecs/game_object.h'],
  ['engine/ecs/billboard_component.cpp', 'engine/ecs/string_utils.h'],
  ['engine/ecs/billboard_component.cpp', 'engine/ecs/transform.h'],
  ['engine/ecs/billboard_component.cpp', 'engine/rendering/billboard/billboard.h'],
  ['engine/ecs/billboard_component.cpp', 'engine/rendering/sprite/texture.h'],
  ['engine/input/key_logger.cpp', 'engine/core/system_timer.h'],
  ['engine/input/key_logger.cpp', 'engine/input/key_logger.h'],
  ['engine/input/mouse.cpp', 'engine/input/mouse.h'],
  ['engine/input/mouse.cpp', 'engine/rendering/d3d11/renderer.h'],
  ['engine/rendering/billboard/billboard.cpp', 'engine/rendering/billboard/billboard.h'],
  ['engine/rendering/billboard/billboard.cpp', 'engine/rendering/d3d11/renderer.h'],
  ['engine/rendering/billboard/billboard.cpp', 'engine/rendering/sprite/texture.h'],
  ['engine/rendering/d3d11/post_process.cpp', 'engine/core/engine_gui.h'],
  ['engine/rendering/d3d11/post_process.cpp', 'engine/rendering/d3d11/post_process.h'],
  ['engine/rendering/d3d11/post_process.cpp', 'engine/rendering/d3d11/renderer.h'],
  ['engine/rendering/d3d11/renderer.cpp', 'engine/rendering/d3d11/renderer.h'],
  ['engine/rendering/d3d11/renderer.cpp', 'engine/scene/render_settings.h'],
  ['engine/rendering/d3d11/shader.cpp', 'engine/core/system_timer.h'],
  ['engine/rendering/d3d11/shader.cpp', 'engine/debug_draw/debug_ofstream.h'],
  ['engine/rendering/d3d11/shader.cpp', 'engine/rendering/d3d11/renderer.h'],
  ['engine/rendering/d3d11/shader.cpp', 'engine/rendering/d3d11/shader.h'],
  ['engine/rendering/d3d11/shader.cpp', 'engine/rendering/lighting/lighting.h'],
  ['engine/rendering/d3d11/shader.cpp', 'engine/rendering/lighting/shadow.h'],
  ['engine/rendering/effect/effect_system.cpp', 'engine/rendering/d3d11/renderer.h'],
  ['engine/rendering/effect/effect_system.cpp', 'engine/rendering/effect/effect_system.h'],
];

const containsAndExports = [];
function addOwned(filePath, nodeId, exported) {
  containsAndExports.push(e('file:' + filePath, nodeId, 'contains', 1.0));
  if (exported) containsAndExports.push(e('file:' + filePath, nodeId, 'exports', 0.8));
}

addOwned('engine/core/job_system.cpp', 'class:engine/core/job_system.cpp:JobNode', true);
['CompleteNode', 'WorkerMain', 'SubmitInternal'].forEach((name) => addOwned('engine/core/job_system.cpp', 'function:engine/core/job_system.cpp:' + name, false));
['JobSystem_Init', 'JobSystem_Shutdown', 'JobSystem_Submit', 'JobSystem_SubmitDep', 'JobSystem_SubmitAfter', 'JobSystem_WaitFor', 'JobSystem_WaitAll', 'JobSystem_DebugDraw'].forEach((name) => addOwned('engine/core/job_system.cpp', 'function:engine/core/job_system.cpp:' + name, true));

['DebugText', '~DebugText', 'SetText', 'Draw', 'MeasureText', 'createBuffer'].forEach((name) => addOwned('engine/debug_draw/debug_text.cpp', 'function:engine/debug_draw/debug_text.cpp:' + name, true));
['Serialize', 'Deserialize', 'SubmitNodeBillboards', 'SubmitSceneBillboards'].forEach((name) => addOwned('engine/ecs/billboard_component.cpp', 'function:engine/ecs/billboard_component.cpp:' + name, true));
addOwned('engine/ecs/string_utils.h', 'function:engine/ecs/string_utils.h:ToWString', true);
['KeyLogger_Initialize', 'KeyLogger_Update'].forEach((name) => addOwned('engine/input/key_logger.cpp', 'function:engine/input/key_logger.cpp:' + name, true));
['Mouse_Initialize', 'Mouse_ProcessMessage', 'Mouse_GetDelta'].forEach((name) => addOwned('engine/input/mouse.cpp', 'function:engine/input/mouse.cpp:' + name, true));
['Initialize', 'Finalize', 'Update', 'Draw', 'Add'].forEach((name) => addOwned('engine/rendering/billboard/billboard.cpp', 'function:engine/rendering/billboard/billboard.cpp:' + name, true));
addOwned('engine/rendering/d3d11/post_process.cpp', 'function:engine/rendering/d3d11/post_process.cpp:LoadShaderFile', false);
['PostProcess_Initialize', 'PostProcess_Shutdown', 'PostProcess_SetEffects', 'PostProcess_GetEffects', 'PostProcess_Update', 'PostProcess_Draw'].forEach((name) => addOwned('engine/rendering/d3d11/post_process.cpp', 'function:engine/rendering/d3d11/post_process.cpp:' + name, true));

['CreateDepthBuffer', 'CreateRenderTargetFromSwapChain', 'ReadClientSize', 'Renderer_ResizeBuffers'].forEach((name) => addOwned('engine/rendering/d3d11/renderer.cpp', 'function:engine/rendering/d3d11/renderer.cpp:' + name, false));
['PostRT_Resize', 'Renderer_BindPostTarget', 'Renderer_ClientToBackbuffer', 'Renderer_BackbufferToClient', 'Renderer_UpdateClientSize', 'Renderer_ResizeToClient', 'Renderer_SetWindowMode', 'SetRenderViewport', 'InitRenderer', 'UninitRenderer'].forEach((name) => addOwned('engine/rendering/d3d11/renderer.cpp', 'function:engine/rendering/d3d11/renderer.cpp:' + name, true));

addOwned('engine/rendering/d3d11/shader.cpp', 'function:engine/rendering/d3d11/shader.cpp:UpdateShadowCB', false);
['Shader_Initialize', 'Shader_Finalize', 'Shader_SetProjectionMatrix', 'Shader_SetViewMatrix', 'Shader_SetSkinPalette', 'Shader_CommitSurfaceFX', 'Shader_BindShadowMaps', 'Shader3D_Begin', 'Shader3D_BeginOutline'].forEach((name) => addOwned('engine/rendering/d3d11/shader.cpp', 'function:engine/rendering/d3d11/shader.cpp:' + name, true));
['Initialize', 'Draw3D', 'LoadCached'].forEach((name) => addOwned('engine/rendering/effect/effect_system.cpp', 'function:engine/rendering/effect/effect_system.cpp:' + name, true));

const calls = [
  e('function:engine/core/job_system.cpp:WorkerMain', 'function:engine/core/job_system.cpp:CompleteNode', 'calls', 0.8),
  e('function:engine/core/job_system.cpp:JobSystem_Submit', 'function:engine/core/job_system.cpp:SubmitInternal', 'calls', 0.8),
  e('function:engine/core/job_system.cpp:JobSystem_SubmitDep', 'function:engine/core/job_system.cpp:SubmitInternal', 'calls', 0.8),
  e('function:engine/core/job_system.cpp:JobSystem_SubmitAfter', 'function:engine/core/job_system.cpp:SubmitInternal', 'calls', 0.8),
  e('function:engine/debug_draw/debug_text.cpp:Draw', 'function:engine/debug_draw/debug_text.cpp:createBuffer', 'calls', 0.8),
  e('function:engine/ecs/billboard_component.cpp:SubmitSceneBillboards', 'function:engine/ecs/billboard_component.cpp:SubmitNodeBillboards', 'calls', 0.8),
  e('function:engine/ecs/billboard_component.cpp:SubmitNodeBillboards', 'function:engine/rendering/billboard/billboard.cpp:Add', 'calls', 0.8),
  e('function:engine/ecs/billboard_component.cpp:SubmitNodeBillboards', 'function:engine/ecs/transform.h:GetWorldPosition', 'calls', 0.8),
  e('function:engine/rendering/d3d11/post_process.cpp:PostProcess_Initialize', 'function:engine/rendering/d3d11/post_process.cpp:LoadShaderFile', 'calls', 0.8),
  e('function:engine/rendering/d3d11/post_process.cpp:PostProcess_Draw', 'function:engine/core/engine_gui.cpp:EngineGUI_GetGameViewportRect', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_UpdateClientSize', 'function:engine/rendering/d3d11/renderer.cpp:ReadClientSize', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeBuffers', 'function:engine/rendering/d3d11/renderer.cpp:CreateRenderTargetFromSwapChain', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeBuffers', 'function:engine/rendering/d3d11/renderer.cpp:CreateDepthBuffer', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeBuffers', 'function:engine/rendering/d3d11/renderer.cpp:PostRT_Resize', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeToClient', 'function:engine/rendering/d3d11/renderer.cpp:Renderer_UpdateClientSize', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeToClient', 'function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeBuffers', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:Renderer_SetWindowMode', 'function:engine/rendering/d3d11/renderer.cpp:Renderer_ResizeToClient', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:InitRenderer', 'function:engine/rendering/d3d11/renderer.cpp:ReadClientSize', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:InitRenderer', 'function:engine/rendering/d3d11/renderer.cpp:CreateRenderTargetFromSwapChain', 'calls', 0.8),
  e('function:engine/rendering/d3d11/renderer.cpp:InitRenderer', 'function:engine/rendering/d3d11/renderer.cpp:CreateDepthBuffer', 'calls', 0.8),
  e('function:engine/rendering/d3d11/shader.cpp:Shader_Initialize', 'function:engine/rendering/d3d11/shader.cpp:Shader_SetSkinPalette', 'calls', 0.8),
];

const importEdges = importPairs.map(([s, t]) => e('file:' + s, 'file:' + t, 'imports', 0.7));
const allNodes = [...files, ...fns];
const allEdges = [...importEdges, ...containsAndExports, ...calls];

const ids = new Set(allNodes.map((x) => x.id));
if (ids.size !== allNodes.length) {
  const seen = new Map();
  for (const node of allNodes) seen.set(node.id, (seen.get(node.id) || 0) + 1);
  console.error('DUPLICATE IDS', [...seen.entries()].filter(([, c]) => c > 1));
  process.exit(1);
}

for (const node of allNodes) {
  if (!node.summary || !node.tags || !node.tags.length || !node.complexity) {
    console.error('INVALID NODE', node.id);
    process.exit(1);
  }
}

const fileOrder = files.map((f) => f.filePath).sort();
const partsCount = 2;
const chunk = Math.ceil(fileOrder.length / partsCount);
const parts = [];
for (let i = 0; i < partsCount; i++) {
  const partFiles = new Set(fileOrder.slice(i * chunk, (i + 1) * chunk));
  const nodes = allNodes.filter((node) => partFiles.has(node.filePath));
  const nodeIds = new Set(nodes.map((x) => x.id));
  const edges = allEdges.filter((edge) => nodeIds.has(edge.source));
  parts.push({ files: [...partFiles], nodes, edges });
}

const allowedTargets = new Set(ids);
for (const p of importPairs) {
  allowedTargets.add('file:' + p[0]);
  allowedTargets.add('file:' + p[1]);
}
allowedTargets.add('function:engine/ecs/transform.h:GetWorldPosition');
allowedTargets.add('function:engine/core/engine_gui.cpp:EngineGUI_GetGameViewportRect');

function validate(part, idx) {
  const local = new Set(part.nodes.map((x) => x.id));
  const bad = [];
  for (const edge of part.edges) {
    if (edge.source === edge.target) bad.push({ reason: 'self', edge });
    if (!local.has(edge.source)) bad.push({ reason: 'missing-source', edge });
    if (!local.has(edge.target) && !allowedTargets.has(edge.target)) {
      bad.push({ reason: 'missing-target', edge });
    }
  }
  if (bad.length) {
    console.error('PART', idx, 'bad edges', JSON.stringify(bad, null, 2));
    process.exit(1);
  }
}

parts.forEach((p, i) => validate(p, i + 1));

let totalN = 0;
let totalE = 0;
parts.forEach((p, i) => {
  const path = outDir + '/batch-4-part-' + (i + 1) + '.json';
  fs.writeFileSync(path, JSON.stringify({ nodes: p.nodes, edges: p.edges }, null, 2));
  totalN += p.nodes.length;
  totalE += p.edges.length;
  console.log('wrote', path, 'nodes', p.nodes.length, 'edges', p.edges.length, 'files', p.files.length);
  console.log('  files:', p.files.join(', '));
});
console.log('TOTAL nodes', totalN, 'edges', totalE, 'imports', importEdges.length);
console.log('unique node ids', ids.size);
