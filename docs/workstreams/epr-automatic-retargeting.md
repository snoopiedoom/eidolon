# EPR automatic humanoid motion and retargeting

## Status

This document is the durable implementation contract for the active EPR/VRM workstream. It
supersedes the earlier requirement that every usable VRM have eight owner-authored semantic pose
anchors before ordinary playback.

The current implementation already provides the reusable substrate: VRM 1.0 humanoid parsing,
measured anatomy, bind-space correction frames, transactional scratch projection, resource-local
EPR control, native presentation, deterministic tracing, and hidden-frame runtime validation.
Those pieces remain. The realization source changes from mandatory per-body anchor authorship to
shared normalized humanoid motion.

## Progress

- R1 is complete: the 55-role pose vocabulary, strict owned VRMA import, deterministic sampling,
  and focused GNU Make checks are implemented.
- R2 is complete: destination rest-frame conversion, optional-role composition, hips scaling,
  desktop in-place/full root policies, unowned-state preservation, and rollback are implemented.
- R3 is complete: model-owned playback, lifecycle controls, deterministic sampling, retarget
  scratch output, and atomic imported-base/EPR publication are implemented.
- The pinned MIT idle varies every humanoid chain and
  passes 251 sidecar-free samples plus hidden endpoint GPU frames on Vampire Cat.
- The hash-locked CMU neutral walk deterministically maps
  301 source samples into 22 humanoid rotations plus hips translation, varies all seven chains, and
  passes the same sidecar-free hidden GPU gate on Vampire Cat.
- The owner accepted both idle and walk through the native transparent visible-review harness,
  including the walk's deformation, contact, weight, and loop quality.
- R4 is active. Its first renderer-neutral slice validates and composes ordered normalized-pose
  layers by explicit humanoid-role and hips-translation ownership.
- Realization Program version 4 now carries validated semantic generator references with explicit
  EPR resource masks, normalized humanoid channels, hips ownership, blend/intensity/rate, and
  takeover policy; gaze and expression remain procedural.
- The fixed-capacity semantic catalog resolves unique generator identifiers to borrowed normalized
  sources, retains stable provenance and timing metadata, and intersects requested ownership with
  both declared source coverage and the pose actually sampled. Typed failure and all mutations are
  transactional.
- The fixed-tick motion executor validates behavior/generator semantics and exact phase shapes,
  derives source-local and normalized time without wall clock, applies minimum-jerk transitions,
  narrows programs to live grants, and transactionally composes one deterministic normalized frame.
  Base and override clips are absolute; additive clips contribute intensity-scaled quaternion and
  hips residuals rather than replacing posture.
- Canonical-control version 4 adds explicit weighted right-arm IK plus tokenized interruption
  continuity. Settle is procedural rather than a catalog clip: the VRM projection transaction
  captures the exact outgoing shoulder/arm/hand local rotations once per settle behavior, blends
  them into the new normalized pose, applies any weighted IK refinement, and publishes continuity
  state only after the complete scratch pose succeeds.
- A versioned right-arm task-target ingress now accepts short-lived body-relative samples from one
  monotonic producer stream. Publication validates the current plan generation, exact active
  behavior, full base/override claim interval, finite bounds, and predecessor chain. Runtime
  sampling requires the exact live grant; expiry and plan replacement release explicitly, rejected
  samples do not mutate the stream, and solve failure does not advance the applied revision. Trace
  version 3 records the complete accepted/rejected/applied/released lifecycle.

The native visible-review harness loops until explicitly closed and is accepted for both R3 clips.
The conversation range-review harness is also owner-accepted for smoothness after the release-build,
keyframe-search, model-cadence, and compact-input-mask fixes. This confirms interactive performance
on the development machine, not a cross-machine FPS budget. The first bounded semantic slice remains
unaccepted; performance acceptance alone does not assign a semantic label or approve a motion pack.

## Product decision

A structurally supported VRM 1.0 humanoid must reach a credible animated baseline without a
calibration sidecar or a pose-editing session.

Automatic playback consists of:

1. a body-independent normalized humanoid motion or procedural generator;
2. standards-based conversion from the motion's authored T-pose into the destination VRM's
   authored T-pose;
3. deterministic motion sampling, masking, blending, and transition ownership;
4. EPR scheduling, interruption, gaze, expression, and dynamic task-space control;
5. atomic projection into the selected VRM followed by constraints and secondary motion.

Manual calibration remains optional package-author tooling for unusual limits, incorrect palm or
foot frames, difficult self-contact, clothing/contact volumes, poor deformation, signature acting,
or clip-specific residual repair. Its absence is never an ordinary playback failure.

## Runtime ownership

