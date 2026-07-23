# Native presentation and graphics stack

## Status

This document defines the intended presentation architecture and the gated migration from the
current SDL-window implementation. It is a design plan, not a claim that the native backends below
already exist.

The central decision is:

> rendering pixels and presenting desktop surfaces are separate systems.

A graphics API produces body and dialogue content. A platform presentation backend gives that
content a native place on screen, moves it, clips it, fades it, routes input to it, and schedules it
with the operating-system compositor. Selecting Vulkan, SDL_GPU, or bgfx does not remove the need
for the second system.

## Problem

The current runtime creates one transparent SDL window and one SDL renderer. `app` owns the window,
display scale, event loop, cadence, renderer selection, drag state, and composition state. `draw`
composes the body and bubbles into that window and calls `SDL_RenderPresent`. Windows-specific code
adds hit testing, regions, readback, and native drag behavior around the SDL-owned surface.

That was a good bootstrap, but it makes unrelated work share one invalidation boundary:

- moving the character means moving the complete native window;
- changing scale can resize both content and the transparent swapchain;
- body and bubbles share one presentation cadence and damage region;
- pixel hit testing can require information recovered from the composed result;
- monitor transitions affect rendering even when content did not change;
- a native title-bar move enters the Win32 moving/sizing modal loop, so application-driven motion
  on the same thread can stop until the drag completes;
- presentation-thread stalls become visible as animation, input, and window-motion stalls at once.

SDL documents `SDL_SetWindowPosition` as a main-thread operation whose result may be asynchronous.
On Windows, SDL ultimately implements it with `SetWindowPos`. Likewise, `SDL_RenderPresent` is a
main-thread operation. Repeating these operations from one game-style loop does not turn them into a
compositor-native scene graph:

