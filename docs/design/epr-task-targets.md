# EPR dynamic task-target contract

## Purpose

Dynamic task targets let a bounded upstream controller refine an already selected EPR behavior
with current geometric evidence. Examples include reaching toward a tracked object, maintaining a
hand relation to the body, or following a user-controlled handle. They are controller samples, not
semantic truth, animation assets, or model-local bone rotations.

Performance Intent remains the authoritative statement of operational and communicative meaning.
Behavior Plans remain the authority that selects behavior and reserves body resources. A task
target may refine a selected behavior, but it cannot create one, grant itself a resource, change
intent, or name a renderer/model node.

## Versioned right-arm sample

The current bounded ingress is `EidolonEprRightArmTaskTarget` version 1. Each immutable sample
contains:

- a revision and exact predecessor revision;
- the plan generation it was produced against;
- opaque producer, tracked-target, and behavior identities;
- a logical sample tick and inclusive validity deadline;
- shoulder-relative hand and elbow-pole coordinates in whole-arm units;
- anatomical wrist pitch, yaw, and roll;
- a normalized IK weight.

The coordinates use the measured body profile's right, up, and forward basis. Runtime conversion
multiplies them by measured upper-plus-lower-arm length and adds the measured shoulder position.
The stream therefore stays body-relative and renderer-neutral. Bind rotations, node indices,
model-space axes, and residual calibration remain body-adapter concerns.

## Publication rules

Publication succeeds only when all of the following are true:

- the schema, identifiers, finite bounds, angles, and weight validate;
- the validity interval is ordered and no longer than 1000 logical milliseconds;
- the revision increases, names the exact accepted predecessor, and stays with the established
  producer;
- the sample does not travel backward behind the last runtime tick;
- the referenced plan generation is current;
- the exact behavior is active, has a compiled Realization Program, and owns a base or override
  right-arm claim covering the complete validity interval.

A rejected sample emits typed trace evidence and changes none of the accepted target stream. The
accepted stream revision and producer survive ordinary expiry and plan replacement, so a delayed
or restarted publisher cannot silently restart its chain at revision one.
A successfully superseding revision releases the prior active sample with `revised` before the new
sample becomes accepted.

## Sampling and ownership

At a control tick, the target can contribute only if its exact behavior has the live right-arm
grant. An attachment to a base posture therefore becomes ineligible while a higher-ranked gesture
override owns the arm; it never follows whichever behavior happens to be current. A future-dated
or temporarily denied target remains dormant until it becomes eligible or expires.

An expired sample releases with `completed`. Accepting a new intent/plan releases any active sample
with `revised`. Rejection of a candidate intent does not release or mutate the target. Producers
must publish a new sample against the new plan and exact behavior before dynamic control resumes.

## Canonical transaction

An eligible sample refines the scratch canonical control after the granted right-arm realizer and
before the two-bone solve. It sets the anatomical hand target, elbow pole, wrist orientation,
weighted IK contribution, and explicit procedural right-arm ownership. It suppresses an unrelated
settle-continuity token for that candidate.

The target revision becomes applied only after the complete canonical candidate solves and
validates. A rejected solve retains the previous published control and applied revision; the still
valid accepted target may be retried at a later logical tick. VRM projection then consumes the
published weighted IK through its existing whole-scratch-pose transaction, after normalized motion
and settle continuity and before constraints or secondary motion.

## Trace and current scope

Trace version 3 records `task_target.accepted`, `task_target.rejected`,
`task_target.applied`, and `task_target.released` with behavior, tracked-target identity, right-arm
resource, target revision, logical tick, plan generation, and typed reason.

Version 1 intentionally exposes only the right arm. Generalizing to another limb, gaze target,
contact, or two-hand relation requires a new bounded typed contract and the same plan/resource
ownership proof; it is not implemented by adding joint fields to Performance Intent.