```text
session truth + semantic performance intent
                    |
                    v
           EPR realization programs
                    |
                    v
 normalized humanoid motion sources + procedural controllers
                    |
                    v
       sampled/mixed canonical humanoid pose
                    |
                    v
 destination VRM T-pose conversion and base-pose commit
                    |
                    v
 EPR additive posture, gaze, expression, dynamic IK, interruption
                    |
                    v
      look-at -> expressions -> constraints -> spring bones
                    |
                    v
                  renderer
```

- Performance Intent and Behavior Plans never name files, model nodes, or animation clips.
- Motion manifests map semantic generator identifiers to VRMA assets, annotated ranges, phase
  points, resource masks, intensity policy, and fallbacks.
- The normalized humanoid core owns no renderer, presentation, native-window, or model-node state.
- The VRM adapter owns source/destination rest-frame conversion and model-local projection.
- Presentation continues to own the desktop target. Hips motion cannot move the native window;
  desktop locomotion is in-place unless a future body-space policy explicitly says otherwise.
- Portrait, sprite, and legacy Rio performance systems remain independent and do not execute
  through this runtime.

## Normalized humanoid contract

The internal pose vocabulary covers the complete VRM humanoid role set, including optional chest,
shoulder, eye, jaw, toe, and finger roles. A sampled pose contains:

- a role mask and normalized quaternion per present humanoid role;
- optional hips translation;
- optional semantic expression weights;
- optional gaze direction;
- deterministic tick, duration, loop, and provenance metadata.

VRMA is a transport and authoring format, not the EPR semantic protocol. Import compiles a VRMA
file into owned normalized tracks. The runtime must not retain pointers into temporary cgltf data.

For VRM 1.0 rest-frame conversion, each sampled source local rotation is converted through the
source rest local/world rotations into `NormalizedLocalRotation`, then reconstructed through the
destination rest world/local rotations. Missing optional roles follow the VRMA compatibility
rules. Hips translation scales by destination/source T-pose hips height. Non-humanoid nodes and
unowned humanoid resources remain unchanged.

## Motion composition

The first motion vocabulary must cover:

- neutral idle;
- attentive/listening transition and loop;
- thinking weight shift;
- responding/opening motion;
- presenting/contrast gesture variants;
- interruption/guarding and recovery;
- neutral settling;
- an in-place walk diagnostic that exposes full-body retargeting defects.

EPR schedules motion generators by semantic family, phase, normalized time, resource mask, blend
weight, intensity, playback rate, and takeover policy. Gaze, blink, expression, target-following,
and genuinely dynamic contact remain procedural layers. IK refines meaningful targets; it does not
invent the entire resting pose.

Motion assets require explicit provenance and redistribution/use evidence. Eidolon does not scrape
VRoid Hub, VRChat, or another application's preview motions.

## Implementation sequence

### R1. Contract and normalized motion core — complete

- replace calibration-first product, roadmap, state, and user-facing instructions;
- add the complete normalized humanoid role/pose vocabulary;
- parse and validate `VRMC_vrm_animation` 1.0 with the existing pinned cgltf;
- copy animation tracks into owned runtime data;
- sample STEP, LINEAR, and CUBICSPLINE tracks deterministically;
- add focused parser/sampler fixtures and GNU Make targets.

### R2. Destination retargeter — complete

- precompute source and destination T-pose local/world rotations;
- convert normalized rotations into destination local rotations;
- combine rotations when optional source/destination roles differ;
- scale hips translation and apply desktop in-place root-motion policy;
- preserve scale, non-humanoid nodes, and unowned resources;
- prove transaction rollback and finite hierarchy reconstruction.

### R3. Imported base-pose runtime — complete

- let the model runtime own motion clips and playback instances;
- sample/mix into a scratch rig before EPR;
- atomically publish the accepted animation as the projection base;
- expose pause, scrub, loop, clip identity, phase, and failure diagnostics;
- render an idle and walk diagnostic on the current private development body without a sidecar.

All five bullets are implemented. `EIDOLON_VRMA_PATH` selects a local clip for development;
`make vrm-playback-check` covers ownership, play/pause/seek/loop, root policy, deterministic failure,
same-control recomposition over successive imported bases, and rollback. `make vrma-check` reports
exact varying tracks and root/torso/head/limb-chain coverage. The official pixiv `test.vrma` remains
a one-track conformance fixture, not visible R3 evidence.

