# Eidolon

A local visual embodiment for agents. Eidolon renders animated companions and their activity
surface without owning the agent's persona or conversation runtime.

The current implementation state, sharp edges, and new-session continuation notes live in
[`docs/development-state.md`](docs/development-state.md).

The project is intentionally split into five layers:

- `app`: state, input, timing, and composition
- `animation`: provider-independent v2 sprite atlas playback
- `portrait`: full-canvas 2D expressions, transitions, and subtle procedural motion
- `motion`: procedural pose goals and skeleton evaluation
- `platform`: the smallest possible native overlay boundary

SDL owns rendering and input. Platform code only handles behavior SDL cannot express uniformly.

## Build

Requirements:

- GNU Make
- LLVM/Clang
- SDL3 development files
- SDL_ttf 3 development files (`make text-setup` installs the pinned Windows package)
- the vendored `cgltf` source at `lib/cgltf`
- the Windows SDK's `fxc.exe` for native D3D11 shader builds on Windows
- the `shadercross` CLI when rebuilding the legacy SDL_GPU shaders on Linux

Windows (the Makefile defaults to `C:/dev/SDL3`):

```powershell
make text-setup
make
./build/windows/eidolon.exe
```

Override `SDL3_ROOT` if the SDK lives elsewhere. It must contain `include/SDL3`,
`lib/x64/SDL3.lib`, and preferably
`lib/x64/SDL3.dll`. The DLL is copied beside Eidolon when present.

`make text-setup` downloads and verifies SDL_ttf 3.2.2 into the ignored `.cache/sdl_ttf`
directory; the normal build never downloads dependencies. Eidolon ships MesloLGS Nerd Font Mono as
its default face under `assets/fonts/MesloLG Nerd Font`. On Windows it adds the installed Microsoft
YaHei, Malgun Gothic, and Segoe UI Emoji faces as fallbacks when available, so Latin, Slovenian,
Chinese, Korean, Nerd Font glyphs, and emoji can share one UTF-8 dialogue stream.

Shaders are authored once in HLSL. On Windows, `make shaders` discovers the newest x64 `fxc.exe`
under the installed Windows SDK and bakes Shader Model 5.0 DXBC for D3D11. The compiler is a build
tool only and is not linked into or shipped beside `eidolon.exe`; use `FXC=/path/to/fxc.exe` to
override discovery. Debug and release objects, tests, and shader blobs use separate build
directories, so changing `MODE` cannot silently reuse the other mode's output. Mode-specific
binaries live under `build/windows/bin/<mode>`; each `make` copies the selected one to the stable
`build/windows/eidolon.exe` launch path. The legacy Linux SDL_GPU path still uses `shadercross` for
SPIR-V/DXIL; override it with `SHADERCROSS=/path/to/shadercross` when needed. `SDL_shadercross`,
DXC, and SPIRV-Cross source snapshots remain under `lib/` for that path.

Linux:

```sh
make
./build/linux/eidolon
```

Linux discovers SDL3 and SDL_ttf 3 through `pkg-config`. Use `make MODE=release` for an optimized build and
`make check` for the small state and animation tests.

When `config/character.cfg` and its portrait PNGs are available, the 2D character provider is
primary and 3D initialization is skipped completely. The bundled manifest expects the ten original
Bunny Asuna portraits (`00` through `08`, plus `99`) under
`assets/characters/asuna-bunny/portraits`. Extracted character art is deliberately ignored by Git;
only the reusable manifest and renderer are repository content.

Expression control is renderer-independent. `src/affect.c` turns lifecycle state or a multi-label
GoEmotions result into six continuous axes: valence, arousal, dominance, certainty, warmth, and
surprise. The portrait consumes only the selected expression, while Rio's future posture,
breathing, gaze, and secondary motion can consume the same axes. Native inference remains optional;
without its local worker, or whenever that worker fails, lifecycle state is the working fallback.

On Windows, install and verify the optional quantized GoEmotions worker with:

```powershell
make affect-setup
make affect-check
make
```

