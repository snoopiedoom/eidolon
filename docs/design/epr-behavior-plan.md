# EPR behavior-plan and temporal-dispatch contract

## Purpose

The behavior plan is the versioned explanation of what the body may do, why, and when. It owns
behavior lifecycle and temporal commitments. It does not own resource grants, body curves,
physical solving, or rendering.

## Plan generations

Each accepted intent revision produces either:

- no plan change;
- one complete new generation referencing its predecessor; or
- a typed rejection that leaves the published generation unchanged.

A generation has a stable id, source intent revision, bounded behavior-unit set, bounded temporal
network, and an explicit delta listing preserved, added, revised, and retired units. Publication is
atomic. Stale generations cannot mutate the current dispatcher, resources, or control state.

## Behavior unit

A unit contains:

- stable behavior id and kind;
- opaque intent provenance;
- creating plan generation;
- lifecycle state and terminal reason;
- semantic resources requested;
- priority, urgency, and interruption policy;
- temporal anchors and difference constraints;
- selected realizer kind after dispatch.

Lifecycle is monotonic:

```text
proposed -> scheduled -> committed -> executing -> retired(completed)
                    \            \-> retired(interrupted|failed)
                     \-> retired(revised|denied)
```

Observed anchors and completed phases are immutable facts. A revision may replace uncommitted
future behavior. An executing behavior follows its declared interruption policy and must settle or
transfer any resource it leaves displaced.

## Temporal network

The first slice uses a bounded incremental simple temporal network over signed integer logical
ticks. Constraints have the form:

```text
minimum <= tick(to) - tick(from) <= maximum
```

Anchors are typed as observed or controllable. Required behavior phases are preparation onset,
stroke onset, nucleus/peak, recovery onset, completion, interrupt, and settle.

Insertion validates the affected network before publication. An inconsistent insertion leaves the
previous generation untouched and emits a typed trace. The first slice does not require a general
STNU, Allen algebra, probabilistic timing, or wall-clock scheduling.

## Dispatcher

The dispatcher:

1. observes logical time and accepted observed anchors;
2. commits controllable phases inside the commitment horizon;
3. chooses only within the validated temporal window;
4. requests body resources before a behavior executes;
5. publishes lifecycle transitions and trace records;
6. never replays an observed or completed phase.

Tie breaking uses the explicit tuple:

```text
urgency, priority, committed-phase rank, earliest anchor, provenance class, behavior id
```

The tuple is total and documented. Storage or iteration order is not a tie breaker.

## Incremental streaming and interruption

A stable semantic beat may insert a future behavior while a response is still streaming. Later
stable beats extend the graph. Provisional evidence may propose behavior outside the commitment
horizon but cannot force early commitment.

On interruption:

- completed phases are preserved and retired normally;
- observed anchors remain immutable;
- uncommitted future phases are retired as revised;
- executing units receive an interrupt anchor and declared policy;
- cleanup is generated from the current canonical state;
- a new plan generation publishes only after its temporal and resource obligations validate.

## Capacity and failure

All first-slice collections have compile-time capacities. Capacity exhaustion rejects the incoming
revision without evicting committed behavior or partially publishing a graph. Trace capacity uses
a deterministic ring policy and exposes dropped-record count; trace loss never changes behavior.

