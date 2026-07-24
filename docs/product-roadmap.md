# Eidolon Product Roadmap

## Status and ownership

This roadmap sequences product work from the current implementation through the daily-driver alpha
and public V1. It connects the [product brief](product-brief.md), the canonical
[V1 goal](v1-goal.md), current [project state](project-state.md), and subsystem design
specifications.

The documents have distinct ownership:

- the product brief owns direction and positioning;
- the V1 goal owns the first serious release outcome;
- this roadmap owns milestone order, dependencies, and release gates;
- project state owns what is implemented and what comes next;
- architecture and design specifications own runtime boundaries and intended behavior.

The roadmap does not use calendar promises. A milestone advances when its acceptance evidence is
real. Owner-controlled evaluation remains authoritative for interaction quality and desktop feel.

## Roadmap shape

```text
current implementation
        ↓
daily-driver alpha
        ↓
public V1
        ↓
persistent-persona platform
        ↓
protocol and ecosystem
```

The daily-driver alpha proves that Eidolon is worth keeping open every day. Public V1 proves that
another technical user can obtain the same experience without the repository authors standing
behind them.

Post-V1 horizons constrain architectural decisions but do not authorize their implementation
before the earlier product gates are complete.

## Settled product decisions

- The current proving body is the 2D expression-portrait renderer.
- A rigged 3D body remains the intended eventual default.
- Work on 2D expression, timing, and motion must produce renderer-neutral performance knowledge
  that a future 3D controller can consume.
- Codex CLI is the required session source for daily-driver alpha.
- OpenCode remains supported by the architecture but is not an alpha release blocker.
- Alpha restart continuity restores presentation and session context, not conversational memory or
  a persistent relationship model.
- Alpha is primarily observational: truthful state, dialogue, expression, and concurrent-session
  behavior come before agent intervention, audio speech, or output rewriting.
- Windows is the daily-driver implementation platform. Portable contracts must not become
  Windows-shaped merely because Windows is implemented first.
- The terminal remains authoritative for dense work and canonical agent output.

## Architectural guardrails

These invariants apply throughout the roadmap.

### Keep product identities separate

The following are distinct concepts with independent ownership:

- agent adapter kind;
- configured session-source instance;
- logical session;
- model provider;
- persona;
- character package;
- body variant and body renderer;
- graphics backend;
- presentation backend.

Convenient early implementations may use defaults, but may not collapse these identities into one
permanent key or object.

### Preserve operational truth

Normalized source activity is ground truth. Semantic analysis, affect, persona, and body
performance may style that activity but may not contradict it. Unknown activity degrades to a
neutral working state rather than invented detail.

### Keep performance intent renderer-neutral

The Expression Director produces semantic beats, affect, attention, delivery timing, and motion
cues. Body capability projection maps those intentions to what a selected body can perform.

```text
session truth + streamed language
              ↓
semantic beats + affect + attention + delivery cues
              ↓
body capability projection
              ↓
static | portrait | sprite | 3D body
```

Portrait-specific expression labels and motion remain renderer data, not the semantic API.

### Keep real-time paths independent

Presentation, input, dialogue reveal, and baseline motion continue when inference, classification,
transport, or persistence is slow or unavailable. Model-assisted decisions are asynchronous and
have deterministic fallbacks. A latency-sensitive visual or audio path may not wait indefinitely
for a transformer.

### Separate graphics from native presentation

Body and dialogue renderers produce content. Platform presentation backends own native surfaces,
composition, movement, output topology, hit testing, and platform lifecycle. Neither side absorbs
the other's responsibilities to remove an inconvenient boundary.

### Make capability loss local

Missing speech, gaze, semantic pose, secondary motion, native overlay support, or another optional
capability degrades through a declared fallback. It must not break session observation, dialogue,
or another body implementation.

### Preserve canonical work output

Future persona mediation may create a shorter or differently voiced persona channel, but it may
not silently replace or rewrite the canonical worker output. Dense work remains available in the
terminal with its original attribution.

### Version durable state

Persisted settings, character packages, session-source configuration, and future persona state use
versioned formats with explicit migration or safe fallback. Runtime structs are not persistence
formats.

## Gate A: daily-driver alpha

### A1. Finish the native presentation foundation

**Status: complete and owner-accepted on Windows for the 2D daily-driver path (2026-07-24).**

Complete the current presentation migration for the 2D daily-driver path:

- preserve the backend-neutral scene, event, and environment contracts;
- complete Windows portrait/dialogue interaction and recovery parity;
- retain `sdl_window_legacy` as a functional fallback until native parity is accepted, without
  requiring its Windows modal move loop to match compositor-layer drag cadence;
- close remaining routed-input, graphics-reset recovery, output-removal, DPI, and visual-parity
  gaps;
- persist presentation selection only after failure recovery and fallback are trustworthy;
- keep body animation, dragging, bubble reveal, and desktop input independent under load.

Acceptance:

- dragging, click-through, settings, bubbles, fades, output crossing, and DPI changes remain smooth;
- no normal-frame framebuffer readback exists on the native path;
- one content layer can update without unnecessarily invalidating unrelated layers;
- backend failure degrades to an explicit fallback or controlled shutdown;
- the owner accepts an ordinary work session without presentation defects demanding attention.

