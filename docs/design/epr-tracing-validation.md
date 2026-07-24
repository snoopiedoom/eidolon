# EPR deterministic tracing and validation

## Purpose

Tracing makes every visible EPR decision causally reconstructable without asking the owner to read
ordinary logs. Validation uses trace and canonical snapshots as structural oracles; image capture
is render evidence, not the only oracle.

## Trace record

Each normalized record contains:

- schema version and record sequence;
- logical tick;
- current intent revision and plan generation;
- event kind;
- stable behavior/program/resource ids when applicable;
- opaque source/session/message/beat/interaction provenance when applicable;
- typed reason and decision inputs;
- deterministic canonical-control hash when output changes.

Records are semantic decisions, not per-frame debug spam. Required kinds include intent accepted or
rejected, plan published or rejected, behavior transition, anchor observed/committed, resource
grant/deny/preempt/transfer/release, realizer selected/fallback/failed, solve committed/rejected,
capability degraded, projection committed/rejected, and control published.

## Normalization

Golden trace excludes addresses, wall-clock timestamps, platform handles, floating locale formats,
and nondeterministic log prefixes. Fixed-width integer values and stable enum names are preferred.
Floating control values are canonicalized before hashing and snapshot comparison.

A bounded ring exposes dropped-record count. Trace overflow does not affect plan or control
decisions.

`make epr-trace` runs the fixed first-slice clock and emits normalized JSONL plus a summary
containing record count, eviction count, trace hash, final canonical-control hash, revision, and
plan generation. Re-running identical source produces identical bytes. Solve/control/projection
records are semantic checkpoints rather than one record per 20 ms sample.

## Automated evidence

`make check` must reach tests for:

- identical evidence/seed/ticks producing identical trace and canonical hashes;
- permutation-independent behavior and resource decisions;
- transactional incremental temporal insertion;
- monotonic phases and no replay after interruption;
- no reactivation of an interrupted semantic beat on a later plan revision;
- explicit grants for every non-neutral control contribution;
- stale intent/plan rejection;
- whole-state rollback on injected realizer/solve/projection failure;
- local optional-capability degradation;
- required-body failure isolation from portrait and session observation;
- static absence of SDL, D3D11, DirectComposition, Win32, scene, and presentation ownership in EPR;
- causal reconstruction from trace alone;
- bounded frame/control work;
- monotonic model consumption through the SDL 3D path while portrait remains default.

Fixtures use a fixed logical clock. Ordinary structural correctness requires no manual log
inspection.

## First-slice causal chain

The deterministic scenario records:

```text
idle
-> performance lease + listening intent
-> thinking intent
-> streamed response
-> stable contrast beat
-> restrained right-arm gesture grant and synchronized peak
-> interruption revision
-> completed peak preserved, future recovery retired
-> right-arm transfer to current-state settle
-> attentive/guarded stable presence
```

Tests must answer which evidence selected the gesture, which constraint scheduled its peak, why the
right arm was granted, what the interruption preserved/retired, where cleanup began, and why the
final control was accepted.

## Owner evidence

The owner judges only whether the rendered sequence reads correctly and feels alive, plus artistic
tuning and body suitability. That judgment does not replace deterministic structural checks.
