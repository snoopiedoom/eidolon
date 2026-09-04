# Building and developing Eidolon

Eidolon uses C17, LLVM/Clang, GNU Make, and SDL3. Upstream build systems may be used inside
quarantined dependency/tool builds, but Eidolon itself does not use CMake.

## Dependencies

Windows requires:

- LLVM/Clang, GNU Make, CMake, and Ninja;
- a MinGW-w64 toolchain supplying Windows headers, import libraries, and the GNU C++ runtime;
- the Windows SDK's x64 `fxc.exe` for D3D11 shaders;
- Blender for the 3D authoring pipeline only;
- initialized dependency trees under `lib/`; substantial upstream libraries use pinned Git
  submodules.

Initialize submodules after cloning or pulling a revision that changes them:

```powershell
git submodule update --init --recursive
```

SDL 3.4.12 and SDL_ttf 3.2.2 are pinned submodules. On Windows, the first build configures their
upstream CMake projects with Ninja and installs the resulting development/runtime tree under
`.cache/sdl/install`. CMake is quarantined to these upstream dependencies; Eidolon itself remains a
GNU Make build. The dependency layer can also be prepared explicitly:

```powershell
make sdl-deps
```

Use `make sdl-clean` to discard only the generated SDL dependency build and install trees. The next
`make` reconstructs them from the pinned submodules. `make text-setup` remains a compatibility alias
for `make sdl-deps`.

The Windows Clang build targets `x86_64-w64-windows-gnu` by default, allowing the LLVM release
archive to use an installed MinGW-w64 SDK instead of requiring MSVC headers and libraries. Override
the target used by both Make and the SDL CMake build only when deliberately changing ABI:

```powershell
make WINDOWS_CLANG_TARGET=x86_64-w64-windows-gnu
```

Submodule initialization is the only SDL source acquisition step. Ordinary builds never download
SDL or SDL_ttf.

Linux discovers SDL3 and SDL_ttf through `pkg-config`. Its renderer currently follows the legacy
SDL_GPU/shadercross path.

## VS Code and clangd

The repository configures VS Code to use clangd and disables the Microsoft C/C++ IntelliSense
engine. Install the recommended `vscode-clangd` extension when VS Code offers it, then generate the
local compilation database:

```powershell
make editor-config
```

`compile_commands.json` is produced from a dry run of the real Make recipes, so clangd receives the
same language standards, preprocessor definitions, SDL3/SDL_ttf paths, and dependency include
paths as Clang. The generated file contains machine-local absolute paths and is intentionally
ignored by Git. Regenerate it after changing compiler flags, dependency roots, build mode, or
machines; use `make editor-config MODE=release` when release-only definitions matter.

## Build and test

```powershell
make                       # debug build and stable launch path
make MODE=release          # optimized mode-specific build
make check                 # ordinary unit/regression suite
make body-host-check       # Windows live-body, DirectComposition recovery, and SDL fallback matrix
make imgui-smoke           # generated C API + SDL backends
make affect-check          # worker inference + async client
make affect-benchmark      # cold/warm beat inference profile
make epr-trace             # deterministic first-slice causal JSONL on stdout
make log                   # tail the Windows debug log
```

Mode-specific binaries and objects live under `build/<platform>/bin/<mode>` and
`build/<platform>/obj/<mode>`. A plain `make` copies its chosen binary and runtime DLLs to the stable
`build/windows/eidolon.exe` launch path. Debug and release outputs never share objects or shader
blobs.

Shaders are authored in HLSL. Windows discovers the newest x64 `fxc.exe` and writes Shader Model 5.0
DXBC. Override discovery with `FXC=/path/to/fxc.exe`. The compiler is a build tool, not a runtime
dependency.

Before handing off a code change, run checks proportional to its risk. The normal complete gate is:

```powershell
make check
make body-host-check
make affect-check
make build/windows/bin/debug/eidolon.exe
make MODE=release build/windows/bin/release/eidolon.exe
git diff --check
```

