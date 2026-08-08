# Character and asset pipeline

Eidolon treats downloaded or extracted character art as local runtime input. Reusable manifests,
import/export tools, and renderer code belong in Git; game assets do not.

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

For calibration-first EPR work, the runtime measures every mapped humanoid bind position and
nearest-semantic-parent segment length, then associates user-approved semantic anchors with the
resulting anatomy fingerprint. The default sidecar path is `<model>.epr-calibration`; an explicit
`EIDOLON_VRM_CALIBRATION_PATH` may select another file. A calibration for a nonredistributable
reference model remains local until its own distribution and derivative-data status is reviewed.

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
