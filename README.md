# Eidolon

**Native agent embodiment, powered by the Eidolon Performance Runtime.**

Eidolon gives working agents a persistent desktop presence. The terminal remains the interface to
the work; Eidolon becomes the interface to the worker.

The centerpiece is **EPR**, a deterministic character-performance runtime written in C. It turns
accepted intent into timed behavior, arbitrates ownership of the body, composes motion, and handles
interruption without replaying completed gestures or publishing half-valid poses. A VRM adapter
retargets normalized humanoid motion into an avatar's authored skeleton; Windows DirectComposition
puts the result on the desktop.

This repository contains the runtime, native host, animation tools, design contracts, and executable
regression tests—not just a character viewer.

[Engineering walkthrough](docs/epr-engineering.md) ·
[EPR architecture](docs/design/epr-overview.md) ·
[Implementation status](docs/project-state.md) ·
[Build guide](docs/development.md)

## EPR: the engineering focus

- **Deterministic behavior.** Versioned intent, plan generations, fixed logical ticks, explicit
  phases, and causal traces separate behavior decisions from frame timing.
- **Resource-owned motion.** Posture, gesture, gaze, and procedural arm control receive explicit
  body-resource grants. Sampled channels are narrowed to actual ownership before composition.
- **Automatic retargeting.** Owned VRMA tracks, a 55-role humanoid vocabulary, authored-rest
  conversion, optional-bone composition, and scaled root motion support shared animation without
  mandatory per-model pose calibration.
- **Transactional pose publication.** Imported animation, normalized EPR motion, optional residuals,
  and procedural owners compose in scratch state. Invalid candidates retain the last valid state.
- **Interruption and continuity.** Completed phases cannot replay; right-arm settling captures the
  actual outgoing pose once per behavior and blends into the next pose.
- **Provenance-aware motion tools.** Pinned, hash-verified captures become exact reviewed frame
  slices. Motion packs own their clips and publish complete binding batches atomically.

The interesting boundary is between *what a character should communicate* and *how a particular
skeleton can perform it*. EPR does not parse agent transports, own dialogue, call the graphics API,
or move native windows. Those remain separate systems.

### What is implemented—and what is not

| Area | Current evidence |
| --- | --- |
| Behavior runtime | Deterministic planning, resource arbitration, realization, interruption, rollback, and trace tests |
| Automatic animation | Sidecar-free idle and walk accepted on the development reference body; parser-to-hidden-GPU checks |
| Semantic motion composition | Catalog, executor, motion-pack ownership, procedural overlays, and dynamic right-arm targets implemented |
| Motion vocabulary | Pinned idle binding available; conversation capture review/slicing implemented; first bounded semantic gesture not yet accepted |
| Native presentation | Windows D3D11/DirectComposition path shared by sprite, portrait, and 3D; captured dragging, transparent composition, and fallback checks |
| End-to-end EPR input | Deterministic synthetic fixture today; dependable live source/session-to-EPR integration remains future work |

EPR is the active engineering focus, not a claim of finished production animation. VRM support is
an experimental reference-body subset of **VRM 1.0**, not arbitrary-avatar compatibility. VRM 0.x,
full MToon shading, spring bones, constraints, contact/balance planning, a complete gesture library,
and broader multi-body acceptance remain unfinished. The renderer's current material path is a
documented base-color fallback.

See the [automatic-retargeting contract](docs/workstreams/epr-automatic-retargeting.md) for completed
R1–R3 work and the active R4 semantic-motion milestone.

## Inspect the implementation

Start with the [EPR engineering walkthrough](docs/epr-engineering.md): it explains the design
tradeoffs and links each mechanism to its implementation and regression evidence.

A short source tour:

| Concern | Entry point |
| --- | --- |
| Runtime orchestration and accepted state | [performance_runtime.c](src/epr/performance_runtime.c) |
| Behavior phases and interruption | [behavior_plan.c](src/epr/behavior_plan.c) |
| Resource arbitration | [body_resources.c](src/epr/body_resources.c) |
| Fixed-tick motion composition | [motion_execution.c](src/epr/motion_execution.c) |
| Clip lifetime and atomic catalog publication | [semantic_motion_pack.c](src/semantic_motion_pack.c) |
| Source/destination rest conversion | [vrma_motion_source.c](src/vrma_motion_source.c), [vrm_retarget.c](src/vrm_retarget.c) |
| Whole-pose projection | [vrm_projection.c](src/vrm_projection.c) |
| Native desktop composition | [windows_dcomp.cpp](src/platform/windows_dcomp.cpp) |

## Build and verify

