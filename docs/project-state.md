# Project state

Updated 2026-07-24. This file records the current implementation frontier and restart checklist.
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
- [~] unobtrusive terminal coexistence — native/fallback presentation parity is owner-accepted;
  the all-day desktop feel still requires a measured workday soak.
- [ ] restart continuity — preferences and discovered sessions recover, but no product-level
  acceptance case yet proves that the same persona and presentation survive restart.
- [ ] approve/cancel/pause/redirect interaction — relay traffic can carry upstream controls, but
  Eidolon exposes no owned intervention surface.
- [ ] measured idle resource budget — performance invariants exist without a recorded idle budget
  and repeatable measurement gate.
- [ ] installer-grade onboarding — setup remains a developer workflow rather than an installable
  product experience.

## Current product

Bunny Asuna is the alpha-driving portrait body: ten full-canvas expressions, full/bust framing,
Unicode JRPG dialogue, local semantic expression planning, and one bubble per visible agent
session. The Mutsuki Dress v2 sprite remains a fallback. Rio's procedural 3D renderer remains
selectable and is deliberately initialized only when requested. A local VRM 1.0 body can now run
the first complete EPR vertical slice through that existing SDL 3D presentation path; the portrait
remains the shipped default.

The settings/debug surface is a separate Dear ImGui window. Preferred renderer, portable
presentation preference, display scale, portrait framing, 3D resolution/rotation, and dialogue
behavior persist through sparse per-user overrides with reset-to-inheritance semantics.

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
- after streaming and dialogue reveal finish, session bubbles remain opaque for five seconds, then
  fade out over three seconds;
- mouse-motion floods are bounded and character dragging coalesces native window moves at the
  configured presentation boundary; Windows uses compositor-owned native dragging so HWND moves
  cannot consume the renderer's frame budget;
- VSync and the independent FPS ceiling are persisted settings; the shipped default follows the
  active monitor, while VSync-off with a zero ceiling is uncapped;
- Windows 3D and SDL composition share one D3D11 device and GPU-resident texture;
- pixel alpha drives click-through while coarse Win32 regions keep DWM region cost bounded;
- the shipped `native` presentation preference selects the portrait-only `win32_dcomp` backend on
  Windows; an explicit legacy preference, sprite/3D body, or native startup failure selects
  `sdl_window_legacy` with a logged reason;
- `win32_dcomp` owns a no-redirection host, independent body/dialogue targets, premultiplied D3D11
  submission, DirectComposition transforms/opacity/z-order, cached-alpha hit testing, native body
  dragging, bounded presentation events for dialogue activation, body context requests, host close,
  routed pointer input, graphics-reset requests, and final-move reflow, revisioned active-host
  environment publication, stable opaque monitor ids, caller-owned topology copying, transactional
  cadence/layout application, and bounded device/backend reconstruction with an explicit logged SDL
  fallback;
- hidden snapshot commands cover dialogue, sessions, settings, portrait motion, pose, and resolution;
- EPR accepts versioned synthetic Performance Intent, publishes incremental behavior plans and
  immutable modality Realization Programs, dispatches fixed logical anchors, arbitrates explicit
  body resources, atomically solves canonical control, and records a bounded deterministic causal
  trace;
- the first EPR scenario covers idle, attention/listening, thinking, streamed response, focused
  expression, eye-first/head-follow gaze, a restrained right-arm contrast gesture, interruption,
  current-state resource transfer and cleanup, guarded settling, and prevention of interrupted
  phase replay;
- one local VRM 1.0 path validates authoritative humanoid roles and optional morph/look-at
  capabilities, projects monotonic control transactions, renders morph/material/alpha data, and
  degrades optional capabilities locally;
- the reference VRM is acquired manually through its Pixiv-authenticated VRoid Hub page, remains
  outside Git under its no-redistribution terms, and is validated locally by `make vrm-check`;
- deterministic performance snapshots and a hidden five-second SDL 3D run complete with the
  portrait/default and native-presentation selections unchanged;
- the Blue Archive wiki downloader groups the complete category into character/variant portrait
  directories, resumes downloads, and emits a source manifest.

## Awaiting interactive confirmation

- Confirm hard-cut expression art plus merged semantic fragments against another mixed-emotion
  response and retain the new performance log if timing still feels wrong.
- Switch all three body renderers through settings and confirm scale, framing, rotation, and restart
  persistence.
- Judge the VRM EPR sequence as a performance: whether attention, thinking, contrast, interruption,
  cleanup, and continued presence read correctly and feel alive. Structural trace and control
  verification is automated.