Do not launch the visible overlay as an automated verification step. Interactive feel belongs to
the user.

## Non-intrusive visual QA

Snapshot commands create a hidden window, disable overlay/session side effects, render one PNG, and
exit:

```powershell
./build/windows/eidolon.exe --snapshot build/windows/qa.png
./build/windows/eidolon.exe --snapshot-dialogue build/windows/qa-dialogue.png "unicode text"
./build/windows/eidolon.exe --snapshot-face build/windows/qa-face.png
./build/windows/eidolon.exe --snapshot-settings build/windows/qa-settings.png
./build/windows/eidolon.exe --snapshot-sessions build/windows/qa-sessions.png
./build/windows/eidolon.exe --snapshot-portrait-motion 1 120 build/windows/qa-expression.png
./build/windows/eidolon.exe --snapshot-pose 1 build/windows/qa-pose.png
./build/windows/eidolon.exe --snapshot-resolution 2048 build/windows/qa-2048.png
./build/windows/eidolon.exe --snapshot-performance 3520 build/windows/qa-epr-peak.png
```

`--snapshot-performance` accepts a fixed logical tick from 0 through 5000 milliseconds in 20 ms
increments. It drives the synthetic EPR evidence fixture, projects the resulting canonical control
through the selected reference VRM, and captures the deterministic legacy snapshot path. This does
not make 3D the default, claim arbitrary VRM compatibility, or route the portrait renderer through
EPR. The VRM must be acquired manually and pass `make vrm-structure-check VRM_PATH=...`; the runtime
never performs VRoid Hub/Pixiv authentication or asset download.

`make vrm-performance-review VRM_PATH=...` still opens the native reference-body camera and loops
the existing calibrated five-second fixture. It remains useful for renderer/projection regression,
but its calibration gate is not the new compatibility gate and it does not prove automatic
retargeting.

