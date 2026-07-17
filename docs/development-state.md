# Development state and continuation notes

This is the durable handoff for continuing Eidolon in a fresh session. It records decisions and
unfinished work that are easy to lose but too implementation-specific for the main README. Update
it when architecture, environment assumptions, or the next milestone materially changes.

## Project identity

Eidolon is a local visual embodiment for an agent, not the agent's persona or conversation runtime.
The immediate daily-driver character is Bunny Asuna from Blue Archive, rendered from complete
transparent expression portraits with JRPG-style dialogue. Rio's procedural 3D renderer remains in
the tree as an optional future provider; it is deliberately not initialized while the portrait
provider is available. The long-term design remains character-agnostic.

The preferred stack is native C17, LLVM/Clang, GNU Make, and SDL3. Do not introduce CMake or a
C#/.NET shell. Windows is the active implementation target; keep Linux compiling when practical,
but do not let the legacy Linux renderer constrain the Windows design.

## Repository state: read before touching Git

As of 2026-07-17, `master` points at `b3fad49` (`add procedural 3d companion runtime`) and tracks
`origin/master`. The active working tree adds the 2D portrait provider, Bunny Asuna manifest, debug
expression selector, hot reload, and portrait config tests. Local extracted character assets and a
local `lib/BA2LW` checkout are intentionally not repository content.

**Do not reset, clean, checkout, or otherwise discard the dirty working tree.** Inspect
`git status --short` first. Existing changes belong to the project. No commit or push was made for
the current implementation slice.

The latest verified state is:

- debug build succeeds with no compiler warnings;
- `make check` passes all fifteen ordinary test executables, including portrait manifest,
  affect-controller, tokenizer, bubble-layout, and active-session refresh validation;
- `make affect-check` passes direct ONNX inference and the asynchronous process-client test;
- hidden snapshot QA renders Bunny Asuna and the complete expression selector correctly;
- portrait canvases are all `927x1280`, so expression changes do not jump;
- runtime 3D initialization is skipped when the portrait provider loads;
- lifecycle state now flows through the renderer-independent affect controller, with smoothing,
  expression dwell, and stale asynchronous-result rejection;
- the user confirmed the 2D pet, dialogue capture, expression transitions, and motion look good;
- the optional native GoEmotions worker is installed locally and verified end-to-end through IPC;
  lifecycle state remains the fallback when its ignored cache/build artifacts are absent.
- dialogue titles and bodies render through cached SDL_ttf text objects with MesloLGS Nerd Font
  Mono as the primary face and optional Windows CJK/Korean/emoji fallbacks;
- dialogue layout preserves valid UTF-8 and typewriter reveal advances by grapheme-like clusters,
  never by raw bytes; hidden snapshot QA confirmed Slovenian, Chinese, Korean, Nerd Font, and emoji
  output.
- dialogue movement has three configured modes: default `follow` retains four lines and exposes the
  next bottom line without pausing, `paged` holds then replaces all five lines, and `manual` waits
  for click-driven whole-page replacement;
- portrait expression changes now drive a separate 520 ms damped semantic accent, while the original
  140 ms crossfade remains responsible only for swapping art;
- expression motion now has asymmetric translation/scale/rotation springs, three deterministic
  variants, interruption anticipation, persistent semantic posture, punctuation-driven speech
  beats, and smoothed attention toward the active session bubble;
