# Configuration

Eidolon separates shipped defaults, character definitions, motion calibration, and personal
preferences. Those layers have different owners and must not collapse into one settings file.

## Precedence

```text
built-in safety defaults
        < system defaults (`config/settings.cfg`)
        < character/motion defaults
        < sparse per-user overrides
```

Changing a setting in the UI writes only that user field after a 500 ms debounce. Reset removes the
field from the user layer so future shipped or character-default changes still flow through. Dear
ImGui does not create `imgui.ini`; Eidolon has one persistence system.

## System defaults

`config/settings.cfg` owns application-wide defaults:

- `preferred_renderer`: `sprite`, `portrait`, or `model_3d`;
- `presentation_preference`: `native` or `sdl_window_legacy`;
- `display_scale`: presentation multiplier;
- `vsync`: requests synchronization to the active display refresh;
- `fps_limit`: an independent presentation ceiling from 1 to 1000, or `0` for no explicit cap;
- `bubble_bounds_mode`: `avatar`, `primary`, `virtual`, or `custom`;
- `model_render_resolution`: independent square 3D target size;
- `model_yaw_degrees`, `model_pitch_degrees`, `model_roll_degrees`.

The supported model targets are 512, 1024, 1536, and 2048 pixels in the UI. Presentation scale is
independent and currently ranges from 0.75x to 4.0x.

The shipped cadence policy is `vsync = true` and `fps_limit = 0`, so presentation follows the
active monitor rather than a fixed application rate. A lower explicit limit wins over VSync. With
VSync disabled, `fps_limit = 0` is genuinely uncapped. If a requested VSync mode is unavailable,
Eidolon uses the active display rate as a software fallback instead of running without a bound.

## User overrides

SDL resolves the per-user file through `SDL_GetPrefPath("snoopiedoom", "Eidolon")`; on Windows it is
normally under `%APPDATA%\snoopiedoom\Eidolon\settings.cfg`.

The file is sparse, may override the system fields above, and may additionally contain:

- `portrait_face_mode`;
- `dialogue_theme`;
- `dialogue_movement`;
- `dialogue_hold_ms`;
- `bubble_custom_x`, `bubble_custom_y`, `bubble_custom_width`, and
  `bubble_custom_height`.

The settings UI shows the effective default and its source beside each persisted field.

Bubble placement treats the mode and custom rectangle as one override. `avatar` constrains bubbles
to the usable work area containing most of the visible character and uses hysteresis near monitor
seams. `primary` pins bubbles to the primary work area. `virtual` permits the bounding rectangle of
all usable displays. `custom` uses the persisted rectangle; negative coordinates are valid. Reset
returns the complete policy to its inherited default.

## Presentation selection

The shipped `native` preference selects the best supported platform presentation at startup. On
Windows, portrait and 3D bodies normally select `win32_dcomp`. Sprite bodies select
`sdl_window_legacy`. Native host, graphics, or environment-bootstrap
failure also selects `sdl_window_legacy` and records the exact reason. Explicit
`sdl_window_legacy` preference skips the native attempt. Snapshots always use the legacy backend.

The Display settings tab persists this portable preference; changes apply at the next launch.
`native` does not encode DirectComposition into the user format, so another platform may resolve it
to its own native backend.

`EIDOLON_PRESENTATION_BACKEND` remains a developer override above persisted configuration:

```powershell
$env:EIDOLON_PRESENTATION_BACKEND = "win32_dcomp"
.\build\windows\eidolon.exe
```

Accepted override values are `native`, `win32_dcomp`, `sdl_legacy`, and `sdl_window_legacy`.
Invalid values are logged and ignored. An override cannot make an unsupported body native: the
body-capability decision still falls back explicitly. The preference UI continues to show the
persisted choice rather than rewriting it from a temporary environment override.