`affect-setup` downloads pinned ONNX Runtime and model artifacts into the ignored `.cache/affect`
directory (roughly 200 MB downloaded, roughly 150 MB retained). `make affect` builds only the worker;
`make affect-check` additionally runs native single-shot and asynchronous-client inference tests.
The normal build never downloads artifacts and does not link ONNX Runtime into the renderer.

Windows forces SDL's `direct3d11` renderer. Rio and SDL composition share that renderer's D3D11
device, avoiding the descriptor-heap failures previously seen in the separate D3D12 path. If
D3D11 model initialization fails, Eidolon keeps the SDL renderer alive and falls back to the sprite.
Render-target resets reacquire SDL's native texture view before drawing again; device reset/loss is
logged and exits cleanly so the process can be restarted against a valid device.

## Prototype controls

- drag the pet with the left mouse button
- `1`: idle
- `2`: running
- `3`: needs input
- `4`: ready/review
- `5`: failed
- `Space`: cycle states
- middle-mouse drag on the 3D model: rotate yaw and pitch
- `Shift` + middle-mouse drag: rotate roll
- double middle-click: reset model rotation
- `F1`: toggle the debug panel; inspect affect source/intent/VAD or override any portrait expression
- `F5`: force reload `config/character.cfg` and `config/motion.cfg`
- `P`: toggle the active 2D character between full-body and per-expression face framing
- `T`: toggle between the preserved `classic` dialogue theme and `academy_heart`
- `C` while a semantic pose is selected: copy its complete C initializer to the clipboard
- `Escape`: close the debug panel, then quit

Portrait state defaults and labels are ordinary text in `config/character.cfg`. Saved edits are
validated and hot-reloaded; invalid edits preserve the last good character. State defaults currently
map idle to `gentle`, running to `responding`, waiting to `worried`, review to `cheerful`, and failed
to `annoyed`. The F1 selector is a temporary visual override and does not rewrite the manifest.
`framing = full` or `framing = portrait` chooses the persisted startup framing; `P` is a temporary
runtime toggle. Each expression owns its portrait crop, so characters with moving or differently
composed heads do not need one global rectangle.

Expression changes add a short semantic motion accent independently from the 140 ms image
crossfade. `motion.accent_strength` and `motion.accent_duration_ms` tune the shared response while
the expression label selects its direction: positive expressions lift and lean in, worried sinks,
and annoyed recoils. Translation, scale, and rotation use different spring frequencies; repeated
expressions select one of three deterministic variants, and an interrupted accent adds a brief
opposite anticipation before committing to the replacement.

The settled pose also retains a tiny expression-specific posture instead of returning to one shared
neutral. Phrase punctuation drives short speaking beats, and the portrait eases toward the side of
the currently speaking session bubble. `motion.posture_strength`, `motion.speech_strength`, and
`motion.attention_strength` independently tune these layers. All transforms compose once around the
portrait's lower-body pivot, above breathing and with sway still disabled by the bundled character
configuration.

Window and rendering dimensions follow SDL's per-display content scale, so `1.0x` remains physically
consistent on high-DPI displays. The scale control is an additional `0.75x..4.0x` multiplier.
Scale changes Rio's presentation size without silently changing the 3D render target. The adjacent
resolution selector chooses a bounded `512`, `1024`, `1536`, or `2048` square target; `1024` is the
Windows default. This makes the quality/cost tradeoff explicit and prevents a large desktop scale
from accidentally multiplying render work. On Windows the selected target remains GPU-resident from
skinning through SDL composition; changing its resolution does not create a per-frame CPU transfer.

Render a deterministic QA frame without leaving the overlay running:

```powershell
./build/windows/eidolon.exe --snapshot build/eidolon-snapshot.png
```

Use `--snapshot-debug` to capture the open debug panel and semantic-pose menu, or
`--snapshot-debug-resolution` to capture the resolution menu. Pose regressions can be captured
without manual input:

```powershell
./build/windows/eidolon.exe --snapshot-pose 1 build/relaxed.png
./build/windows/eidolon.exe --snapshot-debug-pose 1 build/relaxed-debug.png
./build/windows/eidolon.exe --snapshot-resolution 2048 build/resolution-2048.png
./build/windows/eidolon.exe --snapshot-sessions build/sessions.png
./build/windows/eidolon.exe --snapshot-face build/face.png
```

