# Eidolon Performance Runtime overview

> The active 3D realization plan is the durable [automatic-retargeting
> workstream](../workstreams/epr-automatic-retargeting.md). Any calibration-first status language
> below describes the existing reference-body vertical slice, not the product compatibility
> requirement.

For a concise implementation tour with source and test evidence, read the
[EPR engineering walkthrough](../epr-engineering.md).

## Purpose

The Eidolon Performance Runtime (EPR) turns accepted operational and semantic evidence into a
bounded, deterministic stream of body-control snapshots. It owns the decisions that make a shared
body behave coherently over time: behavior planning, temporal commitment, body-resource ownership,
realization, interruption, physical validation, and causal trace.

EPR does not own agent transports, source identity, session truth, dialogue text, classifiers,
character selection, graphics, windows, scene composition, or presentation.

## Runtime boundary

```text
A2 accepted source/session truth + semantic/delivery evidence
        |
        v
Performance Intent ingress
        |
        v
behavior plan + temporal dispatcher
        |
        v
body-resource arbitration
        |
        v
modality realizers -> Realization Programs
        |
        v
controller composition + transactional canonical solve
        |
        v
immutable canonical-control snapshot
        |
        v
body adapter -> renderer -> scene -> presentation
```

The first vertical slice uses deterministic synthetic evidence. A temporary adapter may translate
the current normalized conversation path, but all compatibility identity remains inside that
adapter. A2 will eventually provide the authoritative `(source_id, session_id)` provenance.

## Relationship to 2D bodies

EPR is the physical behavior runtime for the rigged-3D workstream. It is not the implementation of
the 2D portrait director. The portrait system continues to own discrete portrait expression
selection and whole-image performance while EPR owns 3D temporal planning, resource grants,
canonical physical control, and model projection.

Both systems may consume the same renderer-neutral operational, affect, semantic-beat, delivery,
and attention evidence. That shared evidence is a product boundary, not shared mutable body state.
Neither system may import the other's asset labels, motor state, pose representation, or renderer
resources. Selection or failure of an EPR/VRM body leaves the portrait and sprite systems available
with session and dialogue continuity intact.

## Ownership

One `EidolonPerformanceRuntime` instance owns:

- the current accepted intent revision;
- immutable published behavior-plan generations;
- temporal constraints and dispatcher commitments;
- explicit body-resource claims, grants, transfers, and cleanup obligations;
- active Realization Programs;
- current and last-valid canonical control state;
- the monotonic published control revision;
- a bounded structured trace.

The runtime facade sequences these owners. It does not make modality-specific decisions itself.

The body adapter owns model-local nodes, axes, bind transforms, morph targets, and capability
projection. The renderer owns GPU state, skinning, camera, textures, and pixels. Presentation owns
windows, targets, input, cadence, and scene commits.

## Cadences and publication

Planning accepts immutable evidence outside the frame-critical path and publishes a complete plan
generation. Control sampling runs at fixed integer logical ticks and reads exactly one published
generation. It performs bounded arbitration, program sampling, composition, validation, and
whole-state commit. Rendering consumes only the latest complete control revision.

The control path may not:

- segment or classify text;
- parse files or configuration;
- allocate an unbounded graph;
- wait for source or classifier work;
- inspect session collections to choose a winner;
- call SDL, D3D11, DirectComposition, Win32, or scene APIs.

## Determinism

Identical accepted evidence, configuration, body profile, seed, and tick sequence must produce
byte-identical normalized trace and canonical output. All tie breaking uses explicit rank fields
and stable ids. Pointer value, insertion order, array position, hash iteration, wall-clock jitter,
and renderer cadence are forbidden decision inputs.

## Failure isolation

Intent, plan, arbitration, realization, solve, body projection, and renderer initialization fail
at their own boundaries. A rejected candidate leaves the last complete valid state published.
Missing optional body capabilities degrade only their channels. A body failure never stops
portrait presentation or source/session observation.

## First-slice scope

