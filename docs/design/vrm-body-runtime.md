# Experimental VRM reference-body runtime contract

## Status and claim boundary

The current EPR/VRM path is an **experimental supported-reference-avatar vertical slice**. It proves
that Eidolon's 3D performance runtime can drive one manually selected VRM 1.0 body through the
shared native DirectComposition presentation path, with the SDL path retained as a fallback. It is
not general VRM 1.0 support, and no user-facing surface may describe it as "load a VRM" or imply
compatibility with arbitrary conforming avatars.

The experiment may land only behind an explicit experimental/reference-body boundary. Broad VRM
support requires the correctness and compatibility gates in this document rather than more models
happening to work by accident.

## Separate body system and coexistence

EPR/VRM is a separate body-performance system from the 2D portrait director. The systems coexist in
one product but do not own or call into each other's renderer-specific state.

- the portrait system owns portrait labels, atomic image selection, whole-image acting, and portrait
  motion;
- EPR owns 3D behavior planning, temporal commitment, body-resource arbitration, canonical physical
  control, and interruption/settling;
- the VRM adapter owns VRM/glTF semantics and projection of accepted 3D control into model-local
  state;
- both systems may consume the same normalized operational, semantic, attention, and delivery
  evidence through renderer-neutral boundaries;
- sharing evidence does not require the portrait path to run through EPR or require EPR to adopt
  portrait labels and motion primitives;
- body selection, failure, or fallback must preserve persona identity, session identity, dialogue,
  and the other body implementations;
- inactive renderers remain uninitialized unless an explicit transition or authoring workflow needs
  them.

The portrait remains the daily-driver proving body. Progress or failure in the EPR/VRM experiment
does not redefine the 2D alpha gate, and future 3D default selection does not retire portrait or
sprite bodies.

## Runtime ownership

The VRM body runtime parses one asset, publishes a normalized body profile and truthful capability
states, and projects immutable canonical control into model-local state. It is the boundary between
EPR semantics and VRM/glTF representation.

It does not select behavior, own source/session identity, schedule time, arbitrate resources, render
pixels, own presentation, or change the behavior of another body renderer.

VRM humanoid metadata is authoritative for a VRM body. Node-name aliases and Rio-specific bone
names are not used to claim VRM conformance. EPR consumes a normalized profile and never reads VRM
extension objects, material extensions, or glTF node names.

## Current supported-reference-avatar slice

The implementation currently exercises a deliberately narrow asset dialect:

- binary glTF containing `VRMC_vrm` version 1.0;
- at least one skin, with every skin sharing one joint palette and inverse-bind set;
- fewer than 256 joints;
- triangle primitives with `POSITION`, plus `JOINTS_0` and `WEIGHTS_0` for skinned nodes;
- four decoded influences per vertex;
- a bind-face zero-expression baseline plus one explicitly selected `relaxed` position-morph
  runtime channel; `neutral` is parsed independently and is never activated implicitly;
- base-color factor/texture and opaque, mask, or ordinary alpha blending;
- embedded PNG images and one renderer-global linear clamp sampler;
- the D3D11 model renderer submitted either directly to the native DirectComposition body target or
  through the explicit SDL legacy fallback.

This list describes the current implementation, not a valid definition of VRM compatibility.
Unsupported or untested glTF/VRM features must not be reported as executable merely because their
extensions are present.

## Capability truth

Every optional VRM feature uses a state that distinguishes declaration from execution:

```text
ABSENT      extension or semantic feature is not declared
DECLARED    declaration exists at the specification-defined location
PARSED      supported metadata was parsed and validated
EXECUTABLE  the active adapter and renderer can execute the validated subset
```

MToon state is material-specific before it is summarized at body level. Expressions retain state
independently; one unsupported expression or bind type cannot erase every other expression. Look-at
reports its type and authored mappings separately from eye-bone presence. Spring bones and node
constraints remain declared/not executable until their update stages exist.

Capability loss is local. A declared-but-unexecutable expression cannot disable posture, a missing
look-at implementation degrades to head gaze, and a failed VRM body cannot disable portrait,
sprite, dialogue, or session observation.

## Correctness gates

The first corrective implementation slice now:

1. detects `VRMC_materials_mtoon` per material, validates `specVersion`, retains material-specific
   states, and summarizes them without claiming shader execution;
2. parses `neutral` and `relaxed` independently, preserves a precise per-expression diagnostic,
   reports capacity overflow locally, and leaves material/texture expressions declared but not
   executable;
