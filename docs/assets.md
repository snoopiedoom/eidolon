# Character and asset pipeline

Eidolon treats downloaded or extracted character art as local runtime input. Reusable manifests,
import/export tools, and renderer code belong in Git; new game-asset downloads do not.

Legacy Rio source material is already tracked under `assets/blue-archive-rio-battle-full-rip-rig`
and exists in repository history. That is historical content, not a grant of redistribution rights
or the template for new imports. Private VRMs, model-specific calibration sidecars, and the local
Miyu/Miku downloads are excluded from the current publication. History cleanup, if desired, is a
separate operation; excluding new assets does not erase old commits.

This document separates the generic body contract from project-specific acquisition and repair:

- generic sprite-atlas, portrait-set, and 3D runtime requirements apply to any character;
- Blue Archive downloading and Rio authoring are local importer examples;
- extracted game assets are not assumed to be commercially redistributable.

## Planned audio reference

Startup music candidate: **“Below Between Beyond” — Ludo, _Cafe de Touhou 1_.**

This records the product reference only. No audio asset is included in the repository, and
redistribution or release use requires separate rights verification.

## Project-specific: Blue Archive portrait catalog

The Blue Archive wiki's `Category:Character_sprites` currently follows two filename forms:

```text
Character NN.png
Character (Variant) NN.png
```

`tools/download_character_sprites.py` uses the MediaWiki Action API to enumerate the complete file
category and request original image metadata. It groups each playable appearance into the existing
runtime layout:

```text
assets/characters/<character>-<variant>/portraits/<source_filename>
assets/characters/<character>/portraits/<source_filename>        # base appearance
```

`Bunny Girl` is normalized to `bunny`, preserving the established
`assets/characters/asuna-bunny/portraits` directory and filenames such as
`Asuna_(Bunny_Girl)_00.png`.

Preview the catalog before starting the large download:

```powershell
make character-sprites
```

Download everything, or select a smaller set:

```powershell
make character-sprites-download
python tools/download_character_sprites.py --download --character Asuna
python tools/download_character_sprites.py --download --character Asuna --variant "Bunny Girl"
python tools/download_character_sprites.py --manifest-only
```

The complete category contains thousands of high-resolution PNG files, so downloading is explicit.
The downloader is resumable, writes files atomically, compares existing sizes, verifies downloaded
SHA-1 values, retries transient failures, checks free space, and writes
`sprites-manifest.json` under the selected output root. Use `--verify` to hash existing files,
`--jobs` and `--delay` to tune load, and `--force` to redownload matching files.

Filename grouping is deterministic; expression semantics are intentionally not inferred. A later
pipeline can annotate the numbered portraits without coupling acquisition to a classifier.

Run the offline grouping tests with `make character-sprites-check`.

## Generic sprite-atlas requirements

The sprite body renderer consumes Codex-compatible v2 8x11 sprite sheets. `animation.c` owns atlas
rows, frame timing, and lifecycle state selection. `sprite.c` retains a validated CPU atlas, binds
an SDL texture only for the compatibility backend, and can rasterize the current cell into a native
presentation target. A sprite package is presentation-only and cannot restyle dialogue surfaces
owned by Eidolon.

## Generic portrait-set requirements

A portrait character is a directory of full-canvas transparent images plus a strict manifest in
`config/character.cfg`. Images for one model should share dimensions and alignment so expression
changes do not jump. Every expression supplies its own bust/face crop because head placement may
vary between images.

The current Bunny Asuna source canvases are 927x1280 and use a 390,0,350,420 portrait crop. Runtime
expression art changes atomically; whole-image procedural motion is applied afterward.

## Generic 3D runtime requirements

FBX is authoring input; GLB is runtime input. Blender owns import repair, deterministic export, and
visual QA. The C runtime uses `cgltf`; `ufbx` remains an optional validation fallback rather than a
custom application-level FBX inspector.

Useful commands:

```powershell
make model-audit BLENDER=C:/Blender/blender.exe
make model-material-audit BLENDER=C:/Blender/blender.exe
make model-export BLENDER=C:/Blender/blender.exe
make model-preview-glb BLENDER=C:/Blender/blender.exe
make model-mouth-calibrate BLENDER=C:/Blender/blender.exe
```

Audit output and previews live under `build/model-audit`.

### Experimental VRM reference body

