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

A program contains stable behavior/program ids, plan generation, modality, resource channels,
anchor references, curve segments, normalized semantic targets, capability requirements, and
fallback. Curves use integer tick intervals and deterministic interpolation.

Programs contain no glTF node index, VRM JSON property, SDL type, D3D type, DirectComposition type,
Win32 handle, scene layer, or presentation target.

## Canonical control state

The first-slice canonical state owns:

- logical tick and monotonic revision;
- semantic torso/chest/neck/head orientations;
- left/right upper-arm, lower-arm, and hand orientations;
- normalized eye and head gaze contributions plus semantic target;
- named expression weights;
- velocities needed for interruption cleanup;
- validity and capability-degradation flags.

It does not contain model-local matrices or GPU palette data.

## Composition order

At each fixed control tick:

1. clone the last valid state into a candidate;
2. sample base posture programs;
3. apply cooperative gaze/head composition;
4. apply compatible seeded idle residuals;
5. apply granted override gesture/cleanup channels;
6. solve task-space targets against the normalized body profile;
7. validate finite values, joint/range limits, continuity, and capability use;
8. atomically commit the complete candidate and revision, or retain the last valid state;
9. publish the immutable snapshot and deterministic normalized hash.

There is no partial joint commit.

## Physical solving

The existing analytic two-bone geometry may be reused behind this boundary. The canonical solve
adds explicit reach handling, elbow-pole policy, wrist orientation, conservative joint limits, and
whole-candidate rollback. Unsupported optional eye/expression capabilities neutralize only those
channels. Missing required humanoid structure rejects the body profile before control begins.

## Bounded feedback

Typed realizer, composition, solve, and capability feedback is attached to the current plan
generation and trace. It may select a declared deterministic fallback. It cannot mutate source
truth, resurrect stale behavior, or start an unbounded replan inside the control tick.

