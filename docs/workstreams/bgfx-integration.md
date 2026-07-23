# bgfx integration workstream

## Purpose

This document is the resumable working state for evaluating bgfx as Eidolon's shared graphics
backend. It records approved constraints, dependency revisions, completed gates, evidence, failures,
and the exact next checkpoint.

Stable architectural conclusions belong in
[`docs/design/native-presentation.md`](../design/native-presentation.md). This file may describe
unfinished experiments, but it must never present them as production behavior.

## Objective

Determine whether bgfx can provide one maintainable rendering layer for Eidolon's sprite, portrait,
and 3D bodies across Windows, Linux, macOS, Android, and iOS without weakening native compositor
ownership or introducing continuous CPU readback.

The first accepted result is narrower:

> a C17 smoke program builds bgfx from pinned Git submodules through GNU Make, initializes its
> D3D11 backend against a hidden SDL3-created Win32 window, renders deterministic frames, and shuts
> down cleanly while the normal Eidolon build remains unchanged.

## Fixed boundaries

- Eidolon remains C17 and calls only bgfx's generated C99 API.
- bgfx, bx, and bimg form a quarantined C++20 dependency island; the requirement does not leak
  through the C ABI.
- Eidolon itself continues to use GNU Make. An upstream build system is allowed only inside the
  quarantined dependency build if the direct amalgamated build proves brittle.
- bgfx, bx, and bimg are pinned Git submodules under `lib/`; Eidolon's history contains only their
  commit pointers and URLs.
- The current SDL renderer remains the production path until a later gate explicitly replaces it.
- Graphics and presentation remain independent backend axes.
- Eidolon, not bgfx, owns DirectComposition devices, targets, visuals, transforms, hit regions,
  output placement, and compositor commits.
- No accepted animated path performs per-frame GPU-to-CPU readback.
- Experimental windows remain hidden; interactive visual evaluation belongs to the user.

## Dependency record

Record exact upstream commits and licenses here immediately after pinning:

```text
bgfx: 65551a7db19240b4d105f09e665190d196243d92
bx:   5a2b876258ab5843d5e1dfde695b127baf9e354a
bimg: da38bface6384cdd7f69733b08fb58e57a63cffa
```

All three snapshots use the upstream BSD-2-Clause license. Their `LICENSE` files remain available
inside each initialized submodule checkout.

The three trees must be revision-compatible siblings. Source updates are deliberate integration
changes, not an implicit download performed by a normal build.

## Gates

### Gate 0: clean baseline

- [x] presentation/frame-clock milestone committed and pushed;
- [x] `make check` passes;
- [x] Windows debug build passes;
- [x] worktree clean before bgfx changes.

Baseline commit: `d0c5f56ec2deba87e4bed09e16084a882c16b593`.

### Gate 1: reproducible dependency build

- [x] pin compatible bgfx, bx, and bimg revisions as Git submodules;
- [x] retain upstream license files;
- [x] build a static debug library with LLVM/Clang through an isolated GNU Make target;
- [x] build a release variant without sharing objects with debug;
- [x] leave plain `make` and `make check` independent from bgfx;
- [x] document every required Windows system library and compiler flag.

Preferred first attempt: compile bgfx's supported `src/amalgamated.cpp` integration unit and the
required bx/bimg implementation sources directly. Fall back to bgfx's quarantined upstream GENie
build only if the direct build would require maintaining an unofficial source manifest.

### Gate 2: C99 D3D11 smoke

- [x] compile the smoke program as C17;
- [x] obtain an `HWND` from a hidden SDL3 window without creating an SDL renderer;
- [x] initialize bgfx explicitly with `Direct3D11` rather than backend auto-selection;
- [x] clear and submit several frames without shaders or asset dependencies;
- [x] confirm the selected renderer through bgfx capabilities/statistics;
- [x] shut down bgfx before destroying the native window;
- [x] run in both debug and release modes;
- [x] prove ordinary Eidolon tests and builds still pass afterward.

