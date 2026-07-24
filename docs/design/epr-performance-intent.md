# EPR Performance Intent contract

## Purpose

Performance Intent is the immutable, versioned ingress value between accepted session/semantic
truth and EPR behavior selection. It says what is true and what communicative pressure exists. It
does not prescribe joint curves, renderer states, or animation clips.

## Provenance

The eventual A2 adapter supplies opaque stable provenance for:

- source instance (`source_id`);
- session (`session_id`);
- optional turn, response, and message;
- optional semantic beat;
- optional operational transition or user interaction;
- an accepted source/session truth revision.

EPR treats identifiers as opaque equality values or stable handles. It does not allocate them,
derive them from adapter kind, parse their contents, or define reconnect/restart semantics.
Adapter kind is metadata and is never source identity.

Until A2 lands, synthetic fixtures use deterministic fixture handles. Any legacy adapter contains
the current `(provider, session_id)` mapping and marks it compatibility-only. No EPR core structure
has a `provider` field.

## Value

A first-slice intent contains:

- schema version;
- intent revision and predecessor revision;
- opaque provenance;
- logical observation tick;
- operational mode: absent, listening, thinking, responding, interrupted, completed, or errored;
- continuity and urgency;
- normalized affect dimensions;
- zero or more semantic/delivery anchors with stable ids, UTF-8 spans, stability, and kind;
- an explicit performance-subject lease when one shared body may observe several sessions.

Classifier-specific labels and portrait expression catalogue indexes are evidence-local details.
They do not cross this contract.

## Stability and revisions

Evidence is provisional, stable-prefix, or final. A later revision may extend provisional future
evidence, finalize it, or supersede uncommitted intent. It may not rewrite an observed anchor or a
completed behavior phase.

The runtime rejects:

- schema versions it does not support;
- non-monotonic revisions for the same provenance;
- stale predecessor chains;
- duplicate stable ids with incompatible content;
- non-finite affect values or invalid UTF-8 spans;
- an intent that claims a performance subject without an explicit lease.

Rejection is transactional: the published plan and control state do not change.

## A2 integration boundary

A2 owns:

- stable configured source identity;
- source instance versus adapter kind;
- `(source_id, session_id)` session identity;
- reconnect and restart continuity;
- authoritative turn, response, interruption, completion, and error truth;
- ordering, deduplication, and stale-revision rejection at the source/session boundary;
- transcript recovery.

The EPR adapter consumes immutable accepted snapshots/deltas and emits Performance Intent. It may
also accept body-neutral semantic beats and dialogue reveal anchors from their current owners. It
must not infer missing source truth from renderer state or transcript tailing.

## Frame-path rule

Intent construction and validation occur before publication. The control loop reads a bounded
completed intent/plan snapshot. It never performs language-scale planning or blocks waiting for
new evidence.

