# Expression performance specification

## Problem

A single classifier result for a complete mixed-emotion message produces an average that represents
none of its turns. Classifying fixed-size text chunks is equally wrong: wrapping and byte counts do
not correspond to meaning. The character must change expression and movement at the semantic moment
the dialogue reaches it, without frantic face roulette.

## Goals

- prepare expression and acting metadata before visible text reaches each semantic beat;
- preserve mixed emotional turns within one response;
- make timing independent from line wrapping, font metrics, and autoscroll;
- prefer coherent facial continuity when classifications are ambiguous;
- let clear emotional reversals remain immediate and legible;
- degrade deterministically when local inference is unavailable;
- expose enough evidence in logs to tune mappings rather than guess.

## Non-goals

- generating or labeling portrait source art;
- inferring a persistent agent persona;
- lip synchronization or phoneme animation;
- making a renderer depend on GoEmotions label names;
- crossfading between expression images.

## Model

A **track** represents one complete agent message for one session. A track contains ordered
**semantic beats** with original UTF-8 source spans. Every beat stores:

- boundary reason and source offsets;
- top classifier labels and probability distribution;
- continuous affect: valence, arousal, dominance, certainty, warmth, surprise;
- raw face, runner-up, margin, stabilized face, and continuity decision;
- physical performance cue, reason, and intensity;
- request sequence, submission time, classification time, and readiness.

A separate **delivery track** represents how the same text is spoken. It contains deterministic
source-offset marks for phrases, cadence, contrast, hesitation, punctuation accents, questions,
exclamations, and landings. Delivery does not create faces and never waits for inference.

## EPR boundary

Expression and delivery tracks remain evidence owners. For EPR they publish body-neutral stable
beat and reveal anchors through the [Performance Intent contract](epr-performance-intent.md).
Classifier labels, portrait face indexes, request queues, and reveal ownership do not cross into
EPR. EPR owns later behavior selection, temporal commitment, body-resource grants, interruption,
and canonical body control.

The existing portrait director remains supported while this boundary lands. Shared-character
ownership is an explicit performance-subject lease; registry iteration order may not choose which
session moves the body.

## Compilation

The director segments source text at explicit lines, sentence punctuation, contrast pivots, and a
bounded maximum beat length. Tiny discourse fragments (`wait`, `but`, `oh`, `ugh`, `fine`, and
similar) attach to the thought they modify. A standalone heart attaches to the preceding beat.

A live message owns one persistent track. Completed sentences and contrast clauses extend it as
stable prefixes; an unfinished tail and a trailing modifier fragment remain provisional. Extension
never resubmits or reactivates compatible committed beats. A completion event classifies the final
tail. If an agent adapter repairs earlier source text, incompatible uncommitted work is rebuilt and
stale sequence results are rejected.

Segmentation must retain original byte spans. Rendering may wrap or scroll the text afterward, but
it cannot rewrite performance timing.

Every beat is submitted to the persistent local worker through a bounded FIFO request queue. A
bounded per-frame submission burst applies backpressure without blocking presentation or treating a
full queue as worker failure. Results carry sequence IDs so a delayed result cannot attach to a
replaced track. Snapshot dialogue waits at its first beat; streaming dialogue continues through its
ready prefix and waits only at the first unclassified beat boundary. A deadline scaled by beat count
creates deterministic fallback beats instead of freezing reveal.

The delivery compiler runs synchronously when dialogue text is set or extended because it is bounded,
allocation-free, and independent from model inference. Incremental recompilation seeks past the
current reveal offset. Large reveal jumps emit at most the strongest skipped mark so page advance,
manual reveal, and stream repair cannot create a gesture burst.

## Affect and face selection

GoEmotions is an evidence source, not the renderer protocol. Its multi-label distribution projects
into the six continuous affect axes. Lifecycle state supplies the same axes when inference is absent.

Face selection compares affect to renderer-independent expression targets. The director records the
winner, runner-up, and distance margin. When the new winner has only a small advantage over the
previous face and the affect delta is also small, the previous face is retained. Clear reversals are
not delayed merely to satisfy a timer.

Lifecycle changes immediately synchronize affect target and expression intent. A stale intent must
not undo a newly selected lifecycle face in the same frame.

## Reveal scheduling

The dialogue reveal cursor reports an original source offset. Before the first glyph of a beat is
drawn, the director emits one event containing the stabilized face and physical cue. Exactly one
event is emitted per beat activation.

Ready-prefix activation does not require the message to be complete. Appending later beats preserves
the active index, so extending a live track cannot replay an earlier face. If inference briefly
falls behind, reveal gates at the pending beat's first glyph and resumes from the presentation clock
when that batch becomes ready.

Expression art swaps atomically on the next rendered frame. The old texture is neither blended nor
retained. Physical movement remains independent: surprise may jump, warmth may lean/lift, and
negative valence may recoil even though the art itself hard-cuts.

Delivery marks drive a bounded spring across horizontal offset, vertical offset, scale, and angle.
The spring turns discrete textual marks into overlapping motion with inertia and recovery. It is
clamped in both displacement and velocity, advances in bounded substeps after a hitch, and composes
under the larger semantic cue so ordinary speech cannot overpower an emotional turn.

The first beat has no relative motion cue unless its text contains an explicit semantic hint. It
must not compare neutral opening text against a positive lifecycle prior and invent a recoil.

## Diagnostics

Debug logs must make a bad performance reconstructable. Each planned beat records source span,
escaped preview, boundary, inference latency, top labels, affect axes, raw/final face, hold decision,
runner-up margin, cue, reason, and intensity. Activation records session owner, reveal offset,
elapsed time, and actual portrait change. Release logs omit message previews.

## Failure behavior

- worker missing or crashed: lifecycle-derived fallback;
- request deadline exceeded: complete deterministic fallback track;
- classifier queue saturated: retain the track and continue bounded submission on later frames;
- stale sequence: discard;
- unknown portrait label: map through the configured default, never index outside the manifest;
- replacement message: invalidate unfinished work owned by the old track.

## Acceptance criteria

- a response containing curiosity, fear, anger, relief, embarrassment, and affection reaches those
  turns in source order;
- `wait. something moved` and `oh! there she is` do not create 100 ms neutral intermediate faces;
- wrapping the same text at different widths produces identical beat activation offsets;
- a streamed response classifies closed sentences before message completion;
- appending a streamed beat preserves earlier results and cannot replay its active expression;
- reveal can cross ready text but cannot cross an unclassified semantic boundary;
- no expression change draws two portrait textures in one frame;
- a long single-expression sentence still receives several restrained delivery motions;
- a large reveal jump cannot replay every skipped delivery mark in one frame;
- a live text append cannot replay marks before the current reveal cursor;
- dialogue continues when the native worker is absent;
- logs distinguish classifier choice from stabilization and renderer mapping.

## Open decisions

- per-model expression annotation and automatic portrait-label assistance;
- character-specific affect targets without coupling them to the classifier;
- product policy for assigning the explicit EPR performance-subject lease when several session
  bubbles reveal at once.
