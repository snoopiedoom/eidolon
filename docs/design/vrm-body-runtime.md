# VRM 1.0 body runtime contract

## Purpose

The VRM body runtime validates one VRM 1.0 body, publishes a normalized body profile and
capabilities, and projects immutable canonical control into model-local state. It is the boundary
between EPR semantics and VRM/glTF representation.

It does not select behavior, own source/session identity, schedule time, arbitrate resources, render
pixels, or own presentation.

## Required VRM 1.0 input

The first path accepts binary glTF containing `VRMC_vrm` version 1.0 metadata. It validates:

- one usable skinned humanoid hierarchy;
- required VRM humanoid roles and unique node bindings;
- finite bind transforms and inverse-bind matrices;
- hierarchy consistency and joint reach;
- model license metadata suitable for local use.

VRM humanoid metadata is authoritative. Node-name aliases and Rio-specific bone names are not used
to claim VRM conformance.

## Published body profile

The adapter publishes:

- semantic humanoid-role availability;
- normalized forward, up, and right axes;
- height, shoulder width, arm segment lengths, and conservative reach;
- conservative semantic joint limits or validated body-specific limits;
- look-at mode/ranges when present;
- expression names/binds relevant to neutral and focused states;
- optional node-constraint and spring-bone presence;
- a deterministic body-profile fingerprint.

EPR consumes only this profile. It does not read VRM extension structures.

## Capability projection

First-slice capabilities are:

- required: humanoid posture and right-arm chain;
- optional: independent eyes/VRM look-at;
- optional: neutral/focused expression binds;
- declared but deferred: spring-bone simulation and node constraints.

Missing optional look-at degrades gaze to head-only. Missing optional expression leaves neutral
morph weights. Each local degradation emits one structured trace and does not affect posture,
gesture, dialogue, or session observation.

## Projection order and transaction

For each new canonical revision:

1. start from the adapter's last valid model-local projection;
2. map semantic orientations into VRM humanoid local frames;
3. apply head/torso and arm control;
4. apply VRM look-at/eye contribution when supported;
5. apply expression weights when supported;
6. reserve the documented node-constraint/spring update stage without pretending it ran;
7. rebuild hierarchy and skinning palette;
8. validate finite transforms and palette;
9. publish the complete projection or retain the previous valid one.

The renderer consumes a monotonic complete projection. A failed projection cannot expose half an
arm or partially applied morph weights.

## Asset and configuration

The visible first body is a local legally usable VRM 1.0 asset. Large third-party assets and source
archives are not committed. Runtime selection uses an explicit local path/configuration boundary;
it does not replace the current portrait default.

An invalid or unavailable VRM disables only that body variant. Portrait and session observation
remain operational.

## Rendering boundary

Existing model geometry, texture upload, skinning, camera, render target, and SDL texture handoff
remain renderer responsibilities. VRM support may extend material/morph handling only where needed
for the first body. EPR itself has no dependency on SDL, D3D11, DirectComposition, Windows, scene,
or presentation headers.

