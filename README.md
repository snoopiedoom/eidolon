# Eidolon

A local visual embodiment for agents. Eidolon renders animated companions and their activity
surface without owning the agent's persona or conversation runtime.

The project is intentionally split into three layers:

- `app`: state, input, timing, and composition
- `animation`: provider-independent v2 sprite atlas playback
- `platform`: the smallest possible native overlay boundary

SDL owns rendering and input. Platform code only handles behavior SDL cannot express uniformly.

## Build

Requirements:

- GNU Make
- LLVM/Clang
- SDL3 development files
- the vendored `cgltf` source at `lib/cgltf`
- the `shadercross` CLI when shader sources need to be rebuilt

Windows (the Makefile defaults to `C:/dev/SDL3`):

```powershell
make
./build/windows/eidolon.exe
```

Override `SDL3_ROOT` if the SDK lives elsewhere. It must contain `include/SDL3`,
`lib/x64/SDL3.lib`, and preferably
`lib/x64/SDL3.dll`. The DLL is copied beside Eidolon when present.

Shaders are authored once in HLSL and baked offline to SPIR-V and DXIL with
`make shaders`. On Windows the default host-tool path is
`.cache/shadercross/bin/shadercross.exe`; override it with
`SHADERCROSS=/path/to/shadercross` when needed. `SDL_shadercross`, DXC, and SPIRV-Cross are build
tools only and are not linked into or shipped beside `eidolon.exe`. Their source snapshots are
vendored under `lib/`; generated host-tool builds and downloaded compiler payloads remain local.

Linux:

```sh
make
./build/linux/eidolon
```

Linux discovers SDL3 through `pkg-config`. Use `make MODE=release` for an optimized build and
`make check` for the small state and animation tests.

## Prototype controls

- drag the pet with the left mouse button
- `1`: idle
- `2`: running
- `3`: needs input
- `4`: ready/review
- `5`: failed
- `Space`: cycle states
- `Escape`: quit

Render a deterministic QA frame without leaving the overlay running:

```powershell
./build/windows/eidolon.exe --snapshot build/eidolon-snapshot.png
```

## Codex state bridge

Eidolon accepts local, best-effort state events without giving the hook ownership of SDL or the
overlay:

```powershell
./build/windows/eidolon.exe --hook running
./build/windows/eidolon.exe --hook waiting
./build/windows/eidolon.exe --hook review
```

The hook command intentionally exits successfully when Eidolon is closed. A cosmetic integration
must not break a Codex turn.

The renderer also watches `~/.codex/sessions` directly. This is the primary integration for the
Microsoft Store app, where locally configured command hooks may be reported as active without the
external command being launched. The watcher establishes a baseline at startup and displays each
new final agent message; it does not replay an old response when Eidolon starts.

On Windows, the overlay rebuilds its native hit region from the rendered alpha channel whenever
the sprite frame or bubble geometry changes. Fully transparent pixels are outside the Win32 window
region and pass clicks to applications underneath; any visible sprite, bubble, or shadow pixel
remains interactive and draggable.

To connect Codex on Windows, copy `hooks/codex-hooks.windows.json` to
`~/.codex/hooks.json`, then review and trust it through `/hooks`. Both command fields use the
absolute executable path because some packaged Codex builds have been observed falling back to
the generic `command` field. The checked-in Windows config assumes this repository's
`C:\dev\eidolon` location. On Linux, use `hooks/codex-hooks.json`, install `eidolon` on `PATH`, or
replace the plain `eidolon` command with its absolute path.

The mapping is deliberately small:

- session start: idle
- prompt and tool activity: running
- permission request: waiting
- turn stop: review

On `Stop`, the hook reads Codex's `transcript_path`, extracts the latest final agent message, and
sends plain text through the local IPC channel. Eidolon renders it as a five-line JRPG dialogue
page with a typewriter reveal. Click once to reveal the page immediately; click again to advance
when the small triangle is visible. Dragging still moves the overlay.

Codex can load hooks from several layers at once; copying this over an existing global
`hooks.json` would erase those definitions. Merge the `hooks` entries instead when that file
already exists.

## Deferred multi-session direction

Each running session will own a separate dialogue bubble. A later layout layer will place those
bubbles dynamically, avoid overlap, and keep them associated with the shared sprite. Until that
layout work exists, the current single-bubble renderer remains intentionally session-agnostic.

## Live 3D

