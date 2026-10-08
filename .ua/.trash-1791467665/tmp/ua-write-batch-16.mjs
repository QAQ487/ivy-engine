import { writeFileSync } from "fs";

const nodes = [];
const edges = [];

function addNode(n) {
  nodes.push(n);
}

function addEdge(e) {
  edges.push(e);
}

function contains(fileId, childId) {
  addEdge({ source: fileId, target: childId, type: "contains", direction: "forward", weight: 1.0 });
}

function exportsEdge(fileId, childId) {
  addEdge({ source: fileId, target: childId, type: "exports", direction: "forward", weight: 0.8 });
}

function fn(path, name, lineRange, summary, tags, complexity = "simple") {
  const id = `function:${path}:${name}`;
  addNode({
    id,
    type: "function",
    name,
    filePath: path,
    lineRange,
    summary,
    tags,
    complexity,
  });
  contains(`file:${path}`, id);
  exportsEdge(`file:${path}`, id);
  return id;
}

// --- File nodes ---

addNode({
  id: "file:.gitattributes",
  type: "file",
  name: ".gitattributes",
  filePath: ".gitattributes",
  summary: "Repository Git attributes that enable automatic text line-ending normalization; Visual Studio merge and binary-diff rules remain commented out.",
  tags: ["git", "line-endings", "repository", "configuration"],
  complexity: "simple",
});

addNode({
  id: "config:.ua/config.json",
  type: "config",
  name: "config.json",
  filePath: ".ua/config.json",
  summary: "Understand-Anything project setting that stores the knowledge-graph output language as English.",
  tags: ["configuration", "understand-anything", "localization"],
  complexity: "simple",
});

addNode({
  id: "file:IvyEngine.Shaders.targets",
  type: "file",
  name: "IvyEngine.Shaders.targets",
  filePath: "IvyEngine.Shaders.targets",
  summary: "MSBuild target that compiles the engine HLSL shaders with fxc to rom/shader/*.cso before the x64 build.",
  tags: ["build-system", "shaders", "hlsl", "msbuild"],
  complexity: "moderate",
  languageNotes: "Custom CompileHlslShaders target runs BeforeTargets=PrepareForBuild and depends on ShaderCommon.hlsli and LightingCommon.hlsli.",
});

addNode({
  id: "file:IvyEngine.vcxproj",
  type: "file",
  name: "IvyEngine.vcxproj",
  filePath: "IvyEngine.vcxproj",
  summary: "Visual Studio project that builds the IvyEngine.exe editor host, compiling the shared engine plus editor sources and importing HLSL shader compilation.",
  tags: ["build-system", "msbuild", "entry-point", "editor"],
  complexity: "complex",
});

addNode({
  id: "file:IvyEngine.vcxproj.filters",
  type: "file",
  name: "IvyEngine.vcxproj.filters",
  filePath: "IvyEngine.vcxproj.filters",
  summary: "Solution Explorer folder map for IvyEngine.vcxproj, grouping engine, app, library, and shader files into virtual filters.",
  tags: ["build-system", "msbuild", "visual-studio", "organization"],
  complexity: "complex",
});

addNode({
  id: "file:IvyEngineRuntime.vcxproj",
  type: "file",
  name: "IvyEngineRuntime.vcxproj",
  filePath: "IvyEngineRuntime.vcxproj",
  summary: "Visual Studio project that builds IvyEngineRuntime.exe, the GAME_RUNTIME play host that shares the engine scene loop without the editor UI.",
  tags: ["build-system", "msbuild", "entry-point", "runtime"],
  complexity: "complex",
});

addNode({
  id: "file:IvyEngineRuntime.vcxproj.filters",
  type: "file",
  name: "IvyEngineRuntime.vcxproj.filters",
  filePath: "IvyEngineRuntime.vcxproj.filters",
  summary: "Solution Explorer folder map for IvyEngineRuntime.vcxproj, mirroring the engine and app source tree used by the runtime host.",
  tags: ["build-system", "msbuild", "visual-studio", "organization"],
  complexity: "complex",
});

addNode({
  id: "file:docs/diagrams/ivy-engine-architecture.html",
  type: "file",
  name: "ivy-engine-architecture.html",
  filePath: "docs/diagrams/ivy-engine-architecture.html",
  summary: "Standalone architecture diagram showing how IvyEngine.exe and the runtime host share GameScene while content projects stay as a loaded folder plus GameScripts.dll.",
  tags: ["documentation", "architecture", "diagram", "overview"],
  complexity: "moderate",
});

