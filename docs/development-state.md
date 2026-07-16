# Development state and continuation notes

This is the durable handoff for continuing Eidolon in a fresh session. It records decisions and
unfinished work that are easy to lose but too implementation-specific for the main README. Update
it when architecture, environment assumptions, or the next milestone materially changes.

## Project identity

Eidolon is a local visual embodiment for an agent, not the agent's persona or conversation runtime.
The immediate character is Rio from Blue Archive, rendered as a transparent desktop companion with
JRPG-style dialogue. The long-term design should remain character-agnostic even where Rio-specific
motion tuning is deliberately supported.

The preferred stack is native C17, LLVM/Clang, GNU Make, and SDL3. Do not introduce CMake or a
C#/.NET shell. Windows is the active implementation target; keep Linux compiling when practical,
but do not let the legacy Linux renderer constrain the Windows design.

## Repository state: read before touching Git

As of 2026-07-16, `master` points at `cd670d5` (`document motion controller`) and tracks
`origin/master`. The working tree contains the large, intentional implementation developed after
that commit: D3D11 3D rendering, motion/IK, debug UI, session watching, hidden QA snapshots,
resolution control, and overlay performance fixes. Many of those files are still untracked.

**Do not reset, clean, checkout, or otherwise discard the dirty working tree.** Inspect
`git status --short` first. Existing changes belong to the project. No commit or push was made for
the current implementation slice.

The latest verified state is:

- debug build succeeds with no compiler warnings;
- release build succeeds with no compiler warnings;
- `make check` passes all ten test executables;
- hidden snapshot QA preserves Rio's correct gray/red suit and the debug resolution menu;
- `build/windows/eidolon.exe` was restored to the debug build after release verification;
- interactive DWM performance still needs the user's confirmation after the latest compositor fix.

## Local environment and commands

Expected Windows locations:

- repository: `C:/dev/eidolon`
- SDL3 development package: `C:/dev/SDL3`
- Blender executable: `C:/Blender/blender.exe` (or add `C:/Blender` to `PATH`)
- vendored source libraries: `lib/cgltf`, `lib/ufbx`, and `lib/SDL_shadercross`

The dependency sources are vendored in `lib/`; they are not Git submodules. The Windows runtime
uses `cgltf` and native D3D11. `ufbx` is an optional FBX-validation library. SDL_shadercross remains
for the legacy Linux SDL_GPU shader path; Windows shaders are compiled by the Windows SDK's
`fxc.exe` through `tools/compile_d3d11_shader.ps1`.

Common commands:

```powershell
cd C:/dev/eidolon
$env:SDL3_ROOT = "C:/dev/SDL3"
make
make check
make MODE=release
./build/windows/eidolon.exe
```

`make` copies the selected mode to the stable `build/windows/eidolon.exe` path. If a release build
was the last command, run plain `make` again when the stable path should return to debug.

## Non-intrusive visual QA

Never launch visible Eidolon or Blender windows merely to inspect output while the user is working.
Do not control the mouse or steal focus. Eidolon snapshot commands create a hidden window, skip
overlay configuration, IPC, and session watching, write one PNG, and exit:

```powershell
./build/windows/eidolon.exe --snapshot build/windows/qa.png
./build/windows/eidolon.exe --snapshot-debug build/windows/qa-debug.png
./build/windows/eidolon.exe --snapshot-debug-resolution build/windows/qa-resolution-menu.png
./build/windows/eidolon.exe --snapshot-pose 1 build/windows/qa-pose.png
./build/windows/eidolon.exe --snapshot-debug-pose 1 build/windows/qa-pose-debug.png
./build/windows/eidolon.exe --snapshot-resolution 2048 build/windows/qa-2048.png
```

Use Blender only with `--background` for automated inspection. The sole intentional exception is
`make model-mouth-calibrate`, which is an explicitly user-operated live calibration tool.

## Runtime architecture

The main ownership boundaries are:

- `app`: lifecycle, events, display scale, timing, IPC/session polling, and control state;
- `draw`: SDL composition, dialogue/debug UI, snapshots, and hit-mask invalidation policy;
- `model`: GLB decoding, skeleton evaluation, native D3D11 resources, skinning, and render targets;
- `motion`, `humanoid`, `pose`, `pose_solver`, `ik`: procedural motion and semantic pose solving;
- `platform`: Win32 overlay/readback, local IPC, and session-file discovery;
- `animation`: fallback v2 sprite-atlas playback.

