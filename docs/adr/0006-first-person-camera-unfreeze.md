# ADR 0006: Unfreeze for first-person camera (mirror TPS)

Accepted — 2026-09. Partially unfreezes ADR 0005 for one additive camera path.

Backrooms Showcase (look-dev / video hand-in) needs a real first-person mode. Alternatives were hacking `ThirdPerson` distance≈0 or parenting a scene `Camera`. We chose a parallel stack: `CameraMode::FirstPerson`, `FirstPersonCamera_*` producer, `CameraDirectorSource::FirstPerson`, ScriptRuntimeApi v42 appends, and `SampleFirstPersonScript`. TPS stays untouched. PostFX and other engine surfaces remain frozen.