This gate proves compilation, linkage, C/C++ ABI use, basic window interop, and lifecycle only. It
does not prove transparency, DirectComposition compatibility, or production renderer suitability.

### Gate 3: transparent offscreen target

- [x] render premultiplied-alpha content into a bgfx texture-backed framebuffer;
- [x] wrap or export a D3D11 texture without CPU readback;
- [x] establish explicit render-thread and synchronization ownership;
- [x] verify alpha numerically or through a hidden snapshot path;
- [x] handle target resize and destruction without device loss;
- [x] measure whether the bridge requires a GPU copy.

Reject the bridge if it depends on undocumented native-handle behavior that cannot be isolated and
tested.

### Gate 4: DirectComposition proof

- [x] Eidolon creates the Win32 host, D3D11/DirectComposition objects, and composition visual;
- [x] bgfx content reaches a composition-compatible swapchain or surface without CPU readback;
- [x] translation, scale, rotation, opacity, and fade are compositor-only updates;
- [x] unchanged body content is not rerendered while the visual moves;
- [x] body and bubble layers can update independently;
- [x] frame latency, idle cost, resize, output transfer, and device loss are observable;
- [x] user performs the interactive drag and visual-quality acceptance pass.

Possible bridges, in evaluation order:

1. bgfx renders to an externally owned D3D11 target;
2. bgfx renders offscreen and Eidolon performs one GPU-side copy into its composition swapchain;
3. a small, maintainable bgfx desktop-composition patch creates a composition swapchain directly.

The standard desktop bgfx HWND swapchain does not satisfy this gate.

`make bgfx-dcomp-smoke` is the hidden automated bridge probe. `SHOW=1` turns the same executable
into an owner-controlled visual sequence with a draggable host surface and a six-second final hold;
it is never launched by automated verification.

The Windows DirectComposition header is a C++ COM interface and does not compile as C17. The
production backend therefore needs one quarantined Windows-only C++ implementation behind a strict
C ABI, matching the existing Dear ImGui dependency boundary. Native Windows types must not cross
that interface.

### Gate 5: adoption decision

- [x] compare bgfx against the current SDL/D3D11 path and an equivalent SDL_GPU experiment;
- [x] record build complexity, binary size, CPU/GPU time, memory, copies, latency, idle behavior,
  debugging quality, and platform reach;
- [x] accept bgfx only if portability and renderer ownership outweigh its dependency and interop
  costs;
- [x] update the native-presentation design with the measured decision.

**Decision:** do not adopt bgfx into the production renderer. The Windows native-composition
backend owns D3D11 directly. Keep SDL_Renderer for the legacy window, settings, snapshots, and
fallbacks, not as a hidden device-owning bridge inside DirectComposition. Keep the bgfx proof
opt-in as evidence for a future platform where its portability can be measured against that
platform's native compositor contract. Do not use SDL_GPU for Windows DirectComposition until SDL
exposes a public external-resource contract that removes the animated CPU bridge.

### Gate 6: production presentation boundary

- [x] introduce one platform-neutral presentation owner with opaque host, layer, and target ids;
- [x] expose loaded-runtime capability flags independently from graphics renderer selection;
- [x] implement `sdl_window_legacy` as the behavior-preserving first backend;
- [x] move host/renderer lifetime, overlay setup, geometry, VSync, native drag, hit-region updates,
  and final presentation behind that backend;
- [x] add a fake-backend contract test to ordinary `make check`;
- [x] publish renderer-neutral body and dialogue scene descriptions with independent content and
  presentation revisions;
- [ ] remove transitional SDL window/renderer aliases from `EidolonApp` as backend targets replace
  them;
- [ ] enable the Win32 DirectComposition backend only after legacy snapshots and owner-controlled
  interaction remain equivalent.

This gate is the production migration, not another graphics experiment. The first checkpoint keeps
the visible runtime on `sdl_window_legacy`. Body, portrait, dialogue, and snapshot renderers still
borrow its SDL renderer explicitly; the presentation object now owns and destroys that renderer and
its host. Native DirectComposition remains disabled until immutable scene snapshots and independent
layer revisions exist.

