# Runtime applies gravity from project.json

Game_Runtime does not open the editor project, so `physics.gravity` in `project.json` used to stay at the engine default (9.81) in a playable package even when the file was copied next to the exe. Audio buses already load from that file at boot. Gravity now uses the same cwd / exe-adjacent lookup and `Physics_SetGravity`, so Project Settings → Gravity is what players get.

**Considered Options**: Keep gravity editor-only (ADR 0001). That is what shipped the package with 20 in JSON and 9.81 in the sim.
