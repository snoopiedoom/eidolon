# Eidolon documentation

The main README is the project showcase and shortest path to a running build. These documents own
the details needed to change Eidolon without rediscovering its boundaries.

## Reference

- [Architecture](architecture.md) — runtime ownership, rendering paths, text/affect boundaries, and
  performance invariants.
- [Development](development.md) — dependencies, builds, tests, hidden visual QA, logging, and working
  conventions.
- [Configuration](configuration.md) — system defaults, character manifests, motion tuning, and
  sparse user overrides.
- [Integrations](integrations.md) — provider adapters, live streams, legacy readers, normalized
  session identity, and bubble lifecycle.
- [Assets](assets.md) — 2D portrait layout, sprite atlases, Blender/GLB authoring, and Rio-specific
  pipeline notes.
- [Project state](project-state.md) — verified implementation state, unresolved problems, current
  priorities, and the restart checklist.

## Design specifications

Design documents describe intended behavior and invariants. They should remain useful even after a
particular implementation changes.

- [Expression performance](design/expression-performance.md)
- [Procedural motion](design/procedural-motion.md)
- [Multi-session dialogue](design/session-dialogue.md)

New designs belong under `docs/design/`. Each specification should state the problem, goals,
non-goals, ownership, data flow, invariants, failure behavior, and acceptance criteria. Temporary
experiments and chronological session notes do not belong there; conclusions from them do.

When implementation changes a documented boundary, update the owning document in the same change.
When only the current milestone changes, update `project-state.md`.