Eidolon loads the character as 3D; the model is not baked into sprite sheets. FBX remains an
authoring/source format. Blender handles inspection, repair, visual QA, and deterministic GLB
export, then the C runtime loads the processed asset with `cgltf`. `ufbx` remains an optional
validation fallback if a specific FBX imports incorrectly; Eidolon does not grow a custom FBX
inspector by default.

Run the deterministic Blender inventory with `make model-audit`. It writes
`build/model-audit/index.json` with per-FBX mesh, rig, action, shape-key, material, texture, camera,
and bounds data. Blender is an asset-pipeline dependency only; it is never required by Eidolon at
runtime.

The canonical base is `source/Rio Battle/CH0331_Mesh.fbx`: five meshes, 10,842 vertices, and the
complete 127-bone body/facial rig without the duplicated FX body found in
`Animator/CH0331/CH0331.fbx`. None of the 30 FBXs contains imported animation actions or shape keys.
Facial control survives as bones, split meshes, and the `CH0331_EyeMouth` atlas. The reconstructed
`Character_Mouth_Black.png` fills the previously missing 25-vertex mouth overlay driven by
`bone_mouth`.

Run `make model-mouth-calibrate` to place that mouth interactively. Blender opens an unlit scene;
press `N`, open the **Eidolon** sidebar tab, adjust placement, then click **Save calibration**. The
tool writes `build/model-audit/mouth-calibration.json`, and `make model-export` applies it.

`make model-export` assembles the canonical body and separate halo into `assets/model/rio.glb`.
The halo carries an `eidolon_attach_bone` extra naming `Bip001 Head`. Export normalizes the seven
texture-backed materials and rewrites glTF alpha blending to a 0.05 alpha cutout. The character uses
several coplanar face layers; ordinary glTF blending sorts them incorrectly and reduces the eyes to
red discs. `make model-preview-glb` round-trips the GLB through Blender and renders
`build/model-audit/rio-glb-preview.png` to catch that class of error.

At runtime, `cgltf` decodes the GLB and SDL GPU renders its seven material draws with depth testing,
embedded PNG textures, and alpha cutout. One HLSL shader pair is baked to SPIR-V for Vulkan and DXIL
for D3D12. The Windows path is verified on the D3D12 backend.

SDL's GPU renderer explicitly rejects `SDL_WINDOW_TRANSPARENT`, so 3D and desktop composition have
separate owners for now. SDL GPU renders Rio into a 256x256 offscreen texture, reads the static
bind-pose result back once, and the existing transparent `SDL_Renderer` composites that texture with
dialogue. This is a runtime 3D load and render, not a generated sprite asset, but it is not yet a
live animation path. The source GLB has no animation clips, and GPU skinning is not implemented;
skinned meshes therefore use their bind-pose vertex coordinates while ordinary nodes such as the
halo retain their world transform.

Live bone animation needs either a pipelined offscreen readback or a platform compositor capable of
presenting an alpha GPU surface directly. Per-pixel hit testing continues to consume the final
composited alpha channel, so transparent model pixels pass clicks through exactly like transparent
sprite pixels.

### Planned semantic motion controller

Eidolon will generate primary motion procedurally instead of depending on authored idle clips. The
control pipeline keeps semantic decisions slow and inspectable while pose solving and physical
response run at animation rate:

```text
language/session state
        ↓
behavior intent, 1–5 Hz
        ↓
procedural pose goals
        ↓
IK + joint limits, 60 Hz
        ↓
secondary physics, 60 Hz
        ↓
bone matrices → GPU skinning
        ↓
asynchronous transparent-frame readback
```

Behavior intent describes affect, attention, engagement, intensity, and movement quality; it never
drives individual joints. The pose layer composes **semantic motifs** such as stance, arm posture,
spine attitude, gaze, and timing. Motifs are not hard-coded emotions: crossed arms may express
confrontation, concentration, or self-comfort, while hands behind the back may read as confidence,
curiosity, playfulness, or formality. Intent, personality, and context select and weight them.

The first controller milestone is a neutral standing idle with planted feet, relaxed arms, balance
and weight shifting, breathing, stochastic blinking, eye-then-head attention shifts, and spring
motion on the hair chains. Later states reuse the same solver by changing goals and motif weights
rather than switching canned animation files.

## Debug log

The renderer and every short-lived hook client append lifecycle, transcript extraction, and IPC
events to `%LOCALAPPDATA%\Eidolon\eidolon.log` on Windows. Linux uses
`${XDG_STATE_HOME:-~/.local/state}/eidolon/eidolon.log`. Agent text is never written to the log;
only state names, byte counts, and failure boundaries are recorded.