The owner accepted ordinary no-environment DirectComposition portrait/dialogue startup, transparent
per-pixel click-through, smooth native body dragging, cross-monitor movement, dialogue activation,
native body-context settings, cancel-on-drag, one stable final reflow, cross-monitor environment
delivery, native bubble-bound selection, bubble retirement/fade, and the seam-free native dialogue
raster. The owner also accepted routed SDL 3D middle-drag rotation outside the host bounds,
`Shift`+middle roll, double-middle reset, preserved left dragging, fallback launch, clicks,
settings, mixed-DPI/output placement, post-drag resumption, and persisted native/legacy restart
selection. Deterministic active-output retirement proves stable fallback selection, usable-bounds
placement, and application-state continuity; a physical display disconnect remains optional
hardware evidence.
Deterministic hidden probes confirm both same-process DirectComposition reconstruction and forced
runtime fallback to `sdl_window_legacy`. The owner accepted visible placement, continuity, and
interaction after both injected recovery branches.

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
- close, graphics-reset, and middle-button routed-pointer meaning now cross the presentation event
  contract for both Windows backends; SDL primary input still uses a normalized fallback adapter,
  while touch/pen routing remains unimplemented;
- DirectComposition device/backend recovery preserves application state through one fresh native
  reconstruction and explicit SDL fallback; both injected branches are owner-accepted, while real
  device-loss behavior is not yet proven;
- Windows `sdl_window_legacy` dragging enters the native top-level modal move loop, so animation and
  dialogue presentation may pause until release; this is a documented fallback limitation, not the
  cadence target for DirectComposition;
- expression target projection needs more interactive tuning across real dialogue;
- portable Linux font fallback and feature parity remain unfinished;
- the complete character-sprite download is intentionally not part of Git and has not been run as
  part of normal verification;
- Rio pose endpoints remain calibration work, not finished animation: relaxed is stiff, guarded is
  behind the body, and attentive/playful are unconvincing;
- the first EPR ingress is a deterministic synthetic fixture behind the future A2 boundary; live
  A2 source/session provenance is not duplicated here;
- retired-plan compaction is not needed by the bounded first fixture but must land before
  long-running live A2 evidence can produce unbounded performance episodes;
- the VRM path implements the first focused morph and base PBR color/alpha subset; MToon shading,
  spring bones, node constraints, fingers, locomotion, balance, and a broader gesture catalogue are
  deferred;
- planted feet, wrist orientation, lower-body IK, gaze/blink behavior, and secondary physics remain
  future 3D milestones;
- ChatGPT Desktop chat and ZCode expose no verified attachable local stream, so their agent adapters
  correctly remain unavailable rather than scraping or injecting into their processes;
- the packaged desktop app's private Codex stdio app-server cannot be shared safely; the relay
  supports a CLI explicitly started with `--remote`, while Desktop remains on transcript fallback;
- the public-domain notice covers Eidolon's original work only; contributor terms and the project
  license must be deliberately revisited before substantial outside contributions, investment, or
  commercial distribution, and extracted character assets remain separately governed.

## Next priorities

The active roadmap gate is
[A2: make Codex session truth dependable](product-roadmap.md#a2-make-codex-session-truth-dependable).
[A1: finish the native presentation foundation](product-roadmap.md#a1-finish-the-native-presentation-foundation)
is complete and owner-accepted for the Windows 2D daily-driver path. DirectComposition is the
normal portrait selection; `sdl_window_legacy` remains the explicit, capability, and failure
fallback with its accepted modal-drag limitation.

The separate EPR/VRM workstream is at its first owner-feel gate. Its implementation stays on
`sdl_window_legacy`; native DirectComposition 3D and making 3D the default remain downstream of
owner acceptance and do not alter A2 ownership.

1. add source-instance identity and migrate registry ownership from legacy
   `(adapter kind, session id)` to durable `(source_id, session_id)`;
2. expose source/adapter connection and failure state without moving ownership into the
   UI;
3. expand the truthful minimum operational vocabulary required by alpha;
4. prove reconnect and restart continuity without deleting session state;
5. tune Expression Director targets and continuity thresholds from real performance traces;
6. add explicit shared-character performance ownership for concurrent bubbles;
7. make session retirement policy configurable;
8. restore remaining versioned session continuity across restart;
9. define and measure the idle resource budget, then complete an owner-controlled workday soak;
10. begin public-V1 character-package discovery only after the daily-driver alpha gate closes.

## Deferred by the active roadmap

- native DirectComposition 3D, production-path EPR tuning, and making 3D the default body remain
  downstream of owner acceptance of the first SDL EPR performance;
- portrait-catalog expression annotation remains downstream of current Expression Director tuning
  and distributable character-package decisions.
- conversational memory, persona-mediated output, and the local-first persona bridge are post-V1
  horizons, not missing alpha implementation.

## Restart checklist

1. Read the [product brief](product-brief.md), [V1 goal](v1-goal.md),
   [product roadmap](product-roadmap.md), this file, and the specification owning the next task.
2. Run `git status --short`; preserve existing work and extracted local assets.
3. On a fresh machine, set `SDL3_ROOT` as needed and run `make text-setup`.
4. Run `make check`, then build the relevant debug target. Use `make affect-setup` only when the
   ignored local classifier payload is absent.
5. Use hidden snapshots for automated visual inspection. Ask the user to perform visible feel and
   interaction tests.
6. Update this file when the active milestone changes; update architecture/design documents only
   when their boundaries or invariants change.
