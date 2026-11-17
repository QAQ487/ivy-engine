## Ivy Engine (exported docs)

This is a content (game) project built on Ivy Engine. Engine docs live in `docs/ivy-engine/` — start at `docs/ivy-engine/README.md`. The engine version they describe is in `docs/ivy-engine/ENGINE_COMMIT`.

### Engine boundary

- Treat Ivy Engine as a frozen, read-only dependency. Do not read, search, or edit engine source (the `EngineRoot` folder named in `IvyEngine.props`), even when it is on disk.
- Use only the APIs, components, and JSON fields documented in `docs/ivy-engine/`. Do not guess undocumented signatures or call engine internals.
- If a task needs something the docs do not cover, stop and record it in `docs/engine-requests.md` (create it if missing): what is needed, why, and any workaround. Then tell the user instead of working around the engine.
- Gameplay code goes in `game/scripts/`; content goes in `rom/` (scenes, prefabs, assets). Engine defaults resolve from the engine's `resources/` — do not copy them in unless overriding.
- Do not edit `docs/ivy-engine/` (regenerated), `IvyEngine.props`, the `.sln`, or the engine references in `GameScripts.vcxproj` unless the user asks.

Refresh these docs from the engine repo with `python3 tools/export_agent_docs.py --game-repo <this-repo>`. Editor **File → New Project** runs the same export automatically.
