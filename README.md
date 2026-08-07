# Eidolon

**The embodiment layer for your agent.**

Eidolon gives existing agents a native, persistent presence while real work happens.

The terminal remains the interface to the work.
Eidolon becomes the interface to the worker.

Today, Eidolon observes Codex sessions and turns their real activity into dialogue, expression, and
motion. An OpenCode SSE adapter is implemented and transport-tested, but its ordinary local
end-to-end workflow is not yet accepted. Eidolon does not replace the agent runtime or the
terminal.

![Bunny Asuna presenting two active Codex sessions](screen0.png)

V1 proves that an agent doing real work can visibly feel like one persistent persona. See the
[canonical V1 goal](docs/v1-goal.md) for the acceptance sequence.

## What works

- three body renderers: v2 sprite atlases, full-canvas 2D portraits, and skinned 3D models;
- one independently scrolling dialogue bubble per active agent session, with stable placement
  and real session titles;
- normalized agent adapters, a transport-tested OpenCode SSE path, a live in-path Codex CLI relay,
  and optional completion-only Codex transcript and hook fallbacks;
- semantic expression planning over stable streamed prefixes, deterministic delivery timing,
  completion repair, and a local GoEmotions worker with lifecycle fallback;
- Unicode dialogue through SDL_ttf with bundled MesloLGS Nerd Font Mono and Windows CJK/emoji
  fallbacks;
- procedural portrait acting: breathing, semantic posture, speech beats, attention, and damped
  motion accents;
- atomic expression swaps—no crossfade or previous-frame ghosting;
- pixel-exact click-through on Windows and a separate Dear ImGui settings window;
- DirectComposition portrait/dialogue presentation by default on Windows, with persisted
  `sdl_window_legacy` compatibility selection and explicit capability/failure fallback;
- a native D3D11 3D path with GLB loading, GPU skinning, semantic poses, and analytic arm IK;
- an experimental EPR/VRM vertical slice for one supported reference avatar through the legacy SDL
  3D path, including anatomy measurement and versioned semantic-calibration profiles; this is not
  general VRM 1.0 compatibility;
- hidden snapshot commands for visual QA without stealing focus.

The 2D portrait director and EPR/VRM runtime are separate body-performance systems. They coexist in
the same application and may consume shared body-neutral evidence, but neither owns the other's
expression labels, motion/pose state, or assets. The portrait remains the default and does not run
through EPR.

Windows is the active implementation target. Linux support exists, but currently follows the
legacy SDL_GPU path and may lag behind Windows features.

## Quick start

Requirements:

- LLVM/Clang, GNU Make, CMake, and Ninja;
- a MinGW-w64 toolchain supplying the Windows headers and import libraries used by Clang;
- a Windows SDK containing `fxc.exe`;
- the pinned dependency trees under `lib/`.

Initialize dependency submodules after cloning:

```powershell
git submodule update --init --recursive
```

On Windows, the normal build configures the pinned SDL3 and SDL3_ttf submodules with their
upstream CMake projects, then installs the generated headers, libraries, and DLLs under the ignored
`.cache/sdl` tree. Eidolon itself remains a GNU Make build. Build the dependency layer explicitly
when useful, or let the first ordinary build do it:

```powershell
make sdl-deps
make
./build/windows/eidolon.exe
```

Install the optional local expression classifier and verify the complete build with:

```powershell
make affect-setup
make affect-check
make check
```

`make text-setup` remains a compatibility alias for `make sdl-deps`. SDL source arrives only through
`git submodule update`; ordinary builds do not download it. `make affect-setup` remains the explicit,
checksum-verified setup step for the optional classifier.

