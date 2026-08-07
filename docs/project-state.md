# Project state

Updated 2026-08-07. This file records the current implementation frontier and restart checklist.
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
selectable and is deliberately initialized only when requested. One manually selected reference
VRM can run the first EPR vertical slice through that existing SDL 3D presentation path. This is an
experimental supported-reference-avatar path, not general VRM 1.0 support; the portrait remains the
shipped default.

The portrait director and EPR/VRM runtime are separate body-performance systems. They coexist behind
shared session, dialogue, selection, scene, and presentation boundaries, but neither owns the
other's expression labels, motion state, pose state, or assets. Shared body-neutral evidence does
not require the portrait to run through EPR.

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
- Windows 3D shares the selected presentation backend's D3D11 device: legacy composition samples an
  SDL-owned GPU texture, while native composition renders directly into its body swapchain;
- pixel alpha drives click-through while coarse Win32 regions keep DWM region cost bounded;
- the shipped `native` presentation preference selects `win32_dcomp` for portrait and 3D bodies on
  Windows; an explicit legacy preference, sprite body, or native startup failure selects
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
- one experimental reference-avatar path validates authoritative humanoid hierarchy/scale, rejects
  unsupported matrix nodes, reports optional capabilities through absent/declared/parsed/executable
  states, maps an independently parsed `relaxed` position morph over the bind-face baseline, and
  projects monotonic EPR control revisions through the current base-color/alpha subset;
- the reference VRM is acquired manually through its Pixiv-authenticated VRoid Hub page, remains
  outside Git under its no-redistribution terms, and receives the current structural/profile
  preflight through `make vrm-structure-check` (`make vrm-check` remains an alias);
- bind-world correction frames remove raw local-axis assumptions for controlled torso/head/eye/wrist
  rotations, and a complete scratch TRS transaction preserves unowned pose channels while leaving
  the live rig untouched on failure;
- `make vrm-runtime-check` exercises the actual geometry, 39 decoded textures, 226-joint skinning,
  shaders, 252 projected control revisions, and a presented hidden GPU frame for DECAGRAMMATON;
- the owner-selected local default is now Vampire Cat (`2349235869624830263.vrm`), which separately
  passes with 20 draws, 31 textures, 195 joints, and 252 projected revisions; its embedded
  author-only/no-redistribution/no-modification terms keep it a private untracked fixture;
- the VRM path now measures bind position and nearest-semantic-parent length for every mapped
  humanoid role, publishes bilateral proportions/body axes and an anatomy fingerprint, and
  transactionally loads partial semantic-anchor sidecars without changing visible poses;
- `make vrm-calibrate VRM_PATH=...` now rebuilds the actual EPR fixture at eight frozen semantic
  anchors, projects live task-space edits, and deterministically/atomically saves partial sidecars;
- calibration and visible performance review now use the shared transparent/borderless native body
  target instead of a private SDL authoring host; wheel scaling grows the overlay with the body,
  rotation-safe depth fitting prevents orbit inspection from clipping the rig, and Win32 capture
  keeps middle-drag routing alive beyond the host bounds;
- native VRM frames submit directly into the DirectComposition swapchain with no animated
  framebuffer readback; a CPU-projected skinned-mesh mask preserves transparent click-through;
- deterministic performance snapshots, the hidden runtime gate, the five-second native 3D review
  command, and extended wheel/out-of-host native smoke pass with the portrait default unchanged;
- the Blue Archive wiki downloader groups the complete category into character/variant portrait
  directories, resumes downloads, and emits a source manifest.

## Awaiting interactive confirmation

- Confirm hard-cut expression art plus merged semantic fragments against another mixed-emotion
  response and retain the new performance log if timing still feels wrong.
- Switch all three body renderers through settings and confirm scale, framing, rotation, and restart
  persistence.
- Use the in-runtime calibration surface to approve the selected local body's neutral, attentive,
  thinking, responding, contrast, and interrupted/guarded anchors, then judge the derived complete
  sequence. The first review accepted camera/rig/harness correctness and rejected the provisional
  hard-coded poses; EPR trace and control verification remains automated.

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
- legacy Rio pose endpoints remain calibration work, not finished animation, and are separate from
  the EPR/VRM calibration profile;