The DirectComposition backend supports portrait, dialogue, and direct D3D11 rigged-3D layers;
generation-bound CPU/projected-geometry alpha masks; transformed per-pixel hit testing; routed
wheel and captured middle-drag input; dialogue activation; body-context settings; Win32-owned body
dragging; revisioned output/DPI state; deterministic active-output retirement; and bounded
host-close/graphics-reset requests. A device/backend reset stops submissions, attempts one fresh
DirectComposition reconstruction from current product state, then logs and selects
`sdl_window_legacy` if that candidate cannot present a complete current frame. Sprite native
targets, output-local host migration, and evidence from real hardware device loss or display
disconnect remain future or optional work rather than A1 blockers.

Debug builds expose deterministic recovery probes. Set
`EIDOLON_DCOMP_TEST_RESET_AFTER_FRAMES` to a positive frame count to inject a device reset, and
optionally set `EIDOLON_DCOMP_TEST_FORCE_RECOVERY_FALLBACK=1` to force the SDL branch.
`EIDOLON_DCOMP_TEST_FAIL_CREATE=1` forces startup fallback.
`EIDOLON_DCOMP_TEST_REMOVE_ACTIVE_OUTPUT_AFTER_FRAMES` retires the active opaque output at a
positive frame count and selects a surviving output; a synthetic shifted output makes this
deterministic on single-monitor hosts.
`EIDOLON_PRESENTATION_TEST_HIDDEN=1` and
`EIDOLON_PRESENTATION_TEST_EXIT_AFTER_RECOVERY=1` support hidden reset automation.
`EIDOLON_PRESENTATION_TEST_EXIT_AFTER_OUTPUT_REMOVAL=1` exits after the first complete replacement
frame. `EIDOLON_PRESENTATION_TEST_IGNORE_USER_SETTINGS=1` isolates the shipped defaults, and
`EIDOLON_PRESENTATION_TEST_EXIT_AFTER_FRAMES=<n>` terminates after a complete startup frame for
selection proofs. Release builds ignore these test hooks.

The Windows legacy fallback delegates character dragging to the native top-level move loop.
Animation and dialogue presentation may pause until release; this is a documented fallback
limitation rather than the cadence expected from `win32_dcomp`.

## 2D character manifest

`config/character.cfg` is strict, versioned, and hot-reloaded. Invalid edits retain the last good
configuration.

It owns:

- character name and portrait asset directory;
- expression count, file, semantic label, and per-expression pixel crop;
- full-body and portrait presentation heights plus default framing;
- lifecycle-to-expression mappings;
- dialogue theme, movement mode, and page hold;
- portrait breathing, sway, semantic accent, posture, speech, and attention strengths.

Expression images share one transparent canvas. A crop is `x, y, width, height` in source pixels and
allows face/bust framing without changing textures. Switching framing preserves the character's
screen-space center.

Expression selection is atomic. There is intentionally no crossfade duration setting and no
previous texture retained after a change. Motion accents are controlled independently by
`motion.accent_strength` and `motion.accent_duration_ms`.

Dialogue modes are:

- `follow`: type continuously and shift the five-line viewport by one line at its lower edge;
- `paged`: hold, then replace the complete five-line page automatically;
- `manual`: replace the complete page only after a click.

## 3D motion tuning

`config/motion.cfg` is a strict transactional calibration file. Unknown, duplicate, missing,
malformed, or out-of-range values reject the complete edit; the running last-good configuration
survives.

Current keys control:

- deterministic `seed`;
- bind-axis diagnostics `neutral.arm_lower_deg` and `neutral.elbow_add_deg`;
- idle breathing period and chest/neck counter-rotation;
- slow sway period and spine/chest/head rotations.

Units are part of key names. Arm lowering accepts `-45..90` degrees, elbow addition accepts
`-90..90`, and idle rotation amplitudes accept `-15..15`. The legacy Character > 3D Model target
sliders and `src/pose.c` presets are Rio diagnostics, not the EPR/VRM semantic calibration path.

Press `F5` to force-reload character and motion configuration even when file timestamps or hashes
have not changed. This shortcut requires the legacy SDL presentation window to own keyboard focus;
the no-activate native host deliberately does not register a global hotkey.

## EPR and local VRM development

