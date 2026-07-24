# EPR body-resource arbitration and co-articulation

## Purpose

Body-resource arbitration is the sole owner of who may control each semantic part of the body over
a logical interval. It makes conflicts, co-articulation, preemption, transfer, cleanup, and release
explicit. Update order is never ownership.

## First-slice hierarchy

```text
body
├── torso
├── head
├── eyes
├── face_expression
├── left_arm_chain
└── right_arm_chain
```

The hierarchy is semantic and body-neutral. VRM bone names, node indexes, renderer layers, and
session slots are forbidden resource identifiers.

## Claims and grants

A claim names:

- behavior id and plan generation;
- resource;
- mode: base, additive, cooperative, or override;
- logical interval;
- priority and urgency;
- preemption policy;
- composition rule when cooperative/additive;
- cleanup and transfer policy.

No non-neutral controller contribution may reach canonical composition without a live grant.

The arbiter produces immutable grants, denials, preemptions, transfers, and releases. It ranks
claims using the same explicit total order as the plan dispatcher plus resource specificity. Tests
permute input and storage order and require identical outcomes.

## Composition modes

- `base` establishes the reference state for a resource.
- `additive` contributes bounded residual motion around a compatible base.
- `cooperative` combines through a named rule, such as head aim over posture.
- `override` owns task control for the interval and suppresses incompatible contributions.

Compatibility is declared by resource and named composition rule. It is not inferred from update
order or numeric weights.

## Preemption and transfer

Preemption is legal only when the current grant's policy allows it and the incoming claim outranks
it. The arbiter records both reasons. An executing controller that leaves a displaced body part
creates a cleanup obligation.

Transfer atomically moves a resource from one behavior to another and preserves:

- the sampled current canonical pose;
- relevant current velocity;
- the releasing and receiving behavior ids;
- terminal target and tolerances;
- a deadline/fallback policy.

Release occurs only after the cleanup/settle program reaches its ground and velocity tolerances or
a deterministic fallback retires it. A resource is never silently abandoned to bind pose.

## Co-articulation in the first slice

- posture holds base torso and arm references;
- idle receives additive residual grants only on compatible channels;
- gaze owns eyes and receives cooperative head control;
- the contrast gesture receives a timed right-arm override;
- interruption preserves the delivered stroke, retires old recovery, and transfers the right arm
  to a settle program created from actual current state.

The arbiter does not generate curves, solve joints, select gestures, inspect source labels, or own
physical contacts.