## Build contract proven by Gates 1 and 2

`make bgfx-smoke` is an opt-in Windows target. It:

- compiles the smoke caller as C17 against bgfx's C99 header;
- compiles the bx and bgfx amalgamated units plus bimg's core image unit as C++20;
- targets only D3D11, disables bgfx video support, and disables bimg's optional ASTC decoder for
  this lifecycle probe;
- requires SSE4.2, matching the current upstream bx Windows minimum;
- archives dependency objects with `llvm-ar` before linking through `clang++`;
- links SDL3 plus `d3d11`, `dxgi`, `dxguid`, `d3dcompiler`, `gdi32`, `psapi`, `user32`, and `ole32`;
- keeps debug and release objects under separate build roots.

This is deliberately not yet the production texture contract. A body renderer must separately
decide whether it uploads already-decoded pixels, enables bimg decoders, or uses another asset
pipeline.

## D3D11 interop contract proven by Gate 3

`make bgfx-interop-smoke` forces bgfx into single-threaded mode by calling `bgfx_render_frame`
before initialization. This makes the probe thread both the API and render thread, satisfying the
documented render-thread restriction on `bgfx_get_internal_data` and future native overrides.

The accepted ownership and synchronization model is:

1. Eidolon obtains bgfx's D3D11 device on the render thread and retains it while native resources
   are in use.
2. Eidolon creates a BGRA8 render-target texture and owns its COM reference.
3. bgfx wraps the pointer through the public external-texture argument without taking ownership.
4. bgfx renders directly into that texture; this boundary performs no GPU or CPU copy.
5. `bgfx_frame` executes inline on the same thread. Native D3D11 work submitted afterward uses the
   same immediate context and therefore follows bgfx work in command order.
6. Eidolon asks bgfx to destroy the framebuffer and borrowed texture handle, advances frames to
   process destruction, then releases its COM texture.

The hidden numerical verifier performs one explicit GPU copy into a staging texture and maps it only
as test evidence. That readback is not part of the accepted presentation path. Cross-device use is
not proven; the current contract deliberately shares one D3D11 device and immediate context.

## DirectComposition contract proven so far by Gate 4

The automated probe creates an offscreen Win32 host with two independently sized composition
swapchains: one body layer and one bubble layer. Both use BGRA8, flip-sequential presentation, and
premultiplied alpha. bgfx runs headless on the same D3D11 device and wraps each logical back buffer
through its public external-texture argument.

One content frame updates both layers. A later content frame updates only the bubble through the
same external bgfx handle after its first `Present`. This follows D3D11's flip-model contract:
buffer zero is a stable logical interface whose backing identity changes across presentation, and
the application must bind it again before drawing. The probe selects the bgfx framebuffer again,
verifies both bubble revisions numerically, and observes one body presentation and two bubble
presentations. Five DirectComposition commits then change body transform, bubble offset, and bubble
opacity without another body presentation.

The accepted presentation bridge performs no GPU copy and no CPU readback. The hidden test makes
three explicit staging readbacks solely to verify the initial body and bubble colors and the second
bubble revision; these are test evidence and are not part of presentation. The relevant platform
contracts are Microsoft's [flip-model guidance](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
and [`IDXGISwapChain::GetBuffer`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getbuffer).

Lifetime order is explicit: detach bgfx framebuffers, destroy borrowed bgfx handles, advance bgfx
destruction, detach the visual tree, release composition and swapchain objects, release Eidolon's
retained D3D11 reference, and finally shut down bgfx.

Gate 4 instrumentation is deliberately local to the proof executable:

- eight ordinary bubble revisions measure monotonic time from dirty content immediately before
  bgfx submission through `IDCompositionDevice::WaitForCommitCompletion`; the report includes
  average, p95, and maximum values. This is submission-to-composition-completion latency, not a
  claim about scanout or photon latency;
- every composition commit is timed independently so content cost and compositor cost remain
  distinguishable;
