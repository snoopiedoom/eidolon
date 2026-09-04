# Eidolon documentation

The main README is the project showcase and shortest path to a running build. These documents own
the details needed to change Eidolon without rediscovering its boundaries.

## Start with EPR

- [EPR engineering walkthrough](epr-engineering.md) — the problem, architectural decisions,
  implementation tour, reproducible checks, and honest completion boundary.
- [EPR architecture and contracts](design/epr-overview.md) — ownership of intent, plans, resources,
  realization, and body projection.
- [Automatic humanoid motion and retargeting](workstreams/epr-automatic-retargeting.md) — completed
  R1–R3 and the active R4 semantic-motion workstream.
- [Motion catalog](design/epr-motion-catalog.md), [motion-pack ownership](design/epr-motion-pack.md),
  and [dynamic task targets](design/epr-task-targets.md) — the newer runtime and authoring boundaries.

## Product direction

- [Product brief](product-brief.md) — positioning, audience, product boundary, and long-term
  direction.
- [V1 goal](v1-goal.md) — the first product milestone and its acceptance sequence.
- [Product roadmap](product-roadmap.md) — ordered gates from the daily-driver alpha through public
  V1, plus guarded post-V1 horizons.
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
- [Backend-neutral presentation events](design/presentation-events.md)
- [Presentation environment and output topology](design/presentation-environment.md)
- [Eidolon Performance Runtime](design/epr-overview.md) — independent rigged-3D behavior/runtime
  ownership, with linked contracts for intent, behavior planning, resources, realization, the
  experimental VRM reference body, and deterministic validation. It coexists with rather than
  replacing the portrait director.

## Active workstreams

- [EPR automatic retargeting and semantic motion](workstreams/epr-automatic-retargeting.md) — the
  current engineering focus; shared normalized motion replaces required per-body pose calibration.
- [bgfx and native-presentation workstream](workstreams/bgfx-integration.md) — completed graphics
  evaluation plus the current production-presentation gate, evidence, and restart checklist.
- [Experimental EPR/VRM reference-body workstream](design/vrm-body-runtime.md) — the separate 3D
  vertical slice, its truthful support boundary, correctness/composition gates, and compatibility
  expansion order.

New designs belong under `docs/design/`. Each specification should state the problem, goals,
non-goals, ownership, data flow, invariants, failure behavior, and acceptance criteria. Temporary
experiments and chronological session notes do not belong there; conclusions from them do.

When implementation changes a documented boundary, update the owning document in the same change.
When only the current milestone changes, update `project-state.md`.
