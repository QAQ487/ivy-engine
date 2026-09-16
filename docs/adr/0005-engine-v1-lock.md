# ADR 0005: Engine v1.0 lock for swing-demo

## Status

Accepted — 2026-09

## Context

Swing-demo development needs a stable engine surface (camera trauma/speed FOV, time-scale pulse, line sprite) without further non-additive engine churn.

## Decision

Tag `engine-v1.0-lock` on ivy-engine. The engine is frozen at that tag for swing-demo development; additive changes require an explicit unfreeze.

## Consequences

- Content projects should pin against `engine-v1.0-lock`.
- Non-additive engine edits are out of scope until unfreeze.
- ScriptRuntimeApi is at version 41 (append-only from 40).
