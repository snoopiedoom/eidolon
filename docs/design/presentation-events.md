# Backend-neutral presentation event contract

## Problem

Native presentation backends receive input, output, lifecycle, and graphics notifications through
incompatible platform APIs. Win32 delivers window messages, Wayland delivers object callbacks and
input serials, AppKit and UIKit deliver event objects, and Android delivers lifecycle and input
callbacks.

Passing those native objects into `EidolonApp` would make the application platform-specific.
Calling application behavior directly from a native callback would also couple dialogue, session
state, and body behavior to re-entrant operating-system code. Hiding native messages behind names
such as `pointer_down` without defining ordering, coordinates, capture, and overload behavior would
only conceal the coupling.

Eidolon needs a bounded C17 event contract between presentation backends and application behavior.

## Implementation status

The Windows event contract is implemented for the normal portrait `win32_dcomp` backend and its
`sdl_window_legacy` compatibility fallback:

- scene layers commit an exclusive primary action plus optional `route_pointer` capability with
  their visual/input state;
- a fixed-capacity common queue assigns monotonic sequence ids and converts overflow into one
  observable resync event;
- Win32 performs hit testing, capture, and host movement immediately, then emits activation and
  move lifecycle events without calling application behavior from `WndProc`;
- a native right-click emits `layer.context_requested`; `EidolonApp` opens settings only when the
  referenced current layer is the body;
- Win32 publishes revisioned active-host environments and caller-owned output topology through the
  same portable boundary;
- `EidolonApp` drains the complete batch before simulation, advances the matching current dialogue
  layer, and applies the newest environment plus final movement as one layout transaction.

Close and graphics-reset requests now cross the presentation queue for both Windows backends.
DirectComposition emits one typed device/backend reset request after a failed present or compositor
commit; SDL translates its target/device reset events at the presentation boundary. The application
invalidates cached targets and redraws even when the content revision is unchanged after a target
reset. A device/backend reset stops submissions and capture, preserves application state, and
attempts one fresh DirectComposition reconstruction; failure to submit a complete current scene
selects a newly bound SDL fallback, while failure of both paths stops cleanly. Both Windows
backends now emit routed middle-button mouse events through the presentation queue; the SDL-backed
event adapter still translates primary pointer and application-command behavior into fixed-size
`EidolonAppEvent` values. Raw `SDL_Event` values no longer enter `EidolonApp`. The owner confirmed
native activation, body-context settings, cancel-on-drag, click-through, smooth movement, one
stable final reflow, and SDL 3D routed rotation beyond the host bounds. The no-activate native host
deliberately does not claim keyboard focus or register a system-wide `F1` hotkey; right-click is
its reliable settings entry point.
The Windows SDL fallback still delegates character movement to the modal native top-level move
loop, so its animation cadence can pause until release. Equivalent event meaning does not imply
equivalent compositor cadence.

Output, DPI, usable bounds, safe area, orientation, and refresh are revisioned state rather than
lossless interaction history. Their publication, coalescing, topology, and wake semantics are owned
by the [presentation environment contract](presentation-environment.md).

## Goals

- normalize presentation input and lifecycle events without erasing platform capabilities;
- keep latency-critical hit testing, capture, and compositor movement inside the native backend;
- deliver product intent such as bubble activation and completed movement to the application;
- preserve event ordering and layer identity across independent scene revisions;
- define coordinates for mouse, touch, and pen without assuming global desktop coordinates exist;
- survive callback pressure, capture loss, stale layers, and a slow application without blocking
  presentation;
- support equivalent behavior from `sdl_window_legacy` and future native backends.

## Non-goals

- replacing normalized agent/session events or the presence contract;
- exposing `HWND`, `WPARAM`, Wayland objects, AppKit/UIKit objects, Android objects, COM pointers, or
  graphics handles to portable code;
- routing global keyboard input or installing system-wide hooks;
- making the presentation callback run dialogue, affect, session, or persona logic;
- requiring every platform to provide global coordinates, persistent overlays, or compositor-owned
  movement;
- defining a complete gesture-recognition framework in the first implementation;
- treating presentation readiness or frame feedback as ordinary pointer events.

## Ownership

The platform presentation backend owns:

- native callback registration and native object lifetime;
- committed layer hit geometry and topmost hit resolution;
- pointer capture and release;
- immediate operations required to preserve native interaction, including compositor or host
  movement;
