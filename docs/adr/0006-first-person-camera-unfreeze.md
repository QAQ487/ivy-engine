# ADR 0006: Unfreeze for first-person camera (mirror TPS)

Accepted — 2026-09. Partially unfreezes ADR 0005 for one additive camera path.

The remaining freeze restrictions and limited unfreeze scope are superseded by ADR 0007 (2026-10-20). The first-person camera design decision below remains in effect.

Backrooms Showcase (look-dev / video hand-in) needs a real first-person mode. Alternatives were hacking `ThirdPerson` distance≈0 or parenting a scene `Camera`. We chose a parallel stack: `CameraMode::FirstPerson`, `FirstPersonCamera_*` producer, `CameraDirectorSource::FirstPerson`, and ScriptRuntimeApi v42 appends. An early `SampleFirstPersonScript` demo was removed in a later repo cleanup; content projects should author their own first-person script against those APIs. TPS stays untouched. PostFX and other engine surfaces remain frozen.