The first DECAGRAMMATON review accepted the camera/rig/harness and rejected provisional hard-coded
pose authorship. The later manual-anchor experiment proved measurement, transactional sidecars,
scratch projection, and resource-local composition, but manual calibration was rejected as the
ordinary user workflow. The active direction is shared normalized motion, automatic source-to-
destination T-pose conversion, imported base-pose playback, and then EPR layers. R1 includes a
complete 55-role vocabulary, owned VRMA tracks, deterministic STEP/LINEAR/CUBICSPLINE sampling,
and focused GNU Make checks. Run `make vrma-sampler-check`, or preflight a real clip with `make
vrma-check VRMA_PATH=...`; the latter reports exact per-track variation and aggregate humanoid-chain
coverage so a parser fixture cannot be mistaken for a whole-body diagnostic. R2 adds transactional
destination rest-frame conversion, optional-role composition, scaled hips motion, and in-place/full
root policy; run `make vrm-retarget-check`.
R3 adds model-owned clip/player lifetime, deterministic clocked sampling, pause/seek/loop/rate
controls, phase and failure reporting, and atomic imported-base publication before retained EPR;
run `make vrm-playback-check`. Set `EIDOLON_VRMA_PATH` to a local preflighted clip to exercise that
runtime path. `make vrma-idle-fixture` fetches the pinned, hash-verified MIT idle into ignored build
storage. `make vrma-walk-fixture` fetches a pinned CMU neutral-walk BVH and usage-rights file, then
uses the tested deterministic converter to build and preflight a hash-locked 2.5-second VRMA loop.
Run `make vrm-animation-runtime-check VRM_PATH=... VRMA_PATH=...` for the five-second sidecar-free
parser, sampler, retargeter, projection, skinning, shader, and hidden-frame gate. Both pinned clips
pass that gate on Vampire Cat. The owner accepted both idle and walk through the native visible-
review harness, closing R3. R4 begins with renderer-neutral masked pose composition; run
`make humanoid-pose-check` for deterministic layer order, shortest-arc quaternion blending,
unowned-channel preservation, hips ownership, and transactional rejection.
Realization Program version 4 also emits validated semantic generator references. EPR torso, head,
eyes, and arm resources map to disjoint normalized humanoid channels; imported motion keeps legs and
all other unclaimed channels. `make check` covers descriptor names, takeover policies, bounds,
procedural `none` references, disjoint ownership, and invalid-mask rollback.
Run `make epr-motion-catalog-check` for fixed-capacity unique registration, stable source provenance,
typed missing/procedural/incompatible/sample failures, requested/declared/actual ownership
intersection, hips ownership, repeatable normalized samples, and transactional rollback.
Run `make vrma-motion-source-check` for the concrete borrowed-clip adapter: complete clip validation,
authored-rest-invariant normalized rotation, rest-relative hips-height normalization, actual-track
ownership, semantic resource narrowing, loop sampling, malformed-source rejection, and atomic
rollback.
Run `make epr-motion-execution-check` for exact program phase validation, fixed-tick source and
normalized time, minimum-jerk entrances/exits, live-grant narrowing, deterministic base/additive/
override ordering, intensity-scaled residual composition, typed local failures, and whole-frame
rollback. Run `make semantic-motion-pack-check` for atomic multi-binding publication, stable-context
rebasing, ownership transfer, and whole-batch rollback. The
[semantic motion-pack contract](design/epr-motion-pack.md) owns the file/clip lifetime boundary. The
model embeds one pack, registers the pinned verified idle as `idle.neutral` when its ignored fixture
is present, and lends the pack's validated catalog to each EPR runtime.
Complete fixed-tick frames publish through one imported-base/normalized-frame/residual/procedural-
owner transaction; missing active semantics clear stale motion and fall back locally to the complete
accepted controller. Canonical-control version 4 publishes explicit head-gaze deltas, weighted
right-arm IK, tokenized arm continuity, and live procedural resource ownership. Settle carries no
catalog source: projection captures the exact outgoing model-local arm pose once per behavior token,
shortest-arc blends toward normalized motion, then applies weighted IK. The pose and capture commit
together or both roll back. Legacy posture and right-arm anchors cannot leak into a normalized frame.
Runtime lifecycle, all-or-fallback publication, retained-frame replay, destination projection,
continuity, weighted IK, and rollback are covered by `make check`.

`make vrm-animation-review VRM_PATH=... VRMA_PATH=...` opens the same clip on the transparent,
borderless native body target and never times out. Let one complete loop play, then close the window
or press Escape. Middle-drag rotates, Shift+middle-drag rolls, the wheel resizes the character, and
double-middle resets the view. Visible VRM review and calibration targets build and launch optimized
release binaries by default even when ordinary development uses `MODE=debug`; this keeps debug-only
CPU costs from invalidating acting and cadence review. Set `REVIEW_MODE=debug` only when explicitly
diagnosing those tools.

`make vrma-semantic-candidate` reproducibly builds and preflights the full pinned CMU `18_08`
conversation take in ignored storage. The owner accepted the full capture as useful source
material, not as one semantic motion. `make vrma-semantic-candidate-review VRM_PATH=...` now opens
the native range-selection harness: `Space` pauses, arrows scrub, `I`/`O` mark, `R` loops the range,
and `Enter` writes the ignored selection artifact. `make vrma-semantic-slice` validates that
artifact against the pinned BVH cadence, emits exact frame evidence, builds the deterministic VRMA,
and preflights it. `make vrma-semantic-slice-review VRM_PATH=...` reviews the derived clip. The
bounded clip remains unlabeled until that second visible acceptance; it is never substituted for a
missing semantic generator. `make vrma-review-selection-check` and `make bvh-to-vrma-check` cover
range state, strict artifact parsing, duration binding, frame snapping, and generated provenance.

`make vrm-calibrate VRM_PATH=...` remains an optional package-author/debug tool. Its anatomy-bound
sidecars may repair unusual frames, deformation, limits, or contact, but baseline playback must not
depend on them. See [the durable workstream](workstreams/epr-automatic-retargeting.md).

