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
- hidden snapshot commands for visual QA without stealing focus.

Windows is the active implementation target. Linux support exists, but currently follows the
legacy SDL_GPU path and may lag behind Windows features.

## Quick start

Requirements:

- LLVM/Clang and GNU Make;
- an SDL3 development package;
- a Windows SDK containing `fxc.exe`;
- the pinned dependency trees under `lib/`.

Initialize dependency submodules after cloning:

```powershell
git submodule update --init --recursive
```

On Windows, SDL3 defaults to `C:/dev/SDL3`; override `SDL3_ROOT` when necessary.

```powershell
make text-setup
make
./build/windows/eidolon.exe
```

Install the optional local expression classifier and verify the complete build with:

```powershell
make affect-setup
make affect-check
make check
```

`make text-setup` and `make affect-setup` are explicit, checksum-verified dependency setup steps.
Ordinary builds never download anything.

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
- double middle-click to reset 3D rotation;
- press `F5` to reload character and motion configuration when the legacy SDL pet window owns
  keyboard focus;
- press `Escape` to quit when the legacy SDL pet window owns keyboard focus. The no-activate native
  host deliberately does not capture global keyboard shortcuts.

On Windows, portrait bodies normally use `win32_dcomp`. Sprite and 3D bodies, explicit compatibility
selection, and native startup failure select `sdl_window_legacy` with a logged reason. The legacy
backend delegates dragging to the native top-level move loop, which can pause animation and dialogue
presentation until the mouse button is released; presentation resumes after the drag.

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
native-preferred win32_dcomp portrait | explicit/capability sdl_window_legacy fallback
                    ↓
transparent desktop presentation + native hit testing
```

The target stack separates body/dialogue rendering from native presentation so compositor layers can
move and fade without rerendering their content. See the
[native presentation and graphics plan](docs/design/native-presentation.md). Body renderers never
own conversation semantics; presentation backends never own session state. Slow language decisions
stay separate from frame-rate motion and drawing.

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