addNode({
  id: "file:engine/core/system_timer.cpp",
  type: "file",
  name: "system_timer.cpp",
  filePath: "engine/core/system_timer.cpp",
  summary: "High-resolution QPC game timer with start, stop, reset, frame-delta, and thread-affinity helpers used by the main loop.",
  tags: ["utility", "timing", "runtime", "windows"],
  complexity: "moderate",
  languageNotes: "Uses QueryPerformanceCounter and optionally pins the thread via SetThreadAffinityMask so elapsed time cannot go negative across cores.",
});

addNode({
  id: "file:engine/rendering/effect/effect_object.cpp",
  type: "file",
  name: "effect_object.cpp",
  filePath: "engine/rendering/effect/effect_object.cpp",
  summary: "Implements EffectObject playback against EffectSystem, syncing GameObject transforms and spawning one-shot Effekseer effects into the current scene.",
  tags: ["effects", "ecs", "runtime", "effekseer"],
  complexity: "moderate",
});

addNode({
  id: "file:engine/rendering/effect/effect_object.h",
  type: "file",
  name: "effect_object.h",
  filePath: "engine/rendering/effect/effect_object.h",
  summary: "Declares EffectObject, a GameObject that loads .efk/.efkefc effects and exposes fluent play, transform, and spawn helpers.",
  tags: ["effects", "type-definition", "ecs", "api-header"],
  complexity: "moderate",
});

addNode({
  id: "file:engine/ui/ui_button_pointer.h",
  type: "file",
  name: "ui_button_pointer.h",
  filePath: "engine/ui/ui_button_pointer.h",
  summary: "Inline pointer-click helper that edge-detects hover releases and resets leftover mouse-down so hidden UIButton controls cannot fire phantom clicks.",
  tags: ["ui", "utility", "input", "type-definition"],
  complexity: "simple",
});