The optional calibration body runs in a visible authoring mode on the same borderless transparent
DirectComposition target as the desktop 3D path. It retains middle-drag/Shift+middle camera rotation;
the wheel resizes the overlay and its model together so zoom cannot crop against a fixed host.
Double-middle resets both rotation and overlay size. The model depth projection encloses the full
bind-volume diagonal so an arbitrary inspection rotation cannot push geometry through the depth
planes. The model draws directly into the native D3D11 target, and its projected mesh supplies
click-through geometry without a normal-frame framebuffer readback.

## VRM verification boundary

The EPR/VRM path is an experimental supported-reference-avatar system separate from the 2D
portrait director. A 3D build, test, or failure must leave portrait tests, assets, and local
performance state independent.

`make vrm-structure-check VRM_PATH=...` runs cgltf structural validation, validates the supported
humanoid hierarchy/scale and matrix restriction, parses truthful capability states, and builds the
body profile. `make vrm-check` remains its compatibility alias.

The executable half uses the product renderer rather than a second importer:

```text
vrm-structure-check implemented: schema, humanoid semantics, metadata, capability truth
bvh-to-vrma-check  implemented fixture tool: deterministic hierarchy collapse and hash-locked GLB
vrma-sampler-check  implemented R1: 55 roles, strict VRMA parsing, owned deterministic sampling
vrma-check          implemented R1: load, validate, and sample a real VRM Animation file
vrm-retarget-check  implemented R2: rest conversion, optional roles, root policy, atomic rollback
vrm-playback-check  implemented R3: lifecycle, scratch retarget, atomic imported-base/EPR publish
vrm-animation-runtime-check implemented R3: sidecar-free full-body idle/walk and hidden GPU frames
epr-motion-catalog-check implemented R4: semantic resolution and sampled channel ownership
vrma-motion-source-check implemented R4: authored-rest normalization and catalog source adapter
epr-motion-execution-check implemented R4: fixed-tick transitions and normalized frame composition
semantic-motion-pack-check implemented R4: atomic owned-clip loading and catalog publication
vrm-runtime-check   implemented legacy gate: calibrated fixture, renderer, projection, hidden frame
vrm calibration     optional authoring/debug: anatomy-bound anchors and residual repair
```

Each target is evidence only for the boundary it names; none alone is a broad compatibility claim.
Retain focused unit tests, deterministic EPR traces, hidden performance snapshots, and explicit
owner-controlled visible review. The [automatic-retargeting
workstream](workstreams/epr-automatic-retargeting.md) owns the implementation sequence, while the
[VRM reference-body contract](design/vrm-body-runtime.md) owns the adversarial fixture corpus and
compatibility gates.

Automated Blender inspection must use `--background`. `make model-mouth-calibrate` is the sole
intentional live exception because it is an explicitly user-operated calibration tool.

## Logs

Windows writes `%LOCALAPPDATA%\Eidolon\eidolon.log`; Linux writes
`${XDG_STATE_HOME:-~/.local/state}/eidolon/eidolon.log`.

Debug expression traces include escaped beat previews plus source offsets, boundary and cue reasons,
top classifier labels, affect axes, raw and stabilized faces, continuity holds, runner-up margin,
inference latency, reveal timing, and the actual portrait-change source. Release builds omit message
previews.

Presentation diagnostics include logical/window/output dimensions, model target size, cap, and
VSync state. Collect that line first when diagnosing desktop-wide lag.

## Working conventions

- use GNU Make for Eidolon;
- keep substantial upstream libraries as pinned submodules rather than importing their source into
  Eidolon's history;
- preserve unrelated or pre-existing dirty changes;
- inspect `git status --short` before modifying repository state;
- do not commit build output, downloads, logs, extracted game assets, or source archives;
- do not seize the desktop, mouse, or focus for visual testing;
- prefer ownership and scheduling fixes over patches that conceal the symptom;
- update the document that owns a changed boundary.
