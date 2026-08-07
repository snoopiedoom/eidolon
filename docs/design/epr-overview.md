# Eidolon Performance Runtime overview

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
reference body through the shared 3D body renderer. The current calibration-first extension measures
the complete mapped semantic skeleton and transactionally loads partial user-approved anchor
profiles. Interactive capture, live projection, calibrated program compilation, minimum-jerk
posture/gesture sampling, resource-local fallback, model-local residual composition, and
ordinary-playback application are implemented. Owner calibration and performance approval of the
selected reference body are the remaining gate.

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
- [VRM body runtime](vrm-body-runtime.md)
- [tracing and validation](epr-tracing-validation.md)