Pose indices follow the order shown in the F1 selector. Snapshot commands create a hidden window,
skip overlay configuration, IPC, and session watching, render one frame to disk, and exit. They do
not take focus or require desktop input.

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

On Windows, the overlay rebuilds its native hit mask from the rendered alpha channel when visible
geometry changes. Fully transparent pixels pass clicks to applications underneath; any visible
sprite, bubble, or shadow pixel remains interactive and draggable. During model rotation or a
debug-slider drag, the mask is temporarily suspended and readback is deferred; one fresh mask is
built when the interaction ends.

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
sends plain text through the local IPC channel. Eidolon renders it as a rolling five-line JRPG
viewport with a typewriter reveal. The default `follow` movement shifts lines 2–5 upward immediately
when the cursor reaches a sixth line, then continues typing on the freed bottom row. There is no
scroll pause. Clicking still reveals or scrolls immediately, and dragging still moves the overlay.

Dialogue movement is runtime configuration in `config/character.cfg`: `manual` waits for a click
and replaces the whole five-line page; `paged` waits `dialogue.hold_ms` and then replaces the whole
page automatically; `follow` continuously follows the typing cursor one line at a time. The hold
value is retained for `paged` even while another movement is selected, ready for the future menu.

`dialogue.theme = classic` preserves the original dark bubble exactly. The bundled
`academy_heart` preset uses a pale glass panel with cyan structure and pink accents; it is the
current character default and can be compared at runtime with `T`.

Codex can load hooks from several layers at once; copying this over an existing global
`hooks.json` would erase those definitions. Merge the `hooks` entries instead when that file
already exists.

## Multiple-session direction

Each active Codex session owns an independent dialogue bubble keyed by session UUID. Titles come
from `.codex/session_index.jsonl`; dialogue scrolling, activity time, and stable layout slots remain
per-session. Up to four visible bubbles occupy deterministic left/right anchors around the shared
character while the registry tracks eight recent transcripts. Quiet completed bubbles retire after
five minutes; a bubble with unread text remains. Session discovery and transcript parsing do
not leak into drawing. Detailed continuation notes are in `docs/development-state.md`.

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

At runtime, `cgltf` decodes the GLB and native D3D11 renders its seven material draws with depth
testing, embedded PNG textures, and alpha cutout. The HLSL pair is baked to Shader Model 5.0 DXBC.
The runtime copies the 132-node hierarchy and 127-joint skin from the GLB, evaluates procedural local
transforms, and uploads a linear-blend skinning palette at 30 fps. The source contains no animation
clips; primary motion is generated by Eidolon's controller instead. Ordinary nodes such as the halo
retain their exported world transform.

SDL owns the transparent D3D11 renderer, device, context, and swapchain. Rio renders into an
SDL-owned target texture through the renderer's native device; `SDL_RenderTexture` then samples that
same allocation while composing dialogue and debug UI. A scoped D3D11 state save/restore protects
SDL's persistent 2D pipeline state around the native 3D pass. There is no fence, staging map,
full-frame memcpy, or upload in the animated model path.

Per-pixel hit testing reads the final composited alpha channel only for structural invalidations:
initialization, window or model scale changes, state/bubble geometry changes, and completed manual
pose or rotation edits. Ordinary breathing and sway reuse the conservative mask instead of forcing
a synchronous full-window GPU readback twice per second. Windows keeps the exact alpha mask for
`WM_NCHITTEST`, while `SetWindowRgn` receives a coarse 16-pixel envelope with far fewer rectangles;
click-through remains pixel-exact without making DWM composite thousands of one-scan-line regions.
The app presents at an explicit 30 Hz—the model's actual update rate—even when driver VSync is
unsupported, preventing a transparent swapchain from flooding DWM. Hidden snapshots do not
initialize the overlay hit-test path at all.

### Semantic motion controller