- the original dark dialogue theme is preserved as `classic`; `academy_heart` is the selected light
  cyan/pink character theme, and `T` compares the two at runtime.

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
make text-setup
make
make check
make MODE=release
./build/windows/eidolon.exe
```

`make text-setup` downloads the pinned, checksum-verified SDL_ttf 3.2.2 Windows development package
into `.cache/sdl_ttf`; run it once on each fresh checkout. `make` copies SDL3_ttf.dll beside the
executable and copies the selected mode to the stable `build/windows/eidolon.exe` path. If a release
build was the last command, run plain `make` again when the stable path should return to debug.

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
./build/windows/eidolon.exe --snapshot-portrait-motion 1 120 build/windows/qa-expression.png
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

### Affect boundary

`EidolonAffectController` is the sole owner of expression intent. Lifecycle state works immediately;
the asynchronous native worker supplies 28 multi-label probabilities when installed. The controller
projects either source into valence, arousal, dominance, certainty, warmth, and surprise, smooths
the result, rejects stale inference by sequence number, and applies dwell/hysteresis before an
expression changes.

Renderers never depend on GoEmotions labels. The portrait consumes the selected expression; Rio's
procedural posture, breathing, gaze, and motion controller will consume the continuous axes. The
optional worker uses an in-tree RoBERTa byte-level BPE tokenizer and the quantized
`SamLowe/roberta-base-go_emotions` ONNX model through ONNX Runtime's C API. It is a disposable child
process behind an SDL worker thread; renderer frames never wait for inference. Requests classify the
currently visible dialogue window, newer requests replace queued work, and stale results are
rejected.
Worker absence or failure never disables Eidolon. The F1 panel exposes worker/fallback status,
controller source, selected intent, current VAD axes, and evidence.

### Text rendering boundary

Dialogue titles and bodies use SDL_ttf 3's renderer text engine. `EidolonTextRenderer` owns one
engine, the MesloLGS Nerd Font Mono primary face, optional Windows fallback faces, and sixteen cached
text slots. Each visible session owns distinct title/body slots. Text strings, wrap widths, and
colors are updated only when their values change; drawing a stable bubble does not rebuild its text.

`dialogue.c` validates and preserves UTF-8. Invalid input bytes become U+FFFD, layout counts decoded
codepoints rather than bytes, and reveal advances across combining marks, variation selectors,
emoji modifiers, flags, and ZWJ sequences as one grapheme-like cluster. `dialogue.movement` selects
`follow`, `paged`, or `manual`. Follow mode shifts the source cursor by one wrapped line as soon as
typing reaches the viewport edge and retains the other four; paged mode waits the configured
`dialogue.hold_ms` before rebuilding from the next five-line cursor; manual mode performs that same
whole-page replacement only on click. The legacy
`SDL_RenderDebugText` path remains only as a failure fallback and for the ASCII debug panel.

The project includes the complete MesloLG Nerd Font folder under `assets/fonts`, including its
Apache-2.0 license; it adds roughly 195 MiB to the working tree. The default face is
`MesloLGSNerdFontMono-Regular.ttf`. Windows fallbacks are discovered from the system font directory,
not redistributed: Microsoft YaHei for Chinese, Malgun Gothic for Korean, and Segoe UI Emoji.
Portable Linux fallback selection and a runtime font picker remain future work.

Known-session transcript stamps are checked once per presentation frame. Recursive discovery and
session-index file loading run on `eidolon-sessions`, never the presentation thread; the renderer
consumes completed snapshots without waiting. Discovery is requested every 500 ms, but that cadence
must never imply work on the render thread. Session dialogue reveal also stays outside discovery
throttling. Moving it back under that branch recreates the visibly 2 Hz typewriter bug. The registry
test injects a 120 ms discovery scan and asserts that presentation-side polling still returns within
20 ms.

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

### Multiple active sessions

The first implementation is complete and awaiting interactive confirmation. The old single-latest
watcher has been replaced at runtime by a bounded session registry. It discovers the eight most
recent rollout files, keys them by UUID, reads names from `.codex/session_index.jsonl`, and exposes up
to four simultaneously visible bubbles with stable slots and independent scrolling.

Discovery ranks the complete `.codex/sessions` tree by modification time, not the dated directory in
which a rollout was originally created. Long-lived tasks can remain active for days. The platform
scan returns 32 recent candidates so guardian/subagent churn cannot crowd user tasks out before the
registry filters them. Rollouts whose `session_meta.thread_source` is `subagent` are internal plumbing
and never receive bubbles.

The 2D provider also supports `P` framing toggles. Every expression stores a pixel-space
`portrait_crop` in `config/character.cfg`; Asuna currently uses a `390, 0, 350, 420` bust crop at a
360-pixel presentation height. Full-body and bust modes use the same textures, transitions, and
motion. Interactive toggling preserves the character's screen-space center while resizing the
transparent window; crop edits hot-reload with the rest of the manifest.

Portrait motion remains a bottom-anchored whole-image transform because the source portraits are
flat PNGs. Breathing, emotional posture, active-bubble attention, punctuation speech beats, and the
expression-transition spring contribute to one composed transform. Do not create independent
destination rectangles for these layers; that recreates visible fighting and drift. The three
layer strengths and expression-accent timing are hot-reloaded from `config/character.cfg`.

Required behavior:

- session identity is the Codex thread/session UUID, never transcript basename or array position;
- each active session owns its title, transcript path, last observed file state, last activity time,
  dialogue scroll/reveal state, affect request sequence, and bubble layout state;
- the bubble heading is the actual session title when available, with a short stable fallback such
  as the working-directory name or abbreviated UUID;
- new final agent output updates only that session's bubble and affect request;
- bubbles disappear after the session becomes inactive, using an explicit policy rather than
  deleting them merely because another session spoke;
- several bubbles can remain visible simultaneously around the shared character;
- clicking/advancing a bubble must hit-test that bubble, not whichever session updated most recently;
- the existing single-session behavior remains a valid one-session case.

Keep three ownership boundaries separate:

1. `session_registry`: discovery, UUID identity, title metadata, transcript cursor, lifecycle and
   inactivity;
2. `bubble_layout`: pure placement from character/window geometry plus bubble sizes, with no file IO
   or transcript parsing;
3. `dialogue_bubble`: rolling-window layout, typewriter reveal, drawing, and hit testing for one
   session.

Do not make `app.c` own a parallel collection of paths, dialogues, titles, and timestamps. A bounded
registry is appropriate for the first implementation (for example eight sessions), but capacity
overflow needs deterministic eviction of the oldest inactive entry and a log entry.

The first layout iteration can use deterministic anchor slots around the portrait: upper-left,
upper-right, mid-left, and mid-right, expanding the transparent window only when necessary. Score
slots by overlap with the portrait, other bubbles, and window bounds. Preserve a session's slot while
it remains visible so bubbles do not jump whenever another message arrives. More elaborate dynamic
placement can follow after the ownership and hit testing work.

No explicit session-end record was found in the rollout metadata used by the Store app. The first
policy is therefore five minutes of quiet after final output; unread dialogue prevents
retirement. The constant currently lives in `session_registry.c`, not drawing. It should become
runtime configuration after the interaction feels right.

Before implementing, inspect representative rollout JSONL for the real title source. Do not infer a
title by reparsing rendered agent output. Title discovery belongs in the session metadata/parser and
must tolerate old rollouts that lack it.

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
3. Run `make text-setup` on a fresh machine, then `make` and `make check`.
4. Ask the user to run Eidolon, produce output in two live tasks, and confirm title, placement,
   independent rolling autoplay, and five-minute retirement behavior.
5. Fix any interactive layout/hit-test issues without merging registry ownership back into `app.c`.
6. Make the quiet timeout runtime-configurable after the desired behavior is confirmed.
7. Tune the GoEmotions-to-expression projection interactively against real dialogue. Keep inference
   optional; `make affect-setup` restores ignored runtime/model artifacts on a fresh machine.