- a 250 ms idle interval measures process CPU time and fails if content renders, presents, or
  composition commits advance;
- bubble resize destroys the borrowed bgfx handles, advances deferred destruction, releases buffer
  zero, calls `ResizeBuffers`, re-wraps the new buffer, and records both rebuild latency and resource
  generation;
- hidden runs enumerate monitor work areas, move the hidden host to a different output when one is
  available, restore it, and require both transitions to be observed. Visible runs observe monitor
  changes while the user drags the host;
- the adapter LUID, current output, DPI, output count, and transfer count are reported;
- `ID3D11Device::GetDeviceRemovedReason` is checked after every meaningful GPU/composition stage.
  Device-removed, reset, hung, and driver-internal failures are classified and reported with their
  HRESULT before the probe fails closed;
- presentation counters separately report ordinary presents, compositor-only commits, resource
  rebuilds, test-only verification readbacks, bridge copies, and presentation readbacks.

The probe asserts structural invariants but does not impose machine-independent latency or CPU
thresholds. Gate 5 owns comparative performance policy.

## Current checkpoint

Gates 0 through 5 are complete. Gate 6 is in progress at the behavior-preserving presentation
boundary checkpoint. Gate 5 selected direct D3D11 for the Windows compositor backend.
bgfx proved technically valid zero-copy interop, but its measured footprint and dependency cost did
not buy a cross-platform native-target contract. SDL_GPU remained renderer-portable but required an
unacceptable CPU readback bridge into DirectComposition. SDL_Renderer could wrap the external
D3D11 targets, but retaining its unused window swapchain violated presentation ownership and
produced a persistent idle worker on the test machine. No candidate entered the production
renderer. The current production runtime still uses `sdl_window_legacy`; DirectComposition has not
been enabled.

## Restart checklist

1. Read this file and `docs/design/native-presentation.md`.
2. Run `git status --short --branch`; do not absorb unrelated changes.
3. Confirm the current gate and its unchecked acceptance items.
4. Record dependency commit hashes before editing build rules.
5. Keep bgfx targets opt-in until Gate 5.
6. After each gate, update its evidence and the current checkpoint here.
7. Run `git diff --check`, the new focused target, `make check`, and a normal Windows build before
   handoff.

## Evidence log

### 2026-07-22: Gate 6 presentation ownership checkpoint

- Baseline `9d1a86472ef6f20d38eb2dd2f54e44d856df907c` was clean, owner-verified, and matched
  `origin/master` before production work began.
- `presentation` now exposes one C17 backend contract, opaque host/layer/target ids, and runtime
  capability flags without native window or graphics types.
- `sdl_window_legacy` owns the SDL window, SDL renderer, platform overlay lifecycle, geometry,
  VSync, native interactive move, input-region refresh, and present call. `EidolonApp` retains
  clearly marked borrowed SDL aliases only for renderers not yet migrated to backend targets.
- A fake backend verifies dispatch, capabilities, geometry validation, input suspension, present,
  and exactly-once destruction through ordinary `make check`.
- The hidden four-session snapshot completed through the normal runnable `make` layout and matched
  the accepted composition visually. No DirectComposition backend or production selection switch
  is active yet; interactive equivalence remains owner-controlled.
- The second checkpoint publishes stable body and per-session dialogue layers in global logical
  coordinates. Each layer carries independent content and presentation revisions; text reveal,
  expression/sprite/3D changes, and legacy-baked portrait motion affect content, while placement,
  z-order, visibility, and opacity affect presentation. The presentation owner rejects stale scene
  commits before they reach a backend.

### 2026-07-22: workstream created

- baseline `d0c5f56` is clean and matches `origin/master`;
- all ordinary C regression executables passed before the baseline push;
- Windows debug Eidolon built successfully;
- bgfx research established that rendering and native presentation remain separate responsibilities;
- no dependency source or runtime behavior changed in this checkpoint.

### 2026-07-22: dependency revisions pinned

