# EPR semantic motion catalog

## Purpose

The semantic motion catalog binds a Realization Program's stable generator identifier to one
body-independent normalized humanoid motion source. It is the seam between EPR scheduling and
motion assets or procedural generators. Programs never name files, VRMA clips, model nodes, or
renderer objects.

## Catalog and source contract

The runtime catalog is versioned, fixed-capacity, and allocation-free. Each active generator may
appear at most once. A catalog may be partial: resolving an absent generator returns the typed
`missing_generator` result instead of silently selecting another motion.

Each entry borrows an immutable source descriptor containing:

- a stable nonzero provenance identity;
- a sampler callback and caller-owned context whose lifetime exceeds the catalog;
- the maximum humanoid rotations and hips translation the source may emit;
- finite duration and loop policy.

The callback must emit a canonical normalized humanoid pose at a caller-selected source-local time.
Loading files, owning clip memory, and choosing phase time remain outside the catalog. In
particular, a raw `EidolonVrmaClip` sample is not directly catalog-ready because it still contains
source-authored local rotations and hips displacement.

The catalog contains no presentation, renderer, model-node, session, or body-package state. A
callback pointer or context address is never a semantic identity, tie-break input, or trace value.

## VRMA source adapter

`vrma_motion_source` is the supported bridge from one borrowed immutable `EidolonVrmaClip` to a
catalog source descriptor. It validates the complete clip contract before publication, derives
maximum ownership from the tracks actually present rather than the mapped rest skeleton, retains a
caller-supplied stable provenance identity, and samples transactionally.

Each tracked authored local rotation `L` is converted through its authored local rest `Rl` and
authored world rest `Rw` into canonical normalized space as
`N = Rw * inverse(Rl) * L * inverse(Rw)`. Hips translation is represented as
`(T - Trest) / authored_rest_hips_height`. The inverse conversion used by destination retargeting
shares the same renderer-neutral `humanoid_rest` implementation, so importing and projecting cannot
silently diverge. Invalid metadata, mappings, ownership, track shapes, samples, or rest frames leave
the caller's previous source or pose untouched.

## Resolution and ownership

Resolution validates both the catalog and the complete versioned generator reference, then finds
the unique semantic entry. It intersects the program's requested humanoid mask and hips ownership
with the source's declared maximum ownership. A zero intersection is the typed `no_channels`
result. Procedural `none` references return `not_required` and never acquire a source.

Sampling performs a second intersection against the ownership of the pose actually returned by the
source. Channels that were requested but absent from that sample remain owned by the lower layer;
channels emitted by the source but not requested by the program are discarded. Unowned result
storage is canonicalized to identity rotations and zero translation so hashes cannot depend on
irrelevant callback bytes.

Registration, resolution, and sampling are transactional. Invalid catalogs, references, sources,
bindings, callback failures, invalid poses, and empty actual ownership leave the caller's previous
catalog, binding, or sample untouched. Successful samples retain generator, takeover, resource,
source identity, source time/duration/loop, blend, intensity, and playback-rate evidence.

## Timing boundary

The catalog still samples a source time supplied by its caller; it never reads wall clock, renderer
cadence, or pointer identity. `motion_execution` now owns the deterministic bridge from immutable
program anchors and the current fixed logical tick to that source time. Source seconds are elapsed
logical milliseconds from the program origin multiplied by the validated playback rate. Normalized
time is clamped for one-shot sources and wrapped for loops, matching catalog sampling exactly.

Idle is steady from logical tick zero. Posture enters from onset with its bounded transition
duration and then holds. Gesture enters from preparation to onset, holds through recovery, and exits
by completion. Settle owns procedural timing from interrupt to settle but has no catalog source or
normalized-pose contribution; its projection-side continuity weight decays from full at interrupt
to zero at settle. Motion-source entrances and exits use the same deterministic quintic
minimum-jerk curve. Missing, extra,
or non-monotonic phase anchors reject instead of selecting a guessed timing shape.

The executor narrows each program to its live resource grants, resolves and samples the catalog,
then orders contributions as base, cooperative, additive, and override with program-id tie-breaking.
One complete normalized frame is composed transactionally. Additive layers scale their normalized
delta by intensity before quaternion/hips composition; absolute layers retain distinct intensity
and blend weights.

## Current implementation state

`make epr-motion-catalog-check` proves unique fixed-capacity registration, requested/declared/actual
channel intersection, hips ownership, deterministic repeated samples, explicit procedural and
missing-generator results, provenance retention, and transactional rollback.
`make vrma-motion-source-check` proves source-rest invariance across differently authored rigs,
rest-to-identity conversion, hips-height normalization, actual-track ownership, looping, semantic
resource narrowing, malformed-clip rejection, and transactional rollback.
`make epr-motion-execution-check` proves exact phase shapes, fixed-tick source time, loop/clamp
evidence, minimum-jerk entrances/exits, semantic program validation, partial-grant narrowing,
deterministic layer order, real additive residual composition, local typed failure, and whole-frame
rollback.

`make semantic-motion-pack-check` proves atomic multi-binding publication, stable-context rebasing,
ownership transfer, duplicate and missing-file rejection, invalid-second-clip rollback, and catalog
preservation. The [semantic motion-pack contract](epr-motion-pack.md) owns this host-side boundary.

The model embeds one noncopyable semantic motion pack and lends its validated catalog to each EPR
runtime. That pack validates every requested file/source before atomically publishing immutable
bindings. When the pinned, hash-verified MIT idle fixture is present, it registers `idle.neutral`
with source identity `0x42eec1c51cf3978f`. An absent behavior plan can therefore publish a
normalized idle frame. Active plans publish only when every granted non-procedural generator
resolves; a missing posture or gesture clears any stale normalized frame and falls back locally to
the accepted canonical controller.

Projection commits `imported base -> normalized frame -> optional calibration residuals -> explicit
procedural owners` as one scratch-pose transaction and retains the accepted normalized frame across
later imported-base playback updates. Live head/eye gaze and expression grants now publish explicit
procedural ownership and compose above normalized posture/gesture. Right-arm settle is also
procedural: it resolves without a catalog source, publishes a behavior-token continuity envelope,
and lets projection capture the exact outgoing model-local arm pose once. Revised intent carrying
the same token cannot rebase that capture. The transaction shortest-arc blends from the capture to
the normalized pose, then applies an independent weighted task-space IK refinement; rejected frames
publish neither rig nor continuity state. When a normalized frame exists, legacy canonical posture,
combined head control, and right-arm anchors cannot leak into it; the full canonical controller
remains the complete-frame fallback. Dynamic target ingress is connected independently through its
versioned producer/plan/behavior/claim contract. The remaining R4 work is to visibly review, slice,
and bind the provenance-safe posture/gesture vocabulary.