Windows is the actively validated platform. Install Git, LLVM/Clang, GNU Make, Python 3, CMake,
Ninja, MinGW-w64 headers/import libraries, and a Windows SDK containing `fxc.exe`.
See [toolchain setup](docs/development.md) for paths and overrides.

```powershell
git clone --recurse-submodules https://github.com/snoopiedoom/eidolon.git
cd eidolon
gmake MODE=release
gmake check
gmake epr-trace
```

Use `make` if that is your GNU Make executable's name. Eidolon itself is built with GNU Make;
upstream CMake builds are confined to dependencies. Pinned SDL3/SDL3_ttf sources arrive through
submodules, and the first Windows build installs their generated outputs under ignored
`.cache/sdl`. Ordinary builds do not download SDL sources.

`gmake check` runs the ordinary regression suite without a private VRM or model inference.
`gmake epr-trace` prints causal JSONL from the synthetic five-second EPR fixture without opening a
window. It proves runtime behavior, not the visual quality of a completed motion pack.

### Try automatic animation on a local VRM

Supply a VRM 1.0 model you are permitted to use. No private development VRM is included or
downloaded by these commands. First validate the supported renderer subset, then fetch the pinned
idle motion into ignored build storage:

```powershell
gmake vrm-structure-check VRM_PATH="C:/local-assets/character.vrm"
gmake vrma-idle-fixture
gmake vrm-animation-runtime-check VRM_PATH="C:/local-assets/character.vrm" VRMA_PATH="build/fixtures/vrma/standard-idle/standard_idle.vrma"
gmake vrm-animation-review VRM_PATH="C:/local-assets/character.vrm" VRMA_PATH="build/fixtures/vrma/standard-idle/standard_idle.vrma"
```

The animation runtime check exercises five seconds of sidecar-free sampling, retargeting,
projection, skinning, shaders, and hidden endpoint GPU frames. The visible review loops until
closed and uses an optimized release build by default.

Middle-drag rotates; Shift + middle-drag rolls; the wheel resizes the transparent overlay;
double middle-click resets the view. Drag capture continues beyond the rendered character.
Unsupported models should fail preflight rather than silently claim compatibility.

The older `vrm-performance-review` / `vrm-runtime-check` commands exercise the calibrated
reference-body fixture. They are retained regression tools, not the automatic-retargeting
acceptance path. Manual calibration is optional advanced authoring, not normal onboarding.

## One native host, independent bodies

EPR coexists with the 2D portrait director and sprite playback. They share session/dialogue,
selection, scene, and presentation boundaries—not pose state, expression labels, or one universal
animation runtime. The portrait remains the shipped default while EPR matures.

The desktop application also provides independently timed session bubbles, Unicode dialogue,
streamed-prefix expression planning, a live Codex CLI relay, and optional transcript/hook
fallbacks. The OpenCode SSE adapter is transport-tested; its ordinary end-to-end workflow has not
been accepted. These integration capabilities do not imply that live sessions already drive the
new EPR motion vocabulary.

The [existing 2D desktop screenshot](screen0.png) shows this separate portrait/session path, not an
EPR demonstration. On Windows, all three bodies use DirectComposition by default, with an explicit
or failure-selected SDL fallback. Linux currently follows the legacy SDL graphics path; native
Wayland/X11 and macOS presentation are not implemented.

`gmake body-host-check` exercises Windows body switching and native/SDL recovery.
The [development guide](docs/development.md), [configuration reference](docs/configuration.md), and
[integration guide](docs/integrations.md) cover the rest of the application.

## Documentation

- [EPR engineering walkthrough](docs/epr-engineering.md): problem, architecture, code, and evidence.
- [EPR contracts](docs/design/epr-overview.md): intent, plans, resources, realization, and projection.
- [Automatic humanoid motion](docs/workstreams/epr-automatic-retargeting.md): active implementation contract.
- [Motion-pack authoring](docs/design/epr-motion-pack.md): capture provenance, review, and atomic binding.
- [Project state](docs/project-state.md): completed work, limitations, and next checkpoints.
- [Product brief](docs/product-brief.md), [V1 goal](docs/v1-goal.md), and [roadmap](docs/product-roadmap.md): product direction and release gates.
- [Full documentation index](docs/README.md).

## License and assets

The project's existing notice dedicates Eidolon's original work to the public domain.
Dependencies, motion sources, fonts, and character assets retain their own terms. The project
license does not grant rights to third-party characters.

Private development VRMs and newly downloaded character assets remain local and are not part of
this publication. Legacy third-party Rio source material already exists in the repository/history;
do not mistake it for original Eidolon work or a freely licensed reference character. See
[asset provenance and boundaries](docs/assets.md). No installer-grade package or generally
redistributable 3D reference character is claimed.