EPR's first rigged-body experiment uses one supported VRM 1.0 reference avatar and a deliberately
narrow renderer dialect. This is not a claim that Eidolon can load arbitrary VRM 1.0 files. A
candidate must carry authoritative `VRMC_vrm` humanoid metadata and license information; the
current preflight validates the supported hierarchy/transform subset and publishes an explicit
capability profile. The separate runtime check establishes geometry, textures, skinning,
projection, shader, and hidden-frame executability only for the selected asset/machine.

This rigged-3D asset path is independent from the portrait and sprite asset paths. Selecting or
rejecting a VRM never changes their manifests, images, or renderer-local state.

Large third-party VRM files remain local and uncommitted. A distributable character package must
carry compatible rights metadata before it can ship. See the
[experimental VRM reference-body contract](design/vrm-body-runtime.md).

For automatic-retargeting EPR work, the runtime measures mapped humanoid bind positions, authored
rest frames, segment lengths, and hips height, then converts shared normalized motion into the
destination body. A supported body must not require a `.epr-calibration` sidecar for baseline
playback. Sidecars and `EIDOLON_VRM_CALIBRATION_PATH` remain optional package-author residual tools;
any override for a nonredistributable reference model stays local until its distribution and
derivative-data status is reviewed. Motion assets independently require explicit provenance and
redistribution/use evidence.

