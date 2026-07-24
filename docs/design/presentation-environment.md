# Presentation environment and output topology contract

## Problem

Native platforms describe the environment around a presentation host through fragmented,
platform-specific notifications. A monitor crossing may change host geometry, output identity,
content scale, usable bounds, refresh cadence, orientation, safe area, or graphics suitability.
Those facts do not arrive as one portable atomic transaction.

Treating every native notification as an authoritative product event would expose torn
intermediate state. Treating every notification as a reason to query unrelated platform APIs from
`EidolonApp` would leak native ownership and introduce event/query races. Treating output changes
like lossless user actions would also retain obsolete monitor history that the application should
skip.

Eidolon needs a revisioned presentation-environment contract that cleanly separates:

1. lossless edge events that cannot be reconstructed;
2. the current active-host environment, which is small and coalescible;
3. the complete output topology, which is variable-sized and queried separately.

## Goals

- publish one coherent, immutable active-host environment per revision;
- preserve causally unique input and lifecycle edges independently from environment state;
- coalesce obsolete monitor, DPI, bounds, safe-area, orientation, and refresh history;
- keep native handles and native callback ordering inside each platform adapter;
- preserve body position, session identity, dialogue state, and animation across environment
  changes;
- provide explicit coordinate and validity semantics without assuming a global desktop;
- wake the application when a new edge or environment revision becomes observable;
- support Windows, Wayland/X11, macOS, Android, iOS, and `sdl_window_legacy` without flattening their
  capabilities into false equivalence;
- make queue overflow and output removal recoverable through the same authoritative snapshot path.

## Non-goals

- defining graphics-resource recreation or device-loss policy beyond identifying which environment
  fields changed;
- promising stable output identity across process restarts or hardware reconfiguration;
- requiring global coordinates on Wayland, Android, or iOS;
- replaying every intermediate host position during native movement;
- using nominal refresh as a substitute for compositor presentation feedback;
- exposing a complete display-settings API;
- choosing one host-per-output or one sparse host for every platform;
- moving session, dialogue, affect, persona, or body ownership into presentation;
- allocating, blocking, logging repeatedly, or calling application code from native callbacks.

## Terminology

### Edge event

A causally unique occurrence that cannot be reconstructed by querying current state. Examples
include:

```text
layer.activated
move.completed
move.canceled
host.close_requested
graphics.reset_required
```

Required edge events are ordered and may not be silently replaced by a later state snapshot.

### Active presentation environment

A fixed-size value describing the environment currently affecting one presentation host:

```text
host geometry
active output identity
output bounds
usable bounds
safe-area insets
content scale
pixel scale
orientation/transform
nominal refresh information
capabilities and validity
```

Only the latest coherent unapplied revision matters.

### Output topology

The variable-sized set of outputs currently visible to a presentation instance. It exists for
placement policy, settings, migration, and recovery. It is not copied into every active-host
environment event.

### Reconciliation

The backend-owned operation that collects all currently observable native facts, canonicalizes
them into portable values, compares them with the last published environment, and publishes a new
revision only when semantic state changed.

## Ownership

The native presentation backend owns:

- platform callback registration and native object lifetime;
- immediate actions mandated by the platform, such as accepting a DPI-suggested host rectangle;
- accumulation of fragmented native notifications;
- active-output selection for each host;
- construction and canonicalization of environment and topology snapshots;
- opaque native-to-portable output-id mapping;
- publication order, revision counters, coalescing, wake signaling, and bounded diagnostics.

The common `presentation` layer owns:

- fixed-size portable environment and notification types;
- capability and valid-field bit assignments;
- revision and change-mask semantics;
- bootstrap, latest-snapshot, topology-copy, event-drain, and resynchronization operations;
- validation that no native handle, borrowed pointer, or backend-owned object crosses the C17
  boundary.

`EidolonApp` owns:

- the last applied environment revision;
- product placement policy and configured monitor-bound preference;
- preservation of the body anchor;
- one transactional update of coordinate scale, cadence, layout, and output-dependent resources;
- deciding when a newer environment makes older unapplied environment revisions irrelevant;
- session, dialogue, expression, motion, and persona continuity.

The graphics/raster owner consumes applied scale, extent, color, or capability changes and
recreates only resources whose requirements changed. It does not enumerate native outputs.

## State model

The exact C names may change, but the fixed-size active-host value is equivalent to:

