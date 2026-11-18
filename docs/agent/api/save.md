# Save data

Script API v51 (`api:ScriptRuntimeApi` `version >= 51`; older hosts: setters / commit return `false`, getters return the default). Decision record: engine `docs/adr/0009-save-data-slots.md`.

## Model

- A **slot** is one file: name 1–32 characters of `A-Z a-z 0-9 _ -` (e.g. `"slot1"`, `"settings"`, `"auto"`).
- A slot holds **key-value** data. Keys: 1–128 bytes of UTF-8, no control characters.
- `Set*` changes memory only. **Nothing reaches the disk until `api:ScriptApi_SaveCommit`.** Commit at checkpoints.
- Commit is atomic. The previous file is kept as `<slot>.json.bak`, and loading falls back to it when the main file is unreadable. A slot file larger than 4 MiB is refused.
- Folders:
  - packaged game / runtime: `<Saved Games>/<game name>/`, from `project.json` `save.folderName`, else `name`
  - editor Play: `<content project>/.ivy/saves/` (add `.ivy/` to `.gitignore`)
  - Editor Stop drops uncommitted values. The Console logs the folder on first use.

## Functions

| Purpose | Wrapper |
|---------|---------|
| Integers | `bool ScriptApi_SaveSetInt(const char* slot, const char* key, long long value)` / `long long ScriptApi_SaveGetInt(const char* slot, const char* key, long long defaultValue = 0)` — `api:ScriptApi_SaveSetInt`, `api:ScriptApi_SaveGetInt` (integers only) |
| Floats | `api:ScriptApi_SaveSetFloat` / `api:ScriptApi_SaveGetFloat` (`double`; get reads any number; non-finite values are rejected) |
| Bools | `api:ScriptApi_SaveSetBool` / `api:ScriptApi_SaveGetBool` |
| Strings | `bool ScriptApi_SaveSetString(const char* slot, const char* key, const char* utf8Value)` / `std::string ScriptApi_SaveGetString(const char* slot, const char* key, const char* defaultValue = "")` — `api:ScriptApi_SaveSetString`, `api:ScriptApi_SaveGetString` |
| Nested data | `bool ScriptApi_SaveSetJson(const char* slot, const char* key, const char* jsonText)` (false if it does not parse) / `std::string ScriptApi_SaveGetJson(const char* slot, const char* key)` (`""` when missing) — `api:ScriptApi_SaveSetJson`, `api:ScriptApi_SaveGetJson` |
| Keys | `api:ScriptApi_SaveHasKey`, `api:ScriptApi_SaveDeleteKey` |
| Write | `bool ScriptApi_SaveCommit(const char* slot)` — `api:ScriptApi_SaveCommit` |
| Slots on disk | `api:ScriptApi_SaveSlotExists` (committed only), `api:ScriptApi_SaveDeleteSlot`, `std::vector<std::string> ScriptApi_SaveListSlots()` (`api:ScriptApi_SaveListSlots`) |

## Example

```cpp
// Load (defaults when the slot or key does not exist yet)
const long long coins = ScriptApi_SaveGetInt("slot1", "coins", 0);
const std::string level = ScriptApi_SaveGetString("slot1", "level", "forest");

// Change and write at a checkpoint
ScriptApi_SaveSetInt("slot1", "coins", coins + 10);
nlohmann::json player = {{"hp", 80}, {"items", {"key", "map"}}};
ScriptApi_SaveSetJson("slot1", "player", player.dump().c_str());
if (!ScriptApi_SaveCommit("slot1")) { /* the Console says why */ }

// Read nested data back
const auto restored = nlohmann::json::parse(ScriptApi_SaveGetJson("slot1", "player"), nullptr, false);
```

Reference script: engine `samples/scripts/SampleSaveScript.cpp`, used by testbed `scene_save_demo.json`.
