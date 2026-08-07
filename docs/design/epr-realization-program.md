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

For rigged bodies, the normalized body profile may expose a matching set of user-approved semantic
calibration anchors. A posture/gesture realizer selects semantic endpoints from that set and derives
phase-local strength and trajectories; it does not copy model-local rotations into the
renderer-neutral IR. If a required anchor is absent, realization fails locally through a declared
fallback instead of substituting a hard-coded model-specific pose.

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
targets, and bounded scalar parameters. Integer logical ticks and deterministic interpolation own
all curve timing. The program set is compiled transactionally with a plan generation and is
immutable while sampled.

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
- right-arm velocity used for interruption continuity;
- focused-expression weight;
- validity and capability-degradation flags.

It does not contain left-arm pose, model-local bone orientations, matrices, or GPU palette data.
A projection that receives no left-arm resource preserves the incoming base/imported left-arm pose;
it may not invent a body-local relaxed baseline. The reference projection now preserves that
unowned chain. Controlled anatomical rotations pass through precomputed bind-world correction
frames, and the accepted projection commits from a complete scratch TRS pose. A future animation
owner publishes its fresh base pose through the explicit capture seam before EPR projection;
constraints and secondary physics then compose after the atomic EPR commit.

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

Cleanup is the exception that deliberately receives captured current canonical state: the settle
program begins from the actually solved hand and wrist at interruption, then transfers the arm to
the new posture. There is no partial joint commit.

## Physical solving

The existing analytic two-bone geometry may be reused behind this boundary. The canonical solve
adds explicit reach handling, elbow-pole policy, wrist orientation, conservative joint limits, and
whole-candidate rollback. Unsupported optional eye/expression capabilities neutralize only those
channels. Missing required humanoid structure rejects the body profile before control begins.

## Bounded feedback

Typed realizer, composition, solve, and capability feedback is attached to the current plan
generation and trace. It may select a declared deterministic fallback. It cannot mutate source
truth, resurrect stale behavior, or start an unbounded replan inside the control tick.