Eidolon generates primary motion procedurally instead of depending on authored idle clips. The
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
shared GPU texture → transparent SDL composition
```

Behavior intent describes affect, attention, engagement, intensity, and movement quality; it never
drives individual joints. The pose layer composes **semantic motifs** such as stance, arm posture,
spine attitude, gaze, and timing. Motifs are not hard-coded emotions: crossed arms may express
confrontation, concentration, or self-comfort, while hands behind the back may read as confidence,
curiosity, playfulness, or formality. Intent, personality, and context select and weight them.

The runtime now builds a VRM-like humanoid profile from semantic bone roles rather than letting pose
code know Rio's exported joint names. Rio aliases and common humanoid names map into the same hips,
torso, head, arm, hand, leg, and foot roles. The profile validates hierarchy, derives an orthonormal
body frame, and measures limb lengths from the bind pose. A replaceable C solver boundary then runs
an analytic two-bone arm solve with a pole target, reach clamping, and soft extension. Malformed or
non-finite goals are rejected transactionally, so one failed arm cannot leave half a pose applied.

Semantic arm goals are normalized by the character's measured arm length and expressed relative to
each shoulder as **outward, up, forward**. The same values therefore mirror correctly across left and
right arms and survive proportion changes without retuning raw bone angles. Slow weight sway and
breathing still run first through the spine, neck, and head; target-space posing runs afterward and
feeds the existing GPU skinning palette. The legacy **Arm lower** and **Elbow add** controls remain in
Custom/file mode as a bind-axis diagnostic, not as the semantic-pose representation.

The F1 selector is the visual calibration loop. Its initial entries are deliberately reviewable
guesses: bind reference, relaxed/open, attentive, reserved/guarded, and playful/open. Selecting one
installs a mutable runtime copy. Six sliders expose symmetric hand and elbow-pole coordinates; an
asterisk beside the pose name marks edits while keeping the source preset identifiable. Press `C`
to copy the exact two-arm initializer and paste it into review, then promote the corrected values to
`src/pose.c`. File reload or either legacy neutral slider explicitly exits semantic mode, so the IK
layer never silently overrides the control being edited.

This first target-space slice owns arm position and elbow plane. Wrist orientation, shoulder and
forearm twist limits, independent left/right editing, planted-foot IK, stochastic blinking,
eye-then-head attention shifts, pose transitions, and spring motion on the hair chains remain the
next controller layers. Later states reuse the same solver by changing goals and motif weights
rather than switching canned animation files.

Rio's chest is a prominent, heavy secondary-motion mass. Its response should be driven by torso
acceleration through a damped spring with visible inertia, soft settling, anatomical limits, and
subtle left/right variation. It must inherit breathing and abrupt pose changes without becoming a
constant periodic bounce. Keep these parameters character-specific and runtime-tunable alongside
the hair and clothing physics.

### Live motion tuning

Edit `config/motion.cfg` while Eidolon is running. The overlay hashes the file every 250 ms and
applies a complete valid edit without restarting the current animation phase. The format is a
strict, versioned `key = value` list with units in each key. Unknown, duplicate, missing, malformed,
or out-of-range values reject the entire edit; the last-good configuration stays active while the
debug panel shows the error. Compiled defaults remain available if the file cannot be loaded.

The legacy neutral and idle sliders update the same active file-backed configuration. A `*` beside
the debug-panel revision means one of those sliders has made an unsaved runtime-only change; the next
valid file reload replaces it. Semantic target edits use a separate asterisk beside the pose name
and can be copied as a source initializer. `F5` forces a reload even when the contents have not
changed. The `seed` is reserved now so later blink and motif scheduling can remain reproducible.

The neutral controls intentionally have broad calibration ranges: `arm_lower_deg` accepts
`-45..90`, and `elbow_add_deg` accepts `-90..90`. Idle rotation amplitudes accept `-15..15`; negative
values invert the direction. Rejected values report their allowed range in the debug panel and log.

## Debug log

The renderer and every short-lived hook client append lifecycle, transcript extraction, and IPC
events to `%LOCALAPPDATA%\Eidolon\eidolon.log` on Windows. Linux uses
`${XDG_STATE_HOME:-~/.local/state}/eidolon/eidolon.log`. Agent text is never written to the log;
only state names, byte counts, and failure boundaries are recorded.
