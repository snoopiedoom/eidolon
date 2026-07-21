# Runtime architecture

Eidolon is a presentation runtime for local conversational agents. It does not own the agent's
persona, model, prompts, or conversation state. It observes local session output, derives visual
intent, and renders that intent through one character provider.

## System flow

```text
provider transports | in-path relays | optional legacy readers
                         ↓
 provider parsers → normalized conversation events
                ↓
 session registry keyed by (provider, session id)
                ↓
 semantic expression planner + deterministic delivery track
                ↓
 bounded portrait spring | sprite | 2D portrait | procedural 3D
                ↓
       SDL transparent composition
                ↓
  native click-through / desktop compositor
```

Language-scale work must never block presentation. Session discovery, transcript parsing, and local
model inference cross bounded asynchronous boundaries. The presentation thread consumes completed
snapshots and prepared expression tracks.

## Ownership

- `app`: lifecycle, event routing, display scale, timing, renderer selection, and composition-level
  state;
- `conversation_sources`: provider catalog, configuration, capability state, and event bus;
- `providers/*_stream`: one vendor protocol parser per provider, producing only normalized events;
- `providers/live_source`: blocking network transports isolated on cancellable worker threads;
- `relay_core`: transport-neutral bounded bidirectional forwarding, passive observation, and
  symmetric endpoint interruption;
- `providers/codex_relay`: localhost WebSocket ownership, hidden Codex stdio app-server lifetime,
  framing, and server-to-client protocol observation;
- `session_registry`: provider-neutral session identity, optional transcript cursor, activity,
  independent dialogue state, and deterministic eviction;
- `bubble_layout`: pure placement from character, window, and bubble geometry;
- `dialogue`: UTF-8 text validation, reveal, wrapping, scrolling, and pagination for one bubble;
- `delivery`: deterministic phrase, cadence, contrast, hesitation, and punctuation marks attached to
  original UTF-8 reveal offsets;
- `expression_director`: semantic beat segmentation, classifier request association, source-offset
  activation, expression stabilization, and performance cues;
- `affect` / `affect_client`: lifecycle fallback, GoEmotions projection, continuous affect axes,
  asynchronous worker transport, and stale-result rejection;
- `animation`: provider-independent v2 sprite-atlas playback;
- `portrait`: portrait textures, expression selection, framing, and composed whole-image acting;
- `portrait_motion`: bounded spring state for delivery impulses, independent from classifier latency;
- `model`: GLB resources, hierarchy evaluation, D3D11 drawing, and GPU skinning;
- `motion`, `humanoid`, `pose`, `pose_solver`, `ik`: semantic procedural motion and constrained pose
  solving;
- `draw`: transparent SDL composition, dialogue rendering, snapshots, and hit-mask invalidation;
- `text_renderer`: SDL_ttf faces, fallback selection, and reusable cached text objects;
- `settings_ui`: a separate SDL/Dear ImGui window; it displays controls but does not own settings;
- `user_settings`: strict parsing, serialization, sparse override state, and persistence;
- `platform`: only behavior SDL cannot express uniformly—overlay hit testing, local IPC, and session
  file discovery.

Do not duplicate these collections in `app.c`. In particular, session paths, titles, dialogue
objects, and activity timestamps belong to `session_registry`.

## Character providers

The active provider is selected at runtime:

- **Sprite** plays Codex-compatible v2 sprite sheets through `animation`.
- **2D portrait** displays one full-canvas transparent expression image at a time. Expression art
  swaps atomically on the next rendered frame. Breathing, posture, delivery spring, attention, and
  semantic accents compose into one bottom-anchored transform.
- **3D model** evaluates a skinned GLB hierarchy and procedural pose state, then renders into an
  SDL-owned GPU texture.

Provider selection must not initialize expensive inactive providers unnecessarily. When the 2D
portrait is selected successfully, 3D initialization is skipped.

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
waits only if it physically reaches an unclassified beat boundary. The provider's completion
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

SDL owns the transparent window, D3D11 device, context, and swapchain. The 3D renderer borrows that
device and draws into an SDL-owned target texture; SDL samples the same allocation during final
composition. There is no animated full-frame CPU transfer, staging-map loop, or upload.

The D3D11 vertex layout uses `POSITION`, `TEXCOORD0`, `BLENDINDICES0`, and `BLENDWEIGHT0`. The pixel
shader input must retain both `SV_Position` and `TEXCOORD0`. Removing `SV_Position` can bind UV to the
wrong register and make every material sample the atlas origin—the historical “suit replaced by
skin” failure.

## Overlay and performance invariants

- presentation has an authoritative 30 Hz deadline and never performs catch-up bursts;
- VSync is requested but is not the sole limiter;
- ordinary breathing, speaking, and portrait motion do not invalidate the hit mask;
- alpha readback happens only after structural changes such as scale, layout, framing, pose, or a
  completed manual rotation;
- readback is suspended during scale-slider and rotation drags;
- Windows uses cached alpha for pixel-exact `WM_NCHITTEST` and a coarse padded window region for DWM;
- model render resolution is independent from desktop presentation scale;
- discovery and inference never run on the presentation thread.

At large desktop scale, the transparent swapchain area—not the 3D triangle count—is usually the
dominant APU cost. The next compositor optimization is independently sized character and bubble
windows with a conservative character alpha envelope, not CPU rasterization.