On Windows SDL owns the transparent window, D3D11 device, context, and swapchain. `model.c` borrows
that device and renders Rio into an SDL-owned target texture. SDL then samples the same GPU resource
while composing dialogue and debug UI. There is no animated full-frame CPU transfer in the model
path.

The runtime GLB contains seven material draws, 132 hierarchy nodes, and 127 skin joints. Procedural
local transforms are evaluated on the CPU and a linear-blend joint palette is uploaded at 30 Hz.
The model render target is independent from desktop scale and can be selected as 512, 1024, 1536,
or 2048 square pixels; Windows defaults to 1024.

### Shader linkage trap

The D3D11 vertex layout uses `POSITION`, `TEXCOORD0`, `BLENDINDICES0`, and `BLENDWEIGHT0`. The pixel
shader input must declare both `SV_Position` and `TEXCOORD0`. Without `SV_Position`, FXC assigns the
pixel shader's UV to input register 0 while the vertex shader writes UV to register 1, producing
`(0,0)` texture coordinates and making every material sample its atlas origin. The resulting model
looks as if its suit were replaced by skin. This was diagnosed and fixed; do not simplify the pixel
input back to a lone `float2`.

## Transparent overlay and performance invariants

Desktop model scale is `0.75x..4.0x`. A 4x model expands the transparent window to 1288x1128 window
coordinates before DPI scaling. That surface, not the 1024 model target, is the dominant cost on an
APU because DWM must alpha-composite the entire topmost swapchain.

The following rules are deliberate and must not regress:

- presentation has an authoritative 30 Hz deadline, matching model animation;
- VSync is still requested, but it is not trusted as the only limiter;
- slow frames never trigger catch-up presentation bursts;
- ordinary breathing/sway never causes a hit-mask readback;
- a hit mask is rebuilt only for structural changes such as scale, mode/layout, pose, or completed
  rotation/control edits;
- hit-mask readback is suspended while a scale slider or model rotation is actively dragged;
- Win32 receives a padded 16-pixel-tile region envelope rather than thousands of scan-line
  rectangles, while cached alpha and `WM_NCHITTEST` keep click-through pixel-exact;
- `SetWindowRgn` does not request a redundant synchronous redraw because the next present follows.

After a model-scale drag or render-resolution change, the log records a line like:

```text
presentation scale=4.00x logical=1288x1128 window=1288x1128 output=1288x1128 target=1024 cap=30Hz vsync=1
```

If the whole desktop still lags, collect that line from
`%LOCALAPPDATA%/Eidolon/eidolon.log`. The next architectural optimization is not CPU rasterization.
It is splitting the model and bubbles/debug UI into independently sized transparent windows and
cropping the model window to a conservative portrait alpha envelope. Rio currently occupies roughly
114x200 pixels inside the 256x256 base destination, so the square combined surface wastes substantial
transparent compositor area. This split also aligns with the future one-bubble-per-session design.

## Agent/session integration

The reliable integration for the Microsoft Store ChatGPT/Codex desktop app is the direct watcher of
`~/.codex/sessions`. It establishes a baseline at startup and displays newly appended final agent
output in the JRPG dialogue bubble without replaying an old response.

Hook definitions exist in `hooks/`, and Codex reports them installed/active, but the packaged desktop
application did not reliably launch those external commands during this session. Treat hooks as an
unfinished optional fast path, not the primary transport. The CLI and Microsoft Store desktop app
also have distinct session behavior; do not assume installing or testing one proves the other.

Current dialogue handling is single-session. The agreed multi-session direction is one bubble per
running session, followed by dynamic placement/overlap avoidance. Preserve that decision when the
session model is expanded.

## Model and asset pipeline

Authoring input is FBX; runtime input is GLB. Blender owns import repair, deterministic export, and
visual QA. Runtime code should not grow a custom FBX inspector unless Blender demonstrably cannot
handle a specific source.

Canonical inputs:

- body/rig: `assets/blue-archive-rio-battle-full-rip-rig/source/Rio Battle/CH0331_Mesh.fbx`
- halo: `assets/blue-archive-rio-battle-full-rip-rig/source/Rio Battle/Animator/CH0331_Halo/CH0331_Halo.fbx`
- generated mouth texture: `.../Texture2D/Character_Mouth_Black.png`
- runtime export: `assets/model/rio.glb`

