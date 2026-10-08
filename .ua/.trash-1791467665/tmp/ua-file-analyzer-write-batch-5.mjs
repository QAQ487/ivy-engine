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
const inheritsE = (source, target) => edge(source, target, 'inherits', 0.9);

const importData = {
  'engine/editor/editor_ui_panel.cpp': [
    'engine/core/engine_gui.h',
    'engine/ecs/game_object.h',
    'engine/editor/editor_ui_panel.h',
    'engine/editor/editor_undo.h',
    'engine/scripting/ui_canvas_script_factory.h',
    'engine/ui/scene_canvas.h',
    'engine/ui/ui_bar.h',
    'engine/ui/ui_button.h',
    'engine/ui/ui_canvas.h',
    'engine/ui/ui_sprite.h',
    'engine/ui/ui_text.h',
  ],
  'engine/editor/ui_gizmo.cpp': [
    'engine/core/engine_gui.h',
    'engine/editor/editor_undo.h',
    'engine/editor/ui_gizmo.h',
    'engine/input/mouse.h',
    'engine/ui/scene_canvas.h',
    'engine/ui/ui_bar.h',
    'engine/ui/ui_button.h',
    'engine/ui/ui_element.h',
    'engine/ui/ui_sprite.h',
  ],
  'engine/scripting/script_factory.cpp': [
    'engine/scripting/script_factory.h',
    'engine/scripting/ui_canvas_script_factory.h',
  ],
  'engine/scripting/ui_canvas_script_dll_exports.cpp': [
    'engine/scripting/ui_canvas_script.h',
  ],
  'engine/scripting/ui_canvas_script_factory.cpp': [
    'engine/scripting/ui_canvas_script_factory.h',
  ],
  'engine/scripting/ui_canvas_script_factory.h': [
    'engine/scripting/ui_canvas_script.h',
  ],
  'engine/scripting/ui_canvas_script_register.h': [
    'engine/scripting/ui_canvas_script.h',
  ],
  'engine/ui/ui_element.cpp': [
    'engine/core/engine_gui.h',
    'engine/rendering/d3d11/renderer.h',
    'engine/ui/ui_element.h',
    'engine/ui/ui_element_script_host.h',
  ],
  'engine/ui/ui_element_script_host.cpp': [
    'engine/core/engine_gui.h',
    'engine/ui/ui_element.h',
    'engine/ui/ui_element_script_host.h',
  ],
  'engine/ui/ui_element_script_host.h': [
    'engine/scripting/ui_canvas_script_factory.h',
  ],
  'engine/ui/ui_text.cpp': [
    'engine/ecs/string_utils.h',
    'engine/ui/ui_text.h',
  ],
};

const nodes = [];

