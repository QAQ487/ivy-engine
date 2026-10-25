# Editor content hot reload

The Editor watches the open project's `rom/` recursively in Edit, viewport Play and fullscreen Play. Game DLLs continue to use the script reload queue. AI tools must build/deploy game DLLs; the Editor does not compile source files. Runtime/shipping builds, shaders and engine/API changes are excluded.

File notifications trigger background reconciliation. A periodic content audit recovers missed notifications, including changes that preserve file size and timestamp. A batch must remain unchanged for 500 ms. Project switching resets observation, and a busy scene loader retains later batches for subsequent frames. Asset Browser Refresh also requests reconciliation.

Loaded textures are replaced in their existing texture slot after decoding and GPU allocation succeed. IDs remain stable, sizes update, and engine billboard/particle/IBL consumers resolve the current SRV. Paths are normalized, including relative aliases and case differences. Cubemap changes refresh skybox/IBL references. Font atlases update in place, preserving pointers held by text renderers.

Changes requiring new objects use transactional scene preparation. This includes scene/Prefab/UI data, models, material-owned textures, audio, effects and gameplay DLLs. Successful Play reloads restart the current scene in its previous presentation mode; gameplay state can reset. Model candidates remain separate until old scene users are destroyed. Invalid scene JSON, referenced images or model candidates keep the previous scene. Unsaved Editor changes use the existing keep-Editor/load-disk conflict dialog. Deleted assets leave existing resources alive and report the missing path; references are not rewritten automatically.

The content indicator reports pending/success/failure; scene and script indicators retain detailed errors. Content diagnostics go to `content_hot_reload.log` in the project root and rotate by truncation at 1 MiB. Scene timing diagnostics remain in `editor_scene_loading.log`.

## Validation

- `tools/run_content_reload_checks.cmd`: background notifications, debounce/batching, same-stamp overwrite, project switching, 20 changes, and D3D11 WARP texture identity/size/SRV replacement/corrupt-file rollback.
- `tools/run_scene_hot_load_check.ps1`: actual Debug Editor host with temporary scenes; checks scene/Canvas/settings rollback, unsaved conflicts, viewport/fullscreen automatic Play restart, DLL failure/recovery, automatic asset indexing, 20 live texture replacements, invalid textures, new texture references and Prefab edits. It restores the original scene and removes successful asset fixtures.
- `tools/run_2d_regression_checks.cmd`: existing focused engine regressions.

The host check starts from the most recently opened project. Do not run it while another Editor is editing that same project. Automated host evidence does not replace visual acceptance of every asset format in the game. One Editor restart is required to load this engine-side implementation; later supported content changes use hot reload.

## Verified delivery evidence (2026-10-25)

Debug Editor build and the existing 2D regression suite pass. The actual Editor host opened Village & Goblin, passed all scene/texture/Prefab/model scenarios, and restored its original scene. Twenty live texture replacements across viewport/fullscreen Play measured a maximum of 1.033 seconds. D3D11 WARP tests also passed font pointer/atlas-ID preservation and invalid-font rollback.

Sound/effect/material/animation-specific interactive acceptance and manual visual acceptance of the complete game remain unverified. Their content routes are implemented; these automated results must not be described as proof of every format's appearance or sound.
