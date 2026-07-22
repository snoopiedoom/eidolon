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
- [ ] frame latency, idle cost, resize, output transfer, and device loss are observable;
- [ ] user performs the interactive drag and visual-quality acceptance pass.

Possible bridges, in evaluation order:

1. bgfx renders to an externally owned D3D11 target;
2. bgfx renders offscreen and Eidolon performs one GPU-side copy into its composition swapchain;
3. a small, maintainable bgfx desktop-composition patch creates a composition swapchain directly.

The standard desktop bgfx HWND swapchain does not satisfy this gate.

`make bgfx-dcomp-smoke` is the hidden automated bridge probe. `SHOW=1` turns the same executable
into a brief owner-controlled visual sequence; it is never launched by automated verification.

The Windows DirectComposition header is a C++ COM interface and does not compile as C17. The
production backend therefore needs one quarantined Windows-only C++ implementation behind a strict
C ABI, matching the existing Dear ImGui dependency boundary. Native Windows types must not cross
that interface.

### Gate 5: adoption decision

- [ ] compare bgfx against the current SDL/D3D11 path and an equivalent SDL_GPU experiment;
- [ ] record build complexity, binary size, CPU/GPU time, memory, copies, latency, idle behavior,
  debugging quality, and platform reach;
- [ ] accept bgfx only if portability and renderer ownership outweigh its dependency and interop
  costs;
- [ ] update the native-presentation design with the measured decision.

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

## Current checkpoint

Gates 0 through 3 are complete. Gate 4 has proven the static and repeated-content bridge, independent
layers, and compositor-only presentation changes in debug and release. It remains incomplete until
latency/idle, resize, output transfer, and device-loss evidence exists and the owner accepts the
visible probe. The production renderer remains unchanged.

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

### 2026-07-22: Gate 4 bridge proof passed

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
- the visible `SHOW=1` sequence and operational failure/latency instrumentation remain pending.
