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
through the supported reference VRM, and captures the deterministic legacy snapshot path. The
visible calibration and review commands use the native desktop target. This does not make 3D the
default, claim arbitrary VRM compatibility, or route the portrait renderer through EPR.
The VRM must first be acquired manually and pass `make vrm-structure-check VRM_PATH=...`; set
`EIDOLON_VRM_PATH` before invoking the snapshot. The runtime never performs VRoid Hub/Pixiv
authentication or asset download.

Use `make vrm-performance-review VRM_PATH=...` to open the stable reference-body camera, play the
complete scene in real time, and hold the final settled pose for one second. The manual harness
then starts another pass after a one-second idle pre-roll; it remains open until the owner stops
the command with Ctrl+C. Stopping before one complete five-second pass reports failure.

The first DECAGRAMMATON review accepted this camera/rig/harness path and rejected the provisional
hard-coded pose authorship. Do not tune those poses from screenshots. The active workflow is to
measure the loaded humanoid, freeze the live EPR sequence at named semantic anchors, let the owner
adjust task-space handles, and save a matching `<model>.epr-calibration` sidecar. The measurement
and sidecar parser, frozen-anchor session, live task-space editor, deterministic serializer, and
atomic save are implemented. Run `make vrm-calibrate VRM_PATH=...`. The calibrated-program compiler
is next; loading or authoring a sidecar does not yet replace the provisional fixture endpoints.
The calibration body runs in a visible authoring mode on the same borderless transparent
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
vrm-structure-check  implemented: schema, humanoid semantics, metadata, capability truth
vrm-runtime-check    implemented: buffers, geometry, textures, skinning, projection, shaders,
                     the complete five-second fixture, and a hidden presented GPU frame
vrm calibration      implemented authoring slice: measurements, anatomy fingerprint, named fixture
                     capture, live task-space projection, partial sidecar parse/write, atomic save
```

Passing both targets is evidence for the selected reference body on the current machine; it is not
a broad compatibility claim. Retain the focused unit tests, deterministic EPR trace, hidden
performance snapshots, and explicit owner-controlled visible review. The
[VRM reference-body contract](design/vrm-body-runtime.md) owns the required adversarial fixture
corpus and compatibility gates.

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
