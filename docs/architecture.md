# Runtime architecture

The current Eidolon runtime is an embodiment and presentation layer for external conversational
agents. It does not currently own persona persistence, model inference, prompts, or canonical
conversation state. It observes local session output, derives performance intent, and projects that
intent through one body renderer.

Future persona-platform work must remain separable from body rendering, configured session sources,
and agent adapters rather than being folded into the presentation thread.

## Current system flow

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
transparent SDL composition + native hit testing
```

This is the current implementation, not the permanent presentation boundary. The target separates
content rendering from platform-native surface presentation:

```text
renderer-neutral scene snapshot
              ↓
body content + dialogue content + geometry
              ↓
native presentation backend
              ↓
platform compositor layer tree
```

The presentation backend owns hosts, independent body/bubble layers, input regions, output topology,
cadence, and compositor commits. The graphics backend owns pixel production. The two are independent
selections. Native callbacks perform only latency-critical platform mechanics; product input crosses
the bounded [presentation event contract](design/presentation-events.md). Active-host geometry,
scale, output, safe area, refresh, topology, and wake behavior cross the revisioned
[presentation environment contract](design/presentation-environment.md). See the
[native presentation and graphics stack](design/native-presentation.md) for the contract, strategy
analysis, platform mappings, and migration gates.

Language-scale work must never block presentation. Session discovery, transcript parsing, and local
model inference cross bounded asynchronous boundaries. The presentation thread consumes completed
snapshots and prepared expression tracks.

## Ownership

- `app`: product lifecycle, portable event routing, timing, renderer selection, and
  composition-level state;
- `presentation`: backend selection, host/layer/target ownership, scene commits, capability
  reporting, portable edge events, revisioned active-host environments, and output topology;
- `conversation_sources`: adapter catalog, configured source state, capability state, and event bus;
- `providers/*_stream`: one vendor protocol parser per session source, producing only normalized
  events; `providers` is the legacy source-directory name;
- `providers/live_source`: blocking network transports isolated on cancellable worker threads;
- `relay_core`: transport-neutral bounded bidirectional forwarding, passive observation, and
  symmetric endpoint interruption;
- `providers/codex_relay`: localhost WebSocket ownership, hidden Codex stdio app-server lifetime,
  framing, and server-to-client protocol observation;
- `session_registry`: normalized session identity, optional transcript cursor, activity, independent
  dialogue state, and deterministic eviction;
- `bubble_layout`: pure placement from character, window, and bubble geometry;
- `dialogue`: UTF-8 text validation, reveal, wrapping, scrolling, and pagination for one bubble;
- `delivery`: deterministic phrase, cadence, contrast, hesitation, and punctuation marks attached to
  original UTF-8 reveal offsets;
- `expression_director`: semantic beat segmentation, classifier request association, source-offset
  activation, expression stabilization, and performance cues;
- `affect` / `affect_client`: lifecycle fallback, GoEmotions projection, continuous affect axes,
  asynchronous worker transport, and stale-result rejection;
- `animation`: adapter-independent v2 sprite-atlas playback;
- `portrait`: portrait textures, expression selection, framing, and composed whole-image acting;
- `portrait_motion`: bounded spring state for delivery impulses, independent from classifier latency;
- `model`: GLB resources, hierarchy evaluation, D3D11 drawing, and GPU skinning;
- `motion`, `humanoid`, `pose`, `pose_solver`, `ik`: semantic procedural motion and constrained pose
  solving;
- `draw`: transparent SDL composition, dialogue rendering, snapshots, and hit-mask invalidation;
- `text_renderer`: SDL_ttf faces, fallback selection, and reusable cached text objects;
- `settings_ui`: a separate SDL/Dear ImGui window; it displays controls but does not own settings;
- `user_settings`: strict parsing, serialization, sparse override state, and persistence;
- `platform`: behavior SDL cannot express uniformly—native presentation adapters, overlay hit
  testing, local IPC, and session file discovery. Platform callbacks may perform immediate native
  mechanics but never acquire dialogue, session, or persona semantics.

Do not duplicate these collections in `app.c`. In particular, session paths, titles, dialogue
objects, and activity timestamps belong to `session_registry`.

## Body renderers

The active body renderer is selected at runtime:

- **Sprite** plays Codex-compatible v2 sprite sheets through `animation`.
- **2D portrait** displays one full-canvas transparent expression image at a time. Expression art
  swaps atomically on the next rendered frame. Breathing, posture, delivery spring, attention, and
  semantic accents compose into one bottom-anchored transform.
- **3D model** evaluates a skinned GLB hierarchy and procedural pose state, then renders into an
  SDL-owned GPU texture.

Body selection must not initialize expensive inactive renderers unnecessarily. When the 2D
portrait is selected successfully, 3D initialization is skipped. The current global
`preferred_renderer` setting is an implementation seam; the intended product model is a character
package with a default body variant and optional advanced overrides.

The intended character-package boundary is not yet the current selection path:

```text
renderer-neutral performance intent + character package
                         ↓
body capability projection and deterministic fallback mapping
                         ↓
package-selected body variant
                         ↓
visible geometry + face/head bounds + click regions
```

## Affect and expression boundary

Renderers never consume GoEmotions labels. `EidolonAffectController` projects lifecycle state or a
28-label classifier distribution into valence, arousal, dominance, certainty, warmth, and surprise,
then owns the selected expression intent.

For snapshot inputs, the expression director compiles a complete message before reveal. For live
inputs, the same track grows by stable closed prefixes while the response streams:

1. sentence, line, contrast, and bounded-length boundaries become semantic beats;
2. tiny discourse fragments such as `wait`, `but`, `oh`, and `ugh` attach to the thought they modify;
3. closed beats are classified through a persistent FIFO worker while the following text streams;
4. results are stored against original UTF-8 byte offsets;
5. ambiguous adjacent faces preserve continuity when the affect change is small;
6. the reveal cursor activates the prepared face and motion cue before the beat's first glyph.

Missing or failed native inference falls back to deterministic lifecycle-derived affect after a
bounded deadline. It must never freeze dialogue.

Classifier queue pressure is backpressure, not inference failure. Tracks submit a bounded number of
beats per frame and resume later; only a real deadline or worker failure selects fallback. The static
track capacity covers the full dialogue text limit so a long response cannot collapse its tail into
one giant expression beat.

## Layered portrait performance

Expression planning and spoken delivery are deliberately separate clocks:

1. the expression director selects infrequent semantic faces and large acting cues;
2. the delivery compiler derives phrase, contrast, hesitation, accent, and landing marks without
   inference;
3. the dialogue reveal cursor activates both tracks by original source offset;
4. delivery marks inject bounded velocity into a four-channel spring;
5. the portrait composes breathing, expression posture, spring displacement, attention, and large
   semantic cues in that order.

This keeps a long sentence alive without inventing more facial emotions. Large reveal jumps coalesce
skipped delivery marks instead of replaying a burst, and a live stream seeks past newly compiled
history so appended text cannot re-trigger old gestures. Character expression labels lightly style
the same delivery impulses; they do not own timing or punctuation parsing.

The first live delta opens the bubble immediately with a responding intent. Normal completed
sentences and contrast clauses become stable beats as soon as they close; unfinished tails and
modifier fragments such as `wait.` remain provisional until their thought arrives. Extending a track
preserves committed classifier results and activation state. Reveal continues through ready text and
waits only if it physically reaches an unclassified beat boundary. The adapter's completion
snapshot finalizes the provisional tail and repairs dropped or truncated deltas without replaying
old expressions.

## Text boundary

`EidolonTextRenderer` owns one SDL_ttf engine, the MesloLGS Nerd Font Mono primary face, optional
Windows CJK/Korean/emoji fallbacks, and reusable cache slots. Each visible bubble owns separate title
and body slots.

`dialogue.c` preserves valid UTF-8 and replaces invalid bytes with U+FFFD. Reveal advances across
combining marks, variation selectors, emoji modifiers, flags, and ZWJ sequences as one
grapheme-like cluster. Bubble wrapping must not change expression timing because performance events
are attached to source offsets, not rendered lines.

## Windows rendering

The shipped default remains `sdl_window_legacy`: SDL owns the transparent window, D3D11 device,
context, and swapchain. The 3D renderer borrows that device and draws into an SDL-owned target
texture; SDL samples the same allocation during final composition. There is no animated full-frame
CPU transfer, staging-map loop, or upload.

The opt-in `win32_dcomp` backend owns a no-redirection Win32 host, D3D11 device, independent
premultiplied body/dialogue swapchains, DirectComposition visuals, transforms, opacity, z-order,
commits, cached CPU alpha planes, transformed native hit testing, and Win32-owned body dragging.
It currently supports the portrait body only. Native dialogue activation and move completion now
cross the bounded presentation-event queue and are owner-confirmed. Revisioned environment
publication, output-local host migration, sprite/3D targets, and device-loss recovery remain
unfinished.

The D3D11 vertex layout uses `POSITION`, `TEXCOORD0`, `BLENDINDICES0`, and `BLENDWEIGHT0`. The pixel
shader input must retain both `SV_Position` and `TEXCOORD0`. Removing `SV_Position` can bind UV to the
wrong register and make every material sample the atlas origin—the historical “suit replaced by
skin” failure.

## Overlay and performance invariants

- presentation cadence is configured independently through VSync and an optional FPS ceiling;
  active VSync exclusively owns the default cadence, while the software clock owns explicit lower
  ceilings and unavailable-VSync fallback without producing catch-up bursts;
- input processing has a bounded event budget; continuous mouse motion cannot postpone
  presentation indefinitely;
- character dragging coalesces motion into the latest target and performs at most one native window
  move per presentation tick on the portable fallback; Windows delegates the interactive move to
  the native compositor instead of synchronously repositioning the HWND from the render loop;
- VSync is optional and is not the sole limiter;
- the settings renderer is paced by the main presentation loop and cannot introduce a second VSync
  wait;
- ordinary breathing, speaking, and portrait motion do not invalidate the hit mask;
- alpha readback happens only after structural changes such as scale, layout, framing, pose, or a
  completed manual rotation;
- readback is suspended during scale-slider and rotation drags;
- Windows uses cached alpha for pixel-exact `WM_NCHITTEST` and a coarse padded window region for DWM;
- model render resolution is independent from desktop presentation scale;
- discovery and inference never run on the presentation thread.

At large desktop scale, the transparent swapchain area—not the 3D triangle count—is usually the
dominant APU cost. The intended fix is independently sized compositor layers for the character and
each bubble, with renderer-provided hit geometry and no ordinary full-frame readback. It is not CPU
rasterization or an unmeasured graphics-API migration.
