# Configuration

Eidolon separates shipped defaults, character definitions, shared motion sources, optional model
overrides, and personal preferences. Those layers have different owners and must not collapse.

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
Windows, sprite, portrait, and 3D bodies normally select `win32_dcomp`. Native host, graphics, or
environment-bootstrap failure selects `sdl_window_legacy` and records the exact reason. Explicit
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
Invalid values are logged and ignored. The preference UI continues to show the persisted choice
rather than rewriting it from a temporary environment override.

The DirectComposition backend supports sprite, portrait, dialogue, and direct D3D11 rigged-3D layers;
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
$env:EIDOLON_VRMA_PATH = "C:\local-assets\idle.vrma"
$env:EIDOLON_PRESENTATION_BACKEND = "sdl_window_legacy"
.\build\windows\eidolon.exe
```

`EIDOLON_BODY_RENDERER` accepts `sprite`, `portrait`, or `model_3d`. It is ignored by release
builds, does not write user settings, and does not change the shipped portrait default.
`EIDOLON_VRM_PATH` supplies the local experimental reference asset only when the existing 3D
renderer is initialized. `EIDOLON_VRMA_PATH` optionally supplies a local VRM Animation 1.0 clip;
after validation it loops in-place as the imported base beneath the latest EPR control. Invalid
clips fail locally and do not disable the renderer. These are development overrides, not an
arbitrary-avatar support promise. Without a VRM path, legacy Rio 3D remains available but EPR has
no VRM body profile and stays inactive. The first EPR path recognizes VRM 1.0 metadata within its
supported reference-avatar slice; legacy VRM 0.x files are rejected rather than guessed into the
new contract. Third-party VRM and VRMA assets remain local and separately licensed.

`EIDOLON_VRMA_PATH` controls imported base-motion playback; it does not label a clip as an EPR
semantic generator. Separately, `make vrma-idle-fixture` fetches and hash-verifies the pinned MIT
idle in ignored build storage. A build that finds that exact fixture registers it immutably as the
first `idle.neutral` semantic binding. Missing active posture or gesture semantics fall back as a
complete unit instead of relabeling arbitrary local animation or publishing a partial frame.

When a VRM loads, Eidolon measures its mapped humanoid bind positions, segment lengths, authored
rest frames, proportions, and anatomical frame. Automatic retargeting converts shared normalized
motion from its source T-pose into those destination frames. The product contract requires baseline
playback without a calibration sidecar.

R1 exposes `make vrma-sampler-check` for deterministic fixtures. Use `make vrma-check
VRMA_PATH=...` to inspect a real VRM Animation file. R2 exposes `make vrm-retarget-check` for
destination rest-frame conversion, optional-role composition, hips/root policy, and rollback.
R3 is complete and `make vrm-playback-check` verifies lifecycle, deterministic failure, and atomic
imported-base/EPR composition. Its pinned idle and walk passed the hidden gate and owner-visible
review on the current private development body. R4's `make humanoid-pose-check` verifies the first
masked normalized-motion composition boundary; follow the durable [automatic-retargeting
workstream](workstreams/epr-automatic-retargeting.md).

The existing semantic calibration loader and editor remain available as optional package-author
and residual-repair tooling. To select an explicit sidecar:

```powershell
$env:EIDOLON_VRM_CALIBRATION_PATH = "C:\local-assets\character.epr-calibration"
```

`make vrm-calibrate VRM_PATH="C:\local-assets\character.vrm"` opens the legacy reference-fixture
authoring path. Sidecars bind transactionally to an anatomy fingerprint and may remain partial,
but a missing or stale sidecar must not become an ordinary playback failure after imported motion
integration lands. See [Procedural motion](design/procedural-motion.md) for the existing optional
format and historical anchor vocabulary.

An unavailable or invalid configured path emits the acquisition page and validation command and
leaves the already initialized portrait active. EPR starts only after the supported reference asset
passes the current preflight and publishes its preliminary body profile; later runtime creation can
still fail locally. EPR or VRM failure does not stop IPC, configured session sources, or the session
registry. Neither Make nor the runtime downloads the file or handles Pixiv credentials.

`make vrm-structure-check` verifies only the model structural/profile preflight; `make vrm-check`
remains its compatibility alias. `make vrm-runtime-check` exercises the real geometry, textures,
skinning, projection, shaders, and hidden GPU path through the existing calibrated fixture. Its
current eight-anchor requirement is legacy regression evidence, not automatic-retargeting
acceptance. `make vrma-sampler-check` and `make vrma-check VRMA_PATH=...` cover the R1 import
boundary; `make vrm-retarget-check` covers the standalone R2 destination transaction.

The visible review target still exists for acting judgement. General VRM 1.0 compatibility requires
the no-sidecar, multi-body gates in the [automatic-retargeting
workstream](workstreams/epr-automatic-retargeting.md) and the
[experimental VRM reference-body contract](design/vrm-body-runtime.md).

## Agent adapters

`config/providers.cfg` owns opt-in live transports and independent legacy fallbacks. The Codex
relay, passive Codex app-server client, OpenCode SSE client, transcript reader, and hook IPC each
have separate switches. The filename and source symbols retain the legacy `provider` name. In
product terminology, each configured transport is a session source using the corresponding agent
adapter; neither is a model provider or body renderer. Source configuration is currently read at
startup rather than hot-reloaded. See [Integrations](integrations.md) for supported topologies and
launch commands.