```c
typedef struct EidolonPresentationEnvironment {
    uint64_t revision;
    uint64_t topology_revision;
    EidolonPresentationHost host;
    EidolonPresentationOutput active_output;

    EidolonPresentationGeometry host_geometry;
    EidolonPresentationRect output_bounds;
    EidolonPresentationRect usable_bounds;
    EidolonPresentationInsets safe_area;

    float content_scale;
    float pixel_scale;
    float nominal_refresh_hz;
    EidolonPresentationOrientation orientation;
    EidolonPresentationCoordinateSpace coordinate_space;

    uint64_t capabilities;
    uint64_t valid_fields;
    uint64_t changed_fields;
} EidolonPresentationEnvironment;
```

The value contains copied ids and scalars only. `coordinate_space` declares the shared space used by
host geometry and advertised bounds. `topology_revision` lets an otherwise unchanged active-host
snapshot announce that the complete output set changed. Unsupported information is absent through
`valid_fields`; zero never pretends that an unsupported coordinate, topology, or refresh value is
real.

An environment notification carries the complete value:

```c
typedef struct EidolonPresentationEnvironmentChanged {
    EidolonPresentationEnvironment environment;
} EidolonPresentationEnvironmentChanged;
```

The notification is a revisioned state publication, not a lossless history entry. A pending older
environment notification may be replaced by a newer one for the same host.

The complete topology uses caller-owned storage rather than borrowed backend memory. Conceptually:

```c
typedef struct EidolonPresentationTopologyResult {
    uint64_t revision;
    size_t required_count;
    size_t copied_count;
    EidolonPresentationTopologyStatus status;
} EidolonPresentationTopologyResult;

EidolonPresentationTopologyResult
presentation_copy_outputs(EidolonPresentation *presentation,
                          EidolonPresentationOutputInfo *outputs,
                          size_t capacity);
```

Each output record carries its opaque id, declared coordinate space, optional bounds and output
properties, capabilities, and portable flags such as `primary`. Native output handles never cross
the boundary.

If capacity is insufficient or topology changes during the copy, the result reports the required
count and current revision. The caller may retry outside a native callback. No output array is
borrowed across the boundary.

## Identity and lifetime

- host and output ids are opaque and scoped to one presentation instance;
- ids are never native handles, pointers, array indexes, or hashes interpreted by portable code;
- an output id remains stable while that backend output object remains alive;
- output removal retires its id; reappearance may receive a new id;
- environment revisions increase strictly within one presentation instance;
- topology revisions increase strictly when the visible output set or any advertised output
  property changes;
- restarting presentation invalidates prior host ids, output ids, revisions, and interaction
  tokens;
- persisted placement stores a portable policy and coordinates, not a presentation-instance output
  id alone.

## Coordinate and scale contract

The contract distinguishes:

- **host logical** — logical coordinates within the presentation host;
- **output logical** — logical coordinates within the active output;
- **global logical** — desktop-global logical coordinates only when supported;
- **global pixel** — desktop-global native pixels when a backend such as Win32 exposes host and
  monitor geometry in that space;
- **buffer pixel** — physical target pixels;
- **safe-area logical** — output- or host-local insets unavailable to ordinary content.

`content_scale` converts platform logical units into the product's logical presentation scale.
`pixel_scale` converts logical target extent into buffer pixels where the backend can report it.
They may differ on platforms with compositor-side fractional scaling.

Output bounds describe the output's full extent in the declared space. Usable bounds exclude
persistent desktop reservations such as taskbars, docks, or panels when the platform exposes them.
Safe-area insets describe transient or device-specific occlusion constraints such as display
cutouts and mobile system regions.

Orientation is a declared transform, not inferred by swapping width and height. Global coordinates
are optional. A backend without global placement uses output-local anchors and reports that
capability honestly.

## Publication and reconciliation

Native callbacks do not each become a portable environment event. They update backend-owned facts
or mark the environment dirty.

At a backend-defined reconciliation boundary:

1. collect all currently observable host and output facts;
2. canonicalize units, optional fields, and opaque ids;
3. construct one candidate active-host environment;
4. compare semantic fields and the current topology revision with the last published value;
5. assign a new revision only when observable state changed;
6. publish the immutable snapshot before exposing its notification;
7. enqueue or replace the pending `environment.changed` notification;
8. signal the presentation wake source when the queue transitions from unobservable to observable.

The reconciliation boundary may be the end of a native callback batch, a platform configure/done
boundary, or an explicit backend task. It must not wait for rendering, classification, session
work, or application acknowledgement.

## Current implementation

The opt-in `win32_dcomp` backend and default `sdl_window_legacy` backend now implement the
producer/consumer path:

- Win32 callbacks perform mandatory DPI resizing and otherwise mark environment state dirty;
- reconciliation enumerates monitors, preserves process-local opaque output ids, and publishes
  global-pixel host geometry, monitor/work bounds, active output, DPI scale, nominal refresh,
  orientation, primary-output metadata, capabilities, and topology revision;
- the common queue replaces an older pending environment publication without reordering lossless
  activation or movement edges;
- `EidolonApp` drains the complete presentation batch, applies only the newest environment, then
  performs one anchor-preserving cadence/layout transaction;
- native avatar, primary-output, virtual-desktop, and custom bubble bounds no longer query SDL
  display state piecemeal;
- Win32 messages wake the owning thread's native message pump; reconciliation occurs when portable
  presentation work is polled, outside `WndProc`.
- SDL display and host-window events only mark the legacy environment dirty; the adapter
  reconciles one global-logical snapshot, compares cached caller-owned topology, and publishes the
  newest revision through the common bounded queue;
- `EidolonApp` obtains cadence, placement bounds, scale, pixel density, and opaque output identity
  from the presentation snapshot instead of calling SDL display APIs.

The owner confirmed the Win32 cross-monitor environment transaction and bubble-bound behavior.
Close and typed graphics-reset notification now cross the presentation event contract.
Middle-button routed-pointer parity, including SDL capture beyond the host bounds, is
owner-confirmed.
Device/backend reset now replaces only presentation and raster resources: it preserves the current
body anchor and application-owned session, dialogue, expression, motion, and scene state; applies
the replacement backend's authoritative environment; reflows once; and must present the newest
complete scene before the replacement is accepted. One native reconstruction is attempted before
an explicit SDL fallback. Hidden deterministic probes cover both branches; visible recovery and
the resulting interaction are owner-accepted. Real device loss and physical output-removal proof
remain open.
The SDL fallback publishes equivalent environment state, but its Windows modal drag may pause
application consumption until release; native cadence acceptance belongs to `win32_dcomp`. Its
mixed-DPI/output/placement and post-drag resumption still require owner confirmation before native
presentation becomes the normal selection.

## Data flow

```text
native callbacks
      ↓
mandatory immediate platform action + mark environment dirty
      ↓
backend reconciliation boundary
      ↓
immutable environment snapshot R42
      ↓
atomic publication + coalescible environment.changed { R42 }
      ↓
presentation wake source
      ↓
bounded application drain before simulation
      ↓
one environment transaction
      ↓
anchor preservation + cadence/layout/resource reconciliation
```

Lossless edges use the same queue and wake source but remain separate values:

```text
native interaction
      ↓
layer.activated | move.completed | move.canceled
      ↓
ordered lossless edge delivery
```

## Application transaction

When `EidolonApp` receives an environment revision newer than the last applied revision, it applies
the newest available revision as one transaction:

1. preserve the current body anchor in the strongest valid coordinate space;
2. update active output, bounds, scale, orientation, and cadence policy;
3. resize or recreate only output-dependent targets;
4. restore the anchor in the new environment;
5. resolve configured usable/monitor bounds;
6. reflow bubbles once while preserving deterministic session ordering;
7. publish the next scene revision;
8. record the applied environment revision.

If several environment notifications are already queued, the application may skip directly to the
newest revision. It must not replay obsolete intermediate layouts.

Anchor strength degrades explicitly:

```text
global logical anchor, when valid
        ↓
output-local logical anchor
        ↓
normalized position inside usable bounds
        ↓
configured conservative fallback
```

Changing environment does not reset dialogue reveal, expression performance, motion phase, session
ownership, or persona state.

Device/backend replacement uses the same preservation rule. It captures the strongest current body
anchor, ends transient pointer interactions, destroys only the failed presentation/event-pump
resources, bootstraps the candidate's authoritative environment and cadence, restores the anchor,
reflows once, and publishes the newest application-owned scene. A candidate is not made visible or
accepted until that complete scene commits and presents successfully.

## Relationship to native movement

Native dragging remains immediate and independent from application frame cadence.

`move.completed` is a lossless edge containing final observed host geometry and the environment
revision associated with that observation. The application then applies the newest environment
revision available before its one final reflow:

- if the referenced revision is still current, use it directly;
- if a newer environment is already published, apply the newer snapshot;
- if revisions or geometry became ambiguous through overflow, resynchronize from the authoritative
  latest snapshot;
- never query native monitor state piecemeal from `EidolonApp`.

This preserves the causal fact that movement completed without forcing obsolete monitor history to
be replayed.

## Wake and wait contract

Presentation readiness and product events share a wait boundary but not one event vocabulary.

The application wait set may contain:

- native message readiness;
- presentation-event queue readiness;
- frame-latency/compositor readiness;
- software-frame deadline;
- shutdown or worker completion signals.

Enqueuing the first observable edge or environment revision must wake a sleeping application.
Frame feedback wakes presentation but does not become `environment.changed`. Static content may
sleep indefinitely until a wake source changes; continuous animation follows the selected cadence.

No backend may rely on unrelated SDL traffic to make its private native queue observable.

## Queue and overload behavior

- lossless required edges preserve order until the contract's explicit cancel/resync fallback;
- environment notifications are replaceable by a newer revision for the same host;
- topology changes update authoritative backend state even if their notification is coalesced;
- queue overflow never blocks a native callback;
- if required edges cannot be retained, transient interaction is canceled and one sticky
  `queue.resync_required` becomes observable;
- resynchronization retrieves the latest active environment and topology revision, clears
  transient input state, and preserves product state;
- diagnostics count coalesced environment revisions, overflow, resync, and reconciliation failures
  without logging every native notification.

## Platform mappings

### Windows

- use a per-monitor-DPI-aware host;
- treat `WM_DPICHANGED` as both an immediate native sizing obligation and an environment invalidation;
- observe movement, display, work-area, and device notifications as dirty inputs rather than
  separate portable truth;
- derive the current output and usable bounds through Win32 inside the adapter;
- keep `HWND` and `HMONITOR` private;
- publish one reconciled snapshot after the native state is observable.

### Wayland and X11

- Wayland output objects and their asynchronous property/configure boundaries remain adapter-owned;
- global placement is absent unless the compositor protocol actually provides it;
- fractional scale, output transform, logical size, and surface configure state reconcile into one
  host environment;
- X11 may expose global geometry, but that does not make it mandatory in the common contract.

### macOS

- screen-change, backing-property, screen-parameter, and movement notifications dirty the
  environment;
- `NSScreen`, `NSWindow`, and Core Animation objects stay inside the adapter;
- screen frame, visible frame, backing scale, orientation, and cadence reconcile before publication.

### Android

- window metrics, insets, density, rotation, lifecycle, and current display form the environment;
- safe area and window bounds are primary; cross-application persistent overlay remains a separate
  capability;
- surface recreation is graphics/presentation recovery, not persona or session restart.

### iOS and iPadOS

- one window scene defines the presentation host;
- scene geometry, traits, safe area, screen, scale, and orientation form the environment;
- app-hosted presentation is reported honestly; no cross-application overlay capability is
  fabricated.

### SDL legacy

- SDL display/window events invalidate the environment;
- SDL queries occur inside `sdl_window_legacy`, not in product modules;
- the backend emits the same active-environment meaning even when SDL owns the native window;
- SDL ids remain adapter details and do not become the common output-id representation by
  assumption.

## Invariants

- one environment revision is internally coherent and immutable after publication;
- publication happens before its notification becomes observable;
- environment history may coalesce; lossless edge history may not silently coalesce;
- `EidolonApp` never assembles one environment from several native callbacks;
- native handles and borrowed platform objects never cross the C17 boundary;
- unsupported coordinate spaces and metrics are absent, never fabricated;
- environment application is one transaction and causes at most one bubble reflow;
- moving between outputs preserves the strongest valid body anchor;
- environment changes do not reset session, dialogue, expression, animation, or persona state;
- nominal refresh informs cadence but does not impersonate compositor feedback;
- output removal cannot strand a host on a retired output id;
- queue overflow converges through authoritative resynchronization;
- presentation events and environment revisions cannot be starved by the render loop;
- a sleeping application wakes for newly observable presentation work.

## Failure behavior

- **environment query fails:** retain the last valid snapshot, mark diagnostics, and retry only on
  another native invalidation or bounded recovery task;
- **output disappears:** reconcile to a valid fallback output or output-relative safe placement,
  retire the old id, and preserve product state;
- **scale changes during movement:** continue native movement, publish the new environment, and
  reconcile once at completion;
- **topology changes during copy:** return the newer revision and required count; caller retries
  without using a partial topology as authoritative;
- **notification is stale:** discard it when its revision is not newer than the applied revision;
- **notification is lost or coalesced:** bootstrap/resync retrieves the latest published snapshot;
- **queue overflows:** cancel ambiguous transient input, publish resync, and never block native
  dispatch;
- **wake signaling fails:** mark the backend unhealthy and fall back to a bounded application wake;
  do not silently depend on incidental user input;
- **graphics resources reject the environment:** retain session/persona state, stop invalid
  submissions, and follow graphics-reset recovery;