The first slice contains neutral idle, listening/attention, thinking, streamed-response posture,
eye-first/head-follow gaze, one restrained right-arm contrast gesture, interruption, explicit
resource transfer, current-state cleanup, settling, deterministic trace, and the supported VRM 1.0
reference body through the shared 3D body renderer. The runtime measures the complete mapped
humanoid skeleton, imports owned normalized VRMA tracks, converts source rest frames into the
destination, and publishes retargeted base poses beneath EPR transactionally. Optional partial
calibration profiles, live projection, calibrated program compilation, minimum-jerk sampling,
resource-local fallback, and model-local residual composition remain available. The remaining gates
are accepted semantic motion vocabulary and multi-body performance approval without
required calibration. The provenance-safe idle/walk base-motion diagnostic is owner-accepted. R4
now has deterministic normalized-pose composition plus versioned semantic generator references and
explicit EPR-resource-to-humanoid-channel mapping. Its bounded semantic catalog resolves borrowed
normalized sources and transactionally narrows requested ownership against declared and actual
sampled channels. Its concrete VRMA adapter validates source clips, derives actual-track ownership,
and converts authored rotations and hips displacement into canonical normalized space through the
same rest-frame contract used by destination retargeting. Its fixed-tick executor now validates
phase shapes, derives source time, applies minimum-jerk transitions, narrows against live grants,
and transactionally composes deterministic frames with distinct absolute and additive semantics.
The model now embeds a versioned semantic motion pack that atomically publishes owned clips and
rebased borrowed catalog contexts only after the whole requested batch validates. It registers the
pinned verified idle as `idle.neutral` when present and lends the resulting catalog to the EPR
runtime. Only complete plans publish a normalized frame; unresolved active semantics clear stale
motion and fall back locally to the
accepted canonical controller. Projection atomically commits imported base, normalized frame,
optional residuals, and explicit procedural owners, and retains the frame across subsequent
base-motion samples. Canonical-control version 4 exposes live-granted head/eye gaze, expression,
weighted right-arm IK, and tokenized arm continuity above normalized posture/gesture without leaking
legacy posture, combined head control, or arm anchors into the transaction. Settle owns no catalog
source: projection captures the exact outgoing model-local arm pose once per behavior token, blends
it into the normalized pose, applies weighted IK afterward, and commits capture state only with the
whole scratch pose. R4 now also accepts a versioned right-arm task-target stream that remains
separate from Performance Intent. Publication proves a monotonic producer chain, current plan
generation, exact active behavior, and a base/override arm claim covering the complete short
validity interval. Only the exact live grant can apply it; expiry, plan replacement, rejection,
solve rollback, and accepted/applied/released trace evidence are explicit. A reproducible pinned CMU
conversation take is now unlabeled review material. The remaining R4 work is to visibly select,
slice, and atomically bind its provenance-safe vocabulary, not calibrate models one by one.

DirectComposition target submission is now presentation integration around this slice, not EPR
ownership. The EPR slice still excludes default-3D selection, locomotion, balance/contact planning,
fingers, a gesture catalogue, learned generation, and arbitrary non-VRM humanoids.

The VRM output is an experimental supported-reference-avatar slice, not broad VRM 1.0 support. Its
correctness and compatibility gates belong to the
[experimental VRM reference-body contract](vrm-body-runtime.md). None of those gates changes the
portrait runtime or makes 2D acceptance depend on 3D progress.

## Related contracts

- [Performance Intent](epr-performance-intent.md)
- [behavior plan and temporal dispatch](epr-behavior-plan.md)
- [body resources](epr-body-resources.md)
- [Realization Programs](epr-realization-program.md)
- [semantic motion catalog](epr-motion-catalog.md)
- [semantic motion-pack ownership](epr-motion-pack.md)
- [dynamic task targets](epr-task-targets.md)
- [VRM body runtime](vrm-body-runtime.md)
- [tracing and validation](epr-tracing-validation.md)
