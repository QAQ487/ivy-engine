# ADR 0007: Full engine unfreeze

Accepted — 2026-10-20, by explicit owner instruction.

The owner has ended the engine-wide freeze to resume unrestricted engine development, including planned 2D support. This supersedes the freeze and pinning policy in ADR 0005 and the limited unfreeze scope in ADR 0006: all engine systems, including rendering, PostFX, physics, cameras, editor, scripting, assets, and deployment, are open to feature work, fixes, refactoring, and non-additive changes within the requested task scope, without another unfreeze decision.

`engine-v1.0-lock` remains a historical baseline; existing tags and content-project pins are not changed automatically. Existing compatibility requirements and the first-person camera design remain in effect unless explicitly changed for a task. This policy change authorizes development across the engine; it does not initiate implementation or waive task-specific validation.