The first EPR body is a manually acquired local reference asset for an experimental rigged-3D
system. It is separate from the portrait and sprite systems, and it is not a general VRM loader.
Eidolon cannot redistribute the asset and does not implement VRoid Hub/Pixiv authentication. Sign
in through the
[reference model's VRoid Hub page](https://hub.vroid.com/characters/61437424751231571/models/3310288597351780654),
accept the current terms, download the VRM 1.0 file, and run the current structural/profile
preflight:

```powershell
make vrm-structure-check VRM_PATH="C:\local-assets\character.vrm"
make vrm-runtime-check VRM_PATH="C:\local-assets\character.vrm"
make vrm-performance-review VRM_PATH="C:\local-assets\character.vrm"
```

Debug builds then expose an intentionally non-persistent body override:

```powershell
$env:EIDOLON_BODY_RENDERER = "model_3d"
$env:EIDOLON_VRM_PATH = "C:\local-assets\character.vrm"
$env:EIDOLON_PRESENTATION_BACKEND = "sdl_window_legacy"
.\build\windows\eidolon.exe
```

`EIDOLON_BODY_RENDERER` accepts `sprite`, `portrait`, or `model_3d`. It is ignored by release
builds, does not write user settings, and does not change the shipped portrait default.
`EIDOLON_VRM_PATH` supplies the local experimental reference asset only when the existing 3D
renderer is initialized. It is a development override, not an arbitrary-avatar support promise.
Without it, legacy Rio 3D remains available but EPR has no VRM body profile and stays inactive. The
first EPR path recognizes VRM 1.0 metadata within its supported reference-avatar slice; legacy VRM
0.x files are rejected rather than guessed into the new contract. Third-party VRM assets remain
local and separately licensed.

When a VRM loads, Eidolon measures its mapped humanoid bind positions, segment lengths,
proportions, and anatomical frame. It then looks for a versioned semantic calibration sidecar at
`<VRM path>.epr-calibration`. Set `EIDOLON_VRM_CALIBRATION_PATH` to select a different sidecar:

```powershell
$env:EIDOLON_VRM_CALIBRATION_PATH = "C:\local-assets\character.epr-calibration"
```

Sidecars bind to an anatomy fingerprint, may contain only the anchors calibrated so far, and are
loaded transactionally. A missing or stale file leaves the VRM uncalibrated and does not disable
geometry, projection, another body renderer, or session handling. Create or update one inside the
actual runtime fixture with:

```powershell
make vrm-calibrate VRM_PATH="C:\local-assets\character.vrm"
```

The command freezes named anchors, applies task-space edits immediately through scratch projection,
and atomically saves accepted anchors. A matching sidecar with a neutral right-arm anchor enables
ordinary EPR playback. Present state/gesture anchors compile into body-relative programs; missing
anchors degrade only their behavior family and never select the synthetic fixture poses. See
[Procedural motion](design/procedural-motion.md) for the format and anchor vocabulary.

An unavailable or invalid configured path emits the acquisition page and validation command and
leaves the already initialized portrait active. EPR starts only after the supported reference asset
passes the current preflight and publishes its preliminary body profile; later runtime creation can
still fail locally. EPR or VRM failure does not stop IPC, configured session sources, or the session
registry. Neither Make nor the runtime downloads the file or handles Pixiv credentials.

`make vrm-structure-check` verifies only the structural/profile preflight; `make vrm-check` remains
its compatibility alias. `make vrm-runtime-check` then loads and projects the complete deterministic
scene through the real geometry, texture, skinning, shader, and hidden GPU path. The visible review
target loops the complete five-second scene between one-second idle and settled holds until the
owner closes it or presses Escape. It exists for acting judgement. Neither target establishes
executable authored gaze or general VRM 1.0 compatibility. Those gates are defined by the
[experimental VRM reference-body contract](design/vrm-body-runtime.md).

## Agent adapters

`config/providers.cfg` owns opt-in live transports and independent legacy fallbacks. The Codex
relay, passive Codex app-server client, OpenCode SSE client, transcript reader, and hook IPC each
have separate switches. The filename and source symbols retain the legacy `provider` name. In
product terminology, each configured transport is a session source using the corresponding agent
adapter; neither is a model provider or body renderer. Source configuration is currently read at
startup rather than hot-reloaded. See [Integrations](integrations.md) for supported topologies and
launch commands.
