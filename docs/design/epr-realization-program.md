# EPR Realization Program and canonical-control contract

## Purpose

A modality realizer translates one dispatched, granted behavior into a bounded Realization
Program. Controller composition samples active programs into one candidate canonical humanoid
state. Physical validation atomically commits that candidate or retains the last valid state.

The IR is model-neutral and renderer-neutral, but modality-tagged. Posture, gaze, expression, idle,
and gesture do not pretend to be the same channel.

## Realizer contract

A realizer receives:

- one immutable behavior unit and its observed/controllable anchors;
- current resource grants;
- normalized body profile and optional capabilities;
- fixed configuration and seed;
- current canonical state when cleanup continuity requires it.

For rigged bodies, a motion-producing realizer now emits a versioned semantic generator reference.
The reference names the body-independent generator and declares its resource mask, exact normalized
humanoid rotation channels, hips-translation ownership, blend weight, intensity, playback rate,
and takeover policy. Gaze, expression, right-arm settling, and dynamic IK remain explicit
procedural modalities and therefore carry
the validated `none` generator reference. No reference contains model-local rotations, a body
fingerprint, or an asset path.

It emits one versioned bounded program or a typed local failure. It does not mutate the plan,
grant resources, inspect sessions/classifier labels, parse files, or draw.

First-slice realizers are:

- neutral seeded idle residuals;
- attentive, thinking-contained, responding-open, and interrupted-guarded posture;
- eye-first/head-follow gaze;
- one restrained right-arm contrast gesture;
- one semantic focused-expression channel when explicitly mapped and executable;
- right-arm settle generated from current state.

## Realization Program

A first-slice program contains a schema version, stable behavior/program ids, semantic cause, plan
generation, modality, resource and capability masks, copied phase anchors, normalized semantic
targets, bounded scalar parameters, and the versioned generator reference described above. Integer
logical ticks and deterministic interpolation own all curve timing. The program set is compiled
transactionally with a plan generation and is immutable while sampled.

Programs contain no glTF node index, VRM JSON property, SDL type, D3D type, DirectComposition type,
Win32 handle, scene layer, or presentation target.

Calibrated task-space targets may be compiled into a program because they are expressed in
body-relative anatomical coordinates. Optional model-local residual quaternions remain in the body
adapter sidecar and apply only after canonical solving; they never become upstream semantic
meaning.

## Canonical control state

The first-slice canonical state owns:

- logical tick and monotonic revision;
- semantic torso and head orientations;
- normalized eye and head gaze contributions plus semantic target;
- a right-hand task-space target, elbow pole, solved elbow/hand positions, and wrist orientation;
- right-arm velocity retained as task-space evidence;
- an independent right-arm IK weight;
- a settle-behavior continuity token and decay weight;
- focused-expression weight;
- resource-local semantic-anchor weights used only to select calibrated model-local residuals;
- validity and capability-degradation flags.

It does not contain left-arm pose, model-local bone orientations, matrices, or GPU palette data.
A projection that receives no left-arm resource preserves the incoming base/imported left-arm pose;
it may not invent a body-local relaxed baseline. The reference projection now preserves that
unowned chain. Controlled anatomical rotations pass through precomputed bind-world correction
frames, and the accepted projection commits from a complete scratch TRS pose. The model-owned VRMA
player now publishes each retargeted base through that seam and reapplies retained EPR control in
the same transaction; constraints and secondary physics compose after the atomic EPR commit.

## Composition order

At each fixed control tick:

1. create one complete EPR-owned candidate from the last valid/base canonical state for the current
   plan generation;
2. sample the granted base posture program;
3. apply cooperative gaze/head composition;
4. apply compatible seeded idle residuals;
5. apply granted override gesture/cleanup channels;
6. solve task-space targets against the normalized body profile;
7. validate finite values, joint/range limits, continuity, and capability use;
8. atomically commit the complete candidate and revision, or retain the last valid state;
9. publish the immutable snapshot and deterministic normalized hash.

Cleanup is the exception that deliberately carries a continuity token. The settle program owns the
right-arm resource and interrupt-to-settle envelope but emits no normalized clip and no IK target.
During projection, a new token captures the exact outgoing model-local shoulder, upper-arm,
lower-arm, and hand rotations. Later frames with that token reuse the capture while its weight
decays into the current normalized pose. Capture and rig publication share one scratch transaction,
so a failed frame changes neither.

