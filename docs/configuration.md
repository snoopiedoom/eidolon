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

The file is sparse and may additionally contain:

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
`-90..90`, and idle rotation amplitudes accept `-15..15`. Use Character > 3D Model semantic target
sliders for pose authoring; raw neutral values are diagnostics, not a semantic pose format.

Press `F5` to force-reload character and motion configuration even when file timestamps or hashes
have not changed.

## Agent adapters

`config/providers.cfg` owns opt-in live transports and independent legacy fallbacks. The Codex
relay, passive Codex app-server client, OpenCode SSE client, transcript reader, and hook IPC each
have separate switches. The filename and source symbols retain the legacy `provider` name. In
product terminology, each configured transport is a session source using the corresponding agent
adapter; neither is a model provider or body renderer. Source configuration is currently read at
startup rather than hot-reloaded. See [Integrations](integrations.md) for supported topologies and
launch commands.