- translation from native notifications into fixed-size portable events;
- a bounded event queue, coalescing state, overflow counters, and capture-cancellation state.

The common `presentation` layer owns:

- portable event types and capability flags;
- polling/draining operations;
- stable host, output, layer, pointer, and interaction-token identities;
- validation that an event contains no backend-native pointer or borrowed memory;
- the contract for sequence, scene revision, coordinate validity, and overflow recovery.

`EidolonApp` owns:

- dialogue advancement and other product meaning attached to layer activation;
- attention, selection, reflow, and body interaction policy;
- applying a completed move to application layout state;
- discarding stale layer events against the current scene;
- deciding which higher-level system receives routed pointer input.

Scene publication owns each layer's interaction policy. A backend must not infer permanent product
behavior from `EIDOLON_SCENE_LAYER_BODY` or `EIDOLON_SCENE_LAYER_DIALOGUE`. The current
DirectComposition implementation uses committed policy rather than layer kind.

Session, dialogue, affect, persona, and body systems never receive native platform events directly.

## Immediate mechanics and deferred intent

Native callbacks have two distinct responsibilities.

### Immediate backend mechanics

These may occur synchronously inside the platform callback:

- answer native hit testing from the last successfully committed input snapshot;
- capture or release a pointer;
- update a compositor-layer transform or transitional native host position during an accepted move;
- acknowledge an operating-system lifecycle requirement;
- coalesce replaceable motion into bounded backend state;
- enqueue a portable event.

They must remain allocation-free after backend initialization, non-blocking, and independent from
session or language work.

### Deferred application intent

These occur only after the application drains portable events:

- advance or dismiss a dialogue bubble;
- change attention, selection, expression, pose, or persona state;
- reflow bubbles after completed movement;
- open settings or invoke a user command;
- recreate application-owned graphics state after a reset request.

A native callback never calls an `EidolonApp` function. A backend may keep direct manipulation
smooth without waiting for the application, but it reports the resulting transition and final
geometry through the queue.

## Layer interaction policy

Every committed interactive layer declares one exclusive primary policy plus optional routed
input:

```text
pass_through
activate
move_anchor
activate | route_pointer
move_anchor | route_pointer
```

- `pass_through` never consumes pointer input.
- `activate` tracks press and release on the same layer and emits one activation when movement stays
  below the backend-independent drag threshold.
- `move_anchor` permits immediate backend-owned capture and movement and emits move lifecycle
  events.
- `route_pointer` additionally emits bounded events for otherwise-unclaimed input, initially the
  middle-button body-authoring gesture used by 3D rotation.

`activate` and `move_anchor` are mutually exclusive. `route_pointer` is a capability bit rather
than a competing primary action, so native movement never has to wait for application routing.

Alpha or geometric hit regions decide whether a pointer is over the layer. Policy decides what that
hit means. Changing policy is a presentation revision and becomes active atomically with the scene
commit that carries it.

The first native portrait implementation needs `activate` for dialogue layers and `move_anchor` for
the body. More complex gesture arbitration remains an open decision.

## Conceptual event model

The exact C names may change, but every event is a fixed-size discriminated value equivalent to:

```c
typedef struct EidolonPresentationEvent {
    EidolonPresentationEventKind kind;
    uint64_t sequence;
    uint64_t monotonic_ns;
    EidolonPresentationHost host;
    union {
        EidolonPresentationLayerEvent layer;
        EidolonPresentationPointerEvent pointer;
        EidolonPresentationMoveEvent move;
        EidolonPresentationEnvironmentChanged environment;
        EidolonPresentationGraphicsEvent graphics;
    } data;
} EidolonPresentationEvent;
```

Each payload remains fixed-size and contains ids and copied scalars only. The value never contains a
native handle, pointer to backend storage, dynamically allocated string, or object whose lifetime
is controlled by the callback.

The initial event vocabulary is:

```text
layer.activated
layer.context_requested
pointer.down
pointer.motion
pointer.up
pointer.canceled
move.started
move.motion
move.completed
move.canceled
environment.changed
host.close_requested
graphics.reset_required
queue.resync_required
```

`pointer.*` is emitted only for `route_pointer`. `layer.activated` is the stable semantic result of
an `activate` policy, not a synonym for a platform mouse-up message.
`layer.context_requested` carries the hit layer without deciding what product UI it opens.
`move.motion` and replaceable `environment.changed` publications may be coalesced.
`move.completed`, `move.canceled`, context requests, close requests, graphics-reset requests, and
resync requests are causal or structural edges and may not be silently dropped.