- **no usable/global bounds exist:** use safe-area or output-local bounds, then the configured
  conservative fallback.

## Acceptance criteria

- crossing monitors with different scale preserves the body anchor without oscillation or repeated
  bubble reflow;
- one native monitor crossing produces one applied environment transaction even when several
  native notifications occur;
- queued revisions `R42`, `R43`, and `R44` may collapse to `R44` without losing any required edge;
- a `move.completed` edge remains observable even when its associated environment notification is
  replaced by a newer revision;
- removing the active output migrates or falls back without resetting dialogue, sessions,
  expression, or animation phase;
- usable-bounds changes keep body and bubbles inside configured placement bounds;
- scale changes update target pixel extent without changing logical body size unexpectedly;
- refresh changes update cadence ownership without a catch-up burst or animation reset;
- unsupported global coordinates remain invalid on Wayland/mobile and do not become zero-valued
  placement;
- Android/iOS safe-area or orientation changes use the same environment transaction semantics;
- `sdl_window_legacy` and `win32_dcomp` expose equivalent active-environment meaning;
- a sleeping static presentation wakes for an environment revision without requiring unrelated SDL
  input;
- forced queue pressure converges to the latest environment through one observable resync;
- fake-backend tests prove publication-before-notification, revision monotonicity, coalescing,
  stale rejection, and topology-copy retry;
- owner-controlled Windows checks cover mixed-DPI crossing, different refresh outputs, work-area
  changes, output removal/fallback where practical, and stable final bubble placement.

## Implementation sequence

1. [x] Add fixed-size environment, validity, change-mask, orientation, coordinate-space,
   topology-revision, and opaque output-id types.
2. [x] Add latest-environment and caller-owned topology-copy operations.
3. [x] Extend the bounded queue with replaceable environment publication while retaining lossless
   edge ordering.
4. [x] Add fake-backend coverage for validation, coalescing, ordering, and topology-copy retry.
5. [x] Implement Win32 dirty-state collection and one reconciliation function.
6. [x] Integrate Win32 native-message readiness with the application wait boundary.
7. [x] Apply environment revisions transactionally in `EidolonApp`, preserving anchor and
   reflowing once.
8. [x] Translate SDL display/window invalidations into equivalent legacy environment
   publications.
9. [~] Owner-confirm mixed-DPI, refresh, usable-bounds, cross-monitor, and wake behavior on the
   native path; the ordinary cross-monitor path is accepted, while physical output removal remains
   unproven. The SDL fallback's modal drag is documented rather than treated as native cadence
   parity; its remaining environment and post-drag resumption checks remain a pre-default gate.
10. Implement the same contract for later Wayland/X11, macOS, Android, and iOS backends without
    changing portable product ownership.

## Open decisions

- whether the fixed-size active environment should retain separate content and pixel scales on
  every backend or advertise one as optional;
- the bounded representation of variable/unknown refresh and variable-refresh ranges;
- whether color space, HDR state, and subpixel order join this contract or a graphics-capability
  snapshot;
- the configured output-selection policy when a persisted preferred output is absent;
- the maximum topology size used by tests and diagnostics without imposing a runtime hard limit;
- whether a future dedicated presentation thread uses SPSC publication, a seqlock, or another
  bounded mechanism behind the unchanged C17 contract;
- when Windows moves from a transitional shared host to output-local hosts or layer-anchor movement;
- which environment diagnostics become visible in the settings UI.

## Platform references

- [Windows `WM_DPICHANGED`](https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged)
- [Windows `GetDpiForWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow)
- [Windows multi-monitor positioning](https://learn.microsoft.com/en-us/windows/win32/gdi/positioning-objects-on-multiple-display-monitors)
- [Wayland protocol and output model](https://wayland.freedesktop.org/docs/book/Protocol.html)
- [AppKit backing-property changes](https://developer.apple.com/documentation/appkit/nswindow/didchangebackingpropertiesnotification)
- [AppKit screen enumeration and change notification](https://developer.apple.com/documentation/appkit/nsscreen/screens)
- [Android `WindowMetrics`](https://developer.android.com/reference/android/view/WindowMetrics)
- [UIKit `UIWindowScene`](https://developer.apple.com/documentation/uikit/uiwindowscene)
- [UIKit trait environment](https://developer.apple.com/documentation/uikit/uitraitcollection)
- [SDL3 display and window event vocabulary](https://wiki.libsdl.org/SDL3/SDL_EventType)
- [SDL3 usable display bounds](https://wiki.libsdl.org/SDL3/SDL_GetDisplayUsableBounds)