`make vrma-check VRMA_PATH=...` reports each owned humanoid track's role, path, interpolation,
key count, whether it actually varies, and aggregate root/torso/head/arm/leg coverage. A clip that
parses is not therefore a useful whole-body retargeting diagnostic. Pixiv's MIT-licensed
[three-vrm animation example](https://github.com/pixiv/three-vrm/tree/dev/packages/three-vrm-animation/examples/models)
is an official parser-conformance input, but its `test.vrma` currently carries only one humanoid
motion track and could not serve as R3's full-body visible evidence.

The first full-body idle fixture is `standard_idle.vrma` from Virtual Avatar SDK commit
[`ab8f0d4`](https://github.com/hirokazuniimoto/virtual-avatar-sdk/commit/ab8f0d4d2ee5bdfa2321b7ac94bfbf4f0a6547eb).
That repository includes the animation as a default package asset under its MIT license. Eidolon
pins both source URLs and SHA-256 values; `make vrma-idle-fixture` downloads the animation and its
matching license into ignored `build/fixtures/vrma/standard-idle`. It never enters Git. The pinned
animation hash is `42eec1c51cf3978f783d782272e2beac7eb5f945d0f4969de076c569b0f0220b`.
Its current preflight reports 8.217 seconds, 23 humanoid tracks, 20 varying rotation tracks, and
varying root, torso, head, both arm, and both leg chains.

When this ignored fixture is present at build/runtime setup, the model owns its clip lifetime and
registers it immutably as the first EPR semantic generator, `idle.neutral`. Its stable runtime source
identity is `0x42eec1c51cf3978f`, the leading 64 bits of the pinned SHA-256. The fetch target performs
the cryptographic verification; runtime loading validates the VRMA structure and tracks. Other
clips are never inferred or relabeled as missing posture/gesture semantics.

`make vrma-walk-fixture` fetches CMU trial `104_02` (neutral male walk with exact footfalls) from
the BVH mirror at commit
[`09a07f5`](https://github.com/una-dinosauria/cmu-mocap/commit/09a07f54f3bbb58797325f009282d0b2048a2871),
alongside that release's usage-rights file. Both inputs are hash-verified before Eidolon's
deterministic BVH-to-VRMA compiler collapses auxiliary joints into the 22 mapped humanoid roles.
Frame zero supplies the authored T-pose; frames 141 through 441 form the selected 2.5-second
steady-state loop. The generated fixture has 23 tracks, 20 varying rotations, and all seven varying
humanoid chains. Its source SHA-256 is
`c7b350504477fc77e890dad93f700af0c98228dd02e16a9f489b37a6fc32cf62`; its generated VRMA SHA-256
is `81806d6eb858524d38cd7a8c4de9ed11cba3e26002bf9a82e8da89faf6636eb3`. The source, rights, and
derived VRMA remain in ignored build storage because CMU permits project use but prohibits direct
resale of the motion data, including converted data.

`make vrma-semantic-candidate` fetches CMU trial `18_08` from the same pinned mirror and
verifies the matching usage-rights file. The pinned CMU index describes it as a two-subject
conversation in which subject A explains with hand gestures. The deterministic full-take conversion
is 17.375 seconds with 23 tracks, 20 varying rotations, and all seven varying humanoid chains. Its
source SHA-256 is
`b57e6ba15cf2bde4e6e4233da53425dde6d91952fb2bff17a89c5fce18a1a0ce`; the generated VRMA
SHA-256 is `c80760542b587de13f02e81176f88a5148769103bf7e891bf7ce89ee1298982c`.
The source, rights, and output remain ignored. The complete take is review material, not a semantic
binding. The owner accepted it as viable source material, without assigning one meaning to the
whole conversation. `make vrma-semantic-candidate-review VRM_PATH=...` opens it in the native
visible harness and writes an ignored selection only after `I`/`O` marks are replayed and accepted
with `Enter`. `make vrma-semantic-slice` validates and snaps those millisecond marks to exact pinned
BVH frames, builds a deterministic derived VRMA, and preflights it. `make
vrma-semantic-slice-review VRM_PATH=...` is the separate visible acceptance gate for that exact
slice. No slice receives a generator name, source identity, resource mask, phase behavior, or loop
policy before that gate. See the
[semantic motion-pack contract](design/epr-motion-pack.md).

`make vrm-animation-runtime-check VRM_PATH=... VRMA_PATH=...` clears any loaded calibration,
requires all seven varying humanoid chains, publishes 251 deterministic samples across five
seconds, and renders hidden endpoint GPU frames. This is objective sidecar-free execution evidence;
it does not replace visible judgement of deformation, contact, weight, or acting quality. Both the
pinned idle and CMU walk passed this gate and were owner-accepted through the native visible harness
on Vampire Cat, closing R3.

`make vrm-animation-review VRM_PATH=... VRMA_PATH=...` prepares the same sidecar-free playback on
Eidolon's transparent, borderless native body target and loops until the owner closes it. Closing
before one complete loop reports failure. Middle-drag rotates, Shift+middle-drag rolls, the wheel
zooms the character, and double-middle resets the view.

The official [VRoid Project animation pack](https://booth.pm/ja/items/5512385) is a candidate
local diagnostic source. Its seven gestures may be used for testing and commercial work with
credit, but the posted terms prohibit redistribution in an extractable form. It must therefore be
downloaded by the operator, remain ignored, and be preflighted locally; it cannot be vendored or
treated as Eidolon package content. The pack does not supply the required idle/walk pair, so it can
expand local gesture coverage but is not a redistributable R5 motion pack.

The current owner-selected local development default is
`assets/2349235869624830263.vrm` (Vampire Cat by Touko Asada). It passes the supported VRM 1.0
structure and runtime gates. Its embedded metadata restricts avatar use to the author, prohibits
redistribution and modification, and requires credit. The file is ignored by Git and is neither a
public project dependency nor a recommendation. `EIDOLON_VRM_PATH` overrides it at runtime;
`DEFAULT_VRM_MODEL=...` overrides the compiled default at build time.

The first reference body is
[DECAGRAMMATON by Leona_SAN34](https://hub.vroid.com/characters/61437424751231571/models/3310288597351780654).
Its current VRoid Hub terms identify it as VRM 1.0, permit avatar use, require credit, and prohibit
redistribution and modification. The embedded author is `reona`. Operators must authenticate with
their own Pixiv account, acquire the file manually, and re-check the page terms at download time.
Eidolon does not automate authentication or download and must never commit the resulting `.vrm`.

Run the current structural/profile preflight on the manually acquired reference body:

```powershell
make vrm-structure-check VRM_PATH="C:\local-assets\character.vrm"
make vrm-runtime-check VRM_PATH="C:\local-assets\character.vrm"
```

Passing `make vrm-structure-check` does not prove runtime renderability. The runtime target exercises
the real selected-reference geometry, textures, skinning, projection, shaders, and a hidden GPU
frame. Passing both still does not prove that an arbitrary VRM is supported. `make vrm-check` remains
a structural compatibility alias; the contract defines the broader corpus required before the
support claim can expand.

## Project-specific: Rio authoring and repair

The canonical body source is `CH0331_Mesh.fbx`: five meshes, 10,842 vertices, and a 127-bone body
and facial rig. The separate halo attaches to `Bip001 Head`. The source provides no authored actions
or shape keys; facial control survives through bones, split meshes, and the eye/mouth atlas.

The missing `Character_Mouth_Black.png` overlay was reconstructed. The saved default zero-offset
calibration looked best; absence of generated calibration JSON is therefore equivalent to zero
offset.

`make model-export` writes `assets/model/rio.glb`, normalizes seven texture-backed materials, adds
the halo attachment extra, and converts alpha blending to a 0.05 cutout. Ordinary glTF blending
sorts Rio's coplanar face layers incorrectly and can reduce the eyes to red discs. Always inspect
the deterministic GLB preview after material or exporter changes.