## Physical solving

The existing analytic two-bone geometry may be reused behind this boundary. The canonical solve
adds explicit reach handling, elbow-pole policy, wrist orientation, conservative joint limits, and
whole-candidate rollback. Unsupported optional eye/expression capabilities neutralize only those
channels. Missing required humanoid structure rejects the body profile before control begins.

## Bounded feedback

Typed realizer, composition, solve, and capability feedback is attached to the current plan
generation and trace. It may select a declared deterministic fallback. It cannot mutate source
truth, resurrect stale behavior, or start an unbounded replan inside the control tick.

## R4 semantic-generator transition

Realization Program version 4 assigns stable canonical identifiers to neutral idle, attentive,
thinking, responding, interrupted/guarded, right-arm contrast, and right-arm settle. The compiler
derives channel ownership solely from the program's semantic EPR resources and rejects a descriptor
whose explicit humanoid mask or hips ownership disagrees with that mapping. Base, additive, and
override takeover remain explicit; weights and rates are finite and bounded.

The fixed-capacity [semantic motion catalog](epr-motion-catalog.md) now resolves each active
reference to one borrowed canonical normalized source. It rejects duplicates and missing or
channel-incompatible sources explicitly, then narrows requested ownership against both the source's
declared maximum and the pose actually sampled. Sampling preserves source identity/time evidence
and commits only a complete valid result.

The concrete VRMA source adapter now supplies that canonical source contract: it validates each
borrowed clip, normalizes authored rest-relative rotations and hips displacement, and publishes only
the channels backed by actual tracks. `motion_execution` validates the compiled behavior/generator
pair and exact anchor shape, derives source time from fixed ticks, applies minimum-jerk transition
envelopes, narrows against current grants, and submits a deterministic ordered layer set to the
normalized compositor. Intensity scales a normalized contribution; blend weight controls its
influence, and additive generators compose residual quaternions/hips instead of replacing the base.
The model now embeds a versioned semantic motion pack and lends its validated catalog to the runtime.
A requested batch preflights every file, binding, and source before clip ownership moves; catalog
callbacks are then rebased onto stable pack storage and published atomically. The pinned verified
idle becomes `idle.neutral` when present; complete normalized frames are published through one
imported-base/normalized-frame/residual/procedural-owner transaction and retained across base
playback updates. Missing active posture or gesture bindings fail locally to the accepted canonical
controller and clear stale normalized output.

Canonical-control version 4 publishes explicit head-gaze deltas plus a resource mask derived from
live procedural grants. Head/eye gaze and expression therefore compose after normalized
posture/gesture without admitting the legacy combined head or posture controls. The same contract
now carries weighted right-arm IK plus a tokenized continuity envelope. Projection captures exact
outgoing model-local arm rotations once per token, shortest-arc blends them into the normalized
pose, then applies weighted IK, all transactionally. Settle has no catalog source and revised intent
with the same behavior token cannot rebase its capture. The full canonical solve remains a
complete-frame fallback. A separate versioned dynamic task-target ingress now publishes bounded
right-arm samples against an exact plan generation, behavior, and base/override claim. A sample is
eligible only under that behavior's live grant and its revision becomes applied only after the
canonical solve commits. R4 now has a provenance-pinned but deliberately unlabeled captured-motion
candidate; it still needs visible slice selection and complete atomic gesture/posture binding before
the legacy task-space anchor payload can stop being an ordinary behavior dependency.
Optional package residual calibration remains a downstream body-adapter concern.

## Implemented calibration compiler

The reference-body path validates a realization profile against the measured anatomy fingerprint,
requires a neutral right-arm anchor, and compiles every present semantic anchor into bounded
body-relative program data. Posture transitions begin from the current validated posture base and
use a quintic minimum-jerk curve. Contrast gesture preparation, peak, and recovery are compiled as
one complete family; an incomplete family becomes a typed no-op on the right-arm resource. Missing
posture anchors select calibrated neutral and emit a `calibration_missing` trace record. Synthetic
pose constants remain only in the deterministic test fixture and are not an ordinary-playback
fallback.