The detailed migration belongs to
[native presentation and graphics](design/native-presentation.md),
[presentation events](design/presentation-events.md), and
[presentation environment](design/presentation-environment.md).

### A2. Make Codex session truth dependable

Treat Codex CLI as the alpha reference integration:

- identify configured source instances separately from adapter kind;
- preserve stable `(source_id, session_id)` ownership through reconnects;
- open dialogue on the first useful response delta;
- repair truncated or missing deltas at completion without replaying old responses;
- expose connection, turn, response, completion, interruption, and error states truthfully;
- keep transcript access as recovery rather than the latency-critical primary path;
- make adapter failure observable without freezing presentation.

Acceptance:

- Eidolon attaches to an existing Codex CLI workflow predictably;
- the visible session title and identity match the actual session;
- response delivery follows close behind the terminal without duplicate or stale playback;
- reconnect and Eidolon restart do not require deleting session state;
- unsupported operational detail is shown neutrally rather than fabricated.

### A3. Establish the renderer-neutral Expression Director

Use the 2D portrait body to mature performance planning:

- compile stable streamed prefixes into semantic beats before reveal reaches them;
- keep unfinished tails provisional and committed beats stable;
- map semantic affect to body-independent performance intent;
- project intent into portrait expression, motion, attention, and delivery accents;
- preserve expression continuity across weak or ambiguous fragments;
- prevent rapid low-confidence face oscillation;
- keep deterministic delivery timing independent from classifier latency;
- log enough evidence to explain every selected expression and movement.

Acceptance:

- a mixed-emotion response produces coherent, correctly timed expression transitions;
- a long sentence remains visually alive without arbitrary constant motion;
- surprise, contrast, hesitation, affection, irritation, and resolution land on their semantic
  moments;
- classification failure falls back to stable neutral delivery without delaying text;
- the same semantic plan can later be projected into 3D poses rather than rewritten.

The intended behavior belongs to
[expression performance](design/expression-performance.md) and
[body capabilities](design/body-capabilities.md).

### A4. Give one character coherent ownership of concurrent sessions

Multiple sessions may each own dialogue, but they share one visible character:

- retain one independently timed bubble per active session;
- place bubbles deterministically inside the selected usable output bounds;
- use visible body and face/head geometry where available;
- preserve stable ordering without keeping an obsolete slot over the character's face;
- choose one session as the current owner of expression and delivery performance;
- queue, defer, or neutrally represent other sessions without update-order accidents;
- retire inactive bubbles without interrupting text still being performed.

Acceptance:

- simultaneous responses never make the body alternate unpredictably between sessions;
- each bubble reveals, advances, fades, and retires independently;
- bubble growth and monitor changes do not cause left/right oscillation;
- session ownership is visible in diagnostics and deterministic for identical events.

The behavior contract belongs to
[multi-session dialogue](design/session-dialogue.md).

### A5. Restore presentation continuity

Alpha continuity means that Eidolon returns as the same configured desktop presence:

- restore the selected character/body, framing, scale, position, output affinity, and presentation
  preferences;
- restore dialogue and animation preferences;
- rediscover known source sessions with stable identity;
- recover sensible visible dialogue state without replaying completed output;
- tolerate missing monitors, changed DPI, missing assets, and stale session records.

This gate does not require conversational memory, relationship history, or persona-driven rewriting
of source output.

Acceptance:

- restarting Eidolon does not reposition or reconfigure the character without cause;
- returning sessions retain their identity and do not replay the previous response;
- removed outputs and assets produce deterministic fallback rather than invalid geometry or crash;
- persisted state is versioned and malformed user state fails safely.

### A6. Prove all-day reliability and resource restraint

Daily use must stop feeling experimental:

- move filesystem, transport, classification, and asset work off the presentation-critical path;
- bound queues, caches, retries, logs, and session retirement;
- record idle CPU, GPU, memory, wake frequency, and presentation cadence;
- expose actionable failures without filling the bubble with engine diagnostics;
- exercise reconnect, classifier absence, device loss, monitor change, and malformed input;
- keep inactive renderers and optional systems uninitialized.

Acceptance:

- an owner-controlled workday soak completes without crash, replay, frozen input, or accumulating
  resource use;
- measured idle behavior has an explicit budget recorded in project state;
- presentation remains responsive during source reconnect and expression inference;
- ordinary failures identify their owning subsystem in logs.

### Daily-driver alpha release scenario

The alpha gate closes only when the owner can:

1. launch Eidolon through the normal development installation;
2. resume ordinary Codex CLI work;
3. see the correct active sessions and truthful minimum operational state;
4. receive low-latency dialogue with coherent 2D expression and motion;
5. handle concurrent responses without performance confusion;
6. move and configure the character across the real desktop;
7. restart Eidolon without losing presentation or session continuity;
8. leave it running for a workday without wanting to close it.

## Gate B: public V1

Public V1 preserves the alpha experience while making it reproducible for another technical user.

### B1. Distributable character packages