- bgfx, bx, and bimg are Git submodules, so Eidolon stores three commit pointers rather than 2,769
  upstream files;
- the exact upstream revisions are recorded above and all three BSD-2-Clause license files remain
  in their respective upstream trees;
- normal builds do not download, update, or initialize bgfx;
- a fresh checkout initializes them explicitly with `git submodule update --init --recursive`.

### 2026-07-22: Gates 1 and 2 passed

- a clean debug rebuild produced the static archive and selected `Direct3D 11` for eight hidden
  frames;
- a clean release rebuild used its independent object root and passed the same probe;
- the C caller obtained its `HWND` through SDL properties without constructing an SDL renderer;
- bgfx reported a multithreaded render path, a 64x64 BGRA8 HWND swapchain, and clean render-thread
  shutdown;
- the debug D3D11 backend warned that its info queue retained three references during shutdown;
  this did not prevent shutdown, but Gate 3 must revisit native-device and debug-interface ownership;
- `make check` passed all ordinary C regression executables;
- the ordinary Windows debug Eidolon target remained buildable and independent of bgfx;
- `git diff --check` passed.

### 2026-07-22: Gate 3 passed

- the C17 probe forced documented single-threaded bgfx execution so native interop stayed on the
  render thread;
- the selected D3D11 backend advertised external-texture support;
- Eidolon-owned 64x64 and 96x48 BGRA8 textures were wrapped as bgfx framebuffer attachments;
- bgfx cleared both targets with premultiplied-alpha values without an intermediate bridge copy;
- a test-only D3D11 staging copy verified the exact center-pixel BGRA bytes numerically;
- framebuffer destruction, borrowed-handle destruction, COM release, and target recreation passed;
- debug and release probes both printed
  `external D3D11 targets=2 alpha=verified bridge_copies=0`;
- the existing debug info-queue reference warning remained unchanged from Gate 2.

### 2026-07-22: Gate 3 transparent-target proof passed

- the Windows-only C++ probe created a no-redirection Win32 host, DirectComposition device and
  target, root visual, and independent body and bubble visuals behind no production code path;
- each visual used its own premultiplied BGRA8 flip-sequential composition swapchain;
- bgfx rendered directly into both externally owned logical back buffers with no bridge copy;
- the bubble rendered and presented a second content revision through the same bgfx handle after
  D3D11 flip-model rotation, while the body remained at one content revision and one presentation;
- test-only staging readbacks verified the exact body color and both bubble revisions before their
  corresponding presentations; the accepted presentation path contains no readback;
- five compositor commits applied initial placement, body scale/rotation, independent bubble
  movement, and bubble opacity without rerendering unchanged body content;
- debug and release probes both printed
  `content_frames=2 body_revisions=1 bubble_revisions=2 presents=1/2` with
  `content=verified bridge_copies=0 presentation_cpu_readbacks=0`;
- an accidental retained D3D11 device reference initially triggered bgfx's debug shutdown assertion;
  the gate now enforces the corrected ownership order instead of suppressing the assertion;
- bgfx still reports its existing non-fatal D3D11 immediate-context reference warning at debug
  shutdown; this is unchanged in nature from earlier probes and remains adoption evidence;
- owner acceptance was still pending at this checkpoint and is recorded below after instrumentation.

### 2026-07-22: Gate 4 instrumentation passed

- debug and release runs each collected eight clean content-latency samples plus every
  DirectComposition commit latency without including numerical verification readbacks;
- a real bubble `ResizeBuffers` cycle advanced its resource generation, re-wrapped the external
  buffer, rendered verified content, and presented successfully;
- the hidden probe crossed and restored output bounds on the three-monitor test machine without
  showing or activating its host window;
- the idle interval advanced zero content frames, presents, and composition commits;
- all staged device-removal checks returned `S_OK`; failures now retain their operation and HRESULT;
- no bridge copy or presentation CPU readback was introduced. Four explicit staging readbacks
  remain isolated as numerical test evidence.

### 2026-07-22: Gate 4 owner acceptance passed