`make vrma-idle-fixture` fetches a pinned, hash-verified MIT full-body idle into ignored build
storage. `make vrma-walk-fixture` fetches pinned CMU neutral-walk BVH and usage-rights inputs, verifies
their hashes, deterministically collapses auxiliary joints into the VRM humanoid hierarchy, selects
frames 141 through 441 as a steady 2.5-second loop, verifies the generated VRMA hash, and preflights
all seven varying chains. `make vrm-animation-runtime-check VRM_PATH=... VRMA_PATH=...` clears
calibration and proves five seconds of parsing, sampling, retargeting, atomic base publication,
skinning, shaders, and hidden GPU frames. Both fixtures pass that gate on Vampire Cat. The owner
accepted both idle and walk through the same native harness; R3 is closed. The official VRoid
Project gesture pack supplies neither fixture.

`make vrm-animation-review VRM_PATH=... VRMA_PATH=...` owns that explicit native checkpoint and will
not close itself after a timed sequence. It remains available for later clip/body reviews.

### R4. EPR motion programs

- replace body-fingerprinted anchor lookup with semantic motion-generator references;
- add masked motion layers and deterministic transitions to realization programs;
- retain additive/procedural canonical control for gaze, expression, dynamic IK, and small posture
  variation;
- preserve resource arbitration, interruption generations, and trace evidence;
- remove missing-anchor fallback as an ordinary behavior path.

The first R4 slice is implemented in the normalized core: ordered layers carry explicit rotation
and hips-translation masks, blend by deterministic shortest-arc quaternion interpolation, preserve
unowned channels byte-for-byte, and reject invalid composition transactionally. EPR generator and
resource scheduling now consume the same ownership vocabulary: torso, head, eyes, and complete arm
chains map to disjoint normalized channels, while imported base motion retains every unclaimed
channel. Versioned semantic references are compiled for idle, posture, contrast, and settling with
explicit base/additive/override policy. The catalog bridge is now implemented without file, clip,
renderer, or model ownership: it accepts only canonical normalized source callbacks, returns typed
missing/procedural/incompatible/sample failures, and narrows ownership again after every sample.
The VRMA source adapter now validates borrowed immutable clips, derives ownership from actual
tracks, and converts source-local samples through authored rest frames into catalog-ready normalized
poses. Import and destination projection share one renderer-neutral rest-space conversion contract,
including authored hips-height normalization. The fixed-tick executor now selects deterministic
source time, applies minimum-jerk enter/hold/exit envelopes, narrows against live grants, orders
base/cooperative/additive/override contributions, and drives the compositor transactionally. The
model owns the first provenance-safe `idle.neutral` binding and the runtime publishes complete
normalized frames through the imported-base/EPR transaction. That transaction now orders imported
base, normalized posture/gesture, optional residuals, and explicit procedural owners. Live-granted
head/eye gaze, expression, right-arm continuity, and weighted IK compose last; legacy posture,
combined head control, and right-arm anchors are used only by the complete canonical fallback.
A settle program owns the arm resource and timing but has no catalog source: projection captures the
exact outgoing model-local arm pose once per behavior token and shortest-arc blends it into the new
normalized pose without rebasing on revised interruption intent. Capture and token state roll back
with a rejected scratch transaction. The real dynamic task-space ingress is now connected through
a distinct controller stream rather than Performance Intent: versioned body-relative arm targets
must prove their producer chain, plan generation, exact behavior and claim interval, then remain
eligible under that exact live grant. Canonical solve failure retains the last applied revision and
projection consumes successful weighted IK through its existing whole-pose transaction. The
host-side [semantic motion pack](../design/epr-motion-pack.md) now owns clip lifetime and publishes
multi-binding catalog extensions atomically only after every path, binding, source, and final
borrowed-context rebase validates. `make semantic-motion-pack-check` proves rollback. The pinned CMU
`18_08` conversation capture now has a reproducible hash-verified full-take VRMA candidate with all
seven varying chains. The owner accepted the full capture as viable source material, but it remains
deliberately unlabeled. The native authoring harness now records explicit `I`/`O` millisecond marks,
replays the range, and publishes only an Enter-accepted ignored selection. The deterministic slice
tool strictly binds that artifact to the reviewed clip duration, snaps marks to the pinned BVH
cadence, records exact source frames in the derived VRMA, and preflights the result. Range state,
artifact validation, frame resolution, and provenance are covered by GNU Make checks. R4 next
visibly accepts the exact derived slice, pins its hash and semantic metadata, then repeats for the
remaining provenance-safe gesture/posture vocabulary.

### R5. Motion vocabulary and compatibility

- acquire or author the provenance-safe first motion pack;
- annotate phases, masks, timing, intensity, and takeover behavior;
- run the same vocabulary on structurally different VRM 1.0 bodies;
- classify unsupported capabilities and deforming models without opening calibration;
- keep optional package overrides versioned and anatomy-bound.

### R6. Acceptance

- extend `vrm-runtime-check` with clip parsing, sampling, retargeting, mixing, projection, skinning,
  shaders, and a hidden animated frame;