addNode({
  id: "config:external_content_template/ContentProject.sln",
  type: "config",
  name: "ContentProject.sln",
  filePath: "external_content_template/ContentProject.sln",
  summary: "Visual Studio solution template that hosts a content project's GameScripts DLL for Debug/Release Win32 and x64.",
  tags: ["configuration", "visual-studio", "template", "build-system"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/Directory.Build.props",
  type: "file",
  name: "Directory.Build.props",
  filePath: "external_content_template/Directory.Build.props",
  summary: "Directory-level MSBuild hook that imports IvyEngine.props so content-project builds inherit EngineRoot and output paths.",
  tags: ["build-system", "msbuild", "template", "configuration"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/IvyEngine.props",
  type: "file",
  name: "IvyEngine.props",
  filePath: "external_content_template/IvyEngine.props",
  summary: "Content-project MSBuild properties that point EngineRoot at the engine checkout and EngineOutDir at x64/$(Configuration).",
  tags: ["build-system", "msbuild", "template", "configuration"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/build_scripts.bat",
  type: "file",
  name: "build_scripts.bat",
  filePath: "external_content_template/build_scripts.bat",
  summary: "Finds the latest MSBuild via vswhere and compiles GameScripts.vcxproj for x64 with an optional Debug/Release argument.",
  tags: ["build-system", "script", "template", "msbuild"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/deploy/deploy.bat",
  type: "file",
  name: "deploy.bat",
  filePath: "external_content_template/deploy/deploy.bat",
  summary: "Copies IvyEngineRuntime.exe, libfbxsdk.dll, GameScripts.dll, and rom assets into a content-project deploy folder.",
  tags: ["deployment", "script", "template", "packaging"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj",
  type: "file",
  name: "GameScripts.vcxproj",
  filePath: "external_content_template/game/scripts/GameScripts/GameScripts.vcxproj",
  summary: "Template C++20 DLL project that compiles script runtime exports and post-builds GameScripts.dll next to both the content project and the engine hosts.",
  tags: ["build-system", "msbuild", "scripting", "template"],
  complexity: "complex",
});

addNode({
  id: "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj.filters",
  type: "file",
  name: "GameScripts.vcxproj.filters",
  filePath: "external_content_template/game/scripts/GameScripts/GameScripts.vcxproj.filters",
  summary: "Solution Explorer filters that group GameScripts template headers and engine script-export sources under EngineRefs.",
  tags: ["build-system", "msbuild", "visual-studio", "organization"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/game/scripts/GameScripts/framework.h",
  type: "file",
  name: "framework.h",
  filePath: "external_content_template/game/scripts/GameScripts/framework.h",
  summary: "Minimal Win32 precompiled-header prelude that defines WIN32_LEAN_AND_MEAN and includes windows.h.",
  tags: ["windows", "precompiled-header", "template", "utility"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/game/scripts/GameScripts/pch.cpp",
  type: "file",
  name: "pch.cpp",
  filePath: "external_content_template/game/scripts/GameScripts/pch.cpp",
  summary: "Translation unit that creates the GameScripts precompiled header by including pch.h.",
  tags: ["precompiled-header", "template", "build-system"],
  complexity: "simple",
});

addNode({
  id: "file:external_content_template/game/scripts/GameScripts/pch.h",
  type: "file",
  name: "pch.h",
  filePath: "external_content_template/game/scripts/GameScripts/pch.h",
  summary: "GameScripts precompiled header that pulls in framework.h for the content-project DLL.",
  tags: ["precompiled-header", "template", "entry-point"],
  complexity: "simple",
});

addNode({
  id: "document:external_content_template/rom/scene/default_scene.txt",
  type: "document",
  name: "default_scene.txt",
  filePath: "external_content_template/rom/scene/default_scene.txt",
  summary: "One-line content-project default that names scene_sample_menu.json as the scene to open.",
  tags: ["documentation", "scene", "template", "content"],
  complexity: "simple",
});

addNode({
  id: "file:external_template/GameScripts.External.vcxproj",
  type: "file",
  name: "GameScripts.External.vcxproj",
  filePath: "external_template/GameScripts.External.vcxproj",
  summary: "Legacy Utility MSBuild project in external_template that lists engine and script sources for an in-repo GameScripts.External target.",
  tags: ["build-system", "msbuild", "template", "scripting"],
  complexity: "complex",
});

addNode({
  id: "file:pch.cpp",
  type: "file",
  name: "pch.cpp",
  filePath: "pch.cpp",
  summary: "Engine-root precompiled-header translation unit that includes pch.h so Visual Studio can generate the PCH.",
  tags: ["precompiled-header", "build-system", "entry-point"],
  complexity: "simple",
});

// --- Functions / classes ---

const timer = "engine/core/system_timer.cpp";
fn(timer, "SystemTimer_Initialize", [31, 43], "Zeros timer state and captures QueryPerformanceFrequency into ticks-per-second.", ["timing", "initialization", "windows"], "simple");
fn(timer, "SystemTimer_Reset", [46, 53], "Restarts the timer from the current adjusted clock and clears the stopped flag.", ["timing", "reset", "runtime"], "simple");
fn(timer, "SystemTimer_Start", [56, 71], "Resumes a stopped timer by shifting the base time across the paused interval.", ["timing", "runtime", "windows"], "simple");
fn(timer, "SystemTimer_Stop", [74, 83], "Records the current QPC value as stop time and marks the timer paused.", ["timing", "runtime", "windows"], "simple");
fn(timer, "SystemTimer_Advance", [86, 89], "Advances the stopped clock by one tenth of a second for debug stepping.", ["timing", "debug", "runtime"], "simple");
fn(timer, "SystemTimer_GetTime", [92, 99], "Returns seconds elapsed since the timer base using the adjusted clock.", ["timing", "query", "runtime"], "simple");
fn(timer, "SystemTimer_GetAbsoluteTime", [102, 110], "Returns raw QueryPerformanceCounter seconds since the counter epoch.", ["timing", "query", "windows"], "simple");
fn(timer, "SystemTimer_GetElapsedTime", [113, 131], "Returns the clamped frame delta since the last call and updates last-elapsed.", ["timing", "frame-delta", "runtime"], "simple");
fn(timer, "SystemTimer_IsStoped", [134, 137], "Reports whether the system timer is currently paused.", ["timing", "query", "runtime"], "simple");
fn(timer, "LimitThreadAffinityToCurrentProc", [140, 162], "Pins the calling thread to one allowed CPU so QPC elapsed time stays monotonic.", ["timing", "affinity", "windows"], "moderate");
fn(timer, "GetAdjustedCurrentTime", [165, 176], "Returns the recorded stop time when paused, otherwise the live QPC counter.", ["timing", "utility", "windows"], "simple");

const eobj = "engine/rendering/effect/effect_object.cpp";
fn(eobj, "LoadFromFileW", [12, 16], "Loads an Effekseer .efk/.efkefc path through EffectSystem and stores the effect id.", ["effects", "loading", "effekseer"], "simple");
fn(eobj, "Initialize", [39, 49], "Auto-plays the loaded effect and applies the GameObject position, rotation, and scale.", ["effects", "lifecycle", "ecs"], "simple");
fn(eobj, "Finalize", [51, 56], "Stops the playing EffectSystem handle and clears it.", ["effects", "lifecycle", "cleanup"], "simple");
fn(eobj, "LogicUpdate", [58, 82], "Syncs the GameObject transform to the effect each frame and deactivates one-shots when they finish.", ["effects", "update", "ecs"], "moderate");
fn(eobj, "Draw", [84, 86], "No-op draw; batched 3D effect rendering happens in EffectSystem::Draw3D.", ["effects", "rendering", "lifecycle"], "simple");
fn(eobj, "Play", [88, 97], "Restarts the effect at the current transform, stopping any existing handle first.", ["effects", "playback", "runtime"], "simple");
fn(eobj, "Stop", [99, 104], "Stops the playing handle without destroying the EffectObject.", ["effects", "playback", "runtime"], "simple");
fn(eobj, "SetSpeed", [106, 108], "Forwards playback speed to EffectSystem when a handle is active.", ["effects", "playback", "control"], "simple");
fn(eobj, "SetPaused", [109, 111], "Forwards pause state to EffectSystem when a handle is active.", ["effects", "playback", "control"], "simple");
fn(eobj, "SetShown", [112, 114], "Forwards visibility to EffectSystem when a handle is active.", ["effects", "playback", "control"], "simple");
fn(eobj, "Effect_Initialize", [139, 141], "Empty legacy module-init stub retained for the Effect_* C API.", ["effects", "legacy", "stub"], "simple");
fn(eobj, "Effect_Finalize", [143, 144], "Empty legacy module-shutdown stub retained for the Effect_* C API.", ["effects", "legacy", "stub"], "simple");
fn(eobj, "Effect_Update", [146, 147], "Empty legacy per-frame stub; EffectObject::LogicUpdate owns updates.", ["effects", "legacy", "stub"], "simple");
fn(eobj, "Effect_Draw", [149, 150], "Empty legacy draw stub; EffectSystem batches 3D effect drawing.", ["effects", "legacy", "stub"], "simple");

addNode({
  id: "class:engine/rendering/effect/effect_object.h:EffectObject",
  type: "class",
  name: "EffectObject",
  filePath: "engine/rendering/effect/effect_object.h",
  lineRange: [13, 80],
  summary: "GameObject subclass that loads and plays Effekseer effects with auto-play, auto-destroy-on-end, and fluent spawn/attach helpers.",
  tags: ["effects", "ecs", "game-object", "effekseer"],
  complexity: "moderate",
});
contains("file:engine/rendering/effect/effect_object.h", "class:engine/rendering/effect/effect_object.h:EffectObject");
exportsEdge("file:engine/rendering/effect/effect_object.h", "class:engine/rendering/effect/effect_object.h:EffectObject");
addEdge({
  source: "class:engine/rendering/effect/effect_object.h:EffectObject",
  target: "class:engine/ecs/game_object.h:GameObject",
  type: "inherits",
  direction: "forward",
  weight: 0.9,
});

addNode({
  id: "class:engine/ui/ui_button_pointer.h:UIButtonPointerState",
  type: "class",
  name: "UIButtonPointerState",
  filePath: "engine/ui/ui_button_pointer.h",
  lineRange: [7, 9],
  summary: "Tiny pointer state that remembers the previous mouse-down so UIButton can detect a click release.",
  tags: ["ui", "data-model", "input", "state"],
  complexity: "simple",
});
contains("file:engine/ui/ui_button_pointer.h", "class:engine/ui/ui_button_pointer.h:UIButtonPointerState");
exportsEdge("file:engine/ui/ui_button_pointer.h", "class:engine/ui/ui_button_pointer.h:UIButtonPointerState");

const uib = "engine/ui/ui_button_pointer.h";
fn(uib, "UIButtonResetPointerState", [11, 13], "Clears leftover mouse-down so a newly shown button cannot emit a phantom click.", ["ui", "input", "utility"], "simple");
fn(uib, "UIButtonPollClick", [15, 19], "Returns true on a hovered mouse-release edge and then stores the current mouse-down flag.", ["ui", "input", "event-handler"], "simple");

// --- Import edges (must match batchImportData 1:1) ---
addEdge({ source: "file:engine/rendering/effect/effect_object.cpp", target: "file:engine/rendering/effect/effect_object.h", type: "imports", direction: "forward", weight: 0.7 });
addEdge({ source: "file:external_content_template/game/scripts/GameScripts/pch.cpp", target: "file:external_content_template/game/scripts/GameScripts/pch.h", type: "imports", direction: "forward", weight: 0.7 });
addEdge({ source: "file:pch.cpp", target: "file:pch.h", type: "imports", direction: "forward", weight: 0.7 });

// --- depends_on / related / configures ---
const dep = (s, t) => addEdge({ source: s, target: t, type: "depends_on", direction: "forward", weight: 0.6 });
const rel = (s, t) => addEdge({ source: s, target: t, type: "related", direction: "forward", weight: 0.5 });

dep("file:IvyEngine.vcxproj", "file:IvyEngine.Shaders.targets");
dep("file:IvyEngine.vcxproj", "file:engine/core/system_timer.cpp");
dep("file:IvyEngine.vcxproj", "file:engine/rendering/effect/effect_object.cpp");
dep("file:IvyEngine.vcxproj", "file:engine/rendering/effect/effect_object.h");
dep("file:IvyEngine.vcxproj", "file:engine/ui/ui_button_pointer.h");
dep("file:IvyEngine.vcxproj", "file:pch.cpp");

dep("file:IvyEngineRuntime.vcxproj", "file:engine/core/system_timer.cpp");
dep("file:IvyEngineRuntime.vcxproj", "file:engine/rendering/effect/effect_object.cpp");
dep("file:IvyEngineRuntime.vcxproj", "file:engine/rendering/effect/effect_object.h");
dep("file:IvyEngineRuntime.vcxproj", "file:engine/ui/ui_button_pointer.h");

dep("file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj", "file:external_content_template/IvyEngine.props");
dep("file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj", "file:external_content_template/game/scripts/GameScripts/framework.h");
dep("file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj", "file:external_content_template/game/scripts/GameScripts/pch.cpp");
dep("file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj", "file:external_content_template/game/scripts/GameScripts/pch.h");

dep("file:external_content_template/Directory.Build.props", "file:external_content_template/IvyEngine.props");
dep("file:external_template/GameScripts.External.vcxproj", "file:engine/core/system_timer.cpp");
dep("file:external_template/GameScripts.External.vcxproj", "file:engine/rendering/effect/effect_object.cpp");
dep("file:external_content_template/game/scripts/GameScripts/pch.h", "file:external_content_template/game/scripts/GameScripts/framework.h");
dep("file:external_content_template/build_scripts.bat", "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj");
dep("file:external_content_template/deploy/deploy.bat", "file:IvyEngineRuntime.vcxproj");
dep("file:external_content_template/deploy/deploy.bat", "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj");

rel("file:IvyEngine.vcxproj.filters", "file:IvyEngine.vcxproj");
rel("file:IvyEngineRuntime.vcxproj.filters", "file:IvyEngineRuntime.vcxproj");
rel("file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj.filters", "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj");
rel("file:docs/diagrams/ivy-engine-architecture.html", "file:IvyEngine.vcxproj");
rel("file:docs/diagrams/ivy-engine-architecture.html", "file:IvyEngineRuntime.vcxproj");
rel("file:docs/diagrams/ivy-engine-architecture.html", "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj");

addEdge({
  source: "config:external_content_template/ContentProject.sln",
  target: "file:external_content_template/game/scripts/GameScripts/GameScripts.vcxproj",
  type: "configures",
  direction: "forward",
  weight: 0.6,
});

const ids = new Set(nodes.map((n) => n.id));
const neighborOk = new Set([
  "file:pch.h",
  "class:engine/ecs/game_object.h:GameObject",
]);
const importOk = new Set(["file:pch.h"]);

const bad = [];
for (const e of edges) {
  const srcOk = ids.has(e.source);
  const tgtOk =
    ids.has(e.target) ||
    neighborOk.has(e.target) ||
    importOk.has(e.target) ||
    (e.target.startsWith("file:") && true);
  if (!srcOk) bad.push({ e, reason: "missing source" });
  if (!ids.has(e.target) && !neighborOk.has(e.target) && e.target !== "file:pch.h") {
    // targets in this batch or known cross-batch
    if (!ids.has(e.target) && e.target !== "file:pch.h" && e.target !== "class:engine/ecs/game_object.h:GameObject") {
      bad.push({ e, reason: "unknown target" });
    }
  }
}

if (new Set(nodes.map((n) => n.id)).size !== nodes.length) {
  throw new Error("duplicate node ids");
}

const importEdges = edges.filter((e) => e.type === "imports");
if (importEdges.length !== 3) {
  throw new Error("expected 3 import edges, got " + importEdges.length);
}

const out = { nodes, edges };
const dest = "C:/Users/terry/source/repos/ivy-engine/.ua/intermediate/batch-16.json";
writeFileSync(dest, JSON.stringify(out, null, 2) + "\n");
console.log(JSON.stringify({ nodeCount: nodes.length, edgeCount: edges.length, bad: bad.length, dest }, null, 2));
if (bad.length) console.error(bad);
