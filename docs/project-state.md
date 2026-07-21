# Project state

Updated 2026-07-21. This file records the current implementation frontier and restart checklist.
Stable design and operating knowledge belongs in the other documents; this file may change as
milestones move.

## V1 scorecard

`[x]` means verified, `[~]` means partial or still awaiting interactive proof, and `[ ]` means the
product capability is absent.

- [x] Codex session attachment — the explicit CLI relay and independent transcript fallback are
  verified; ChatGPT Desktop's private session remains fallback-only.
- [~] OpenCode session attachment — the adapter and deterministic SSE probe work, but the installed
  local server is blocked before an ordinary end-to-end user workflow.
- [~] source/session identity — registry keys distinguish adapter kind and session id, with real
  titles and deterministic fallbacks, but explicit source-instance identity is not implemented.
- [~] faithful operational-state representation — turn and response lifecycle exists, but the
  presence vocabulary does not yet represent reading, editing, tools, approval, blockage, and
  interruption comprehensively.
- [~] natural response animation — streamed semantic beats, delivery motion, and atomic expression
  changes are implemented; broader interactive tuning and shared-character arbitration remain.
- [ ] speech — visual dialogue exists, but audio speech and lip synchronization do not.
- [~] concurrent sessions — independent bubbles work; explicit shared-character performance
  ownership remains unimplemented.
- [~] unobtrusive terminal coexistence — transparent click-through and non-intrusive QA exist, but
  final desktop feel remains owner-verified.
- [ ] restart continuity — preferences and discovered sessions recover, but no product-level
  acceptance case yet proves that the same persona and presentation survive restart.
- [ ] approve/cancel/pause/redirect interaction — relay traffic can carry upstream controls, but
  Eidolon exposes no owned intervention surface.
- [ ] measured idle resource budget — performance invariants exist without a recorded idle budget
  and repeatable measurement gate.
- [ ] installer-grade onboarding — setup remains a developer workflow rather than an installable
  product experience.

## Current product

Bunny Asuna is the daily-driver portrait body: ten full-canvas expressions, full/bust framing,
Unicode JRPG dialogue, local semantic expression planning, and one bubble per visible agent
session. The Mutsuki Dress v2 sprite remains a fallback. Rio's procedural 3D renderer remains
selectable and is deliberately initialized only when requested.

The settings/debug surface is a separate Dear ImGui window. Preferred renderer, display scale,
portrait framing, 3D resolution/rotation, and dialogue behavior persist through sparse per-user
overrides with reset-to-inheritance semantics.

## Verified implementation

- Windows debug and release builds compile cleanly with Clang;
- all ordinary C regression executables included in `make check` pass;
- normalized events feed sessions keyed by the legacy `(provider, session id)` fields, where
  `provider` currently records adapter kind rather than a source instance;
- the Codex in-path relay passed a live hidden handshake through localhost WebSocket framing, a
  relay-owned stdio app-server child, JSONL forwarding, and symmetric teardown;
- the transport-neutral relay core has deterministic bidirectional forwarding and observation
  coverage; stock app-server peer-client fanout is no longer part of the CLI live topology;
- OpenCode SSE passed a deterministic live transport probe; the installed server itself is blocked
  before streaming because local `oh-my-opencode` returns HTTP 500 without a default model;
- native GoEmotions inference and asynchronous client checks pass;
- Dear ImGui works through Dear Bindings' generated C17 API and SDL backends;
- legacy session discovery is asynchronous, scans every five seconds, and keeps known-file stamp
  checks independent from presentation;
- up to four bubbles keep independent reveal/scroll state and stable layout slots;
- Unicode dialogue preserves grapheme-like clusters and uses cached SDL_ttf objects;
- expression plans are prepared per semantic beat and activated by original UTF-8 source offset;
- tiny discourse fragments merge into the thought they modify and ambiguous faces preserve
  continuity;
- portrait art swaps atomically with no crossfade; damped semantic motion remains independent;
- Windows 3D and SDL composition share one D3D11 device and GPU-resident texture;
- pixel alpha drives click-through while coarse Win32 regions keep DWM region cost bounded;
- hidden snapshot commands cover dialogue, sessions, settings, portrait motion, pose, and resolution;
- the Blue Archive wiki downloader groups the complete category into character/variant portrait
  directories, resumes downloads, and emits a source manifest.

## Awaiting interactive confirmation

- A Win32 border/frame could flash for one frame when a model click removed the hit-test region.
  Zero-motion clicks no longer remove the region, rotation suspends it only after actual movement,
  and native caption/frame styles plus non-client painting are suppressed. Confirm the flash is gone
  on the user's desktop.
- Confirm hard-cut expression art plus merged semantic fragments against another mixed-emotion
  response and retain the new performance log if timing still feels wrong.
- Switch all three body renderers through settings and confirm scale, framing, rotation, and restart
  persistence.

## Known limitations

- the settings `model` selector exposes one registered body asset per renderer instead of
  manifest-backed character discovery;
- registry identity uses adapter kind rather than an explicit source-instance id, so two configured
  sources using the same adapter kind can collide on a shared session id;
- the five-minute quiet-session retirement timeout is compiled policy rather than configuration;
- simultaneous session reveals lack explicit shared-character performance ownership and can still
  depend on update order;
- monitor-aware bubble placement uses conservative body and upper-body/face rectangles until each
  renderer reports tighter visible geometry;
- expression target projection needs more interactive tuning across real dialogue;
- portable Linux font fallback and feature parity remain unfinished;
- the complete character-sprite download is intentionally not part of Git and has not been run as
  part of normal verification;
- Rio pose endpoints remain calibration work, not finished animation: relaxed is stiff, guarded is
  behind the body, and attentive/playful are unconvincing;
- planted feet, wrist orientation, lower-body IK, gaze/blink behavior, and secondary physics remain
  future 3D milestones;
- ChatGPT Desktop chat and ZCode expose no verified attachable local stream, so their agent adapters
  correctly remain unavailable rather than scraping or injecting into their processes;
- the packaged desktop app's private Codex stdio app-server cannot be shared safely; the relay
  supports a CLI explicitly started with `--remote`, while Desktop remains on transcript fallback.

## Next priorities

1. collect interactive confirmation for the Win32 click-border fix and hard-cut expressions;
2. interactively verify monitor-aware bubble placement, stable slots, drag-release reflow, and
   mixed-DPI anchoring across sprite, portrait, and 3D bodies;
3. add source-instance identity and source/adapter status controls without moving ownership into the
   UI;
4. add explicit shared-character performance ownership for concurrent bubbles;
5. replace the hard-coded body-asset selector with character-package discovery;
6. make session retirement policy configurable;
7. expand normalized operational state toward the presence contract;
8. tune affect targets and continuity thresholds from real expression traces;
9. calibrate the four initial Rio semantic poses before adding transition dynamics;
10. consider expression annotation/detection only after the downloaded portrait catalog is present.

## Restart checklist

1. Read [the documentation index](README.md), this file, and the specification owning the next task.
2. Run `git status --short`; preserve existing work and extracted local assets.
3. On a fresh machine, set `SDL3_ROOT` as needed and run `make text-setup`.
4. Run `make check`, then build the relevant debug target. Use `make affect-setup` only when the
   ignored local classifier payload is absent.
5. Use hidden snapshots for automated visual inspection. Ask the user to perform visible feel and
   interaction tests.
6. Update this file when the active milestone changes; update architecture/design documents only
   when their boundaries or invariants change.