- the first EPR ingress is a deterministic synthetic fixture behind the future A2 boundary; live
  A2 source/session provenance is not duplicated here;
- retired-plan compaction is not needed by the bounded first fixture but must land before
  long-running live A2 evidence can produce unbounded performance episodes;
- the VRM path is limited to the selected reference-avatar experiment and must not be exposed as an
  unrestricted "load a VRM" capability;
- MToon is detected and version-checked per material, but the renderer still provides only its
  documented base-color fallback rather than executable MToon shading;
- `neutral` and `relaxed` are independent and the bind face is now the zero-expression baseline;
  other preset/custom expressions and material-color/texture-transform bind payloads are not yet
  retained or executable;
- authored look-at type, offset, and range maps are parsed, but authored bone/expression look-at is
  deliberately parsed/not executable and degrades to head-only gaze;
- projection preserves the unowned left arm and unrelated TRS, transforms canonical angular control
  through authored bind frames, and commits from scratch; future imported animation must publish its
  fresh base through the capture seam before EPR, with constraints/physics ordered afterward;
- the calibration sidecar measures, fingerprints, parses, captures, edits, and atomically saves
  partial anchors; left-arm/residual editing and compiling EPR programs from accepted anchors are
  not implemented yet;
- matrix nodes are rejected for the supported slice, and humanoid nearest-ancestor plus positive
  scale validation now run before profile publication;
- the renderer remains a narrow embedded-PNG, global-clamp-sampler, shared-skin-palette,
  position-morph, base-color/alpha dialect; `make vrm-runtime-check` proves that dialect for the
  selected asset/machine, not general VRM compatibility;
- MToon shading, material/texture-transform expressions, spring bones, node constraints, fingers,
  locomotion, balance, and a broader gesture catalogue are deferred until the correctness and
  composition gates close;
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
normal portrait/3D selection; `sdl_window_legacy` remains the explicit, sprite-capability, and failure
fallback with its accepted modal-drag limitation.

The separate EPR/VRM workstream has closed its corrective implementation/runtime gates. Its first
owner-feel review accepted the camera, rig, and harness but rejected the provisional pose
authorship, so the workstream is now implementing calibration-derived realization. It now shares
the native scene/presentation path without sharing the portrait performance system; making 3D the
default remains downstream and does not alter A2 or portrait ownership.

Primary daily-driver priorities:

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

Parallel EPR/VRM reference-body priorities:

1. use the implemented in-runtime calibration session to author the selected local body's named task-space
   anchors; bounded residual and left-arm controls remain follow-up authoring tools;
2. compile posture/gesture programs from approved anchors and degrade
   missing anchors locally without guessed fallback poses;
3. calibrate the selected local body and repeat the owner-controlled visible performance judgement;
4. extend the hostile corpus with renderer-level JPEG/URI/sampler/skin-palette fixtures and official
   sample models;
5. retain the imported-animation -> EPR -> look-at -> constraints -> spring seams before adding any
   downstream pose owners; VRMA remains optional rather than the primary calibration path.

## Deferred by the active roadmap

- visible acceptance of native DirectComposition 3D, production-path EPR tuning, and making 3D the
  default body remain downstream of owner acceptance of the calibrated EPR performance;
- the EPR/VRM workstream's correctness gates are not deferred by the 2D roadmap; they are required
  before the experimental reference body is treated as landed;
- portrait-catalog expression annotation remains downstream of current Expression Director tuning
  and distributable character-package decisions.
- conversational memory, persona-mediated output, and the local-first persona bridge are post-V1
  horizons, not missing alpha implementation.

## Restart checklist

1. Read the [product brief](product-brief.md), [V1 goal](v1-goal.md),
   [product roadmap](product-roadmap.md), this file, and the specification owning the next task.
2. Run `git status --short`; preserve existing work and extracted local assets.
3. On a fresh machine, initialize submodules and run `make sdl-deps` (or let the first `make` build
   the pinned SDL3/SDL3_ttf dependency layer automatically).
4. Run `make check`, then build the relevant debug target. Use `make affect-setup` only when the
   ignored local classifier payload is absent.
5. Use hidden snapshots for automated visual inspection. Ask the user to perform visible feel and
   interaction tests.
6. Update this file when the active milestone changes; update architecture/design documents only
   when their boundaries or invariants change.
