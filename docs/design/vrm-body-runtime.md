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
- finite role-node positions and non-zero right-arm segment lengths;
- expression binds that reference a real mesh morph target with a position delta;
- model name, author, license URL, commercial-use declaration, and credit declaration.

The adapter records declared usage metadata; it does not decide whether the operator has legal
authority to use an asset.

VRM humanoid metadata is authoritative. Node-name aliases and Rio-specific bone names are not used
to claim VRM conformance.

## Published body profile

The VRM adapter retains the authoritative humanoid-role-to-node map, validated neutral/focused
morph binds, metadata, and declarations for look-at, spring bones, and MToon. Its first-slice EPR
body profile publishes:

- normalized forward, up, and right axes;
- semantic shoulder/head positions, right-arm segment lengths, and conservative reach;
- conservative shoulder/head and elbow limits;
- required-humanoid/right-arm plus optional eyes/look-at and expression capability flags;
- a deterministic body-profile fingerprint.

EPR consumes only this profile. It does not read VRM extension structures.
Body height, shoulder width, authored look-at ranges, and body-specific joint ranges are future
profile extensions rather than values invented from incomplete data.

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

1. reset a private candidate to the VRM bind rotations;
2. map semantic orientations into VRM humanoid local frames;
3. apply head/torso, a body-local relaxed left-arm baseline, and controlled right-arm state;
4. apply VRM look-at/eye contribution when supported;
5. apply expression weights when supported;
6. rebuild hierarchy and skinning palette;
7. validate finite rotations and complete hierarchy evaluation;
8. publish the complete projection or restore the previous valid rotations and expression.

The renderer consumes a monotonic complete projection. A failed projection cannot expose half an
arm or partially applied morph weights.

## Asset and configuration

The visible first body is a local legally usable VRM 1.0 asset. Large third-party assets and source
archives are not committed. Runtime selection uses an explicit local path/configuration boundary;
it does not replace the current portrait default.

The first reference implementation uses
[DECAGRAMMATON on VRoid Hub](https://hub.vroid.com/characters/61437424751231571/models/3310288597351780654).
The operator owns the Pixiv-authenticated acquisition step and passes the resulting path through
`EIDOLON_VRM_PATH`. `make vrm-check VRM_PATH=...` validates the format, required humanoid roles, and
body profile before launch. Make and the runtime must not download the asset, receive Pixiv
credentials, or weaken the model's embedded terms. The current terms forbid redistribution and
modification and require credit, so the asset remains outside Git.

An invalid or unavailable VRM disables only that body variant. Portrait and session observation
remain operational.

## Rendering boundary

Existing model geometry, texture upload, skinning, camera, render target, and SDL texture handoff
remain renderer responsibilities. The first body path supports skinned morph-position blending,
base-color factors/textures, and opaque/mask/blend alpha. Full MToon shading, spring-bone
simulation, and node-constraint evaluation are deliberately deferred even when their extensions
are declared. EPR itself has no dependency on SDL, D3D11, DirectComposition, Windows, scene, or
presentation headers.