The optional EPR/VRM performance body is also not downloaded or redistributed by Eidolon. Sign in
to VRoid Hub with Pixiv, manually acquire the
[reference VRM 1.0 model](https://hub.vroid.com/characters/61437424751231571/models/3310288597351780654),
and run the current structural/profile preflight before using it:

```powershell
make vrm-structure-check VRM_PATH="C:\local-assets\character.vrm"
make vrm-runtime-check VRM_PATH="C:\local-assets\character.vrm"
make vrm-calibrate VRM_PATH="C:\local-assets\character.vrm"
make vrm-performance-review VRM_PATH="C:\local-assets\character.vrm"
$env:EIDOLON_VRM_PATH = "C:\local-assets\character.vrm"
# Optional once a matching calibration sidecar exists:
$env:EIDOLON_VRM_CALIBRATION_PATH = "C:\local-assets\character.epr-calibration"
```

This development checkout currently selects `assets/2349235869624830263.vrm` (Vampire Cat by
Touko Asada) as its compiled 3D default. The file remains ignored and local. Its embedded metadata
permits avatar use only by the author, prohibits redistribution and modification, and requires
credit; this selection is therefore a private owner-directed test fixture, not a distributable
Eidolon asset or public reference-model recommendation. `EIDOLON_VRM_PATH` still overrides it.

The linked DECAGRAMMATON model page currently permits avatar use but forbids redistribution and modification and
requires credit. Do not add the downloaded file to this repository. Eidolon never receives or
stores Pixiv credentials.

`make vrm-structure-check` is the schema/profile preflight; `make vrm-check` remains its compatibility
alias. `make vrm-runtime-check` drives the complete five-second fixture through buffer loading,
geometry, textures, skinning, projection, shaders, and a hidden GPU frame. Passing both proves the
selected reference path on that machine, not arbitrary VRM compatibility. The visible review target
opens the native desktop 3D path for owner judgement. It repeats a one-second idle pre-roll, the complete
five-second performance, and a one-second settled hold until the command is stopped with Ctrl+C.
`make vrm-calibrate` freezes the same deterministic fixture at eight named semantic
anchors. Adjust the task-space torso, head, hand, elbow-pole, and wrist controls, accept each anchor,
and use **save sidecar**; stop the command with Ctrl+C when finished. The
current hard-coded performance poses remain provisional test stimuli until the next compiler slice
derives the complete performance from the approved sidecar. The remaining compatibility gates stay in the
[VRM reference-body contract](docs/design/vrm-body-runtime.md).

The bundled Bunny Asuna manifest expects ten transparent portraits under
`assets/characters/asuna-bunny/portraits`. Extracted game art and the Rio source rip are deliberately
excluded from Git; a fresh checkout without those assets still retains the reusable engines and
fallback sprite path.

## Use

- left-drag the character to move Eidolon;
- left-click a dialogue bubble to advance manual dialogue;
- right-click the character to open settings; `F1` is also available when an Eidolon SDL window
  owns keyboard focus;
- middle-drag a 3D model to rotate yaw/pitch;
- hold `Shift` while middle-dragging to rotate roll;
- use the mouse wheel over a 3D model to resize its transparent overlay without cropping it;
- double middle-click to reset 3D rotation and overlay size;
- press `F5` to reload character and motion configuration when the legacy SDL pet window owns
  keyboard focus;
- press `Escape` to quit when the legacy SDL pet window owns keyboard focus. The no-activate native
  host deliberately does not capture global keyboard shortcuts.

On Windows, portrait and 3D bodies normally use `win32_dcomp`. Sprite bodies, explicit compatibility
selection, and native startup failure select `sdl_window_legacy` with a logged reason. The legacy
backend delegates dragging to the native top-level move loop, which can pause animation and dialogue
presentation until the mouse button is released; presentation resumes after the drag.
The visible VRM calibration and performance-review commands use the same borderless, transparent
DirectComposition body target as the desktop runtime. The VRM renderer submits directly into that
target, publishes a projected animated-mesh input mask without GPU readback, and receives wheel and
captured middle-drag input through the shared presentation event boundary. Snapshots and explicit
compatibility selection retain the SDL backend.

Settings persist as sparse per-user overrides. Every field can return to its shipped or
character-defined default without freezing a copy of that default into the user file. Presentation
preference changes apply at the next launch.

## Design

The current runtime is:

```text
configured Codex / OpenCode session source
                    ↓
vendor-specific agent adapter
                    ↓
normalized source + session events
                    ↓
session registry
                    ↓
lifecycle state + semantic expression + delivery cues
                    ↓
selected sprite | portrait | 3D body renderer
                    ↓
renderer-neutral scene + body/dialogue content
                    ↓
native-preferred win32_dcomp portrait/3D | explicit/capability sdl_window_legacy fallback
                    ↓
transparent desktop presentation + native hit testing
```

The target stack separates body/dialogue rendering from native presentation so compositor layers can
move and fade without rerendering their content. See the
[native presentation and graphics plan](docs/design/native-presentation.md). Body renderers never
own conversation semantics; presentation backends never own session state. Slow language decisions
stay separate from frame-rate motion and drawing.

Portrait/sprite realization and EPR/rigged-3D realization remain independent branches between the
shared evidence and scene boundaries. Renderer-neutral evidence enables coherent character behavior
across bodies; it does not collapse them into one implementation.

## Documentation

- [Documentation index](docs/README.md)
- [Product brief](docs/product-brief.md)
- [V1 goal](docs/v1-goal.md)
- [Product roadmap](docs/product-roadmap.md)
- [Current project state](docs/project-state.md)
- [Architecture](docs/architecture.md)
- [Building, testing, and debugging](docs/development.md)
- [Configuration](docs/configuration.md)
- [Agent adapters and session integration](docs/integrations.md)
- [Character and asset pipeline](docs/assets.md)
- [Design specifications](docs/design/README.md)
- [Native presentation and graphics plan](docs/design/native-presentation.md)
- [Experimental VRM reference-body contract](docs/design/vrm-body-runtime.md)
- [Backend-neutral presentation event contract](docs/design/presentation-events.md)
- [Presentation environment and output topology contract](docs/design/presentation-environment.md)

Eidolon is an early-stage native project with a working Windows development path; the
[daily-driver alpha](docs/product-roadmap.md#gate-a-daily-driver-alpha) is the next product gate,
not a capability already claimed. Character assets remain the responsibility of the local user and
are not part of the reusable engine repository.

## License

Public domain. Do whatever.

This applies to Eidolon's original work only. Vendored dependencies and local character assets
retain their respective terms.