`environment.changed` carries the complete fixed-size active-host environment for its revision.
The latest revision supersedes older unapplied environment publications; full output topology is
queried separately through caller-owned storage.

Presentation readiness, frame-latency objects, compositor feedback, and timers remain members of
the presentation wait set. They wake the loop but do not enter this product-event queue.

## Identity and ordering

- each backend assigns a strictly increasing sequence to accepted events;
- sequence is authoritative within one presentation instance; wall-clock time is diagnostic only;
- pointer ids remain stable from down through up or cancellation;
- events reference stable ids, never addresses of layer records;
- layer events carry the committed scene revision used for hit testing;
- an event for a retired layer is discarded by the application without mutating another layer;
- an environment publication remains valid independently from layer retirement;
- move completion contains final observed geometry and its environment revision, preserving the
  causal edge even when a newer environment supersedes that state;
- restarting a presentation creates a new instance identity and invalidates old interaction tokens.

Native serials required by Wayland or another platform remain backend-owned. Portable code receives
an opaque, presentation-instance-scoped interaction token only if it must request a native action
in response to an event. The token is not serialized or interpreted outside `presentation`.

## Coordinate contract

An event may carry several coordinate spaces with explicit validity flags:

- **host logical** — logical coordinates within the presentation host;
- **layer local** — coordinates after inverse committed transform, in the layer's content space;
- **output logical** — coordinates within the active output;
- **global logical** — desktop-global coordinates only where the platform supports them;
- **buffer pixel** — native target pixels for authoring or diagnostic use, never the default
  application coordinate.

Logical coordinates are independent from target resolution. Scale, rotation, pivot, and host offset
are resolved from the same committed scene revision used by hit testing. A backend may omit
unsupported output/global coordinates but may not fill them with fabricated zeroes.

Buttons and modifiers are copied into portable bitsets. Device kind distinguishes mouse, touch,
pen, and unknown. Touch and pen are not converted into fake mouse ids inside the contract.

## Data flow

```text
native callback + last committed input snapshot
                         ↓
immediate hit/capture/move mechanics
                         ↓
fixed-size normalized presentation event
                         ↓
bounded backend queue
                         ↓
presentation_poll_event()
                         ↓
EidolonApp routing against current scene
                         ↓
dialogue activation | final reflow | body interaction | lifecycle recovery
```

The application drains presentation events alongside backend-neutral fallback application events
before simulation and scene publication. SDL remains the current wait/pump implementation, but its
event union is contained inside the adapter. A bounded per-tick budget prevents motion floods from
starving rendering. Terminal and structural events remain observable even when replaceable motion
is coalesced.

## Queue and overload behavior

- queue capacity is fixed when the presentation instance is created;
- native callbacks do not allocate, wait for the render thread, or invoke user code;
- the backend coalesces consecutive motion for the same pointer/interaction, accumulating relative
  layer-local movement, and the latest geometry for the same host;
- a pending environment publication may be replaced by the newest revision for the same host;
- down, up, cancel, activation, move completion/cancellation, close, reset, and resync events are
  not ordinary coalescible state;
- when pressure threatens a complete pointer or move sequence, the backend cancels that interaction
  and sets a sticky resync condition;
- the next available slot exposes `queue.resync_required`; the application retrieves the
  authoritative active environment/topology revision and clears transient interaction state;
- overflow increments bounded diagnostics rather than producing one log line per dropped motion;
- a full queue never blocks native callback dispatch or frame presentation.

The implementation may use a ring buffer, but the contract requires bounded behavior rather than a
particular lock-free algorithm. If callbacks can arrive off the presentation thread, synchronization
must remain bounded and must not call application code while holding a backend lock.

## Invariants

- no platform-native event type or handle crosses the C17 presentation boundary;
- a native callback never mutates dialogue, session, affect, persona, or body state;
- hit testing and emitted coordinates use the last successfully committed scene, not pending state;
- layer interaction policy becomes active atomically with its visual and input geometry;
- transparent or `pass_through` pixels reach the underlying application;
- one physical activation produces at most one `layer.activated`;
- one native context request identifies the current committed layer without backend-owned product
  behavior;
