# Assets & content paths

## Path resolution

`mod:content_path` helpers (`engine/core/content_path.h`):

- `api:IsSafeRomPath` / `api:ResolveRomForRead` — project `rom/` file if present, else `<exe-dir>/resources/<rest>`
- `api:ResolveRomForWrite` — **project only**
- Process helpers: `api:CurrentRomRoots`, `api:ReadableFileCurrent`, `api:WritableFileCurrent`

Do not invent absolute/`..` rom paths; unsafe rom-shaped strings resolve empty.

## Textures

- `int ScriptApi_TextureLoad(const wchar_t* fileName, bool mipmap = true)` — `api:ScriptApi_TextureLoad`
- `api:ScriptApi_TextureGetSRV`
- `api:ScriptApi_TextureGetWidth` / `api:ScriptApi_TextureGetHeight`
- `api:ScriptApi_TextureGetWhiteTexture`
- `api:ScriptApi_TextureRelease`

## Prefabs

- `GameObject* ScriptApi_PrefabInstantiate(const char* prefabPath, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 rotation = {0,0,0}, GameObject* parent = nullptr, bool startActive = true)` — `api:ScriptApi_PrefabInstantiate`
- `void ScriptApi_PrefabDestroy(GameObject* obj)` — `api:ScriptApi_PrefabDestroy`
- `void ScriptApi_PrefabSetBeforeDestroyCallback(void (*callback)(GameObject* obj))` — `api:ScriptApi_PrefabSetBeforeDestroyCallback`
- Serialize/load: `api:PrefabSerializer` (`Deserialize`, `LoadFromFile`, `SaveToFile`, …)

Formats: [`../scene-prefab.md`](../scene-prefab.md).

## Scene load from scripts / UI

- `void ScriptApi_SceneCanvasRequestSceneFile(const char* sceneFile, bool playOnLoad)` — `api:ScriptApi_SceneCanvasRequestSceneFile`
- `bool ScriptApi_EngineGUILoadFromFile(const char* filename)` — `api:ScriptApi_EngineGUILoadFromFile`
- `void ScriptApi_EngineGUIStartPlay()` / `void ScriptApi_EngineGUIStopPlay()` — `api:ScriptApi_EngineGUIStartPlay` / `api:ScriptApi_EngineGUIStopPlay`
- `bool ScriptApi_RequestExitGame()` — `api:ScriptApi_RequestExitGame`
