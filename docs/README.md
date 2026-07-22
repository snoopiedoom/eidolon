# Eidolon documentation

The main README is the project showcase and shortest path to a running build. These documents own
the details needed to change Eidolon without rediscovering its boundaries.

## Product direction

- [Product brief](product-brief.md) — positioning, audience, product boundary, and long-term
  direction.
- [V1 goal](v1-goal.md) — the first product milestone and its acceptance sequence.
- [Current project state](project-state.md) — verified implementation, V1 scorecard, limitations,
  and restart checklist.

## Technical reference

- [Architecture](architecture.md) — runtime ownership, rendering paths, text/affect boundaries, and
  performance invariants.
- [Development](development.md) — dependencies, builds, tests, hidden visual QA, logging, and working
  conventions.
- [Configuration](configuration.md) — system defaults, character manifests, motion tuning, and
  sparse user overrides.
- [Integrations](integrations.md) — agent adapters, live streams, legacy readers, normalized
  session identity, and bubble lifecycle.
- [Assets](assets.md) — 2D portrait layout, sprite atlases, Blender/GLB authoring, and Rio-specific
  pipeline notes.

## Design specifications

Design documents describe intended behavior and invariants. They should remain useful even after a
particular implementation changes.

- [Expression performance](design/expression-performance.md)
- [Procedural motion](design/procedural-motion.md)
- [Multi-session dialogue](design/session-dialogue.md)
- [Presence contract](design/presence-contract.md)
- [Body capabilities](design/body-capabilities.md)
- [Native presentation and graphics stack](design/native-presentation.md)

New designs belong under `docs/design/`. Each specification should state the problem, goals,
non-goals, ownership, data flow, invariants, failure behavior, and acceptance criteria. Temporary
experiments and chronological session notes do not belong there; conclusions from them do.

When implementation changes a documented boundary, update the owning document in the same change.
When only the current milestone changes, update `project-state.md`.