- the owner ran the draggable `SHOW=1` composition probe and accepted its interaction and visual
  quality as smooth;
- Gate 4 is complete; this acceptance does not pre-empt Gate 5's comparative backend decision.

### 2026-07-22: Gate 5 measured adoption decision

`make MODE=release graphics-backend-benchmark` builds one backend-selectable harness four ways and
runs the same workload through each. Every run owns the same no-redirection host, body and bubble
composition swapchains, content revisions, 16 timed bubble updates, resize, compositor-only
transforms, idle interval, output transfer, pixel verification, and device-loss checks. The
Eidolon profile uses a 1024x1024 body target and grows the bubble from 768x256 to 896x320.

An illustrative release run on the AMD integrated-GPU development machine produced:

- direct D3D11: 0.009 ms average CPU submission, 0.065 ms average D3D11 GPU time, 0.160 ms average
  content-to-compositor completion, 27.24 MiB working set, 13.77 MiB private memory, and a 171.5
  KiB executable;
- the current SDL_Renderer/D3D11 public external-texture path: 0.007 ms CPU submission, 0.044 ms
  GPU time, 0.259 ms content-to-compositor completion, 51.51 MiB working set, 21.57 MiB private
  memory, and a 173.5 KiB executable plus the already-required SDL runtime. Its renderer still
  owned an otherwise unused hidden HWND swapchain; an isolated one-second initialization-idle
  probe repeatedly charged 0.48–0.61 CPU seconds to one background worker while direct D3D11
  charged zero;
- bgfx/D3D11: 0.173 ms CPU submission, 0.549 ms GPU time, 0.862 ms
  content-to-compositor completion, 80.97 MiB working set, 139.06 MiB private memory, and a 950.5
  KiB statically linked executable;
- SDL_GPU/D3D12 bridged into D3D11: 0.434 ms average submission with a 0.512 ms p95, 0.940 ms
  content-to-compositor completion with a 6.287 ms p95, 89.66 MiB working set, 87.11 MiB private
  memory, 40 bridge transfers, and 20 presentation CPU readbacks. SDL_GPU exposes no public GPU
  timestamp query, so its internal D3D12 GPU time is reported as unavailable instead of fabricated.

The small sample is a regression probe, not a universal performance claim. Submission and GPU
timings distinguish renderer cost from the noisier compositor completion. All four paths crossed
the same three-monitor topology, passed resize and numerical alpha checks, and reported no device
loss. Only SDL_GPU copied or read presentation content. Application work counters stayed still in
every idle interval; process CPU instrumentation separately exposed the SDL_Renderer worker, which
is why zero logical work is not accepted as proof of quiescence.

Build and debugging costs differ materially:

- direct D3D11 uses the Windows SDK, native HRESULT/device-removal diagnostics, and no new runtime;
- SDL_Renderer uses Eidolon's existing C dependency and public native-device/external-texture
  properties, but it remains window-renderer-owned and brings an unused swapchain and observed idle
  worker when used only as a DirectComposition device bridge;
- bgfx adds three pinned source trees, C++20 amalgamated builds, SSE4.2, static payload, an internal
  device access contract, verbose backend diagnostics, and the existing debug shutdown reference
  warning. Its broad renderer reach does not remove platform compositor implementations;
- SDL_GPU is already cross-platform and C-native, but SDL 3.4.10 exposes neither external
  GPU-texture import/export nor GPU timestamps. On Windows it therefore requires an opaque D3D12
  device plus a separate D3D11 compositor device and a synchronous CPU bridge.

Direct D3D11 wins the Windows gate because it is the only path with zero bridge copies, compositor
ownership, no dummy swapchain, native GPU timing, and measured idle quiescence. The portability
benefit is not yet worth bgfx's cost: only its Windows D3D11 native-target path is
proven, while Linux, macOS, Android, and iOS still require platform-specific presentation and
interop work. bgfx remains an opt-in experiment. A future non-Windows backend may reopen the
decision with that platform's equivalent measurements; it does not inherit a Windows verdict.
