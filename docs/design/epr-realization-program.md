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

It emits one versioned bounded program or a typed local failure. It does not mutate the plan,
grant resources, inspect sessions/classifier labels, parse files, or draw.

First-slice realizers are:

- neutral seeded idle residuals;
- attentive, thinking-contained, responding-open, and interrupted-guarded posture;
- eye-first/head-follow gaze;
- one restrained right-arm contrast gesture;
- neutral/focused expression when supported;
- right-arm settle generated from current state.

## Realization Program

A first-slice program contains a schema version, stable behavior/program ids, semantic cause, plan
generation, modality, resource and capability masks, copied phase anchors, normalized semantic
targets, and bounded scalar parameters. Integer logical ticks and deterministic interpolation own
all curve timing. The program set is compiled transactionally with a plan generation and is
immutable while sampled.

Programs contain no glTF node index, VRM JSON property, SDL type, D3D type, DirectComposition type,
Win32 handle, scene layer, or presentation target.

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
The first VRM adapter lowers its bind T-pose left arm as a body-local neutral baseline; that is not
a hidden EPR gesture channel.

## Composition order

At each fixed control tick:

1. create one complete neutral candidate for the current plan generation;
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
