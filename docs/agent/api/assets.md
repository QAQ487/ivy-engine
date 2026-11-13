# Assets & content paths

## Path resolution

`api:content_path` helpers (`engine/core/content_path.h`):

- `api:IsSafeRomPath` / `api:ResolveRomForRead` — project `rom/` file if present, else `<exe-dir>/resources/<rest>`
- `api:ResolveRomForWrite` — **project only**
- Process helpers: `api:CurrentRomRoots`, `api:ReadableFileCurrent`, `api:WritableFileCurrent`

Do not invent absolute/`..` rom paths; unsafe rom-shaped strings resolve empty.

## Textures

- `api:ScriptApi_TextureLoad`
- `api:ScriptApi_TextureGetSRV`
- `api:ScriptApi_TextureGetWidth` / `api:ScriptApi_TextureGetHeight`
- `api:ScriptApi_TextureGetWhiteTexture`
- `api:ScriptApi_TextureRelease`

## Prefabs

- `api:ScriptApi_PrefabInstantiate` — path, position, optional rotation/parent/`startActive`
- `api:ScriptApi_PrefabDestroy`
- `api:ScriptApi_PrefabSetBeforeDestroyCallback`
- Serialize/load: `api:PrefabSerializer` (`Deserialize`, `LoadFromFile`, `SaveToFile`, …)

Formats: [`../scene-prefab.md`](../scene-prefab.md).

## Scene load from scripts / UI

- `api:ScriptApi_SceneCanvasRequestSceneFile`
- `api:ScriptApi_EngineGUILoadFromFile` / `api:ScriptApi_EngineGUIStartPlay` / `api:ScriptApi_EngineGUIStopPlay`
- `api:ScriptApi_RequestExitGame`
