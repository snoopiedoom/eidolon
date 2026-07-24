# Body capabilities specification

## Problem

Eidolon currently exposes sprite, portrait, and 3D as global renderer choices. That is useful for
development but makes an engine backend look like the product. A character should select an
intended body while the shared performance system degrades gracefully across bodies with different
expressive power.

## Goals

- preserve one coherent art and performance direction across multiple body implementations;
- let ordinary users choose a character rather than an engine backend;
- advertise capabilities without renderer-specific branches in session or persona logic;
- degrade unsupported performance intent locally and predictably;
- avoid initializing expensive inactive renderers;
- keep dialogue and session observation alive when body assets are missing or incomplete.

## Non-goals

- requiring every character to provide every body tier;
- pretending a static image can perform authored 3D poses;
- defining a final distributable package format in this revision;
- moving session identity, persona, or semantic classification into a renderer;
- claiming the current global configuration already implements character packages.

## Ownership

A **character package** will own character metadata, one or more body variants, assets, performance
defaults, and rights information. It selects one intended default body.

A **body variant** is one concrete appearance implemented by one **body renderer**. The body
renderer owns asset loading, drawing, body-local motion, and capability realization. Shared
performance code produces renderer-neutral intent. Configuration and authoring tools may expose an
advanced renderer override without making that override the ordinary product model.

Persona and session identity are independent from body selection. Changing a body changes visible
capability and geometry, not who is speaking or which session owns a bubble.

## Body tiers

### Static image

One transparent image. It may still support scale, breathing, restrained sway, tilt, attention
offset, and deterministic delivery accents applied to the complete image.

### Expression portrait set

Aligned images with semantic labels and framing metadata. It adds discrete expression changes while
retaining whole-image motion.

### Sprite atlas

Authored animation states and directions. It can map operational and performance intent to packaged
state animation, subject to the atlas's declared rows and timing.

### Rigged 3D model

A hierarchy, skin, materials, and semantic humanoid mapping. It may add gaze, IK, semantic poses,
procedural motion, and secondary physics.

Future renderers such as Live2D fit the same contract by advertising capabilities rather than
creating another session or persona pathway.

## Advertised capabilities

A body variant may advertise:

```text
expression_labels
continuous_affect
speech_motion
attention_target
semantic_pose
gaze
lip_sync
click_regions
secondary_motion
```

Capabilities describe what a loaded body can realize; they do not promise that the current shared
performance runtime already produces every corresponding intent.

## Target data flow

```text
operational state + semantic affect + delivery marks
                         ↓
renderer-neutral performance intent
                         ↓
body capability projection
                         ↓
supported local expression / motion / pose / attention
                         ↓
body geometry + visible bounds + click regions
```

Body geometry returns to composition and bubble layout. This feedback is geometric only; the body
cannot acquire session ownership through it.

The first concrete rigged-body projection is the [VRM 1.0 body runtime](vrm-body-runtime.md).
EPR consumes its normalized profile and optional capabilities; it never branches on VRM fields or
model node names.

## Selection and degradation

- a character package selects its intended default body variant;
- ordinary users choose a character package, not `sprite`, `portrait`, or `model_3d`;
- authoring and debug controls may override the body renderer explicitly;
- unsupported intent follows the body variant's deterministic fallback mapping;
- missing expression labels fall back to the package's default expression;
- missing pose or gaze support preserves dialogue and uses supported attention/motion cues;
- body changes may reposition bubbles but preserve stable session ordering where possible;
- inactive expensive renderers do not initialize.

The current system has a global `preferred_renderer`, one active character manifest, and separate
renderer-specific configuration. Migrating to character packages requires an explicit manifest and
selection boundary; documentation must not describe that migration as complete.

## Invariants

- one art direction governs space, attention, restraint, dialogue, and interaction across bodies;
- body selection never changes persona or session identity;
- session and semantic systems never branch on asset filenames or renderer internals;
- a missing body capability never breaks dialogue or session observation;
- capability projection is deterministic for identical intent and package state;
- expensive inactive renderers remain uninitialized;
- visible body, face/head, and click geometry is reported independently from the transparent source
  canvas;
- rights and redistribution metadata travels with a distributable character package.

## Failure behavior

- package missing or invalid: retain the last valid package or load the shipped fallback body;
- requested body unavailable: use the package default or fallback without changing identity;
- asset decode failure: disable only the affected body variant and preserve session/dialogue state;
- unsupported intent: degrade locally without fabricating a capability;
- body geometry unavailable: provide conservative bounds for layout and hit testing;
- renderer initialization failure: release partial resources and continue through the configured
  fallback body where available.

## Acceptance criteria

- a static image, portrait set, sprite atlas, and rigged 3D model can consume the same abstract
  performance event without session-layer renderer branches;
- unsupported intent degrades without dropping dialogue or session visibility;
- selecting a character loads its intended body without asking an ordinary user to understand
  renderer backends;
- an advanced override changes only body realization;
- switching body or framing preserves session ordering and reflows bubbles from updated geometry;
- inactive 3D resources are not initialized for a 2D body;
- invalid package edits retain the last known-good package or deterministic fallback;
- character rights information is present before a package is considered distributable.

## Open decisions

- package manifest format, versioning, inheritance, and hot-reload transaction;
- whether multiple body variants may remain warm for instant switching;
- canonical expression vocabulary versus package-local aliases;
- capability negotiation between future persona behavior and body packages;
- authoring validation and preview requirements for third-party packages;
- distribution, signing, trust, and licensing policy.
