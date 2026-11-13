# Agent Engine Docs (docs-only pack)

## Goal

Make Ivy Engine usable by AI coding agents that work in a **separate game (content) repo** without reading engine source. Agents consume a concise English doc pack under `docs/agent/`, exported into the game repo. No MCP server.

## Non-goals

- MCP / tooling server
- Regenerating the full `ENGINE_API_REFERENCE.md` catalog
- Deleting existing docs (including `docs/_api_extract.md`)
- Claiming Windows Editor/Runtime PASS from Linux-only verification

## Audience & constraints

- Agents in an external content project (`project.json`, `rom/`, `game/scripts/`).
- Chunked docs: read only the files needed for the current task.
- Every API name, signature, and JSON field in `docs/agent/` must be verified against current engine headers / loaders — never invented.
- Terminology follows root `CONTEXT.md`.

## Doc pack layout

| Path | Role |
|------|------|
| `docs/agent/README.md` | Entry index: what the engine is, game layout, where to look |
| `docs/agent/scripting.md` | C++ script lifecycle, common `ScriptApi_*`, minimal examples |
| `docs/agent/scene-prefab.md` | Scene/prefab JSON; link `docs/schemas/` |
| `docs/agent/api/<system>.md` | Per existing system only (render, physics-3d, physics-2d, ui, input, audio, camera, assets, …) |
| `docs/agent/build-run.md` | Build/run Editor & Runtime; Debug vs Release (bridge Debug-only; Release `/O2`) |
| `docs/agent/testing-bridge.md` | Condensed bridge: env vars, common verbs, Editor Play-mode verbs; link full `RUNTIME_TEST_BRIDGE.md` |

Drift markers: markdown code spans of the form `` `api:Identifier` ``. `tools/agent_docs_check.py` extracts those identifiers and requires each to appear in engine headers under `engine/` or `src/app/` (token scan). Docs must pass the check.

## Existing docs

Minimal targeted edits to `ENGINE_API_REFERENCE.md`, `ENGINE_MANUAL.md`, `ENGINE_USER_GUIDE.md` where stale vs source (registered components including 2D/sprites, `resources/` resolution, culling settings already present where accurate, ScriptRuntimeApi version notes, folder map). Do not delete files; redundancy recommendations belong only in the PR description.

## Export tool

`tools/export_agent_docs.py` (Python 3, stdlib only):

- `--game-repo <path>`: copy `docs/agent/` and `docs/schemas/` → `<game>/docs/ivy-engine/`
- Write a VERSION/marker file with the source engine git commit hash
- Create/update a delimited managed section in `<game>/AGENTS.md` and `<game>/CLAUDE.md` pointing at `docs/ivy-engine/README.md` (begin/end markers; idempotent; content outside markers untouched)
- Remove stale files **only** under the managed `docs/ivy-engine/` folder
- Validate: path exists and is a directory; refuse if it is the engine repo itself; refuse path traversal
- `--dry-run` prints actions without writing

## Drift check

`tools/agent_docs_check.py` (Linux-runnable): extract `` `api:…` `` identifiers from `docs/agent/`, verify against headers, exit non-zero with missing names listed.

## Tests (TDD)

`unittest` under `tools/` (existing convention), temp dirs:

- Export: copy, hash marker, idempotent managed section, preserve outside content, input validation, dry-run
- Drift check: missing name fails; present name passes

## Verification (this change)

Linux only: Python unit tests + `agent_docs_check.py` against authored docs. Do not claim Windows msbuild / named-pipe / D3D11 verification.
