# Building and developing Eidolon

Eidolon uses C17, LLVM/Clang, GNU Make, and SDL3. Upstream build systems may be used inside
quarantined dependency/tool builds, but Eidolon itself does not use CMake.

## Dependencies

Windows requires:

- LLVM/Clang and GNU Make;
- an SDL3 development package containing `include/SDL3`, `lib/x64/SDL3.lib`, and `SDL3.dll`;
- the Windows SDK's x64 `fxc.exe` for D3D11 shaders;
- Blender for the 3D authoring pipeline only;
- ordinary vendored source directories under `lib/`, never Git submodules.

The Makefile defaults `SDL3_ROOT` to `C:/dev/SDL3`. Override it instead of changing the Makefile:

```powershell
$env:SDL3_ROOT = "D:/sdk/SDL3"
make
```

SDL_ttf is a pinned setup dependency:

```powershell
make text-setup
```

This downloads and verifies SDL_ttf 3.2.2 into ignored `.cache/sdl_ttf`. The normal build never
downloads dependencies.

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
same language standards, preprocessor definitions, SDL3/SDL_ttf paths, and vendored-library include
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
```

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
- keep dependencies as ordinary vendored source, not submodules;
- preserve unrelated or pre-existing dirty changes;
- inspect `git status --short` before modifying repository state;
- do not commit build output, downloads, logs, extracted game assets, or source archives;
- do not seize the desktop, mouse, or focus for visual testing;
- prefer ownership and scheduling fixes over patches that conceal the symptom;
- update the document that owns a changed boundary.