- run the complete five-second EPR sequence without a calibration sidecar;
- visibly review the current private body plus at least two structurally different VRM 1.0 bodies;
- accept only when idle, gesture, interruption, recovery, and settling read intentionally.

VRM 0.x is not part of R1-R6. The normalized humanoid core must permit a later 0.x importer without
creating a second EPR runtime.

## Deterministic acceptance

Automated evidence must cover:

- malformed extension and humanoid mappings;
- invalid accessor shape, time order, translation target, scale track, and non-finite data;
- STEP, quaternion LINEAR, and CUBICSPLINE sampling at exact ticks;
- destination models with different rest rotations and missing optional bones;
- hips-height translation scaling and in-place root extraction;
- quaternion normalization, finite world matrices, and unchanged model scale;
- masked blending, unowned-channel preservation, interruption, and rollback;
- repeated identical pose/control/trace hashes;
- animated hidden GPU frames and complete GNU Make checks.

Visible acceptance never asks the user to construct poses. Optional authoring tools are evaluated
separately from the zero-calibration loading contract.

## Completed checkpoints

R1 established this contract, the normalized pose core, strict VRMA parsing/sampling, and focused
VRMA checks. R2 added the standalone destination transaction and `make vrm-retarget-check`, covering
authored rest-axis conversion, missing optional-role composition, hips-height scaling, in-place and
full root motion, preserved non-humanoid state/scale, malformed source rejection, and atomic
rollback. R3 now adds model-owned clip/player lifetime, deterministic clocked sampling, pause, seek,
loop, rate and phase controls, local failure diagnostics, and atomic publication of each accepted
retargeted base with the last valid EPR control reapplied in the same transaction. The imported idle
and pinned CMU walk both pass the sidecar-free hidden gate and are owner-accepted on Vampire Cat
through the same native seam with no `.epr-calibration` file involved. R3 is closed. R4 has begun
with the renderer-neutral masked-pose compositor and its deterministic ownership, rollback, and
repeatability tests. Its semantic catalog now adds bounded unique registration, stable source
identity, requested/declared/actual channel intersection, hips ownership, deterministic sampling
evidence, and transactional typed failure. Its concrete VRMA adapter additionally proves authored-
rest-invariant normalized rotations and hips translation, actual-track ownership, malformed-source
rejection, and atomic source/sample publication. Fixed-tick execution now proves exact phase
validation, deterministic source time, minimum-jerk transition envelopes, live-grant narrowing,
stable layer order, additive residual semantics, and whole-frame rollback. The model now embeds a
versioned semantic motion pack whose owned clips back one borrowed catalog. Batch loading validates
and preflights every source before atomic publication; duplicate, missing-file, or malformed-member
failure preserves the previous pack and caller ownership. The pack binds the pinned, hash-verified
MIT idle as `idle.neutral` when its ignored fixture is present. The EPR runtime borrows that catalog
and publishes complete normalized frames; an absent plan produces idle, while an active plan with a
missing posture or gesture clears stale motion and falls back locally to the accepted canonical
controller. Projection commits imported
base, normalized frame, optional calibration residuals, and explicit procedural owners atomically,
then replays the retained frame over successive imported bases. Head/eye gaze and expression now
publish live-grant ownership and compose above normalized posture/gesture. Canonical-control version
4 adds live-granted right-arm IK and tokenized continuity. Settle contributes no normalized clip;
the projection transaction captures the exact outgoing shoulder/upper-arm/lower-arm/hand local
rotations once per token, blends toward the normalized pose, applies weighted IK afterward, and
commits its continuity state only with the whole pose. Legacy posture, combined head control, and
right-arm anchors remain only in the complete fallback. The versioned dynamic right-arm target
stream is implemented with plan/behavior/claim validation, exact live-grant sampling, bounded
expiry, monotonic producer revisions, explicit lifecycle trace, and applied-revision rollback.
R4 remains active: accept the first deterministically bounded captured-motion slice, pin and bind
its semantic metadata, repeat for the required vocabulary, then publish the pack atomically and
remove the legacy anchor payload as an ordinary dependency.

## Restart checklist

1. Read this document, `docs/product-brief.md`, `docs/v1-goal.md`, `docs/product-roadmap.md`, and
   `docs/project-state.md`.
2. Inspect `git status` and preserve local VRMs, sidecars, and unrelated user changes.
3. Continue the earliest incomplete R-stage; do not restart semantic-anchor authoring.
4. Keep `base/imported motion -> EPR -> look-at/expression -> constraints -> spring bones` intact.
5. Use GNU Make and add deterministic tests before owner-visible motion review.
6. Do not broaden the VRM compatibility claim until the multi-body R6 gate passes.
