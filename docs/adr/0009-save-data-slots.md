# Save data: JSON slot files in Saved Games, explicit commit

Games had no engine way to keep progress or settings: each script would have to pick a folder, a format and its own crash safety, and editor testing would write into the same place as the shipped game. Once players have saves, the folder and the file format are hard to change, so they are fixed here.

**Decision**

- A save is a named **slot** (`[A-Za-z0-9_-]{1,32}`, so it is always a safe file name) holding key-value data. One file per slot: `<slot>.json` = `{ "format": "ivy-save", "version": 1, "savedAt": "<UTC ISO 8601>", "data": { ... } }`. Values are JSON (int, float, bool, string, or any nested JSON the script supplies as text).
- **Folder:** a packaged game / runtime host writes to `FOLDERID_SavedGames\<project.json save.folderName, else name>` (sanitised; falls back to `%LOCALAPPDATA%\<name>\saves`, then `<exe>\saves`). Editor Play writes to `<content project>\.ivy\saves\`, so testing never touches a player's saves; that folder is outside `rom/`, so packaging and hot reload ignore it.
- **Explicit commit:** `ScriptApi_SaveSet*` changes memory only; `ScriptApi_SaveCommit(slot)` writes. Editor Stop drops uncommitted values.
- **Crash safety:** commit writes `<slot>.json.tmp`, copies the old file to `<slot>.json.bak`, then `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`. Loading falls back to `.bak` when the main file is unreadable, and a commit after that keeps the good backup. Files over 4 MiB are refused. A file with a newer `version` is read-only.
- Scripts reach it only through `ScriptRuntimeApi` v51 (plain C types across the DLL boundary). Core logic is `engine/save/save_store` (Linux-tested); `engine/save/save_system.cpp` is the Windows layer.

**Considered Options**

- *`%APPDATA%\<Game>`*: hidden from players and mixed with app settings; Saved Games is the folder Windows provides for this. *Next to the exe*: fails under Program Files.
- *Same folder for editor and game*: editor testing would overwrite real progress.
- *Whole-slot JSON text API only*: fewer functions, but every script re-implements key lookup; key-value plus JSON values covers both small and nested data.
- *Write on every Set / auto-commit on exit*: many small writes or surprise writes; checkpoints are the game's decision.
- *Binary or encrypted format*: harder to debug and to migrate; tamper resistance is not a goal for now.
