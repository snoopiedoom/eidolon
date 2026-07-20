# Project state

Updated 2026-07-20. This file records the current implementation frontier and restart checklist.
Stable design and operating knowledge belongs in the other documents; this file may change as
milestones move.

## Current product

Bunny Asuna is the daily-driver 2D provider: ten full-canvas expressions, full/bust framing,
Unicode JRPG dialogue, local semantic expression planning, and one bubble per visible provider
session. The Mutsuki Dress v2 sprite remains a fallback. Rio's procedural 3D renderer remains
selectable and is deliberately initialized only when requested.

The settings/debug surface is a separate Dear ImGui window. Preferred renderer, display scale,
portrait framing, 3D resolution/rotation, and dialogue behavior persist through sparse per-user
overrides with reset-to-inheritance semantics.

## Verified implementation

- Windows debug and release builds compile cleanly with Clang;
- all seventeen ordinary C regression executables pass;
- provider-neutral events feed sessions keyed by `(provider, session id)`;
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
- Switch all three providers through settings and confirm scale, framing, rotation, and restart
  persistence.

## Known limitations

- the settings model selectors still expose one hard-coded model per provider instead of manifest
  discovery;
- the five-minute quiet-session retirement timeout is compiled policy rather than configuration;
- simultaneous session reveals lack explicit shared-character performance ownership and can still
  depend on update order;
- expression target projection needs more interactive tuning across real dialogue;
- portable Linux font fallback and feature parity remain unfinished;
- the complete character-sprite download is intentionally not part of Git and has not been run as
  part of normal verification;
- Rio pose endpoints remain calibration work, not finished animation: relaxed is stiff, guarded is
  behind the body, and attentive/playful are unconvincing;
- planted feet, wrist orientation, lower-body IK, gaze/blink behavior, and secondary physics remain
  future 3D milestones;
- ChatGPT Desktop chat and ZCode expose no verified attachable local stream, so their adapters
  correctly remain unavailable rather than scraping or injecting into their processes;
- the packaged desktop app's private Codex stdio app-server cannot be shared safely; the relay
  supports a CLI explicitly started with `--remote`, while Desktop remains on transcript fallback;
- streamed text currently uses a responding expression until completion; incremental semantic-beat
  classification at sentence boundaries is not implemented yet.

## Next priorities

1. collect interactive confirmation for the Win32 click-border fix and hard-cut expressions;
2. classify completed semantic beats incrementally while provider deltas continue arriving;
3. add provider status/configuration controls to settings without moving ownership into the UI;
4. add explicit shared-character performance ownership for concurrent bubbles;
5. replace hard-coded settings model entries with manifest-backed discovery;
6. make session retirement policy configurable;
7. tune affect targets and continuity thresholds from real expression traces;
8. calibrate the four initial Rio semantic poses before adding transition dynamics;
9. consider expression annotation/detection only after the downloaded portrait catalog is present.

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