3. uses the bind face as the zero-expression baseline and maps EPR's focused semantic channel to
   executable `relaxed` only at the projection boundary, including `isBinary` behavior;
4. parses authored look-at type, offset, and range maps but truthfully reports it parsed/not
   executable and degrades to head-only gaze;
5. rejects matrix-bearing nodes with a precise supported-slice diagnostic;
6. validates the nearest humanoid ancestor through allowed intermediary nodes and requires positive
   nonzero humanoid scale;
7. constructs UTF-16 surrogate pairs correctly in compact metadata strings.

Remaining correctness work includes retaining all preset/custom expression definitions rather than
only the two selected reference-body candidates, parsing material-color and texture-transform bind
payloads, distinguishing metadata output truncation from malformed JSON, preserving the complete
author list in authoritative metadata, and replacing the raw `VRMC_vrm` JSON hash before any code
uses it as durable asset identity.

## Pose composition and ownership

The body adapter may write only semantic resources present in the accepted canonical transaction.
It may not reset or invent unowned channels. In particular, absence of a left-arm contribution
cannot authorize a body-local relaxed left-arm pose.

The target 3D evaluation order is:

```text
base or imported pose
        -> EPR-owned humanoid deltas and IK
        -> authored VRM look-at
        -> expression evaluation
        -> node constraints
        -> spring bones
        -> final hierarchy and skin matrices
```

This order belongs only to the rigged 3D body system; it does not wrap the portrait renderer.

At load time, the adapter precomputes bind-space correction frames for controlled humanoid bones.
Canonical anatomical pitch, yaw, and roll are transformed through those frames rather than assuming
that every bone uses model-local X/Y/Z as anatomical axes.

Projection solves into a complete scratch pose and atomically commits its local TRS and expression
result only after hierarchy/world validation. Failure never mutates the live rig or expression.
The projection retains an explicit captured base pose for its owned nodes, so a future imported
animation stage can publish a fresh base before EPR evaluation; constraints and secondary physics
remain downstream consumers of the committed pose.

The current projection no longer resets the entire rig or writes an unowned left-arm baseline. It
stages the captured base TRS only for EPR-owned chest, head, optional eye, and right-arm nodes while
copying every unowned live channel into scratch. Controlled torso, head, eye, and wrist rotations use
precomputed bind-world correction frames; right-arm targeting remains world-space. Callers that add
imported animation must capture the new base after animation and before projection. VRM look-at,
constraints, and spring bones then compose after EPR in the documented evaluation order.

## Body profile

The adapter publishes two related body-semantic records.

The bounded EPR control profile contains only the measurements required by the current control
slice:

- model-space forward, up, and right axes;
- semantic shoulder/head positions and right-arm segment lengths;
- conservative reach and joint limits;
- required humanoid/right-arm availability;
- independent eye, look-at, and expression support states;
- a deterministic anatomy fingerprint.

The complete VRM measurement record contains bind positions and nearest-semantic-parent segment
length for every mapped humanoid role, plus skeleton height, shoulder width, torso length,
bilateral upper/lower arm and leg lengths, and the orthonormal anatomical frame. It is derived from
authoritative `VRMC_vrm` humanoid mapping rather than node aliases. Its fingerprint hashes
quantized semantic bind measurements: a skeleton change invalidates calibration, while a
texture-only change need not.

The anatomy fingerprint is not a distribution identity, rights identity, or hash of the entire
VRM file. The raw `VRMC_vrm` JSON hash likewise remains runtime metadata only. Character-package
identity and durable asset migration require a separate owning contract.

Authored look-at mappings, bind correction frames, body-specific joint ranges, and mesh-derived
clearance volumes extend these records only when their ownership and validation are explicit. The
profiles do not expose renderer resources to EPR.

## Model calibration and generated realization

The primary realization path is calibration-first. Eidolon measures a new VRM automatically, then
asks the user to approve a small vocabulary of semantic anchors while the actual EPR scenario is
running. Runtime behavior is derived from those anchors; model-specific guessed pose constants are
not the compatibility strategy.

```text
VRM humanoid mapping + bind transforms
        -> anatomical axes, segment lengths, proportions
        -> matching partial calibration sidecar
        -> user-approved semantic anchors
        -> EPR phase/intensity/transition generation
        -> IK, limits, bind correction, residual correction
        -> transactional pose commit
```

An anchor stores declared body-resource ownership, normalized task-space torso/head/arm controls,
and optional model-local quaternion residuals. Task-space values transfer scale and proportion;
residuals reproduce corrections that the generic solver cannot infer from lengths alone. Both are
needed: skeletal measurement provides mechanics, while user approval provides intentional acting.