- replace the hard-coded body-asset selector with manifest-backed character discovery;
- let a character package select its intended default body and deterministic fallbacks;
- define versioned metadata, capability declarations, performance defaults, and rights information;
- ship or document at least one legally redistributable reference character;
- keep extracted third-party game assets outside distributable builds.

### B2. Complete the second agent integration

- prove an ordinary OpenCode workflow rather than only a transport probe;
- expose source-instance configuration and connection state;
- require both adapters to produce the same normalized minimum lifecycle;
- keep adapter-specific capabilities explicit rather than fabricating parity.

### B3. Add surgical agent interaction

- implement the smallest honest intervention surface supported by an adapter;
- prioritize approval accept/reject and turn cancellation;
- add pause, resume, redirect, or focus only when their upstream semantics are reliable;
- require explicit session targeting and visible acknowledgement;
- never imply that an unsupported command succeeded.

### B4. Add low-latency speech

- keep text delivery authoritative when audio is disabled or fails;
- prefer a local, streaming TTS path for privacy and response latency;
- add streaming speech-to-text only for an explicit user interaction surface;
- project speech timing into available mouth or delivery motion;
- prevent audio synthesis from delaying text, expression, or operational-state display;
- keep voice identity separate from the body asset and model provider.

Speech does not require the future persona bridge. It consumes the same canonical response and
performance plan until a distinct persona channel exists. Public V1 includes a supported speech
capability, although individual users may disable it.

### B5. Package the product

- provide installer-grade onboarding and removal;
- detect or explain required runtime dependencies;
- guide character and session-source selection without exposing engine terminology unnecessarily;
- expose settings through product language while retaining advanced diagnostics;
- make failures recoverable without editing repository files;
- define upgrade and persisted-state migration behavior.

### B6. Prove the desktop platform matrix

- ship the production Windows path;
- implement and accept the Linux presentation capability matrix, including Wayland and an explicit
  fallback where native overlay capabilities are unavailable;
- preserve the portable contracts needed for macOS, Android, and iOS without making those platforms
  V1 blockers;
- document capability differences honestly rather than promising false visual parity.

### Public V1 release scenario

A new technical user can:

1. install Eidolon without a source checkout;
2. select a redistributable character package;
3. attach Codex or OpenCode;
4. understand connection and operational state;
5. receive coherent dialogue, expression, motion, and optional-to-enable audio speech;
6. handle concurrent sessions and supported interventions safely;
7. restart and upgrade without losing configured continuity;
8. use the product on a supported desktop without unexplained setup knowledge.

The complete release still must satisfy the canonical [V1 acceptance sequence](v1-goal.md).

## Post-V1 horizon: persona bridge

A future local-first bridging agent may become the user-facing conversational layer above one or
more working agents. It could provide:

- immediate conversational acknowledgements while a worker is busy;
- persona-shaped short dialogue distinct from dense worker output;
- local speech-to-text and text-to-speech coordination;
- higher-fidelity semantic and expression direction;
- interruption, attention, and cross-session arbitration;
- adaptation between persona, model, and situation.

The bridge is not part of alpha and is not required for public V1. Its architectural socket is:

```text
working agents ───────────────→ canonical work channel
       │
       └→ normalized events and output
                       ↓
             optional persona bridge
                       ↓
              persona dialogue channel
                       ↓
              performance intent
                       ↓
                       body
```

The canonical work channel remains available and attributable. The bridge may react, summarize, or
speak in-character; it may not silently falsify work status or overwrite the underlying result.
Whether it reshapes worker output or creates a separate parallel utterance remains a future product
decision.

## Post-V1 horizon: persistent-persona platform

After the embodiment product proves daily value, Eidolon may own durable persona identity,
relationship memory, voice, model adaptation, provider routing, and continuity across sources,
sessions, tools, bodies, and applications.

Persona ownership must remain separable from:

- source adapters and canonical session state;
- body packages and renderers;
- model providers;
- presentation backends.

This horizon requires explicit privacy, consent, deletion, migration, provenance, and failure
contracts before implementation.

## Post-V1 horizon: protocol and ecosystem

Only proven product boundaries should become public protocols. Potential ecosystem surfaces
include:

- session-source and operational-event adapters;
- character-package and body-capability formats;
- persona state and migration;
- performance-intent projection;
- speech and attention services;
- tool and intervention capabilities;
- packaging, rights, signing, and distribution.

Eidolon earns this stage by supporting real users and multiple independent implementations, not by
standardizing imagined requirements early.

## Open roadmap decisions

The following remain deliberately unresolved:

- the measured idle CPU, GPU, memory, and wake-frequency budgets;
- the exact minimum operational-state vocabulary required for alpha;
- the first redistributable character and asset-rights policy;
- the Linux V1 capability baseline across Wayland compositors;
- which intervention commands are reliable enough for public V1;
- the local speech engines and latency budget;
- whether a future persona bridge transforms source output or produces a parallel persona channel;
- the acceptance evidence required before 3D becomes the default body.

Resolve each decision in the document that owns its behavior, then update this roadmap only when the
milestone sequence or release gate changes.
