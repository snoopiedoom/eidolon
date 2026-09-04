# EPR semantic motion-pack ownership

## Purpose

The semantic motion pack owns the normalized animation clips that back EPR's renderer-neutral
motion catalog. It is the host-side lifetime and publication boundary between files on disk and the
catalog's borrowed immutable source callbacks. It does not choose behavior, timing, resource
grants, model nodes, or presentation state.

This layer exists outside `src/epr`: file loading and clip allocation are host concerns, while the
bounded EPR core receives only a validated `EidolonEprMotionCatalog`.

## Versioned binding

Each asset request carries:

- a versioned semantic binding;
- one non-`none` generator identifier;
- one stable nonzero source identity derived from reviewed provenance;
- an explicit loop policy;
- one local VRMA path.

The filename and memory address are never semantic identity. The pack refuses duplicate generators,
already-bound generators, zero identities, invalid clips, aliased clip ownership, and catalogs that
do not point back into their owning pack.

## Atomic publication

Batch loading is all-or-nothing:

1. validate the existing pack and every binding shape;
2. load every file into temporary owned clips;
3. validate every clip as a normalized VRMA source and preflight every catalog insertion;
4. move all clips into their stable generator slots;
5. rebuild source descriptors so borrowed contexts point at the final pack storage;
6. publish the complete catalog only after the resulting pack validates.

Any failure before publication destroys only temporary file-loaded clips. The prepared-clip API
instead leaves every caller-owned clip untouched. A defensive failure during final rebasing moves
all clips back to the caller and restores the original catalog. A partial live semantic vocabulary
is never exposed by one batch request.

Because catalog entries borrow addresses inside the owner, a live `EidolonSemanticMotionPack` must
not be copied. The model embeds one pack, lends a copy of its catalog to EPR, and destroys the pack
after consumers stop. The legacy single-asset model call is a one-element wrapper over the same
transaction.

`make semantic-motion-pack-check` covers empty-pack validity, multi-binding commit, stable-context
rebasing, sampling after ownership transfer, invalid-second-clip rollback, duplicate rejection,
missing-file rollback, catalog preservation, and destruction.

## Provenance and candidate review

A structurally valid capture is not automatically a semantic generator. Candidate acquisition,
clip selection, semantic labeling, and runtime binding are separate decisions.

`make vrma-semantic-candidate` reproducibly fetches CMU trial `18_08`, described by the pinned CMU
index as a two-person conversation take in which subject A explains with hand gestures. It fetches
the matching usage-rights text, verifies both source hashes, deterministically converts the complete
17.375-second take, verifies the generated VRMA hash, and runs structural/sampling preflight. All
downloads and generated motion stay in ignored `build/fixtures` storage.

The complete take is review material only. It is not registered as `posture.responding`,
`gesture.contrast.right`, or any other generator. Owner-visible review must first identify bounded
frame ranges that read correctly on the supported reference body. Each accepted slice then receives
its own generated hash, stable source identity, loop policy, resource mask, phase behavior, and
semantic binding. A rejected take does not weaken catalog failure semantics or fall back to a
mislabeled clip.

Run the native visible checkpoint with:

```powershell
gmake vrma-semantic-candidate-review VRM_PATH="C:\dev\eidolon\assets\2349235869624830263.vrm"
```

The owner accepted the complete take as viable captured source material. It remains deliberately
unlabeled: that acceptance does not imply that the entire conversation is one semantic generator,
and no bounded range has yet been accepted for runtime binding.

`vrma-semantic-candidate-review` now writes a strict ignored review-selection artifact. Press
`Space` to pause, use Left/Right to scrub 250 ms (`Shift` changes that to one second), and press `I`
and `O` on the first and last intentional poses. `R` loops only the marked range. Middle-drag and
the wheel retain the native camera controls. `Enter` accepts the range; closing or Escape cancels.

The accepted millisecond range is not the source identity. `make vrma-semantic-slice` validates its
schema and reviewed duration, snaps both marks deterministically to the pinned BVH sample cadence,
records exact inclusive/exclusive source-frame evidence inside a derived VRMA, and preflights it.
`make vrma-semantic-slice-review VRM_PATH=...` then reviews that exact derived clip. Only after that
bounded clip is owner-accepted may its generated hash, source identity, semantic label, resource
mask, phase behavior, and loop policy be pinned and atomically bound into the motion pack.
