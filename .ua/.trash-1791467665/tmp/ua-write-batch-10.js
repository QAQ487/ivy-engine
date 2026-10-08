const fs = require("fs");

const uaDir = "C:/Users/terry/source/repos/ivy-engine/.ua";
const input = JSON.parse(fs.readFileSync(`${uaDir}/tmp/ua-file-analyzer-input-10.json`, "utf8"));
const neighborMap = JSON.parse(fs.readFileSync(`${uaDir}/tmp/ua-file-analyzer-neighbor-10.json`, "utf8"));

function n(id, type, name, filePath, summary, tags, complexity, extra = {}) {
  const node = { id, type, name, summary, tags, complexity };
  if (filePath) node.filePath = filePath;
  Object.assign(node, extra);
  return node;
}
function fn(path, name, lineRange, summary, tags, complexity) {
  return n("function:" + path + ":" + name, "function", name, path, summary, tags, complexity, { lineRange });
}
function cls(path, name, lineRange, summary, tags, complexity) {
  return n("class:" + path + ":" + name, "class", name, path, summary, tags, complexity, { lineRange });
}
function e(source, target, type, weight) {
  return { source, target, type, direction: "forward", weight };
}

const nodes = [
  n("file:engine/audio/audio_listener_sync.h", "file", "audio_listener_sync.h", "engine/audio/audio_listener_sync.h",
    "Declares main-thread helpers that copy the active camera pose onto the 3D audio listener each frame.",
    ["audio", "api-header", "listener", "runtime"], "simple"),

  n("file:engine/core/frame_transform_snapshot.cpp", "file", "frame_transform_snapshot.cpp", "engine/core/frame_transform_snapshot.cpp",
    "Builds and clears a flat per-frame world-pose snapshot of active player and bullet objects for neighbor reads during parallel Update.",
    ["snapshot", "transforms", "ecs", "runtime"], "simple",
    { languageNotes: "Anonymous-namespace vector is rewritten on the main thread before JobSystem_WaitAll so workers never mutate live poses." }),

  n("file:engine/core/frame_transform_snapshot.h", "file", "frame_transform_snapshot.h", "engine/core/frame_transform_snapshot.h",
    "Public snapshot API and FrameTransformEntry layout for read-only player/bullet world position and rotation.",
    ["snapshot", "transforms", "api-header", "data-model"], "simple"),

  n("file:engine/ecs/audio_system.cpp", "file", "audio_system.cpp", "engine/ecs/audio_system.cpp",
    "Keeps a registry of AudioComponent instances and flushes their deferred play/stop commands on the main thread.",
    ["ecs", "audio", "command-queue", "runtime"], "simple"),

  n("file:engine/ecs/audio_system.h", "file", "audio_system.h", "engine/ecs/audio_system.h",
    "Declares register, unregister, and flush entry points for deferred AudioComponent commands.",
    ["ecs", "audio", "api-header", "command-queue"], "simple"),

  n("file:engine/ecs/effect_system_flush.cpp", "file", "effect_system_flush.cpp", "engine/ecs/effect_system_flush.cpp",
    "Keeps a registry of EffectComponent instances and flushes deferred Effekseer play/stop commands on the main thread.",
    ["ecs", "vfx", "command-queue", "runtime"], "simple"),

  n("file:engine/ecs/effect_system_flush.h", "file", "effect_system_flush.h", "engine/ecs/effect_system_flush.h",
    "Declares EffectComponent register/unregister/flush APIs named to avoid clashing with the Effekseer EffectSystem header.",
    ["ecs", "vfx", "api-header", "command-queue"], "simple"),

  n("file:engine/ecs/visual_system.h", "file", "visual_system.h", "engine/ecs/visual_system.h",
    "Declares VisualSystem_UploadSkinning, the post-Update GPU bone-palette and CPU-skinned vertex-buffer publish.",
    ["ecs", "skinning", "api-header", "rendering"], "simple"),

  n("file:engine/rendering/environment/skybox.h", "file", "skybox.h", "engine/rendering/environment/skybox.h",
    "Declares skybox initialize, finalize, settings reload, and view/projection draw entry points.",
    ["skybox", "rendering", "environment", "api-header"], "simple"),

  n("file:engine/rendering/lighting/lighting.cpp", "file", "lighting.cpp", "engine/rendering/lighting/lighting.cpp",
    "Implements the Lighting constant-buffer pipeline: up to four lights, object-space transforms, fog from render settings, and VS/PS bind on slot 3.",
    ["lighting", "d3d11", "constant-buffer", "rendering"], "moderate",
    { languageNotes: "Lights are transformed WS→OS with inv(world); CB_LIGHTING is static_assert 16-byte aligned for D3D11 constant buffers." }),

  n("file:engine/rendering/lighting/lighting.h", "file", "lighting.h", "engine/rendering/lighting/lighting.h",
    "Public Lighting namespace: Light/Params CPU structs, MAX_LIGHTS=4, and initialize/add/update/bind API for directional, point, and spot lights.",
    ["lighting", "api-header", "data-model", "rendering"], "moderate"),

  n("file:engine/scene/scene_lighting.h", "file", "scene_lighting.h", "engine/scene/scene_lighting.h",
    "Declares SceneLightingSyncFromNodes, which copies authored scene-graph lights into the Lighting system each draw.",
    ["lighting", "scene-graph", "api-header", "sync"], "simple"),

  n("file:src/app/scene/game_scene.cpp", "file", "game_scene.cpp", "src/app/scene/game_scene.cpp",
    "Implements GameScene play/edit loop: boot preload, shadow/lighting setup, parallel player Update with snapshot/flushes, and cascaded shadow plus post-process draw.",
    ["entry-point", "scene", "game-loop", "rendering"], "complex",
    { languageNotes: "GAME_RUNTIME vs editor ifdefs split boot preload, grid, and EngineGUI; player Updates may run through JobSystem_Submit then main-thread flushes." }),

  n("file:src/app/scene/game_scene.h", "file", "game_scene.h", "src/app/scene/game_scene.h",
    "Declares GameScene, the playable SceneBase that owns initialize/update/draw and per-pass render-object selection.",
    ["scene", "type-definition", "game-loop", "api-header"], "simple"),

  cls("engine/core/frame_transform_snapshot.h", "FrameTransformEntry", [13, 17],
    "Read-only snapshot row holding a GameObject identity plus world position and rotation for neighbor queries.",
    ["data-model", "transforms", "snapshot"], "simple"),

  fn("engine/core/frame_transform_snapshot.cpp", "FrameTransformSnapshot_Capture", [12, 27],
    "Rewrites the snapshot from active objects tagged TAG_PLAYER or TAG_BULLET, storing world position and rotation.",
    ["snapshot", "transforms", "ecs"], "simple"),

  fn("engine/core/frame_transform_snapshot.cpp", "FrameTransformSnapshot_Clear", [39, 41],
    "Clears the process-wide transform snapshot vector at session teardown.",
    ["snapshot", "cleanup", "runtime"], "simple"),

  fn("engine/ecs/audio_system.cpp", "AudioSystem_Register", [13, 19],
    "Adds an AudioComponent to the flush registry if it is not already present.",
    ["ecs", "audio", "registry"], "simple"),

  fn("engine/ecs/audio_system.cpp", "AudioSystem_Unregister", [21, 26],
    "Removes an AudioComponent from the flush registry with erase-remove.",
    ["ecs", "audio", "registry"], "simple"),

  fn("engine/ecs/audio_system.cpp", "AudioSystem_FlushCommands", [28, 33],
    "Calls FlushCommands on every registered AudioComponent after the parallel player Update batch.",
    ["ecs", "audio", "command-queue"], "simple"),

  fn("engine/ecs/effect_system_flush.cpp", "EffectSystem_RegisterComponent", [13, 19],
    "Adds an EffectComponent to the deferred-command registry if it is not already present.",
    ["ecs", "vfx", "registry"], "simple"),

  fn("engine/ecs/effect_system_flush.cpp", "EffectSystem_UnregisterComponent", [21, 26],
    "Removes an EffectComponent from the deferred-command registry with erase-remove.",
    ["ecs", "vfx", "registry"], "simple"),

  fn("engine/ecs/effect_system_flush.cpp", "EffectSystem_FlushCommands", [28, 33],
    "Calls FlushCommands on every registered EffectComponent on the main thread.",
    ["ecs", "vfx", "command-queue"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "Initialize", [49, 77],
    "Creates the lighting constant buffer and seeds a default directional light plus ambient color.",
    ["lighting", "initialization", "d3d11"], "moderate"),

  fn("engine/rendering/lighting/lighting.cpp", "Finalize", [79, 82],
    "Releases the lighting constant buffer and clears cached device pointers.",
    ["lighting", "cleanup", "d3d11"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "ClearLights", [84, 86],
    "Resets the CPU light count so the next Add* calls refill the frame's light list.",
    ["lighting", "runtime", "scene-lights"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "AddDirectionalLight", [87, 94],
    "Appends a directional light, coercing non-spot/point types to DIRECTIONAL, up to MAX_LIGHTS.",
    ["lighting", "directional-light", "scene-lights"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "AddPointLight", [96, 102],
    "Appends a point light with type forced to POINT, up to MAX_LIGHTS.",
    ["lighting", "point-light", "scene-lights"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "AddSpotLight", [104, 110],
    "Appends a spot light with type forced to SPOT, up to MAX_LIGHTS.",
    ["lighting", "spot-light", "scene-lights"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "SetAmbientColor", [112, 112],
    "Stores the scene ambient RGB used when uploading the lighting constant buffer.",
    ["lighting", "ambient", "runtime"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "GetDirection", [114, 114],
    "Returns the world-space direction of the first stored light.",
    ["lighting", "query", "directional-light"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "GetParams", [115, 115],
    "Returns a copy of the CPU-side lighting Params including ambient and the light array.",
    ["lighting", "query", "data-model"], "simple"),

  fn("engine/rendering/lighting/lighting.cpp", "UpdatePerObject", [117, 177],
    "Transforms lights and view into object space, copies fog from render settings, and uploads CB_LIGHTING for the current world matrix.",
    ["lighting", "constant-buffer", "transforms"], "moderate"),

  fn("engine/rendering/lighting/lighting.cpp", "Bind", [179, 183],
    "Binds the lighting constant buffer to vertex and pixel shader slot 3.",
    ["lighting", "d3d11", "bind"], "simple"),

  cls("src/app/scene/game_scene.h", "GameScene", [13, 31],
    "Playable scene that overrides SceneBase initialize, finalize, update, and draw, plus per-pass object submission.",
    ["scene", "game-loop", "rendering"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "DrawBootLoadingScreen", [85, 186],
    "Runtime boot overlay: cream clear, optional ivy logo, progress bar, and debug-text label while assets preload.",
    ["boot", "loading-screen", "ui-overlay"], "complex"),

  fn("src/app/scene/game_scene.cpp", "PumpBootMessages", [188, 197],
    "Drains the Win32 message queue during boot preload and returns false on WM_QUIT.",
    ["boot", "event-handler", "win32"], "simple"),

  fn("src/app/scene/game_scene.cpp", "RunBootAssetPreload", [199, 231],
    "Runs SceneAssetPreloader over rom/scene with a pumping message loop and loading-screen presents unless skip_boot_preload exists.",
    ["boot", "asset-pipeline", "preload"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "WideFromUtf8Local", [233, 242],
    "Converts a UTF-8 string to a wide string via MultiByteToWideChar for the runtime window title.",
    ["utility", "string", "encoding"], "simple"),

  fn("src/app/scene/game_scene.cpp", "TryReadProjectName", [244, 268],
    "Reads the name field from a nearby project.json, trimming whitespace, for the runtime window title.",
    ["configuration", "json", "utility"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "ApplyRuntimeWindowTitleFromNearbyProject", [270, 286],
    "Sets the Win32 window title from project.json next to the cwd or executable, defaulting to Ivy Engine.",
    ["boot", "win32", "configuration"], "simple"),

  fn("src/app/scene/game_scene.cpp", "Initialize", [291, 350],
    "Starts shadows, lighting, GUI, skybox, billboards, particles, and either editor scene load or runtime boot preload.",
    ["initialization", "scene", "game-loop"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "Finalize", [352, 373],
    "Tears down player objects, canvas, GUI, shadows, particles, billboards, and skybox.",
    ["cleanup", "scene", "game-loop"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "Update", [375, 485],
    "Runs physics, pose snapshot, parallel-safe player jobs, audio/effect/visual flushes, cameras, and canvas scene requests.",
    ["game-loop", "job-system", "physics", "ecs"], "complex"),

  fn("src/app/scene/game_scene.cpp", "Draw", [487, 524],
    "Sets the game viewport, draws the 3D scene and Effekseer into PostRT, then UI overlays and the editor GUI.",
    ["rendering", "game-loop", "post-process"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "SetRenderObject", [526, 552],
    "Submits shadow or main-pass geometry: player draws plus scene models, or editor grid/proxies when not in play visuals.",
    ["rendering", "shadows", "scene-graph"], "moderate"),

  fn("src/app/scene/game_scene.cpp", "Object_Draw", [554, 637],
    "Syncs scene lights, renders cascaded shadow maps, draws skybox and main pass with shadow SRVs, then billboards, particles, and debug draw.",
    ["rendering", "shadows", "skybox", "billboard"], "complex"),
];

const edges = [];

for (const [filePath, imports] of Object.entries(input.batchImportData)) {
  for (const resolved of imports) {
    edges.push(e("file:" + filePath, "file:" + resolved, "imports", 0.7));
  }
}

const contained = [
  ["file:engine/core/frame_transform_snapshot.h", "class:engine/core/frame_transform_snapshot.h:FrameTransformEntry"],
  ["file:engine/core/frame_transform_snapshot.cpp", "function:engine/core/frame_transform_snapshot.cpp:FrameTransformSnapshot_Capture"],
  ["file:engine/core/frame_transform_snapshot.cpp", "function:engine/core/frame_transform_snapshot.cpp:FrameTransformSnapshot_Clear"],
  ["file:engine/ecs/audio_system.cpp", "function:engine/ecs/audio_system.cpp:AudioSystem_Register"],
  ["file:engine/ecs/audio_system.cpp", "function:engine/ecs/audio_system.cpp:AudioSystem_Unregister"],
  ["file:engine/ecs/audio_system.cpp", "function:engine/ecs/audio_system.cpp:AudioSystem_FlushCommands"],
  ["file:engine/ecs/effect_system_flush.cpp", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_RegisterComponent"],
  ["file:engine/ecs/effect_system_flush.cpp", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_UnregisterComponent"],
  ["file:engine/ecs/effect_system_flush.cpp", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_FlushCommands"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:Initialize"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:Finalize"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:ClearLights"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:AddDirectionalLight"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:AddPointLight"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:AddSpotLight"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:SetAmbientColor"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:GetDirection"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:GetParams"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:UpdatePerObject"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:Bind"],
  ["file:src/app/scene/game_scene.h", "class:src/app/scene/game_scene.h:GameScene"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:DrawBootLoadingScreen"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:PumpBootMessages"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:RunBootAssetPreload"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:WideFromUtf8Local"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:TryReadProjectName"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:ApplyRuntimeWindowTitleFromNearbyProject"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Initialize"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Finalize"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Update"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Draw"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:SetRenderObject"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Object_Draw"],
];
for (const [src, tgt] of contained) edges.push(e(src, tgt, "contains", 1.0));

const exported = [
  ["file:engine/core/frame_transform_snapshot.h", "class:engine/core/frame_transform_snapshot.h:FrameTransformEntry"],
  ["file:engine/core/frame_transform_snapshot.cpp", "function:engine/core/frame_transform_snapshot.cpp:FrameTransformSnapshot_Capture"],
  ["file:engine/core/frame_transform_snapshot.cpp", "function:engine/core/frame_transform_snapshot.cpp:FrameTransformSnapshot_Clear"],
  ["file:engine/ecs/audio_system.cpp", "function:engine/ecs/audio_system.cpp:AudioSystem_Register"],
  ["file:engine/ecs/audio_system.cpp", "function:engine/ecs/audio_system.cpp:AudioSystem_Unregister"],
  ["file:engine/ecs/audio_system.cpp", "function:engine/ecs/audio_system.cpp:AudioSystem_FlushCommands"],
  ["file:engine/ecs/effect_system_flush.cpp", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_RegisterComponent"],
  ["file:engine/ecs/effect_system_flush.cpp", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_UnregisterComponent"],
  ["file:engine/ecs/effect_system_flush.cpp", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_FlushCommands"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:Initialize"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:Finalize"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:ClearLights"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:AddDirectionalLight"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:AddPointLight"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:AddSpotLight"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:SetAmbientColor"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:GetDirection"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:GetParams"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:UpdatePerObject"],
  ["file:engine/rendering/lighting/lighting.cpp", "function:engine/rendering/lighting/lighting.cpp:Bind"],
  ["file:src/app/scene/game_scene.h", "class:src/app/scene/game_scene.h:GameScene"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Initialize"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Finalize"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Update"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Draw"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:SetRenderObject"],
  ["file:src/app/scene/game_scene.cpp", "function:src/app/scene/game_scene.cpp:Object_Draw"],
];
for (const [src, tgt] of exported) edges.push(e(src, tgt, "exports", 0.8));

const calls = [
  ["function:engine/ecs/audio_system.cpp:AudioSystem_FlushCommands", "function:engine/ecs/audio_component.h:FlushCommands"],
  ["function:engine/ecs/effect_system_flush.cpp:EffectSystem_FlushCommands", "function:engine/ecs/effect_component.h:FlushCommands"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/core/frame_transform_snapshot.cpp:FrameTransformSnapshot_Capture"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/ecs/audio_system.cpp:AudioSystem_FlushCommands"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/ecs/effect_system_flush.cpp:EffectSystem_FlushCommands"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/ecs/visual_system.cpp:VisualSystem_UploadSkinning"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/audio/audio_listener_sync.cpp:AudioListenerSync_Tick"],
  ["function:src/app/scene/game_scene.cpp:Initialize", "function:engine/rendering/environment/skybox.cpp:Skybox_Initialize"],
  ["function:src/app/scene/game_scene.cpp:Finalize", "function:engine/rendering/environment/skybox.cpp:Skybox_Finalize"],
  ["function:src/app/scene/game_scene.cpp:Object_Draw", "function:engine/rendering/environment/skybox.cpp:Skybox_Draw"],
  ["function:src/app/scene/game_scene.cpp:Initialize", "function:engine/rendering/lighting/lighting.cpp:SetAmbientColor"],
  ["function:src/app/scene/game_scene.cpp:Object_Draw", "function:engine/scene/scene_lighting.cpp:SceneLightingSyncFromNodes"],
  ["function:src/app/scene/game_scene.cpp:Object_Draw", "function:engine/ecs/billboard_component.h:SubmitSceneBillboards"],
  ["function:src/app/scene/game_scene.cpp:Initialize", "function:engine/rendering/particle/particle_system.h:Initialize"],
  ["function:src/app/scene/game_scene.cpp:Finalize", "function:engine/rendering/particle/particle_system.h:Finalize"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/rendering/particle/particle_system.h:Tick"],
  ["function:src/app/scene/game_scene.cpp:Update", "function:engine/rendering/particle/particle_system.h:Reset"],
  ["function:src/app/scene/game_scene.cpp:Object_Draw", "function:engine/rendering/particle/particle_system.h:Render"],
];
for (const [src, tgt] of calls) edges.push(e(src, tgt, "calls", 0.8));

const nodeIds = new Set(nodes.map((x) => x.id));
const importSet = new Set();
for (const arr of Object.values(input.batchImportData)) for (const p of arr) importSet.add(p);

const neighborSymbols = new Map();
for (const neighbors of Object.values(neighborMap)) {
  for (const nb of neighbors) {
    if (!neighborSymbols.has(nb.path)) neighborSymbols.set(nb.path, new Set());
    for (const s of nb.symbols || []) neighborSymbols.get(nb.path).add(s);
  }
}

function edgeOk(edge) {
  if (edge.source === edge.target) return "self-edge";
  if (nodeIds.has(edge.source) && nodeIds.has(edge.target)) return null;
  for (const end of [edge.source, edge.target]) {
    if (nodeIds.has(end)) continue;
    if (end.startsWith("file:")) {
      const p = end.slice(5);
      if (importSet.has(p) || neighborMap[p] || Object.values(neighborMap).some((ns) => ns.some((n) => n.path === p))) continue;
      return "unknown file " + end;
    }
    const m = /^(function|class):(.+):([^:]+)$/.exec(end);
    if (m) {
      const path = m[2];
      const sym = m[3];
      const set = neighborSymbols.get(path);
      if (set && set.has(sym)) continue;
      if (nodeIds.has(end)) continue;
      return "unknown symbol " + end;
    }
    return "bad id " + end;
  }
  return null;
}

const failed = [];
for (const edge of edges) {
  const err = edgeOk(edge);
  if (err) failed.push({ edge, err });
}

const importEdges = edges.filter((x) => x.type === "imports").length;
const expectedImports = Object.values(input.batchImportData).reduce((n, a) => n + a.length, 0);
const dupIds = nodes.map((x) => x.id).filter((id, i, a) => a.indexOf(id) !== i);

const graph = { nodes, edges };
const outPath = `${uaDir}/intermediate/batch-10.json`;
fs.writeFileSync(outPath, JSON.stringify(graph, null, 2));

console.log("nodes", nodes.length);
console.log("edges", edges.length);
console.log("imports", importEdges, "expected", expectedImports);
console.log("contains", edges.filter((x) => x.type === "contains").length);
console.log("exports", edges.filter((x) => x.type === "exports").length);
console.log("calls", edges.filter((x) => x.type === "calls").length);
console.log("dupIds", dupIds);
console.log("failedEdges", failed.length);
if (failed.length) console.log(JSON.stringify(failed, null, 2));
console.log("wrote", outPath);

const byType = {};
for (const node of nodes) {
  if (!byType[node.type]) byType[node.type] = 0;
  byType[node.type]++;
  if (!node.summary) console.log("empty summary", node.id);
  if (!node.tags || node.tags.length < 3 || node.tags.length > 5) console.log("bad tags", node.id, node.tags);
  if (!["simple", "moderate", "complex"].includes(node.complexity)) console.log("bad complexity", node.id);
}
console.log("byType", byType);
console.log("fileNodes", nodes.filter((x) => x.type === "file").length, "batchFiles", input.batchFiles.length);