- capture loss always produces cancellation or an observable resync requirement;
- dragging remains responsive when rendering misses a frame;
- drag completion produces one final geometry suitable for one application reflow;
- event pressure cannot postpone presentation indefinitely;
- stale layer events cannot activate a newly reused slot or another session bubble;
- unsupported coordinates and capabilities are absent and reported, never fabricated;
- presentation events are separate from normalized agent/session events.

## Failure behavior

- stale layer or scene revision: discard the product event and retain current state;
- capture lost: stop native manipulation and enqueue cancellation;
- queue pressure: coalesce replaceable events, then cancel/resync rather than block;
- unsupported native interaction: reject the policy or report capability fallback before commit;
- output disappears during movement: cancel or migrate according to backend capability, publish
  the resulting environment, and preserve session/dialogue state;
- scale changes mid-interaction: retain the strongest valid anchor, publish a new environment
  revision, and ensure move completion identifies the observed revision;
- graphics reset: stop target submissions, enqueue one reset request, and keep non-presentation
  state alive; invalidate and redraw targets locally, or replace device/backend presentation
  resources through one bounded native reconstruction followed by explicit fallback;
- application drains slowly: native manipulation may continue, but only bounded/coalesced state is
  retained;
- malformed backend event: reject it at the common boundary and increment a bounded diagnostic.

## Acceptance criteria

- clicking an opaque native dialogue layer advances exactly that session bubble once;
- right-clicking the native body opens settings without making the no-activate overlay steal
  keyboard focus;
- pressing or releasing outside the originally activated layer does not activate it;
- transparent portrait and bubble pixels continue to reach the underlying desktop application;
- native body dragging remains smooth while breathing, expression, and dialogue presentation
  continue;
- drag release emits one completion with final geometry and causes one application reflow rather
  than per-motion reflow;
- moving across monitors publishes deterministic environment revisions without resetting dialogue,
  session ordering, or body animation;
- forced capture loss cancels the interaction without leaving a stuck pressed or dragging state;
- a motion-flood test demonstrates bounded queue use, coalescing, and observable resync behavior;
- a retired bubble cannot receive a delayed activation intended for its former layer id/revision;
- `sdl_window_legacy` and `win32_dcomp` produce equivalent activation and move-completion meaning,
  while backend capabilities may produce different in-drag cadence;
- the Win32 adapter remains the only translation unit that interprets Win32 messages;
- ordinary builds, presentation contract tests, the hidden DirectComposition smoke, and
  owner-controlled interaction checks pass.

## Implementation sequence

1. [x] Add fixed-size presentation event and layer-interaction-policy types to the C17 contract.
2. [x] Add a bounded queue and polling operation owned by each presentation instance/backend.
3. [x] Publish interaction policy atomically with committed layer geometry.
4. [x] Translate DirectComposition `WndProc` input into activation, context, move lifecycle,
   cancellation, close, and reset events while retaining immediate native mechanics.
5. [x] Drain and route presentation events in `EidolonApp`; remove direct layer-kind behavior from
   the Win32 adapter.
6. [x] Implement equivalent SDL legacy translation without changing product behavior. The
   fixed-size fallback application adapter preserves primary pointer and command behavior; close,
   reset, and middle-button 3D authoring now cross the presentation contract.
7. [x] Add deterministic queue, stale-revision, capture-loss, and coordinate-transform tests.
8. [x] Perform owner-controlled bubble-click, drag, mixed-DPI, cross-monitor, and click-through
   checks. The native portrait path is accepted; the legacy modal-drag limitation is confirmed and
   documented. Hidden deterministic probes cover native reconstruction and forced SDL fallback;
   both recovery branches are owner-accepted. The fallback regression pass, persisted selection,
   body-capability fallback, and post-drag resumption are owner-accepted. Physical output removal
   remains optional hardware evidence rather than pre-default work.

Environment and topology implementation proceeds through the separate sequence in
[`presentation-environment.md`](presentation-environment.md).

## Open decisions

- exact portable button/modifier bit assignments and interaction-token lifetime;
- whether future compositor-layer dragging moves a shared character anchor or one body layer;
- how mobile multi-touch arbitration interacts with dialogue activation and character movement;
- which diagnostics belong in the event value versus presentation counters;
- whether a future dedicated presentation thread changes the queue from same-thread bounded FIFO to
  SPSC without changing the public contract.