// ---- files ----
nodes.push(file(
  'engine/editor/editor_ui_panel.cpp',
  'editor_ui_panel.cpp',
  'Implements the editor canvas inspector: hierarchy listing, type-specific property editors, asset drag-drop, draw-order undo, and UI element creation.',
  ['editor', 'ui-panel', 'inspector', 'event-handler'],
  'complex',
  { languageNotes: 'Anonymous-namespace helpers plus EngineGUIUI public API; large ImGui property switch on UIButton, UIText, UISprite, and UIBar.' },
));
nodes.push(file(
  'engine/editor/editor_ui_panel.h',
  'editor_ui_panel.h',
  'Declares the EngineGUIUI canvas editor API for creating UI widgets and drawing hierarchy and property panels.',
  ['editor', 'ui-panel', 'type-definition', 'api-handler'],
  'simple',
));
nodes.push(file(
  'engine/editor/editor_undo.h',
  'editor_undo.h',
  'Defines the editor undo command interface, undo stack, node/UI locators, and push helpers for transform, component, and canvas edits.',
  ['editor', 'undo', 'command-pattern', 'type-definition'],
  'moderate',
  { languageNotes: 'Classic command pattern: EditorCommand virtual Undo/Redo/Describe plus a singleton EditorUndoStack capped at 100 entries.' },
));
nodes.push(file(
  'engine/editor/ui_gizmo.cpp',
  'ui_gizmo.cpp',
  'Implements viewport move/resize gizmos for selected canvas elements, including hit testing, drag apply, and undo snapshots.',
  ['editor', 'gizmo', 'ui-manipulation', 'event-handler'],
  'complex',
));
nodes.push(file(
  'engine/editor/ui_gizmo.h',
  'ui_gizmo.h',
  'Public UiGizmo entry points for handling input, drawing overlay handles, and reporting mouse capture while a UI element is selected.',
  ['editor', 'gizmo', 'type-definition'],
  'simple',
));
nodes.push(file(
  'engine/scripting/script_factory.cpp',
  'script_factory.cpp',
  'Implements ScriptFactory registration, instantiation, JSON property (de)serialization, and Windows DLL load/unload with hot-reload.',
  ['factory', 'scripting', 'hot-reload', 'service'],
  'complex',
  { languageNotes: 'Hot-reload copies the DLL to a temp path before LoadLibrary so the original file can be overwritten; retired modules are released later.' },
));
nodes.push(file(
  'engine/scripting/ui_canvas_script.h',
  'ui_canvas_script.h',
  'Abstract UICanvasScript base with create/update/click/destroy hooks and JSON serialize/deserialize for canvas-attached scripts.',
  ['scripting', 'type-definition', 'ui', 'lifecycle'],
  'simple',
));
nodes.push(file(
  'engine/scripting/ui_canvas_script_dll_exports.cpp',
  'ui_canvas_script_dll_exports.cpp',
  'Exports the C ABI used by UI script DLLs: register, create, destroy, and enumerate canvas script classes.',
  ['scripting', 'dll-exports', 'entry-point', 'factory'],
  'simple',
  { languageNotes: 'extern "C" __declspec(dllexport) ABI so ScriptFactory can GetProcAddress create/destroy/enumerate without C++ name mangling.' },
));
nodes.push(file(
  'engine/scripting/ui_canvas_script_factory.cpp',
  'ui_canvas_script_factory.cpp',
  'Implements the singleton UICanvasScriptFactory that registers creators and binds scripts from a loaded DLL module.',
  ['factory', 'scripting', 'ui', 'hot-reload'],
  'moderate',
));
nodes.push(file(
  'engine/scripting/ui_canvas_script_factory.h',
  'ui_canvas_script_factory.h',
  'Declares UICanvasScriptFactory, a singleton registry that creates UICanvasScript instances and pins DLL modules for live instances.',
  ['factory', 'scripting', 'type-definition', 'singleton'],
  'simple',
));
nodes.push(file(
  'engine/scripting/ui_canvas_script_register.h',
  'ui_canvas_script_register.h',
  'Provides REGISTER_UI_SCRIPT, which auto-registers a UICanvasScript type either into the DLL registry or the static factory.',
  ['scripting', 'macro', 'registration', 'type-definition'],
  'simple',
  { languageNotes: 'Conditional compilation: SCRIPT_DLL_BUILD/_WINDLL uses DllUICanvasScript_Register; otherwise registers with UICanvasScriptFactory::Instance().' },
));
nodes.push(file(
  'engine/ui/scene_canvas.h',
  'scene_canvas.h',
  'Declares the shared scene UI canvas lifecycle, JSON load/save, editor undo helpers, button-action binding, and overlay/timer APIs.',
  ['ui', 'canvas', 'api-handler', 'serialization'],
  'simple',
));
nodes.push(file(
  'engine/ui/ui_bar.h',
  'ui_bar.h',
  'Declares UIBar, a progress-bar UIElement with label, fill textures, colors, and an optional per-frame update callback.',
  ['ui', 'component', 'data-model', 'type-definition'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_button.h',
  'ui_button.h',
  'Declares UIButton, an interactive UIElement with hover/pressed textures, text, navigation links, and scripted click actions.',
  ['ui', 'component', 'event-handler', 'type-definition'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_element.cpp',
  'ui_element.cpp',
  'Implements UIElement layout (anchor, design-space scale, hit test) and attached-script host forwarding.',
  ['ui', 'layout', 'scripting', 'utility'],
  'moderate',
  { languageNotes: 'Layout is authored in 1920x1080 design space and scaled to the active render viewport or play-mode game viewport.' },
));
nodes.push(file(
  'engine/ui/ui_element.h',
  'ui_element.h',
  'Defines UIAnchor, UIScreenRect, and the UIElement base class for overlay widgets with position, visibility, hit testing, and scripts.',
  ['ui', 'data-model', 'type-definition', 'component'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_element_script_host.cpp',
  'ui_element_script_host.cpp',
  'Implements UIElementScriptHost lifecycle: JSON load/save, factory creation, play-mode update/click, teardown, and hot reload.',
  ['scripting', 'lifecycle', 'serialization', 'service'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_element_script_host.h',
  'ui_element_script_host.h',
  'Declares UIElementScriptHost, which owns a UICanvasScript instance and its serialized properties for a UI element.',
  ['scripting', 'type-definition', 'lifecycle'],
  'simple',
));
nodes.push(file(
  'engine/ui/ui_sprite.h',
  'ui_sprite.h',
  'Declares UISprite, a textured overlay widget with optional atlas source rects and Unity-style radial-360 fill.',
  ['ui', 'component', 'type-definition', 'data-model'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_text.cpp',
  'ui_text.cpp',
  'Implements UIText construction, bitmap-font drawing with alignment, font loading, and screen-rect measurement.',
  ['ui', 'component', 'rendering', 'text'],
  'moderate',
));
nodes.push(file(
  'engine/ui/ui_text.h',
  'ui_text.h',
  'Declares UIText, a UIElement that renders aligned bitmap text with configurable font path, size, spacing, and color.',
  ['ui', 'component', 'type-definition', 'text'],
  'moderate',
));

// ---- classes ----
const pUndo = 'engine/editor/editor_undo.h';
nodes.push(cls(pUndo, 'EditorCommand', [23, 28],
  'Abstract undo command with virtual Undo, Redo, and Describe used by every editor mutation.',
  ['command-pattern', 'undo', 'type-definition'], 'simple'));
nodes.push(cls(pUndo, 'EditorUndoStack', [30, 56],
  'Singleton undo/redo stack with a 100-command depth cap, history labels, and a recording flag.',
  ['undo', 'singleton', 'command-pattern'], 'moderate'));

nodes.push(cls('engine/scripting/ui_canvas_script.h', 'UICanvasScript', [7, 19],
  'Virtual base for canvas-attached scripts with OnCreate, OnUpdate, OnClick, OnDestroy, and JSON property hooks.',
  ['scripting', 'lifecycle', 'type-definition'], 'simple'));
nodes.push(cls('engine/scripting/ui_canvas_script_factory.h', 'UICanvasScriptFactory', [11, 37],
  'Singleton factory that maps script class names to creators and loads DLL-exported UI canvas scripts.',
  ['factory', 'singleton', 'scripting'], 'moderate'));

nodes.push(cls('engine/ui/ui_element.h', 'UIElement', [44, 102],
  'Base overlay widget with anchor-relative position, visibility, hit testing, and an attached script host.',
  ['ui', 'component', 'data-model'], 'moderate'));
nodes.push(cls('engine/ui/ui_bar.h', 'UIBar', [23, 96],
  'Progress-bar UIElement showing current/max values with label text and customizable bar textures.',
  ['ui', 'component', 'progress-bar'], 'moderate'));
nodes.push(cls('engine/ui/ui_button.h', 'UIButton', [22, 154],
  'Clickable UIElement with state textures, text, directional navigation links, and bindable actions.',
  ['ui', 'component', 'event-handler'], 'complex'));
nodes.push(cls('engine/ui/ui_sprite.h', 'UISprite', [38, 108],
  'Textured UIElement supporting atlas source rects, tint, rotation, and radial fill.',
  ['ui', 'component', 'sprite'], 'moderate'));
nodes.push(cls('engine/ui/ui_text.h', 'UIText', [33, 102],
  'Text UIElement that draws a bitmap font with alignment, scale, spacing, and color.',
  ['ui', 'component', 'text'], 'moderate'));
nodes.push(cls('engine/ui/ui_element_script_host.h', 'UIElementScriptHost', [11, 37],
  'Owns a UICanvasScript instance, its class name, and JSON properties, including hot-reload recreate.',
  ['scripting', 'lifecycle', 'serialization'], 'moderate'));

// ---- functions ----
const pPanel = 'engine/editor/editor_ui_panel.cpp';
nodes.push(fn(pPanel, 'AcceptTexturePathDrop', [44, 57],
  'Accepts an ImGui ASSET_PATH drag-drop payload when the path has a texture extension.',
  ['validation', 'drag-drop', 'utility'], 'simple'));
nodes.push(fn(pPanel, 'DrawFontPathInput', [68, 81],
  'Renders an ImGui text field that accepts font-asset path drops into a buffer.',
  ['editor', 'drag-drop', 'ui-panel'], 'simple'));
nodes.push(fn(pPanel, 'DrawAnchorCombo', [111, 123],
  'Draws an ImGui combo that edits a UIAnchor enumeration value.',
  ['editor', 'ui-panel', 'utility'], 'simple'));
nodes.push(fn(pPanel, 'CreateUiElement', [173, 181],
  'Factory that adds a Button, Text, Sprite, or Bar to the scene canvas from a UiCreateType.',
  ['factory', 'editor', 'ui'], 'simple'));
nodes.push(fn(pPanel, 'DrawCanvasElementHierarchy', [183, 205],
  'Lists scene-canvas elements in ImGui and updates the current UI selection.',
  ['editor', 'ui-panel', 'hierarchy'], 'moderate'));
nodes.push(fn(pPanel, 'DrawCanvasElementProperties', [207, 533],
  'Draws the selected canvas element inspector, including type-specific fields, script class, draw order, and delete.',
  ['editor', 'inspector', 'undo'], 'complex'));

const pGizmo = 'engine/editor/ui_gizmo.cpp';
nodes.push(fn(pGizmo, 'GetElementSize', [73, 86],
  'Reads design-space width and height from a UIButton, UISprite, or UIBar.',
  ['gizmo', 'utility', 'ui'], 'simple'));
nodes.push(fn(pGizmo, 'SetElementSize', [88, 98],
  'Writes width and height back onto a resizable canvas widget.',
  ['gizmo', 'utility', 'ui'], 'simple'));
nodes.push(fn(pGizmo, 'DistancePointToSegment', [105, 121],
  'Computes the distance from a point to a line segment for arrow-shaft hit tests.',
  ['gizmo', 'hit-test', 'utility'], 'simple'));
nodes.push(fn(pGizmo, 'HitTestMoveArrow', [132, 153],
  'Tests whether the mouse is over the X or Y move-arrow shaft or arrowhead.',
  ['gizmo', 'hit-test', 'input'], 'moderate'));
nodes.push(fn(pGizmo, 'HitTestHandle', [155, 192],
  'Picks the hovered resize handle, move body, or axis arrow for a selected element rect.',
  ['gizmo', 'hit-test', 'input'], 'moderate'));
nodes.push(fn(pGizmo, 'DrawMoveArrow', [194, 217],
  'Draws a colored axis arrow on the ImGui foreground draw list.',
  ['gizmo', 'rendering', 'overlay'], 'moderate'));
nodes.push(fn(pGizmo, 'DrawHandle', [219, 229],
  'Draws a square resize handle, highlighting it when hovered.',
  ['gizmo', 'rendering', 'overlay'], 'simple'));
nodes.push(fn(pGizmo, 'ApplyResize', [231, 294],
  'Applies a drag on a resize handle while preserving min size and converting to design space.',
  ['gizmo', 'ui-manipulation', 'layout'], 'complex'));
nodes.push(fn(pGizmo, 'ApplyMove', [296, 315],
  'Moves the selected element from a drag delta and marks the scene dirty.',
  ['gizmo', 'ui-manipulation', 'layout'], 'moderate'));
nodes.push(fn(pGizmo, 'IsInteracting', [321, 323],
  'Returns whether a gizmo drag is currently active.',
  ['gizmo', 'state', 'input'], 'simple'));
nodes.push(fn(pGizmo, 'WouldCaptureMouse', [325, 334],
  'Reports whether the mouse is over a gizmo handle so the editor can steal the click.',
  ['gizmo', 'hit-test', 'input'], 'simple'));
nodes.push(fn(pGizmo, 'HandleInput', [336, 415],
  'Drives gizmo hover, drag begin/end, resize/move apply, and UI-edit undo snapshots.',
  ['gizmo', 'event-handler', 'undo'], 'complex'));
nodes.push(fn(pGizmo, 'Draw', [417, 473],
  'Draws the selection outline, resize handles, anchor, and move arrows for the selected element.',
  ['gizmo', 'rendering', 'overlay'], 'complex'));

const pSf = 'engine/scripting/script_factory.cpp';
nodes.push(fn(pSf, 'SerializeScriptProperties', [62, 93],
  'Serializes a live script instance to JSON via C++ or DLL serialize exports.',
  ['scripting', 'serialization', 'json'], 'moderate'));
nodes.push(fn(pSf, 'DeserializeScriptProperties', [95, 112],
  'Applies JSON properties onto a script instance using C++ or DLL deserialize exports.',
  ['scripting', 'serialization', 'json'], 'moderate'));
nodes.push(fn(pSf, 'LoadScriptDll', [114, 219],
  'Copies a script DLL to a temp path, LoadLibrarys it, registers GameObject and UI canvas creators, and enumerates exports.',
  ['scripting', 'hot-reload', 'factory'], 'complex'));
nodes.push(fn(pSf, 'UnloadScriptDll', [221, 231],
  'Unregisters a named DLL script and retires its module handle for later release.',
  ['scripting', 'hot-reload', 'factory'], 'simple'));
nodes.push(fn(pSf, 'InvokeExportOnLoadedDlls', [237, 258],
  'Calls a named C export on every currently loaded script DLL.',
  ['scripting', 'dll-exports', 'factory'], 'moderate'));
nodes.push(fn(pSf, 'UnloadAllDlls', [281, 304],
  'Clears all DLL-backed creators and unloads every loaded script module.',
  ['scripting', 'hot-reload', 'factory'], 'moderate'));
nodes.push(fn(pSf, 'RequestReload', [306, 316],
  'Queues a named script DLL path for a later hot-reload pass.',
  ['scripting', 'hot-reload', 'factory'], 'simple'));
nodes.push(fn(pSf, 'DrainReloadQueue', [323, 339],
  'Reloads every queued script DLL and invokes an onReloaded callback for each name.',
  ['scripting', 'hot-reload', 'factory'], 'moderate'));

const pDll = 'engine/scripting/ui_canvas_script_dll_exports.cpp';
nodes.push(fn(pDll, 'DllUICanvasScript_Register', [13, 15],
  'Registers a UI canvas script creator into the process-local DLL registry.',
  ['scripting', 'registration', 'factory'], 'simple'));
nodes.push(fn(pDll, 'CreateUICanvasScript', [18, 24],
  'C-export factory that constructs a registered UICanvasScript by class name.',
  ['scripting', 'dll-exports', 'factory'], 'simple'));
nodes.push(fn(pDll, 'DestroyUICanvasScript', [27, 29],
  'C-export that deletes a UICanvasScript created by the DLL factory.',
  ['scripting', 'dll-exports', 'lifecycle'], 'simple'));
nodes.push(fn(pDll, 'EnumerateUICanvasScripts', [32, 35],
  'C-export that reports every registered UI canvas script name to a callback.',
  ['scripting', 'dll-exports', 'registration'], 'simple'));

const pFact = 'engine/scripting/ui_canvas_script_factory.cpp';
nodes.push(fn(pFact, 'Create', [23, 32],
  'Looks up a registered UI canvas script creator and returns a ScriptPtr, or logs if missing.',
  ['factory', 'scripting', 'ui'], 'simple'));
nodes.push(fn(pFact, 'GetRegisteredNames', [38, 45],
  'Returns a sorted list of registered UI canvas script class names.',
  ['factory', 'scripting', 'ui'], 'simple'));
nodes.push(fn(pFact, 'LoadFromModule', [47, 87],
  'Resolves create/destroy/enumerate exports from an HMODULE and registers each UI script, pinning the module.',
  ['factory', 'scripting', 'hot-reload'], 'complex'));

const pEl = 'engine/ui/ui_element.cpp';
nodes.push(fn(pEl, 'GetAnchorDesignPosition', [16, 54],
  'Maps a UIAnchor enum to a design-space origin for the given screen size.',
  ['layout', 'ui', 'utility'], 'moderate'));
nodes.push(fn(pEl, 'UIGetDesignSize', [104, 109],
  'Returns the 1920x1080 design resolution used for UI layout.',
  ['layout', 'ui', 'utility'], 'simple'));
nodes.push(fn(pEl, 'UIGetLayoutScale', [111, 131],
  'Computes design-to-viewport scale, preferring the play-mode game viewport when playing.',
  ['layout', 'ui', 'utility'], 'moderate'));
nodes.push(fn(pEl, 'GetScreenPosition', [152, 167],
  'Converts an element\'s anchor-relative design position into scaled viewport coordinates.',
  ['layout', 'ui', 'positioning'], 'moderate'));
nodes.push(fn(pEl, 'ScreenDeltaToDesignDelta', [169, 175],
  'Converts a viewport pixel delta into design-space units using the current layout scale.',
  ['layout', 'ui', 'utility'], 'simple'));
nodes.push(fn(pEl, 'HitTest', [177, 187],
  'Returns whether a viewport-local point falls inside the element\'s screen rectangle.',
  ['ui', 'hit-test', 'input'], 'simple'));

const pHost = 'engine/ui/ui_element_script_host.cpp';
nodes.push(fn(pHost, 'LoadFromJson', [19, 31],
  'Reads script class and properties from JSON and instantiates the attached script.',
  ['scripting', 'serialization', 'json'], 'simple'));
nodes.push(fn(pHost, 'SaveToJson', [33, 47],
  'Writes the script class name and serialized properties back to JSON.',
  ['scripting', 'serialization', 'json'], 'simple'));
nodes.push(fn(pHost, 'CreateScript', [49, 65],
  'Creates a UICanvasScript from the factory and deserializes stored properties onto it.',
  ['scripting', 'factory', 'lifecycle'], 'moderate'));
nodes.push(fn(pHost, 'Update', [67, 79],
  'During play, lazily calls OnCreate then OnUpdate on the attached canvas script.',
  ['scripting', 'lifecycle', 'event-handler'], 'simple'));
nodes.push(fn(pHost, 'InvokeOnClick', [81, 93],
  'During play, ensures OnCreate ran then forwards a click to the attached script.',
  ['scripting', 'event-handler', 'lifecycle'], 'simple'));
nodes.push(fn(pHost, 'HotReload', [102, 112],
  'Tears down the current script instance and recreates it from the (possibly reloaded) factory.',
  ['scripting', 'hot-reload', 'lifecycle'], 'simple'));

const pText = 'engine/ui/ui_text.cpp';
nodes.push(fn(pText, 'UIText', [11, 26],
  'Constructs a UIText with default white color, scale, spacing, and left alignment.',
  ['ui', 'constructor', 'text'], 'simple'));
nodes.push(fn(pText, 'Draw', [38, 70],
  'Draws the bitmap string at the scaled screen position using the configured alignment and color.',
  ['ui', 'rendering', 'text'], 'moderate'));
nodes.push(fn(pText, 'SetFont', [80, 99],
  'Loads a font texture into the text renderer and applies character spacing.',
  ['ui', 'text', 'resource'], 'moderate'));
nodes.push(fn(pText, 'GetScreenRect', [118, 147],
  'Computes the aligned text bounding box in viewport space for hit testing and gizmos.',
  ['ui', 'layout', 'hit-test'], 'moderate'));

// ---- edges ----
const edges = [];

for (const [from, tos] of Object.entries(importData)) {
  for (const to of tos) edges.push(importsE(from, to));
}

const fileChildren = {
  [pUndo]: [
    `class:${pUndo}:EditorCommand`,
    `class:${pUndo}:EditorUndoStack`,
  ],
  'engine/scripting/ui_canvas_script.h': [
    'class:engine/scripting/ui_canvas_script.h:UICanvasScript',
  ],
  'engine/scripting/ui_canvas_script_factory.h': [
    'class:engine/scripting/ui_canvas_script_factory.h:UICanvasScriptFactory',
  ],
  'engine/ui/ui_element.h': ['class:engine/ui/ui_element.h:UIElement'],
  'engine/ui/ui_bar.h': ['class:engine/ui/ui_bar.h:UIBar'],
  'engine/ui/ui_button.h': ['class:engine/ui/ui_button.h:UIButton'],
  'engine/ui/ui_sprite.h': ['class:engine/ui/ui_sprite.h:UISprite'],
  'engine/ui/ui_text.h': ['class:engine/ui/ui_text.h:UIText'],
  'engine/ui/ui_element_script_host.h': [
    'class:engine/ui/ui_element_script_host.h:UIElementScriptHost',
  ],
  [pPanel]: [
    `function:${pPanel}:AcceptTexturePathDrop`,
    `function:${pPanel}:DrawFontPathInput`,
    `function:${pPanel}:DrawAnchorCombo`,
    `function:${pPanel}:CreateUiElement`,
    `function:${pPanel}:DrawCanvasElementHierarchy`,
    `function:${pPanel}:DrawCanvasElementProperties`,
  ],
  [pGizmo]: [
    `function:${pGizmo}:GetElementSize`,
    `function:${pGizmo}:SetElementSize`,
    `function:${pGizmo}:DistancePointToSegment`,
    `function:${pGizmo}:HitTestMoveArrow`,
    `function:${pGizmo}:HitTestHandle`,
    `function:${pGizmo}:DrawMoveArrow`,
    `function:${pGizmo}:DrawHandle`,
    `function:${pGizmo}:ApplyResize`,
    `function:${pGizmo}:ApplyMove`,
    `function:${pGizmo}:IsInteracting`,
    `function:${pGizmo}:WouldCaptureMouse`,
    `function:${pGizmo}:HandleInput`,
    `function:${pGizmo}:Draw`,
  ],
  [pSf]: [
    `function:${pSf}:SerializeScriptProperties`,
    `function:${pSf}:DeserializeScriptProperties`,
    `function:${pSf}:LoadScriptDll`,
    `function:${pSf}:UnloadScriptDll`,
    `function:${pSf}:InvokeExportOnLoadedDlls`,
    `function:${pSf}:UnloadAllDlls`,
    `function:${pSf}:RequestReload`,
    `function:${pSf}:DrainReloadQueue`,
  ],
  [pDll]: [
    `function:${pDll}:DllUICanvasScript_Register`,
    `function:${pDll}:CreateUICanvasScript`,
    `function:${pDll}:DestroyUICanvasScript`,
    `function:${pDll}:EnumerateUICanvasScripts`,
  ],
  [pFact]: [
    `function:${pFact}:Create`,
    `function:${pFact}:GetRegisteredNames`,
    `function:${pFact}:LoadFromModule`,
  ],
  [pEl]: [
    `function:${pEl}:GetAnchorDesignPosition`,
    `function:${pEl}:UIGetDesignSize`,
    `function:${pEl}:UIGetLayoutScale`,
    `function:${pEl}:GetScreenPosition`,
    `function:${pEl}:ScreenDeltaToDesignDelta`,
    `function:${pEl}:HitTest`,
  ],
  [pHost]: [
    `function:${pHost}:LoadFromJson`,
    `function:${pHost}:SaveToJson`,
    `function:${pHost}:CreateScript`,
    `function:${pHost}:Update`,
    `function:${pHost}:InvokeOnClick`,
    `function:${pHost}:HotReload`,
  ],
  [pText]: [
    `function:${pText}:UIText`,
    `function:${pText}:Draw`,
    `function:${pText}:SetFont`,
    `function:${pText}:GetScreenRect`,
  ],
};

for (const [fp, ids] of Object.entries(fileChildren)) {
  for (const id of ids) edges.push(contains(fp, id));
}

const exported = [
  [pPanel, `function:${pPanel}:CreateUiElement`],
  [pPanel, `function:${pPanel}:DrawCanvasElementHierarchy`],
  [pPanel, `function:${pPanel}:DrawCanvasElementProperties`],
  [pUndo, `class:${pUndo}:EditorCommand`],
  [pUndo, `class:${pUndo}:EditorUndoStack`],
  [pGizmo, `function:${pGizmo}:IsInteracting`],
  [pGizmo, `function:${pGizmo}:WouldCaptureMouse`],
  [pGizmo, `function:${pGizmo}:HandleInput`],
  [pGizmo, `function:${pGizmo}:Draw`],
  ['engine/scripting/ui_canvas_script.h', 'class:engine/scripting/ui_canvas_script.h:UICanvasScript'],
  [pDll, `function:${pDll}:DllUICanvasScript_Register`],
  [pDll, `function:${pDll}:CreateUICanvasScript`],
  [pDll, `function:${pDll}:DestroyUICanvasScript`],
  [pDll, `function:${pDll}:EnumerateUICanvasScripts`],
  ['engine/scripting/ui_canvas_script_factory.h', 'class:engine/scripting/ui_canvas_script_factory.h:UICanvasScriptFactory'],
  ['engine/ui/ui_element.h', 'class:engine/ui/ui_element.h:UIElement'],
  [pEl, `function:${pEl}:UIGetDesignSize`],
  [pEl, `function:${pEl}:UIGetLayoutScale`],
  [pEl, `function:${pEl}:ScreenDeltaToDesignDelta`],
  ['engine/ui/ui_bar.h', 'class:engine/ui/ui_bar.h:UIBar'],
  ['engine/ui/ui_button.h', 'class:engine/ui/ui_button.h:UIButton'],
  ['engine/ui/ui_sprite.h', 'class:engine/ui/ui_sprite.h:UISprite'],
  ['engine/ui/ui_text.h', 'class:engine/ui/ui_text.h:UIText'],
  ['engine/ui/ui_element_script_host.h', 'class:engine/ui/ui_element_script_host.h:UIElementScriptHost'],
];
for (const [fp, id] of exported) edges.push(exportsE(fp, id));

edges.push(inheritsE('class:engine/ui/ui_bar.h:UIBar', 'class:engine/ui/ui_element.h:UIElement'));
edges.push(inheritsE('class:engine/ui/ui_button.h:UIButton', 'class:engine/ui/ui_element.h:UIElement'));
edges.push(inheritsE('class:engine/ui/ui_sprite.h:UISprite', 'class:engine/ui/ui_element.h:UIElement'));
edges.push(inheritsE('class:engine/ui/ui_text.h:UIText', 'class:engine/ui/ui_element.h:UIElement'));

// cross-file / neighbor calls
edges.push(callsE(`function:${pPanel}:CreateUiElement`, 'function:engine/ui/scene_canvas.cpp:SceneCanvas_GetCanvas'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementHierarchy`, 'function:engine/ui/scene_canvas.cpp:SceneCanvas_GetCanvas'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, 'function:engine/ui/scene_canvas.cpp:SceneCanvas_GetCanvas'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, 'function:engine/ui/scene_canvas.cpp:SceneCanvas_BindButtonAction'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, 'function:engine/editor/editor_undo.cpp:EditorUndo_PushUiReorder'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, 'function:engine/editor/editor_undo.cpp:EditorUndo_DiscardUiEdit'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, 'function:engine/editor/editor_undo.cpp:EditorUndo_PushUiDelete'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, 'function:engine/core/engine_gui.cpp:EngineGUI_MarkSceneDirty'));
edges.push(callsE(`function:${pPanel}:DrawCanvasElementProperties`, `function:${pFact}:GetRegisteredNames`));

edges.push(callsE(`function:${pGizmo}:ApplyMove`, 'function:engine/core/engine_gui.cpp:EngineGUI_MarkSceneDirty'));
edges.push(callsE(`function:${pGizmo}:HandleInput`, 'function:engine/ui/scene_canvas.cpp:SceneCanvas_SerializeElement'));
edges.push(callsE(`function:${pGizmo}:HandleInput`, 'function:engine/editor/editor_undo.cpp:EditorUndo_PushUiEdit'));
edges.push(callsE(`function:${pGizmo}:HandleInput`, 'function:engine/core/engine_gui.cpp:EngineGUI_MouseToViewportLocal'));
edges.push(callsE(`function:${pGizmo}:HandleInput`, 'function:engine/core/engine_gui.cpp:EngineGUI_MarkSceneDirty'));

edges.push(callsE(`function:${pSf}:LoadScriptDll`, `function:${pFact}:LoadFromModule`));

edges.push(callsE(`function:${pEl}:UIGetLayoutScale`, `function:${pEl}:UIGetDesignSize`));
edges.push(callsE(`function:${pEl}:UIGetLayoutScale`, 'function:engine/core/engine_gui.cpp:EngineGUI_IsPlaying'));
edges.push(callsE(`function:${pEl}:UIGetLayoutScale`, 'function:engine/core/engine_gui.cpp:EngineGUI_GetGameViewportRect'));
edges.push(callsE(`function:${pEl}:GetScreenPosition`, `function:${pEl}:GetAnchorDesignPosition`));
edges.push(callsE(`function:${pEl}:GetScreenPosition`, `function:${pEl}:UIGetLayoutScale`));
edges.push(callsE(`function:${pEl}:ScreenDeltaToDesignDelta`, `function:${pEl}:UIGetLayoutScale`));

edges.push(callsE(`function:${pHost}:LoadFromJson`, `function:${pHost}:CreateScript`));
edges.push(callsE(`function:${pHost}:Update`, 'function:engine/core/engine_gui.cpp:EngineGUI_IsPlaying'));
edges.push(callsE(`function:${pHost}:InvokeOnClick`, 'function:engine/core/engine_gui.cpp:EngineGUI_IsPlaying'));
edges.push(callsE(`function:${pHost}:HotReload`, `function:${pHost}:CreateScript`));

edges.push(callsE(`function:${pText}:Draw`, `function:${pEl}:GetScreenPosition`));
edges.push(callsE(`function:${pText}:Draw`, `function:${pEl}:UIGetLayoutScale`));
edges.push(callsE(`function:${pText}:GetScreenRect`, `function:${pEl}:GetScreenPosition`));
edges.push(callsE(`function:${pText}:GetScreenRect`, `function:${pEl}:UIGetLayoutScale`));

// validate uniqueness
const ids = new Set();
for (const n of nodes) {
  if (ids.has(n.id)) throw new Error('duplicate node ' + n.id);
  ids.add(n.id);
}
for (const e of edges) {
  if (e.source === e.target) throw new Error('self edge ' + e.source);
}

const importCount = edges.filter((e) => e.type === 'imports').length;
if (importCount !== 36) throw new Error('expected 36 imports, got ' + importCount);

const filesAlpha = [...new Set(nodes.map((n) => n.filePath))].sort();
console.log('files', filesAlpha.length, filesAlpha);
console.log('nodes', nodes.length, 'edges', edges.length);

const parts = Math.ceil(Math.max(nodes.length / 60, edges.length / 120));
const chunk = Math.ceil(filesAlpha.length / parts);
const groups = [];
for (let i = 0; i < parts; i++) {
  groups.push(filesAlpha.slice(i * chunk, (i + 1) * chunk));
}

const neighborOrImport = new Set();
for (const [from, tos] of Object.entries(importData)) {
  neighborOrImport.add(from);
  for (const to of tos) neighborOrImport.add(to);
}
// known neighbor files used as call/import targets
[
  'engine/core/engine_gui.h',
  'engine/core/engine_gui.cpp',
  'engine/ecs/game_object.h',
  'engine/ecs/string_utils.h',
  'engine/editor/editor_undo.cpp',
  'engine/input/mouse.h',
  'engine/rendering/d3d11/renderer.h',
  'engine/scripting/script_factory.h',
  'engine/ui/scene_canvas.cpp',
  'engine/ui/ui_canvas.h',
].forEach((p) => neighborOrImport.add(p));

const neighborSymbols = new Set([
  'function:engine/ui/scene_canvas.cpp:SceneCanvas_GetCanvas',
  'function:engine/ui/scene_canvas.cpp:SceneCanvas_BindButtonAction',
  'function:engine/ui/scene_canvas.cpp:SceneCanvas_SerializeElement',
  'function:engine/editor/editor_undo.cpp:EditorUndo_PushUiReorder',
  'function:engine/editor/editor_undo.cpp:EditorUndo_DiscardUiEdit',
  'function:engine/editor/editor_undo.cpp:EditorUndo_PushUiDelete',
  'function:engine/editor/editor_undo.cpp:EditorUndo_PushUiEdit',
  'function:engine/core/engine_gui.cpp:EngineGUI_MarkSceneDirty',
  'function:engine/core/engine_gui.cpp:EngineGUI_MouseToViewportLocal',
  'function:engine/core/engine_gui.cpp:EngineGUI_IsPlaying',
  'function:engine/core/engine_gui.cpp:EngineGUI_GetGameViewportRect',
  'function:engine/ecs/string_utils.h:ToWString',
]);

const outDir = 'C:/Users/terry/source/repos/ivy-engine/.ua/intermediate';
mkdirSync(outDir, { recursive: true });

let totalNodes = 0;
let totalEdges = 0;
groups.forEach((group, idx) => {
  const groupSet = new Set(group);
  const partNodes = nodes.filter((n) => groupSet.has(n.filePath));
  const partIds = new Set(partNodes.map((n) => n.id));
  const partEdges = edges.filter((e) => partIds.has(e.source));

  const sameBatchIds = new Set(nodes.map((n) => n.id));
  const sameBatchFiles = new Set(filesAlpha);
  for (const e of partEdges) {
    const fileTarget = e.target.startsWith('file:') ? e.target.slice(5) : null;
    const ok =
      partIds.has(e.target) ||
      sameBatchIds.has(e.target) ||
      (fileTarget && (neighborOrImport.has(fileTarget) || sameBatchFiles.has(fileTarget))) ||
      neighborSymbols.has(e.target);
    if (!ok) {
      throw new Error(`part ${idx + 1} invalid target ${e.target} from ${e.source}`);
    }
    if (!partIds.has(e.source)) {
      throw new Error(`part ${idx + 1} source not in part ${e.source}`);
    }
  }

  const part = { nodes: partNodes, edges: partEdges };
  const name = parts === 1 ? 'batch-5.json' : `batch-5-part-${idx + 1}.json`;
  writeFileSync(`${outDir}/${name}`, JSON.stringify(part, null, 2));
  totalNodes += partNodes.length;
  totalEdges += partEdges.length;
  console.log(name, 'nodes', partNodes.length, 'edges', partEdges.length, 'files', group.length);
});

console.log('TOTAL nodes', totalNodes, 'edges', totalEdges, 'parts', parts);