The first anchor registry covers neutral, attentive, thinking, responding, contrast preparation /
peak / recovery, and interrupted/guarded. The format supports partial progress. Missing anchors
disable or locally degrade only their behavior family; they do not authorize hard-coded fallback
postures or whole-rig resets.

The implemented authoring slice measures and fingerprints the skeleton, parses and validates
versioned partial sidecars transactionally, freezes named controls from the actual fixture, applies
live task-space edits through the same scratch projection, and writes deterministic sidecars with
same-directory atomic replacement. It looks for
`<model-path>.epr-calibration` unless `EIDOLON_VRM_CALIBRATION_PATH` explicitly selects another
file. Run `make vrm-calibrate VRM_PATH=...` to author one. Loading currently does not alter ordinary
playback; program generation from approved anchors is the next implementation slice.

VRMA remains an optional later realization generator for authored nuclei, hand shapes, showcase
motion, or genuinely new motion families. It is not required for calibration-derived posture,
transition, interruption, or settling.

## Verification split

Structural validity and runtime executability are separate results:

```text
vrm-structure-check
    glTF and VRMC_vrm parsing
    required humanoid roles, hierarchy, and positive scale
    metadata and declared capability report

vrm-runtime-check
    buffer and image loading
    geometry, skin palette, and sampler construction
    projection initialization against the loaded rig
    shader/pipeline creation and hidden GPU frame
    executable capability report
```

`make vrm-structure-check VRM_PATH=...` owns the structural/profile preflight and `make vrm-check`
remains its compatibility alias. `make vrm-runtime-check VRM_PATH=...` invokes the actual application
renderer, advances all 251 fixture samples through projection, and requires geometry, decoded
textures, GPU skinning state, shaders/pipeline state, and a presented hidden frame. Structural
success alone remains insufficient, and runtime success applies only to the tested asset/machine.

The compatibility corpus must cover, at minimum: no expressions, `relaxed` only, material-only
expression, expression look-at, asymmetric bone look-at, per-material MToon, JPEG, URI images,
repeat samplers, rotated bind frames, matrix nodes, more than 16 binds, differing skin palettes,
invalid humanoid hierarchy, and zero or negative humanoid scale. Official sample models should
supplement the supported reference avatar.

## Asset and configuration

The first reference implementation uses
[DECAGRAMMATON on VRoid Hub](https://hub.vroid.com/characters/61437424751231571/models/3310288597351780654).
The operator owns the Pixiv-authenticated acquisition step and passes the resulting path through
`EIDOLON_VRM_PATH`. Eidolon must not download the asset, receive Pixiv credentials, weaken embedded
terms, or add the asset to Git. The current terms forbid redistribution and modification and require
credit.

An invalid or unavailable reference VRM disables only that experimental body variant. The portrait
default and all source/session/dialogue behavior remain operational.

## Landing and expansion order

Before landing the experiment as a supported-reference-avatar slice:

1. make the experimental/reference-model boundary visible in configuration, logs, and docs;
2. close the MToon, expression-baseline, look-at-truth, matrix-node, and humanoid-validation gates;
3. make structural checking report only what it actually validates;
4. run deterministic EPR traces and the hidden 3D reference-body scenario;
5. replace the provisional guessed pose targets with anatomy-bound, user-approved calibration
   anchors and derived transitions;
6. complete the owner-controlled visible performance review without changing the portrait default.

The implementation gates through the hidden run are closed for DECAGRAMMATON. The first visible
review rejected the provisional pose authorship while accepting the camera, rig, and harness. The
measurement, versioned profile, interactive capture/application, and atomic-save slices are now
implemented; calibrated program generation and a new owner review remain. Broad compatibility
stays outside this claim.

The visible calibration/review harness uses the shared borderless transparent DirectComposition
body target and remains distinct from headless snapshot mode. Wheel scaling resizes the overlay
around the body instead of magnifying into a fixed render rectangle, while the orthographic depth
range contains the model's rotation-safe bind volume. The renderer submits directly to the native
D3D11 target; projected skinned geometry supplies click-through without framebuffer readback. This
presentation integration is owner-accepted around the reference slice; it is not new EPR or broad
VRM ownership.

Only then broaden compatibility through JPEG/URI decoding, authored samplers, material-color and
texture-transform expressions, deliberate MToon fallback/shading, constraints, spring bones,
fingers, locomotion, balance, and a wider gesture catalogue. None of those expansions turns off or
absorbs the 2D body systems.
