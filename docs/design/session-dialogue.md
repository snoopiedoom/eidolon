# Multi-session dialogue specification

## Problem

Several provider sessions may produce output concurrently, but one latest-message bubble loses
identity, scroll state, and context. Eidolon needs one bubble per active session around a shared
character without letting provider discovery leak into rendering or letting bubbles jump whenever
another session speaks.

## Goals

- preserve session identity and independent dialogue state;
- show multiple active outputs simultaneously;
- use real session titles with stable fallbacks;
- keep placement deterministic and visually stable;
- retire inactive bubbles on a short, predictable presentation timeout;
- route clicks to the bubble actually clicked;
- keep the one-session case simple.

## Ownership

`session_registry` owns identity, metadata, transcript progress, activity, dialogue, affect track,
visibility, and eviction. `bubble_layout` owns only geometry. `dialogue` owns reveal, scrolling,
pagination, drawing data, and hit testing for one text stream. `app` coordinates them but does not
maintain a parallel session collection.

## Discovery and identity

The pair `(provider, session id)` is the key. Discovery order, transcript filename, and title are not
keys. Legacy transcript discovery examines recent files asynchronously, ignores subagent rollouts,
and publishes completed snapshots to the presentation thread.

New final output updates only the matching registry entry. Reopening or appending a long-lived
rollout must not create a second bubble merely because its dated directory is old.

## Visibility policy

The first bounded implementation tracks eight sessions and presents four. Active/unread entries take
priority. Capacity overflow evicts the oldest inactive entry deterministically and logs it.

Each bubble has an independent five-second presentation lease. Text deltas, completed messages, and
turn activity renew the matching session's lease. Five seconds without provider activity hides that
bubble even when its dialogue has not finished revealing; the session and its dialogue state remain
tracked and the next output makes it visible again. The timeout is policy and should become
runtime-configurable.

## Layout

The initial anchors are upper-left, upper-right, mid-left, and mid-right around the shared character.
A visible session keeps its slot while possible. Placement scores portrait overlap, bubble overlap,
and window bounds; adding or retiring another bubble must not reshuffle every survivor.

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
- one quiet bubble disappears after five seconds without hiding or resetting another session;
- new output renews a hidden session and restores its bubble;
- subagent rollouts never appear as user bubbles;
- a slow discovery scan does not stall presentation or dialogue reveal;
- concurrent reveals choose the shared expression through explicit ownership, not array order.
