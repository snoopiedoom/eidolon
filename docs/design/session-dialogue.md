# Multi-session dialogue specification

## Problem

Several agent sessions may produce output concurrently, but one latest-message bubble loses
identity, scroll state, and context. Eidolon needs one bubble per active session around a shared
character without letting adapter discovery leak into rendering or letting bubbles jump whenever
another session speaks.

## Goals

- preserve session identity and independent dialogue state;
- show multiple active outputs simultaneously;
- use real session titles with stable fallbacks;
- keep placement deterministic and visually stable;
- keep inactive bubbles fully visible for five seconds, then fade them out over three seconds;
- route clicks to the bubble actually clicked;
- keep the one-session case simple.

## Ownership

`session_registry` owns identity, metadata, transcript progress, activity, dialogue, affect track,
visibility, and eviction. `bubble_layout` owns only geometry. `dialogue` owns reveal, scrolling,
pagination, drawing data, and hit testing for one text stream. `app` coordinates them but does not
maintain a parallel session collection.

## Discovery and identity

The durable conceptual key is `(source_id, session_id)`; adapter kind is metadata describing how the
source is interpreted. Current source retains the legacy `provider` field and keys by
`(provider, session id)`, so explicit source-instance identity remains an implementation gap.
Discovery order, transcript filename, and title are not keys. Legacy transcript discovery examines
recent files asynchronously, ignores subagent rollouts, and publishes completed snapshots to the
presentation thread.

New final output updates only the matching registry entry. Reopening or appending a long-lived
rollout must not create a second bubble merely because its dated directory is old.

## Visibility policy

The first bounded implementation tracks eight sessions and presents four. Active/unread entries take
priority. Capacity overflow evicts the oldest inactive entry deterministically and logs it.

Each bubble has an independent five-second presentation lease. Text deltas, completed messages, and
turn activity renew the matching session's lease. Five seconds without adapter activity hides that
bubble even when its dialogue has not finished revealing; the session and its dialogue state remain
tracked and the next output makes it visible again. The timeout is policy and should become
runtime-configurable.

## Layout

Layout consumes renderer-neutral visible-body geometry rather than the complete transparent source
canvas when tighter bounds exist. Face/head geometry may carry a stronger avoidance weight than
ordinary body overlap. The one-bubble case chooses the best available side for the current geometry
instead of blindly choosing the first enumerated slot.

Before placement, the app resolves one explicit usable-bounds policy. The default follows the
monitor containing most of the visible character, with hysteresis at seams. Primary-monitor,
virtual-desktop, and custom rectangular bounds are deliberate user choices. Monitor indices are not
durable configuration. A topology change or character drag may select a new avatar monitor, but a
new bubble alone may not escape into another display.

A visible session keeps its slot while that slot remains viable. Adding or retiring another bubble
must not reshuffle every survivor, but preserving an obsolete slot may not cover the face or push a
bubble outside the active display's usable bounds. Bubble growth must not produce left/right
oscillation. Body, framing, display, or scale changes may reposition affected bubbles while
preserving stable session order where possible.

Identical geometry and session order produce identical placement. Missing body-bound information
degrades to a conservative rectangle. Multiple bubbles may compact, but bubble overlap, body
overlap, face/head avoidance, and usable display bounds remain explicit inputs rather than hidden
renderer policy.

The eventual compositor may use independently sized transparent windows for the character and
bubbles. That optimization must preserve the same pure layout inputs and stable slot identity.

## Dialogue behavior

Every bubble independently supports:

- `follow`: continuous typing with one-line viewport following;
- `paged`: timed complete-page replacement;
- `manual`: click-driven complete-page replacement.

Autoscroll timing is presentation-rate work, not session-discovery work. UTF-8 source offsets and
expression beat offsets remain private to that bubble.

## Shared-character arbitration

Only one character is rendered, so concurrent bubble tracks cannot all own its face in the same
frame. Ownership must be explicit and deterministic:

1. a newly revealed final output claims performance ownership;
2. a user click/advance on another bubble transfers ownership to that bubble;
3. only the owner may emit portrait expression and physical cues;
4. non-owning bubbles continue revealing text independently;
5. ownership remains until another explicit claim or the owner retires.

Iteration order through registry entries must never decide the visible face. This arbitration is a
required follow-up to the first multi-session implementation.

## Interaction

Hit testing resolves the clicked layout slot before advancing dialogue. Character dragging and 3D
rotation remain separate interactions. Transparent pixels pass through to the desktop; visible
bubble, shadow, and character pixels remain interactive.

## Acceptance criteria

- two sessions can type and scroll independently without replacing each other;
- a session retains title and slot across repeated messages;
- clicking the left bubble never advances the right bubble;
- one quiet bubble remains opaque for five seconds, fades continuously for three seconds, and then
  disappears without hiding or resetting another session;
- new activity during the fade restores full opacity immediately without changing the session slot;
- new output renews a hidden session and restores its bubble;
- subagent rollouts never appear as user bubbles;
- a slow discovery scan does not stall presentation or dialogue reveal;
- concurrent reveals choose the shared expression through explicit ownership, not array order;
- layout uses visible body bounds instead of the complete transparent source canvas when available;
- face/head avoidance outweighs ordinary body overlap where both cannot be avoided;
- the one-bubble case chooses the best available side for current geometry;
- bubbles remain inside the active display's usable bounds at every supported presentation scale;
- avatar-monitor bounds keep every bubble on the character's selected monitor even when another
  display offers an otherwise valid candidate;
- primary, virtual-desktop, and custom rectangular policies produce deterministic bounds, including
  negative desktop coordinates;
- body, framing, or scale changes preserve stable session ordering where possible;
- bubble growth cannot cause left/right oscillation;
- multiple bubbles may compact without covering the face merely to retain an obsolete slot;
- identical geometry produces deterministic placement;
- missing body geometry uses a conservative fallback rectangle;
- snapshot QA covers sprite, portrait, and 3D geometry separately;
- automated snapshots do not replace owner-controlled interactive evaluation of placement feel.