The canonical body has five meshes and the complete facial/body rig. The halo is exported as a
separate attachment to `Bip001 Head`. The missing mouth overlay texture was reconstructed and the
live calibration was saved at the default zero offset, which looked best. The calibration JSON is a
generated file under `build/model-audit`; its absence is equivalent to zero offset during export.

Useful asset commands:

```powershell
make model-audit BLENDER=C:/Blender/blender.exe
make model-material-audit BLENDER=C:/Blender/blender.exe
make model-export BLENDER=C:/Blender/blender.exe
make model-preview-glb BLENDER=C:/Blender/blender.exe
make model-mouth-calibrate BLENDER=C:/Blender/blender.exe
```

The GLB exporter normalizes seven texture-backed materials and uses alpha cutout. Ordinary glTF
alpha blending breaks Rio's coplanar face layers and reduces the eyes to red discs. Always inspect
the deterministic GLB preview after material/export changes.

## Procedural motion state

The intended controller remains:

```text
language/session state
        -> behavior intent, 1-5 Hz
        -> procedural pose goals
        -> IK + joint limits, 60 Hz
        -> secondary physics, 60 Hz
        -> bone matrices -> GPU skinning
        -> shared GPU texture -> transparent SDL composition
```

Implemented now:

- a semantic humanoid profile derived from Rio/common bone aliases;
- bind-pose hierarchy evaluation and GPU skinning;
- procedural breathing and slow weight sway;
- normalized shoulder-relative arm targets;
- analytic two-bone arm IK with pole targets, reach clamping, and transactional failure;
- runtime semantic-pose presets and six target/pole calibration sliders;
- live, strict, transactional `config/motion.cfg` reload;
- yaw/pitch rotation with middle-drag, roll with Shift+middle-drag, and double-middle reset.

Pose review status is not “finished animation”:

- bind A/calibration now establishes a correct baseline pose;
- relaxed/open is directionally correct but still somewhat stiff;
- reserved/guarded preserves relative crossing but currently places the crossed arms behind Rio with
  palms facing forward; the intended guarded motif is crossed in front;
- attentive is not yet semantically convincing and previously read as elbows pulled awkwardly back;
- playful/open remains an uncalibrated initial guess.

The fast iteration loop is: select a preset in F1, adjust semantic hand/pole sliders, press `C` to
copy the complete initializer, review it, then promote corrected values into `src/pose.c`. Do not go
back to guessing raw `neutral.arm_lower_deg` values for semantic poses.

`config/motion.cfg` currently contains the user's tuned breathing/sway values. Invalid edits reject
the whole configuration and preserve the last-good state. Keep this transactional behavior.

## Next motion milestones

Recommended order after the compositor fix is confirmed interactively:

1. calibrate relaxed, guarded, attentive, and playful arm targets with the user;
2. add pose transitions and velocity/acceleration continuity rather than snapping goals;
3. add wrist orientation plus shoulder/forearm twist limits;
4. add planted-foot/stance control and lower-body IK;
5. add eye-first/head-follow attention, stochastic blinking, and expression control;
6. add secondary spring systems for hair, clothing, and Rio-specific chest mass;
7. connect slow language/session intent to motif weights rather than direct joint commands.

Secondary motion should be acceleration-driven damped response with anatomical limits and settling,
not a permanent sine-wave bounce. Keep character-specific parameters runtime-tunable.

## Working conventions

- Use GNU Make only.
- Preserve unrelated and pre-existing dirty changes.
- Use hidden snapshots or Blender background renders for automated visual QA.
- Let the user perform interactive feel/performance testing and report what they see; do not seize
  their desktop to reproduce it.
- The user values being kept in the loop during longer implementation work.
- Do not commit, push, publish, delete, or rewrite history without explicit permission.
- The repository's first commit convention is `init`; that commit already exists.
- Prefer fixing ownership and scheduling boundaries over layering patches on symptoms.

## Resume checklist

1. Read this file and the main README.
2. Run `git status --short`; preserve the current dirty implementation.
3. Run `make` and `make check`.
4. Ask the user to validate 4x scale at 1024 render resolution with the latest 30 Hz compositor fix.
5. If lag remains, inspect the new presentation log line before changing rendering again.
6. Continue semantic-pose calibration unless the user chooses the window-splitting optimization or
   hook investigation first.
