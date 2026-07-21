# Presence contract specification

## Problem

Animation can make a character lively while still misrepresenting the agent doing the work. Eidolon
needs a small agent-neutral vocabulary that carries operational truth from configured session
sources through adapters into performance without coupling renderers to Codex, OpenCode, or future
vendor protocols.

## Goals

- represent the agent's real operational state before styling it;
- keep vendor protocol details outside session, persona, and body systems;
- allow semantic affect and persona behavior to style one truthful state in different ways;
- preserve session identity and ordering across reconnects and body changes;
- degrade unknown or missing activity conservatively;
- leave room for a small, explicit command surface without claiming unsupported control.

## Non-goals

- replacing the terminal or canonical agent runtime;
- mirroring every vendor-specific event;
- deriving operational state from message sentiment;
- making model inference part of the presentation thread;
- claiming that future commands or persistent persona ownership already exist.

## Ownership

Each configured **session source** represents one external runtime endpoint or connection and has a
stable source id. It uses one **agent adapter**, the reusable parser and transport implementation for
that runtime kind, to produce normalized events. The source owns connection state and source-local
correlation; the adapter owns protocol interpretation. Neither owns visible behavior.

The session registry owns `(source_id, session_id)` identity, current operational state, activity
time, and dialogue continuity. Adapter kind is metadata, not identity. A future persona layer may
style state but may not rewrite its operational meaning. The active body follows its deterministic
fallback mapping for unsupported intent. Body renderers own only drawing and local motion state.

Existing source symbols and directories still use `provider` in several places. That legacy name
currently conflates adapter kind with configured source identity; it is not a model provider or body
renderer.

## Operational vocabulary

The current normalized implementation directly represents source connection changes, session
metadata, turn start/completion, response deltas, and completed response snapshots. The broader
product contract should converge on:

```text
source.connected
source.disconnected
session.discovered
session.updated
session.retired
session.focused
turn.started
turn.completed
activity.listening
activity.thinking
activity.reading
activity.editing
activity.running_tool
activity.waiting
activity.blocked
approval.requested
approval.resolved
response.delta
response.completed
turn.interrupted
error.reported
```

Entries not supported by a particular adapter are absent, not inferred. The current implementation
does not yet expose this complete vocabulary.

Potential future commands are:

```text
turn.cancel
turn.pause
turn.resume
approval.accept
approval.reject
direction.submit
session.focus
```

These commands are design targets only. Passing an upstream control frame through the Codex relay
does not mean Eidolon owns or exposes the corresponding product action.

## Target data flow

```text
configured source: connection + source-local sequence
                         ↓
agent adapter: vendor parsing + normalization
                         ↓
normalized source / session event
                         ↓
session registry: identity + operational truth
                         ↓
persona/affect styling + delivery performance
                         ↓
body capability projection
                         ↓
body-local expression, pose, attention, and dialogue
```

Operational state changes should arrive independently from response text. Semantic classification
may begin or continue asynchronously, but it cannot delay or replace the operational transition.

## Invariants

- operational state is ground truth; semantic analysis may style the performance but may not
  contradict it;
- `approval.requested` directs attention toward the user even when affect changes whether that
  attention looks confident, apologetic, annoyed, or uncertain;
- a classifier cannot make the character appear idle while the agent is blocked for approval;
- unknown activity degrades to a neutral working state rather than inventing tool use or intent;
- one source's session identifier cannot collide with another source's identifier;
- a source-local monotonic sequence or revision rejects stale events and preserves accepted dialogue
  order; wall-clock timestamps remain diagnostic metadata only;
- source or adapter failure never freezes rendering or erases another source's sessions;
- changing bodies does not alter session or persona identity.

## Failure behavior

- unsupported vendor event: ignore it or map it to a documented conservative state and record a
  bounded diagnostic;
- malformed or stale event: reject it without mutating current session truth;
- source disconnect: retain known session state long enough for deterministic reconnect/retirement
  policy, while visibly avoiding false active work;
- semantic classifier unavailable: retain operational state and use deterministic styling;
- body lacks the requested expression: project to its nearest supported neutral working cue;
- command unsupported or ambiguously delivered: report unavailable or unresolved; never display
  success optimistically.

## Acceptance criteria

- equivalent Codex and OpenCode activity produces the same normalized operational meaning;
- thinking, tool execution, approval blockage, response streaming, completion, interruption, and
  failure remain distinguishable when their adapter supplies that evidence;
- affect changes styling without changing the underlying operational label;
- disconnecting one source cannot stall presentation or mutate another source's sessions;
- an unknown event cannot invent a detailed action;
- a body without semantic poses still communicates active versus waiting state through supported
  attention, motion, or dialogue cues;
- logs identify source id, adapter kind, normalized state, styling decision, and body projection
  without exposing private response text in release builds.

## Open decisions

- stable source-id allocation and the minimal source-local revision/correlation fields required for
  reconnectable sources;
- whether focus is user-owned, adapter-owned, or resolved through a separate attention policy;
- command authorization, confirmation, cancellation, and ambiguous-delivery semantics;
- how persistent persona state observes operational history without becoming canonical session
  storage;
- which operational states require dedicated body-package assets versus composable motion.
