# Eidolon Product Brief

## Positioning

**Eidolon — the embodiment layer for your agent.**

Eidolon gives existing agents a native, persistent presence while real work happens.

The terminal remains the interface to the work.
Eidolon becomes the interface to the worker.

Eidolon is not primarily a novelty chat box, an isolated AI pet, or a replacement for dense work
interfaces. It makes the agent already doing real work visibly feel like one continuous persona.

## First audience

- coding-agent power users;
- local-model users;
- persona programmers;
- technically capable users already working through Codex, OpenCode, terminals, or custom local
  agents.

The same core product should later reach ordinary users through better packaging and defaults. That
must not weaken the underlying system or turn product direction into a list of imagined beginner
features.

## Product promise

One persistent persona visibly and truthfully inhabits the agents, models, sessions, and tools
acting on the user's behalf.

The critical word is **truthfully**. Animation creates liveliness; congruence with actual agent
state creates presence. Listening, thinking, reading, editing, running a tool, waiting, being
blocked, requesting approval, responding, interruption, completion, and failure must reflect real
session activity. Semantic analysis may style that performance, but may not contradict operational
ground truth.

## First-release product boundary

The first release observes and embodies existing agent sessions. It does not initially replace:

- the terminal;
- the agent runtime;
- model inference;
- dense code, logs, diffs, and long-form output;
- general-purpose tool orchestration.

The terminal remains authoritative for dense work. The first release provides a parallel channel
for presence, attention, operational state, short dialogue, expression, interruption, approval
requests, session identity, and presentation/session continuity. Durable relationship memory and
persona-owned conversational continuity belong to the later persistent-persona platform.

## Development sequence

1. Build the native agent-embodiment product.
2. Architect toward a complete persistent-persona platform.
3. Earn the right to become a protocol and ecosystem.

These are stages of maturity, not competing strategies.

## Long-term direction

Eidolon may eventually become the durable runtime where identity, memory, model providers, tools,
agents, bodies, and applications agree on interfaces. That is a destination, not a claim about the
current implementation.

The persona should eventually remain portable across models, model providers, sessions, tools,
bodies, and applications. Current Eidolon does not yet own persistent persona storage, memory,
speech, model routing, or general orchestration.

## Art and embodiment direction

**One art direction, many possible bodies.**

Art direction is the shared performance language: how a character occupies desktop space, shows
attention and operational state, moves, speaks through dialogue surfaces, responds to interaction,
and remains unobtrusive during real work. It is not a choice between 2D and 3D.

A body may be a static image, expression portrait set, sprite atlas, rigged 3D model, or future
implementation such as Live2D. The same abstract performance intent should degrade locally to each
body's capabilities. Ordinary users should eventually choose a character package whose intended
body is already selected; renderer overrides belong in advanced and authoring controls.

The current implementation still selects a global renderer and does not yet implement the complete
character-package abstraction. See the
[body-capabilities specification](design/body-capabilities.md).

## Competitive advantage

Eidolon's intended advantage is the combination of truthful agent-state embodiment, low-latency
presence, native desktop integration, persona portability, and direct connection to real work.
Only implemented portions should be presented as current capabilities.

## Non-goals for the first release

- replacing the terminal;
- implementing a complete agent platform;
- training a first-party general model;
- building a model-provider marketplace;
- reproducing every AIRI subsystem;
- creating a universal protocol before real product use proves its requirements.