- [SDL_SetWindowPosition](https://wiki.libsdl.org/SDL3/SDL_SetWindowPosition)
- [SDL Windows window backend](https://github.com/libsdl-org/SDL/blob/main/src/video/windows/SDL_windowswindow.c)
- [SDL_RenderPresent](https://wiki.libsdl.org/SDL3/SDL_RenderPresent)
- [Win32 moving/sizing modal loop](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-entersizemove)

The expensive work observed so far is dominated by transparent surface area, readback, native
window movement, and synchronization. Rio's triangle and draw-call counts have not established a
graphics-submission bottleneck. Replacing D3D11 with Vulkan before repairing presentation would
therefore add ownership without addressing the measured failure.

## Goals

- move, scale, rotate, fade, and order body and bubble surfaces without rerendering unchanged
  content;
- let the native compositor own final placement and presentation;
- keep body renderers independent from window-system and session-source details;
- preserve one renderer-neutral scene across Windows, Linux, macOS, Android, and iOS;
- expose platform capability differences explicitly instead of promising false parity;
- eliminate ordinary GPU-to-CPU readback and full transparent-canvas invalidation;
- keep an SDL fallback so unsupported compositors and early ports remain usable;
- permit a later graphics-API migration without another presentation rewrite;
- remain event-driven and inexpensive while idle.

## Non-goals

- rewriting session, affect, delivery, or persona logic;
- selecting Vulkan because it is newer;
- forcing one graphics API onto every operating system;
- hiding unsupported mobile behavior behind a misleading common API;
- building a general game engine or scene editor;
- requiring every body renderer to expose native GPU resources immediately;
- replacing the separate SDL/Dear ImGui settings window.

## Two independent backend axes

The architecture has two backend families.

### Presentation backend

The presentation backend owns native surfaces and compositor integration:

```text
win32_dcomp
wayland_layer
wayland_toplevel
x11_composite
macos_core_animation
android_surface_control
ios_core_animation
sdl_window_legacy
```

### Graphics backend

The graphics backend produces pixels in a render target:

```text
sdl_renderer_legacy
d3d11
sdl_gpu
vulkan
metal
optional future implementation
```

These axes must not be represented as one enum. `Vulkan` does not mean `Wayland`, `D3D11` does not
mean `DirectComposition`, and `Metal` does not decide whether content lives in a macOS panel or an
iOS scene.

The platform package chooses a compatible pair. Core code sees opaque host, layer, render-target,
and synchronization handles rather than native types.

## Target data flow

```text
normalized session state + semantic performance
                         ↓
renderer-neutral scene snapshot
                         ↓
        ┌────────────────┴────────────────┐
        ↓                                 ↓
body renderer                      dialogue renderer
pixels + body geometry             pixels + bubble geometry
        └────────────────┬────────────────┘
                         ↓
native presentation backend
                         ↓
compositor layer tree
                         ↓
display
```

The scene snapshot contains stable identifiers, content revisions, transforms, opacity, z-order,
visible bounds, face/head bounds, and click regions. It does not contain `HWND`, `wl_surface`,
`CALayer`, `ASurfaceControl`, Vulkan images, or session-adapter objects.

## Ownership

- `app` owns product lifecycle and publishes immutable scene snapshots;
- session, affect, expression, delivery, and motion modules own semantic state exactly as today;
- each body renderer owns body assets, animation evaluation, body-local rendering, and geometric
  output;
- dialogue owns text layout and bubble content;
- `presentation` owns backend selection, hosts, layers, output topology, input routing, cadence, and
  compositor commits;
- one platform backend owns every native presentation object it creates;
- one graphics backend owns every GPU object it creates;
- a platform-specific interop bridge owns synchronization when graphics and presentation use
  different APIs.

No backend may acquire session ownership. No body renderer may move a native window.

## Presentation contract

The first interface should remain deliberately smaller than a general scene API. Conceptually it
needs operations equivalent to:

```c
bool presentation_open(const EidolonPresentationConfig *config);
void presentation_close(void);

size_t presentation_enumerate_outputs(EidolonOutput *outputs, size_t capacity);
EidolonHost presentation_create_host(const EidolonHostDesc *desc);
EidolonLayer presentation_create_layer(EidolonHost host,
                                       const EidolonLayerDesc *desc);
void presentation_destroy_layer(EidolonLayer layer);

EidolonRenderTarget presentation_acquire_target(EidolonLayer layer,
                                                const EidolonTargetDesc *desc);
bool presentation_submit_content(EidolonLayer layer,
                                 const EidolonLayerContent *content);
void presentation_set_layer_state(EidolonLayer layer,
                                  const EidolonLayerState *state);
void presentation_set_input_region(EidolonLayer layer,
                                   const EidolonInputRegion *region);

bool presentation_commit(const EidolonSceneRevision *revision);
EidolonWaitResult presentation_wait(const EidolonWaitSet *events);
EidolonPresentationCapabilities presentation_capabilities(void);
```

The real C interface may split or rename these operations, but it must preserve their ownership.
Backends are selected behind one vtable or equivalent static dispatch table; platform `#ifdef`s do
not spread through `app`, body renderers, or dialogue.

### Required capability reporting

A backend reports at least:

```text
persistent_over_other_apps
global_placement
multiple_outputs
compositor_transform
compositor_opacity
compositor_animation
per_pixel_input
native_interactive_toplevel_move
gpu_zero_copy
presentation_feedback
background_visibility
```

Capabilities describe loaded runtime behavior, not compilation targets. For example, a Wayland
build may discover layer-shell support on one compositor and fall back to a normal toplevel on
another.

## Layer model

The minimum scene contains:

- one body layer;
- zero to four independently owned dialogue layers;
- optional transient interaction or approval layers;
- no full-monitor color buffer unless a platform proves that it is required and inexpensive.

Each layer has two independent revisions:

- **content revision** changes when pixels change;
- **presentation revision** changes when position, scale, rotation, opacity, clipping, or z-order
  changes.

Dragging, whole-image portrait motion, display scaling, bubble placement, and fade-out normally
change only presentation state. Typing new glyphs, swapping an expression image, advancing a sprite
frame, or rendering a new 3D pose changes content.

This distinction is the performance contract. A backend that rerenders pixels for a transform has
fallen back; it must report that limitation rather than silently redefining the layer model.

## Content interchange

The first implementation supports two content paths:

1. **portable upload:** premultiplied RGBA pixels plus a damage rectangle;
2. **native target:** a backend-created opaque render target consumed without CPU readback.

Portable upload keeps static images, text, tests, and unsupported platforms working. Native targets
are required for continuously animated 3D and are the desired path for all active bodies.

Core code never imports an arbitrary native texture handle. An interop object records:

- owner backend;
- pixel format and alpha convention;
- extent and color space;
- acquire and release synchronization;
- content revision;
- destruction callback.

Unsupported interop fails during backend negotiation and selects a configured fallback. It never
downloads a full animated frame every tick as an invisible compatibility mechanism.

## Alpha and hit testing

Compositors arrange pixels; they do not understand Eidolon's semantic click regions. Hit geometry
therefore remains renderer-owned and presentation-consumed.

- dialogue bubbles provide analytic rounded rectangles and tails;
- portrait and sprite packages provide an asset alpha mask or conservative body envelope;
- 3D provides projected geometry or a conservatively updated mask;
- face/head bounds remain distinct from ordinary body bounds;
- transforms are applied to hit geometry by the presentation backend;
- structural content changes may rebuild a mask asynchronously;
- breathing, dragging, fades, and whole-layer transforms do not trigger GPU readback.

Failure degrades to a conservative interactive rectangle. It must not freeze presentation or make
the complete transparent host consume input.

## Frame scheduling

One software clock cannot model every native compositor. The shared scheduler decides *whether* a
scene needs another content or presentation update; the backend decides *when* it can be presented.

The event loop waits on platform events and presentation readiness together:

- Windows: message queue plus a DXGI frame-latency waitable object;
- Wayland: display file descriptor plus `wl_surface.frame` callbacks and buffer release;
- X11: event connection plus Present completion where available;
- macOS/iOS: display-link callback and drawable availability;
- Android: `AChoreographer` callback and buffer/surface transaction completion;
- legacy SDL: the existing frame clock and SDL event queue.

No backend spins while idle. No backend accumulates catch-up frames. A missed frame advances motion
from monotonic time and presents the newest valid state.

VSync and an explicit FPS ceiling remain user policy, but they map onto backend behavior instead of
forcing a universal delay loop. Continuous content can follow the display; static content sleeps
until state changes. The backend must log requested mode, active mode, cadence owner, output refresh,
frame latency, and fallback reason.

## Input and dragging

For a compositor-layer backend, dragging changes a body-layer transform. It does not move the host
window. The compositor samples the latest committed transform while body animation continues on its
own content cadence.

A native top-level move remains a fallback. It is not equivalent:

- Windows caption dragging enters the moving/sizing modal loop;
- Wayland `xdg_toplevel.move` transfers the operation to the compositor and consumes a valid input
  serial;
- X11 `_NET_WM_MOVERESIZE` delegates the operation to the window manager;
- macOS `performWindowDragWithEvent:` delegates it to WindowServer.

The input contract routes pointer id, device kind, global/output position when available, layer id,
layer-local position, buttons, modifiers, timestamp, and compositor serial where required. It must
support mouse, touch, and pen without synthesizing mouse-only assumptions into core state.

## Output topology and DPI

Hosts are output-local where the platform supports it. Logical scene coordinates are independent
from buffer pixels.

- output bounds and usable bounds are separate;
- each output reports logical scale, pixel scale, transform/orientation, and refresh information;
- the character has one global logical anchor where the platform exposes global placement;
- moving between outputs preserves the anchor and rebuilds only output-dependent resources;
- cross-output transfer must not reset session ordering, animation, or dialogue reveal;
- platforms without global coordinates expose output-relative placement only;
- mobile scenes expose their safe area and orientation instead of pretending to be desktop
  monitors.

## Graphics strategy analysis

### Option A: keep SDL_Renderer everywhere

SDL_Renderer is the smallest and safest 2D API. It already provides hardware acceleration, text and
image composition, and a working snapshot path. Its weaknesses are ownership rather than raw speed:

- presentation terminates at `SDL_RenderPresent` on an SDL window;
- the API is primarily 2D;
- advanced external-resource and compositor integration is backend-specific;
- the current application composes all visible content into one window.

**Decision:** retain it for the legacy presentation backend, settings UI, snapshots, and migration
bridges. Do not make it the permanent native-overlay presentation API.

### Option B: migrate rendering to SDL_GPU

SDL_GPU is a modern C API over Vulkan, D3D12, and Metal. It supports command buffers, offscreen
textures, render and compute passes, multiple frames in flight, and explicit present modes. It is
already in SDL3 and matches Eidolon's language and dependency preferences:

- [SDL_GPU overview and platform requirements](https://wiki.libsdl.org/SDL3/CategoryGPU)
- [SDL GPU window ownership](https://wiki.libsdl.org/SDL3/SDL_ClaimWindowForGPUDevice)
- [SDL_GPUTexture opaque handle](https://wiki.libsdl.org/SDL3/SDL_GPUTexture)

It is the strongest candidate for a future shared 3D graphics layer, especially because Eidolon
already vendors SDL_shadercross. HLSL can remain the authored shader language and be compiled
offline to SPIR-V, DXIL/DXBC, MSL, and metallib formats:

- [SDL_GPU shader formats](https://wiki.libsdl.org/SDL3/SDL_GPUShaderFormat)
- [SDL_shadercross workflow](https://moonside.games/posts/introducing-sdl-shadercross/)

However, the public GPU device and texture types are opaque, and normal presentation claims an
`SDL_Window`. SDL 3.4.10 does not provide a public external-image import/export contract for
DirectComposition, SurfaceControl, Core Animation, and arbitrary Wayland child surfaces. SDL's
open [external-texture issue](https://github.com/libsdl-org/SDL/issues/14077) describes this as
functionality still required before the GPU renderer can replace existing renderers.

**Decision:** do not migrate first. Build a focused interop spike after the presentation contract
exists. SDL_GPU becomes the preferred shared renderer only if it can satisfy all of these without
CPU readback:

1. render into or copy into a native compositor layer;
2. preserve premultiplied alpha correctly;
3. expose safe acquire/release synchronization;
4. support multiple independently sized body and bubble targets;
5. survive device loss and output transfer;
6. retain hidden snapshot testing.

The Windows spike rendered correctly through SDL_GPU/D3D12, but the only public bridge to Eidolon's
D3D11 composition swapchain was a synchronous GPU-to-CPU download followed by a D3D11 upload. The
Eidolon-sized release probe performed 40 bridge transfers and 20 presentation readbacks. This
fails the native-presentation invariant regardless of its acceptable small-sample throughput.
Failure of this spike does not invalidate SDL_GPU for legacy windows or selected platforms; it
rejects SDL_GPU as the Windows DirectComposition renderer until public external-resource interop
exists.

### Option C: use raw Vulkan everywhere

Vulkan provides explicit submission, memory, synchronization, and resource control. This can reduce
driver CPU overhead for workloads with high draw-call or resource-management pressure. The same
control also makes the application responsible for those systems. Khronos calls synchronization
one of Vulkan's most complex areas and warns that incorrect synchronization can produce both hidden
bugs and unnecessary GPU idling:

- [Vulkan synchronization guide](https://docs.vulkan.org/guide/latest/synchronization.html)
- [Vulkan memory allocation guide](https://docs.vulkan.org/guide/latest/memory_allocation.html)
- [Vulkan basics and CPU-overhead rationale](https://docs.vulkan.org/samples/latest/samples/vulkan_basics.html)

Vulkan is native on Windows, Linux, and supported Android devices. It is not Apple's native API;
macOS and iOS require a portability implementation such as MoltenVK, which translates a Vulkan
subset and SPIR-V onto Metal:

- [MoltenVK](https://github.com/KhronosGroup/MoltenVK)

Raw Vulkan also does not create DirectComposition visuals, Android overlay permission, Wayland
surface roles, Core Animation layers, hit regions, or mobile lifecycle. On Windows, presenting a
Vulkan HWND swapchain would preserve the current whole-window ownership problem. Sharing Vulkan
images with a D3D composition swapchain would introduce external-memory and cross-API
synchronization complexity solely to reach an API D3D11 already feeds directly.

**Decision:** Vulkan is an eligible native renderer for Linux and Android, and a possible future
SDL_GPU-selected backend. It is not the universal Eidolon API and is not justified by the current 3D
workload. Reconsider a direct Vulkan renderer only after profiling proves CPU submission or a needed
GPU feature is unavailable through the selected abstraction.

### Option D: adopt bgfx

bgfx is mature, broad, and technically credible. It supports D3D11, D3D12, Metal, OpenGL/GLES,
Vulkan, WebGPU, Windows, Linux, macOS, Android, and iOS. It provides a C99 API and examples for
multiple windows:

- [bgfx repository and supported backends](https://github.com/bkaradzic/bgfx)
- [bgfx overview](https://bkaradzic.github.io/bgfx/overview.html)
- [bgfx examples](https://bkaradzic.github.io/bgfx/examples.html)

Its cost is architectural:

- the current implementation requires C++20 and brings `bx`, `bimg`, shader tools, and an upstream
  GENie build that Eidolon bypasses through supported amalgamated units;
- bgfx owns a render thread, graphics device, command submission, and normal window framebuffer;
- compositor integration still requires platform-specific hosts, input, surface roles, and
  lifecycle;
- advanced native interop uses internal data whose contract requires render-thread and bgfx-internal
  knowledge;
- adapting its owned swapchains to DirectComposition or SurfaceControl would be the hardest part of
  Eidolon, not something bgfx removes.

The official build expects sibling `bx`, `bimg`, and `bgfx` trees and its own project-generation
workflow:

- [bgfx build documentation](https://bkaradzic.github.io/bgfx/build.html)
- [bgfx internals and render-thread model](https://bkaradzic.github.io/bgfx/internals.html)

**Decision:** evaluate bgfx as an opt-in shared graphics backend, never as the presentation layer.
The experiment must prove its C ABI, backend lifecycle, transparent offscreen rendering, and native
target interop before it can affect production. Eidolon retains compositor ownership regardless of
the result. A normal bgfx window swapchain is not evidence of DirectComposition or equivalent
mobile/desktop compositor integration.

The Windows experiment has now proven one narrower native-target contract: in documented
single-threaded mode, Eidolon can obtain bgfx's D3D11 device, create and own a BGRA8 render-target
texture, and pass that texture through bgfx's public external-texture argument. bgfx renders
directly into it with no bridge copy. This proves same-device texture interop, premultiplied-alpha
preservation, recreation, and lifetime ordering.

The Windows probe extended that contract to Eidolon-owned DirectComposition swapchains. bgfx can
wrap and render directly into a premultiplied BGRA8 flip-sequential logical back buffer, reuse
the wrapper after `Present`, rebind it as D3D11 flip-model presentation requires, and update body and
bubble swapchains independently. DirectComposition then changes layer transform, offset, and
opacity without another body submission. Test-only staging readbacks verify both content revisions;
the measured presentation bridge has zero GPU copies and zero CPU readbacks. This proves the
D3D11/DirectComposition bridge. Resize, output transfer, device-loss observation, latency, idle
behavior, and interactive quality also passed their separate gate. It does not prove equivalent
native-target contracts on other bgfx backends.

The final comparison did not justify adoption. Against the same 1024x1024 body and growing bubble
workload, direct D3D11 used 27.24 MiB working set and 13.77 MiB private memory. bgfx remained
zero-copy but used 80.97 MiB working set and 139.06 MiB private memory, produced a 950.5 KiB static
probe rather than a 171.5 KiB native probe, and took about nineteen times as long in measured CPU
submission. GPU and full-frame latency stayed small for both. Those costs would be defensible if
bgfx removed several platform rendering implementations; the experiment proved only the Windows
D3D11 bridge, while every native compositor still needs its own host, lifecycle, synchronization,
and interop proof.

SDL_Renderer could also wrap each composition back buffer through its public external-D3D11 texture
property with negligible submission overhead. It still required an SDL-owned hidden HWND and
swapchain solely to own the renderer. Before any composition layer existed, repeated isolated idle
probes charged 0.48–0.61 CPU seconds per second to one background worker on the AMD test machine;
direct D3D11 charged zero. That dummy presentation owner is both a measured cost and the wrong
architecture for a native-composition backend. SDL_Renderer therefore remains a legacy,
settings, snapshot, and fallback renderer rather than the device owner behind DirectComposition.

**Measured decision:** use direct D3D11 in the Windows native-composition backend. Retain bgfx as an
opt-in research target, not a production dependency. Reopen the decision only when a non-Windows
native backend can measure a concrete reduction in renderer implementations or a feature
unavailable through the platform-aligned path.

### Option E: platform-aligned native graphics

The direct pairing is:

```text
Windows       D3D11 initially, DirectComposition presentation
Linux         Vulkan, Wayland/X11 presentation
macOS / iOS   Metal, Core Animation presentation
Android       Vulkan with GLES fallback, SurfaceControl/Window presentation
```

This gives the cleanest zero-copy route and best platform diagnostics. It also creates three real
graphics implementations, device-loss paths, and synchronization models.

**Decision:** use platform-native graphics where compositor integration requires it, beginning with
the existing D3D11 renderer. Keep CPU scene evaluation, assets, shaders, material descriptions, and
tests shared. Do not preemptively write Metal and Vulkan renderers before their platform backends are
active.

## Selected strategy

Eidolon adopts a hybrid stack:

1. native presentation backends own hosts, layers, input, outputs, cadence, and compositor commits;
2. the existing SDL-window path becomes an explicit legacy fallback;
3. Windows keeps D3D11 during the DirectComposition migration because it already renders Rio and
   feeds composition swapchains directly;
4. SDL_Renderer remains for settings, snapshots, fallback presentation, and temporary 2D bridges;
5. the first bgfx and SDL_GPU gates are complete: neither enters the Windows production renderer;
   their opt-in probes remain available for future platform-specific reevaluation;
6. Vulkan is used directly only where platform integration or measured workload justifies it;
7. no graphics abstraction enters the core stack until it preserves native presentation ownership
   and avoids animated CPU readback.

This deliberately optimizes for native composition first. A graphics migration can then be measured
against a correct presentation architecture.

## Platform plans

### Windows: DirectComposition and D3D11

The first production backend uses one transparent, non-activating host per output and a
DirectComposition visual tree. The host should be a borderless tool window with
`WS_EX_NOACTIVATE`; topmost behavior remains product policy. A measured
`WS_EX_NOREDIRECTIONBITMAP` path avoids allocating a DWM redirection surface for a window whose
visible content comes entirely from composition visuals. Body and bubbles are separate child
visuals backed by independently sized composition swapchains or surfaces:

- [Win32 extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles#ws_ex_noredirectionbitmap)

DirectComposition composes bitmap-backed visuals, applies offsets/transforms/opacity, and commits a
batch atomically for DWM's next frame. Visuals are clipped to their target window, which is why
output-local hosts are preferable to one enormous virtual-desktop swapchain:

- [DirectComposition basic concepts](https://learn.microsoft.com/en-us/windows/win32/directcomp/basic-concepts)
- [CreateSwapChainForComposition](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition)
- [DirectComposition visual-tree example](https://learn.microsoft.com/en-us/windows/win32/directcomp/how-to--build-a-visual-tree)
- [DXGI frame-latency waitable object](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)

Microsoft's `dcomp.h` exposes C++ COM interfaces rather than a C-compatible vtable declaration.
The Windows backend is therefore a small C++ implementation behind the platform-neutral C ABI;
Eidolon core, scene ownership, and every public presentation type remain C17. This is a language
boundary around a platform header, not permission for native COM types or presentation policy to
leak into the renderer.

The host uses native hit testing to return transparent outside transformed input geometry. Crossing
an output boundary transfers the logical layer to the adjacent host without resetting content or
session state. Movement, scale, rotation, and bubble fade are visual properties. Rio redraws only
because her pose changes, not because her screen position changes.

Microsoft recommends the newer Windows.UI.Composition visual layer for modern desktop applications;
it supplies compositor-process animations independent of the UI thread, but brings WinRT activation
and ABI work into a C17 project. DirectComposition is selected initially because it has a direct
Win32 COM and D3D11 path. A Windows.UI.Composition spike becomes worthwhile if compositor-owned
input-driven animation, effects, or a newer presentation API materially improves the accepted
DirectComposition proof:

- [Windows visual layer in desktop applications](https://learn.microsoft.com/en-us/windows/uwp/composition/visual-layer-in-desktop-apps)

### Wayland

Wayland gives every `wl_surface` content, local coordinates, input regions, frame callbacks, and
buffer release. `wl_subsurface` adds parent-relative positioning and z-order without automatic
clipping to the parent:

- [Wayland core surface and subsurface protocol](https://wayland.app/protocols/wayland)
- [xdg-shell interactive move](https://wayland.app/protocols/xdg-shell)

The preferred desktop-presence backend uses layer-shell where the compositor advertises it. It
creates one output-local layer surface with no exclusive zone and no keyboard focus, then attaches
body and bubble subsurfaces. Layer-shell is an extension and cannot be assumed:

- [wlr layer-shell protocol](https://wayland.app/protocols/wlr-layer-shell-unstable-v1)

The fallback is a normal `xdg_toplevel`. Standard Wayland does not give ordinary clients arbitrary
global placement; user-driven `xdg_toplevel.move` is available with a valid input serial. Capability
reporting must expose that difference.

Rendering begins with Vulkan where supported. EGL/GLES or the SDL legacy window remains a fallback.
Frame production follows `wl_surface.frame`; buffer reuse follows release events.

### X11

The X11 backend uses small ARGB windows or child windows, compositor-aware transparency, Shape or
XFixes input regions, and Present feedback where available. `_NET_WM_MOVERESIZE` delegates fallback
toplevel movement to the window manager, and EWMH state describes above/taskbar behavior:

- [Extended Window Manager Hints](https://specifications.freedesktop.org/wm/latest-single/)

X11 and Wayland are separate backends. A compile-time `linux` backend that branches throughout its
event loop is not an acceptable replacement.

### macOS

The macOS backend uses a transparent borderless `NSPanel` or `NSWindow`, a Core Animation layer
tree, and `CAMetalLayer` content. Core Animation owns transform, opacity, and compositing; Metal owns
animated GPU content. `performWindowDragWithEvent:` remains a top-level fallback and returns after
handing movement to WindowServer:

- [NSWindow performWindowDragWithEvent](https://developer.apple.com/documentation/appkit/nswindow/performdrag%28with%3A%29?language=objc)
- [CAMetalLayer](https://developer.apple.com/documentation/quartzcore/cametallayer)
- [Metal overview](https://developer.apple.com/metal/)

The backend is implemented in Objective-C or Objective-C++ behind a C ABI. Core Eidolon remains C17.

### Android

Android has two product modes:

1. an ordinary Activity-hosted embodiment;
2. an optional cross-application overlay using `TYPE_APPLICATION_OVERLAY`.

The NDK `ASurfaceControl` API exposes a system-compositor surface hierarchy and atomic transactions
for buffers, position, scale, alpha, crop, visibility, and z-order. It is the native layer model for
supported Android versions:

- [Android NDK SurfaceControl](https://developer.android.com/ndk/reference/group/native-activity)
- [SurfaceControl.Transaction](https://developer.android.com/reference/android/view/SurfaceControl.Transaction)

System overlays require the special `SYSTEM_ALERT_WINDOW` permission and are placed below critical
system windows. They are an opt-in capability, not default onboarding:

- [TYPE_APPLICATION_OVERLAY](https://developer.android.com/reference/android/view/WindowManager.LayoutParams)

Vulkan is the preferred animated 3D renderer when device support satisfies the required feature
set; GLES or the SDL-hosted path remains a fallback. `AChoreographer` owns frame callbacks. Touch
dragging changes layer position rather than invoking desktop window semantics.

### iOS and iPadOS

iOS uses `UIWindowScene`, Core Animation, and `CAMetalLayer` inside the application's scenes. Scene
lifecycle, safe areas, orientation, touch routing, and suspension are backend responsibilities:

- [UIKit scene lifecycle](https://developer.apple.com/documentation/uikit/managing-your-app-s-life-cycle)
- [Supporting multiple iPad windows](https://developer.apple.com/documentation/uikit/supporting-multiple-windows-on-ipad)
- [CAMetalLayer](https://developer.apple.com/documentation/quartzcore/cametallayer)

iOS does not expose a general-purpose persistent window above other applications. Background scenes
are eventually suspended, and Picture in Picture is a specialized media contract rather than an
arbitrary interactive overlay:

- [Apple Picture in Picture](https://developer.apple.com/documentation/avkit/adopting-picture-in-picture-in-a-standard-player)

The honest capability is therefore an app-hosted embodiment. Future notifications, Live Activities,
widgets, or PiP-like product surfaces are separate integrations, not alternate body renderers.

## Migration plan

### Phase 0: preserve and instrument the current runtime

- keep the current SDL window path working;
- retain VSync/FPS configuration and hitch diagnostics;
- record content revisions, presentation revisions, readback count, native moves, swapchain extent,
  compositor commits, and frame latency;
- add no new graphics dependency.

Gate: existing checks and user-owned interaction behavior remain unchanged.

### Phase 1: extract the presentation boundary

- introduce opaque host/layer/target ids and capability flags;
- move window creation, output enumeration, event integration, cadence, hit testing, drag routing,
  and presentation out of `app`/`draw`;
- implement `sdl_window_legacy` with behavior equivalent to the current runtime;
- split body and dialogue scene descriptions even if the legacy backend still composites them into
  one target;
- keep the settings UI outside this abstraction.

Gate: snapshots and interactive SDL behavior match before any native compositor backend is enabled.

**Status:** in progress. The first production checkpoint now has an opaque C17 presentation owner,
runtime capability flags, a fake-backend contract test, and a behavior-preserving
`sdl_window_legacy` backend. Host and SDL-renderer lifetime, overlay setup, geometry, VSync, native
drag, input-region refresh, and final present flow through that owner. Existing body and dialogue
renderers still borrow the legacy SDL renderer and have not yet published independent scene layers
or revisions. DirectComposition therefore remains disabled.

### Phase 2: Windows portrait proof

- create one output-local Win32 host and DirectComposition device/tree;
- create one portrait visual with premultiplied alpha;
- move, scale, rotate, and fade it through visual properties;
- implement transformed hit geometry;
- integrate the DXGI waitable object with the Win32 message queue;
- cross an output boundary without resetting the portrait or grabbing focus.

Gate: dragging at monitor refresh does not pause breathing, expression, dialogue reveal, or desktop
input; no normal-frame readback occurs.

### Phase 3: independent dialogue layers

- give each session bubble its own layer and content revision;
- update text content only as reveal changes;
- implement five-second hold and three-second fade as opacity state;
- preserve deterministic placement and session ordering while transforms change;
- remove the default local-state placeholder from normal operation.

Gate: one bubble can type or fade without invalidating body or sibling-bubble content.

### Phase 4: sprite and 3D targets

- route sprite frames into the body layer;
- let Rio render directly into a backend target;
- preserve GPU skinning and material correctness;
- remove Windows framebuffer readback from ordinary click-through;
- handle device loss and resource recreation transactionally.

Gate: all three bodies work through the same scene and presentation contract; inactive renderers stay
uninitialized.

### Phase 5: graphics-backend spike — complete for Windows

- build bgfx, bx, and bimg from pinned Git submodules behind an opt-in GNU Make target;
- prove the bgfx C99 API against a hidden D3D11 window, then a transparent offscreen target and
  DirectComposition bridge;
- implement the smallest equivalent SDL_GPU offscreen body renderer and expose every required
  bridge transfer;
- test D3D12/DirectComposition, Vulkan/Wayland, Metal/Core Animation, and Android SurfaceControl
  interop separately;
- measure CPU time, GPU time, memory, frame latency, power behavior, and implementation size against
  the native path;
- accept a shared backend only for combinations that remain zero-copy or have a measured,
  acceptable GPU-only copy and remain observable;
- test raw Vulkan only on Linux/Android unless a measured Windows requirement appears;
- keep every candidate opt-in until the measured adoption decision.

Gate: the selected graphics backend improves portability or measured behavior without weakening
presentation ownership.

### Phase 6: Linux and macOS

- implement Wayland layer-shell, Wayland toplevel fallback, and X11 as separate capabilities;
- implement macOS Core Animation/Metal behind a C ABI;
- retain the SDL legacy backend during bring-up;
- add platform-owned hidden integration probes without seizing the desktop.

Gate: each backend passes its own capability and failure-degradation matrix.

### Phase 7: Android and iOS

- create native application shells and lifecycle bridges;
- implement Activity/SurfaceControl and UIWindowScene/Core Animation hosts;
- add touch-first input and mobile-safe-area placement;
- make Android cross-app overlay an explicit permission-gated mode;
- document iOS app-hosted limits in product UX;
- add battery/thermal-aware cadence policy.

Gate: mobile builds preserve persona/session semantics while exposing truthful presentation
capabilities.

## Verification

### Automated

- a fake presentation backend verifies ownership, revision, and fallback behavior;
- identical scene snapshots produce identical layer state;
- transform-only changes never increment content revision;
- stale backend callbacks cannot mutate a newer scene revision;
- output removal reparents or falls back without losing session state;
- backend failure retains dialogue/session observation and selects the configured fallback;
- snapshots remain deterministic and do not require a visible compositor;
- all native objects are released after partial initialization and device loss;
- debug validation layers remain clean for D3D, Vulkan, and Metal paths.

### Performance evidence

Collect, per layer and frame:

- CPU update and render time;
- GPU time where supported;
- compositor commit and present latency;
- buffer count, target extent, and allocated bytes;
- content uploads and damaged area;
- GPU readbacks;
- native top-level moves;
- dropped, coalesced, and late frames;
- active backend, graphics API, output, scale, and fallback reason.

Claims such as "Vulkan is faster" are unacceptable without this evidence on the target hardware.

### Interactive

The owner verifies:

- drag smoothness and continued animation;
- click-through around transparent pixels;
- no focus theft or taskbar pollution;
- cross-monitor DPI and refresh transitions;
- stable bubble placement and fade;
- sprite, portrait, and 3D parity;
- settings and restart persistence;
- Android permission UX and mobile touch behavior when those ports exist.

Automated snapshots do not prove desktop feel.

## Failure behavior

- native backend unavailable: select `sdl_window_legacy` and log the missing capability;
- layer creation failure: keep session state, hide only the failed layer, and offer configured
  fallback content;
- GPU interop failure: reject the zero-copy path during initialization; never improvise continuous
  CPU readback;
- output removed: move affected layers to the configured fallback output and preserve logical
  ordering;
- compositor disconnect: release native objects, keep non-presentation state, and retry only through
  bounded platform policy;
- device loss: stop submissions, recreate graphics and presentation resources, then submit the
  newest scene revision;
- permission rejected on Android: remain app-hosted;
- application backgrounded on iOS: persist state and stop expensive rendering;
- unsupported feature: degrade through declared capability mapping rather than platform-name checks
  in core code.

## Acceptance criteria

- movement, opacity, scale, and rotation of unchanged content are compositor operations;
- body and every visible bubble have independent content and presentation revisions;
- normal dragging performs no repeated native top-level repositioning on compositor-layer backends;
- normal animated presentation performs no GPU-to-CPU full-frame readback;
- input outside declared regions reaches the underlying application;
- frame pacing is driven by native readiness and user policy, without a hidden 30 Hz path;
- output and DPI changes preserve body/session identity and dialogue state;
- the Windows path remains D3D11 until a measured migration gate passes;
- Linux distinguishes Wayland layer-shell, Wayland toplevel, and X11 behavior;
- Android distinguishes app-hosted and permission-gated overlay modes;
- iOS accurately reports app-hosted presentation rather than cross-app persistence;
- renderer failure cannot stop session observation or dialogue ownership;
- SDL legacy presentation remains buildable until every supported platform has an accepted native
  replacement.

## Open decisions

- exact minimum Windows version and whether Windows.UI.Composition ever replaces DirectComposition;
- one output-local host versus another sparse-host strategy after measuring transparent host cost;
- the public native-target interop structure and synchronization ownership;
- whether dialogue text becomes a native vector/text layer or remains raster content;
- the first supported Wayland compositor matrix and layer-shell fallback UX;
- minimum Android API for the native compositor backend;
- graphics device sharing across multiple hosts and targets;
- whether a non-Windows bgfx or SDL_GPU backend can prove a better native-target contract than its
  platform-aligned renderer; the Windows Phase 5 comparison selected direct D3D11;
- plug-in ABI and trust model for future third-party body renderers.
