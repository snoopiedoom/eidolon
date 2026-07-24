# Eidolon Performance Runtime architecture-reconciliation blueprint

Status: proposed; owner approval required before production implementation
Baseline: A1-complete commit `6a5ac9288b342e850a120b3eb179403cf6389292`
Workstream: `epr/vrm-v1` in `C:\dev\eidolon-epr`
Scope: architecture reconciliation and the first SDL 3D / VRM 1.0 vertical slice

This document reconciles the proposed EPR thesis, research corpus, and schema against the
A1-complete repository. It is an approval artifact, not an implementation specification frozen for
all future EPR work. No production implementation is authorized by this document alone.

The central conclusion is narrower than the thesis: EPR needs two hard runtime cores in its first
slice:

1. a versioned temporal plan whose commitments and revisions are explicit; and
2. explicit, deterministic body-resource grants and transfers.

Performance Intent, modality realization, canonical control, VRM projection, and tracing remain
real boundaries because they isolate replaceable inputs, physical output, body compatibility, and
diagnosis. Several other thesis nouns do not deserve independent stateful subsystems yet.

## Decisions and protected boundaries

- A1 remains closed. EPR consumes body output through the existing SDL 3D path and does not acquire
  presentation ownership.
- A2 owns source and session truth. EPR will consume the eventual stable
  `(source_id, session_id)` contract through one ingress boundary and will not define a competing
  identity model.
- The first slice uses deterministic synthetic evidence. A temporary legacy evidence adapter is
  optional and, if added, is isolated at ingress.
- The existing portrait path remains functional and remains the default body.
- DirectComposition 3D, making 3D the default, arbitrary non-VRM humanoids, large gesture
  catalogues, contact planning, locomotion, balance, fingers, spring-bone control, and learned
  generation are outside the first slice.
- VRM 1.0 is the first body ecosystem. EPR semantics are not VRM property names, glTF node names,
  model-local axes, or renderer types.
- Runtime C structures are ephemeral. The research YAML remains a pressure test; it is not adopted
  as persistence or hot-reload format by this blueprint.

## Inspection boundary and relevance

The code inspection stayed on the path needed to answer an EPR ownership or integration question.

| Subsystem inspected | Current implementation | Why it matters to EPR |
| --- | --- | --- |
| normalized source events | `conversation.*`, `conversation_sources.*`, provider stream boundary | Establishes the evidence EPR can temporarily observe and the identity/order fields A2 must replace. |
| session and dialogue ownership | `session_registry.*`, `dialogue.*` | Identifies the real owners of session text, reveal cursors, streaming state, and per-dialogue performance tracks. |
| lifecycle and operational state | `state.*`, event handling in `app.c` | Shows that current global lifecycle state is not the eventual A2 operational truth and must not be copied into EPR as a session model. |
| affect and semantic beats | `affect.*`, `expression_director.*` | Supplies reusable beat evidence and exposes the current mixture of classifier, portrait expression, and physical-cue concerns. |
| delivery tracks | `delivery.*` and reveal-offset activation | Supplies deterministic source-offset anchors while exposing timing that currently lives in app update order. |
| body capability projection | `body-capabilities.md`, renderer readiness flags | Confirms a documented contract exists but no runtime body-capability projection object exists yet. |
| GLB/model loading and skinning | `model.*`, build-time Rio asset selection | Establishes what can be retained as GPU/render infrastructure and what VRM 1.0 metadata is entirely absent. |
| hierarchy and skeleton evaluation | `motion.*` | Reveals bind/current/model-world ownership, per-frame reset behavior, and Rio-specific prerequisites. |
| humanoid-role discovery | `humanoid.*` | Provides useful semantic roles and measurements but currently discovers them through node-name aliases, not VRM metadata. |
| semantic pose representation | `pose.*` | Provides a small task-space prototype while demonstrating that static arm endpoints are not a behavior or canonical control representation. |
| IK and pose solving | `ik.*`, `pose_solver.*` | Provides reusable two-bone math and a limited rollback pattern; lacks orientation, joint limits, and whole-state transactions. |
| procedural motion | `motion.*`, `motion_config.*`, portrait spring motion | Identifies reusable local motor ideas and the current hidden temporal ownership inside modality code. |
| renderer selection and presentation | selection in `app.c`, `draw.*`, `scene.*`, `raster_sdl_legacy.*` | Proves the SDL 3D target can validate EPR without EPR owning SDL, D3D11, DirectComposition, HWNDs, targets, or scene commits. |
| configuration and hot reload | `motion_config.*`, user settings | Supplies a good parse-validate-swap precedent and clarifies that the current seed is configured but unused by 3D motion. |
| snapshots and regression tests | `make check`, focused motion/pose/session/scene tests, hidden snapshot commands | Shows strong unit seams and hidden image capture, but no golden EPR trace/control comparison or automated causal reconstruction yet. |

The native DirectComposition implementation was not reopened. Its documented scene/presentation
contract and the final SDL model-texture handoff were sufficient to establish the boundary.

## 1. Existing-system map

### Actual evidence-to-presentation flow

```text
configured live source / relay / transcript recovery
        |
        v
provider parser -> EidolonConversationEvent
                  provider + session_id + turn/message ids
        |
        v
EidolonConversationBus (bounded ring; whole events may be dropped)
        |
        v
EidolonSessionRegistry, keyed today by (provider, session_id)
        |
        +--> session entry
        |      title, path, turn/message ids, output text
        |      streaming/activity/visibility/retirement state
        |      EidolonDialogue
        |          reveal/page state
        |          ExpressionTrack
        |          DeliveryTrack
        |
        +--> transcript discovery/recovery
        |
        v
app.c orchestration
        |
        +--> global EidolonState and global AffectController
        +--> classifier submission/result routing
        +--> reveal-offset activation of expression and delivery cues
        +--> portrait attention/expression/physical impulses
        |
        v
portrait renderer -> scene snapshot -> presentation backend
```

The current 3D path is parallel, not downstream of the session-performance path:

```text
build-time Rio GLB path
        |
        v
model loader -> motion hierarchy -> Rio-name finalize -> alias humanoid profile
        |
        +--> app-owned manual semantic-pose selection
        |
        v
every model frame:
    reset all local rotations to bind
    + analytic idle sine motion
    + optional copied two-arm semantic pose and analytic IK
    + hierarchy rebuild
    + skinning palette
        |
        v
D3D11 or SDL_GPU model render -> SDL_Texture
        |
        v
SDL legacy raster draws finished texture -> scene/presentation
```

There is currently no runtime connection from normalized session evidence, affect, expression
beats, or delivery cues to the 3D skeleton. There is also no current owner for a behavior plan,
resource ledger, canonical humanoid control state, plan revision history, or deterministic
performance trace.

### Current owners of relevant state

| State collection | Actual owner now | Reconciliation |
| --- | --- | --- |
| configured source definitions and source connection/capability status | `EidolonConversationSources` | Remains outside EPR; A2 will refine source-instance truth. |
| normalized event queue and overflow count | `EidolonConversationBus` | Remains transport plumbing; EPR must receive only accepted/reconciled evidence. |
| legacy `(provider, session_id)` identity, title/path, activity, output, streaming, visibility, retirement | `EidolonSessionRegistry` | A2-owned migration; EPR does not copy or replace it. |
| per-session page/reveal cursor and source-offset mapping | each `EidolonDialogue` inside its session entry | Remains dialogue ownership. It publishes anchors/evidence; EPR never owns visible text. |
| fallback IPC dialogue | `EidolonApp.dialogue` | Legacy compatibility only; not a new identity source. |
| expression beats, classifier correlation/readiness, active beat | each dialogue's `EidolonExpressionTrack` | Split: segmentation/classifier evidence stays upstream; body-neutral performance intent crosses EPR ingress. |
| delivery marks and next emitted mark | each dialogue's `EidolonDeliveryTrack` | Source-offset marks are reusable evidence; behavior timing and resource selection move into EPR. |
| global hook lifecycle state | `EidolonApp.state` | Remains current app state; it is not truthful per-session A2 operational state. |
| current/target affect and selected portrait expression intent | `EidolonApp.affect` | Portrait behavior remains supported. EPR receives body-neutral affect dimensions, not portrait catalogue indexes. |
| classifier lifetime, request sequence, and result routing | app/classifier integration | Remains semantic evidence production, outside frame-critical EPR control. |
| shared-character performance ownership across visible sessions | no explicit owner | Must become explicit before live multi-session EPR; it cannot be inferred from registry array order. |
| selected body renderer and effective/persisted preference | app plus system/user settings | Remains product/config ownership. EPR sees a body capability profile, never a renderer selection. |
| motion configuration and watch revision | `EidolonApp.motion_config` and `motion_config_watch` | Parse-validate-swap pattern is reusable; first EPR configuration remains bounded and versioned. |
| GLB resources, GPU state, skinning joint map and palette | `EidolonModelRenderer` | Retain as renderer/body-adapter infrastructure; remove behavior selection and private semantic-pose ownership. |
| model nodes, bind transforms, mutable local transforms, world transforms | `EidolonModelRenderer.motion` | Becomes the VRM adapter's model-local projection state, not EPR's semantic truth. |
| humanoid alias mapping and measurements | `EidolonModelRenderer.humanoid` | Replace discovery with VRM metadata for the VRM path; reuse measurements and semantic-role concepts where correct. |
| selected semantic pose | duplicated in `EidolonApp.semantic_pose` and `EidolonModelRenderer.semantic_pose` | Replace with one EPR-published canonical control snapshot plus renderer-owned last consumed revision. |
| current physical body state and velocity | no persistent canonical owner | New EPR control/solve owner; current code reconstructs from bind pose each model frame. |
| scene layer identities and content/presentation revisions | `EidolonScene` | Remains downstream. EPR does not publish scene layers. |
| window, targets, input regions, cadence, scene commit, present | selected presentation backend | Protected A1 ownership; forbidden to EPR. |

### Current operational and semantic behavior

- `EidolonState` contains `idle`, `running`, `waiting`, `review`, and `failed`. It is global and
  hook-oriented. Conversation events frequently flatten new session output into global `review`;
  this is not sufficient as per-session listening, thinking, responding, interrupted, completed, or
  errored truth.
- Expression beats are attached to original UTF-8 source offsets and incrementally extend stable
  prefixes. This is the strongest reusable temporal provenance in the current system.
- Dialogue reveal may block at a beat until classifier evidence is ready. That protects portrait
  synchronization today, but it violates the intended rule that language-scale planning must not
  block frame-critical presence. EPR must accept late evidence and use deterministic fallback
  without freezing dialogue.
- Delivery tracks deterministically derive marks such as hesitation, accent, contrast, landing,
  question, and exclamation. They correctly preserve source order and prevent ordinary replay with
  `next_index`; they do not express plan commitments or body-resource ownership.
- Body capability projection exists in design language, not in production structures. Runtime code
  has only coarse renderer readiness and `humanoid_ready`.
- Every visible session currently activates performance in registry entry order and mutates the same
  global portrait affect/attention state. The last eligible entry wins. This is an actual
  update-order ownership bug, not merely a thesis concern.

## 2. Thesis-to-code mapping

The decisions below use five dispositions:

- **reuse** means the invariant and ownership can remain;
- **extend** means preserve the seam while adding a missing invariant;
- **split** means useful behavior exists but ownership is mixed;
- **replace** means the current representation cannot safely serve the EPR role;
- **new** means no production equivalent exists.

| EPR subsystem | Closest current code/data | Decision for the first slice |
| --- | --- | --- |
| Performance Intent | `EidolonState`, affect dimensions, expression beats, delivery marks | **Split and new contract.** Preserve upstream semantic evidence, but introduce a versioned immutable intent value carrying operational posture, affect, beat anchors, urgency, continuity, and opaque provenance. Do not copy classifier labels or portrait expression indexes. |
| Performance Intent normalizer | app orchestration and track preparation | **Do not create a stateful subsystem.** Ingress validation/adaptation is enough. A separate normalizer would duplicate A2 and semantic owners without protecting an invariant. |
| Behavior Planner | no equivalent; scattered app cue handling | **New, bounded deterministic rule planner.** It turns intent deltas into behavior units and explicit revisions. It does not own text classification, physical solving, or rendering. |
| Behavior Plan Graph | expression beat array and delivery mark array are the nearest shapes | **New.** Use a fixed-capacity graph with stable behavior ids, provenance, lifecycle state, dependencies, anchors, claims, and plan generation. Runtime YAML/object graphs are unnecessary. |
| temporal network and dispatcher | reveal offsets, delivery `next_index`, model 33 ms throttle, modality-local springs/sines | **Replace central timing; retain local motor integration.** Implement a bounded incremental STN-like network over integer logical ticks. It needs difference constraints and observed/controllable anchors, not a general STNU or Allen-algebra engine. |
| behavior units and synchronization anchors | `EidolonExpressionBeat`, `EidolonDeliveryMark` | **Extend semantics.** Preserve text spans and source-offset anchors as provenance. Add preparation, onset, nucleus/peak, recovery, completion, interruption, and settle anchors. |
| body-resource claims | none | **New.** Claims name semantic hierarchical resources, mode, priority, interval, preemption policy, and transfer/cleanup policy. |
| resource arbitration | implicit app update order and model pose overwrite | **Replace.** One deterministic arbiter owns grants, denials, preemption, transfer, and release. Explicit rank tuples and stable ids decide ties; container order may not. |
| co-articulation | affect smoothing, portrait spring, idle added before semantic arm pose | **Split.** Reuse bounded smoothing/integration ideas inside realizers. New controller composition blends only compatible granted programs and traces suppression or transfer. |
| interruption and revision | expression stable-prefix extension; dialogue repair; clearing a semantic pose | **Replace plan semantics.** Preserve the useful stable-prefix idea. Every accepted intent revision creates or extends a plan generation; stale generations cannot mutate current state. Completed phases stay retired, uncommitted future work may be replaced, and executing work receives an explicit interrupt/settle path. |
| modality realizers | portrait cue logic, idle motion, semantic arm goals, analytic IK | **Split.** First slice has posture, gaze, one-arm gesture, neutral expression, and idle realizers. They consume granted behavior units and emit programs; they never choose sessions, inspect classifier labels, or draw. |
| Realization Program IR | `EidolonSemanticPose` is a small arm-task prototype | **Replace with a versioned ephemeral IR.** The IR is body-semantic and model-neutral, but necessarily modality-tagged. Calling it “modality-neutral” is incoherent because posture, gaze, expression, and gesture have different channels and constraints. |
| controller composition | no equivalent | **New.** At fixed logical ticks, sample active granted programs, compose base/additive/override channels in defined order, and produce one candidate canonical state. |
| canonical humanoid control state | model-local `EidolonMotionRig` mutable rotations/world matrices | **New.** Own semantic joint orientations, gaze targets, expression weights, root/posture channels, velocities needed for cleanup, validity, tick, and revision. Model-local nodes and matrices are projections, not canonical truth. |
| physical constraints and solving | `humanoid` measurements, two-bone `ik`, four-joint rollback in `pose_solver` | **Reuse math, extend transaction.** Reuse two-bone geometry and measurement concepts. Add wrist orientation, joint limits, finite/range validation, capability-aware local degradation, and whole-candidate commit/rollback against the last valid state. |
| VRM capability projection | alias-based `humanoid`, documented body capability contract | **New VRM 1.0 adapter.** Parse `VRMC_vrm` humanoid, expressions and look-at metadata; validate the skin/hierarchy; publish a capability/body profile; map canonical output to model nodes/morphs. Name aliases remain only a non-VRM compatibility path later. |
| deterministic trace | unstructured logs and focused unit assertions | **New structured trace.** Record accepted evidence, intent, plan revision, temporal decisions, resource arbitration, realization choice/fallback, solve transaction, capability degradation, feedback, and canonical output hash. |
| runtime feedback | renderer failure flags and logs | **New bounded feedback.** Realizer/solve/capability failures feed the current plan revision as typed results; they do not mutate source truth or trigger unbounded replanning in a render frame. |

### Minimal logical module boundaries

These are ownership boundaries, not a demand for one source file per thesis noun:

```text
performance ingress
    validates immutable intent/evidence and provenance
            |
            v
behavior_plan
    owns plan generations, behavior lifecycle, temporal constraints, dispatcher
            |
            v
body_resources
    owns claims, grants, preemption, transfer, release
            |
            v
realization
    owns modality realizers, Realization Programs, controller composition
            |
            v
canonical_control
    owns current/last-valid candidate, physical solve transaction
            |
            v
vrm_body
    owns VRM metadata, body profile/capabilities, model-local projection
            |
            v
model renderer
    owns GPU resources, skinning, camera, pixels
```

`performance_trace` observes typed decisions at every boundary without becoming an owner of those
decisions. A small `performance_runtime` facade owns instances, bounded queues, publication, and
cross-module transaction order. It must not become a god controller.

## 3. Contradiction report

### Confirmed contradictions and corrections

1. **Shared-body performance currently has no owner.** Visible sessions mutate one portrait
   controller in registry array order. EPR cannot inherit this. An explicit performance subject or
   lease must enter EPR before any behavior planning.

2. **The thesis hard-codes `uint64_t source_id` and `session_id`.** A2 has not yet fixed the runtime
   representation, and stable external ids may be strings or opaque values. EPR will preserve A2
   values verbatim at its boundary or use boundary-interned handles whose equality derives solely
   from A2. It will not define identifier allocation, restart identity, or adapter-kind semantics.

3. **A stateful Performance Intent Normalizer is not justified.** Existing upstream systems already
   own operational and semantic evidence. EPR needs a contract validator/adapter, not a duplicate
   truth collection.

4. **The full thesis temporal machinery is too broad for V1.** A bounded incremental network of
   integer difference constraints is sufficient for preparation, onset, peak, recovery,
   interruption, and observed stream anchors. General uncertainty calculus, contact timing,
   locomotion, and whole-body coordination would add machinery before an invariant needs it.

5. **“Modality-neutral Realization Program” is the wrong promise.** Programs must identify posture,
   gaze, expression, or gesture channels to compose and validate them. The correct invariant is
   body-semantic and model/renderer-neutral, with explicit modality.

6. **The current GLB loader is not a latent VRM adapter.** It does not parse VRM 1.0 extensions,
   first-person/look-at metadata, expression morph/material binds, node constraints, or spring
   bones. It accepts one skin, basic PBR base-color texture, joint/weight attributes, and triangles.
   VRM support is a new semantic loader/adapter above reusable glTF/GPU plumbing.

7. **Model-local bone data currently masquerades as semantic state.** The mutable `MotionRig`
   contains glTF node transforms and Rio cache indexes. It is useful projection state, not a
   canonical humanoid control state.

8. **The working 3D path is Rio-coupled before generic humanoid discovery runs.** Motion
   finalization requires exact `Bip001 ...` node names. The later alias profile does not remove that
   prerequisite. VRM loading must bypass the Rio finalize contract and derive roles from
   `VRMC_vrm`.

9. **Semantic and rendering concerns are mixed.** `EidolonModelRenderer` stores and solves a copied
   semantic pose. The app also stores the pose. The renderer should consume a versioned solved pose
   snapshot and own only model/GPU state. Portrait code does not consume GoEmotions labels directly,
   which is a good boundary to preserve, but the shared expression-intent enum is documented in code
   as Asuna manifest order and is not body-neutral.

10. **Language-scale readiness can stall dialogue.** Expression classification may block reveal at
    a beat. EPR planning must never block frame-critical body presence or dialogue progress. A late
    classification result may revise uncommitted future behavior; otherwise deterministic neutral
    fallback proceeds.

11. **Temporal behavior is hidden inside modalities.** The model owns a private 33 ms update gate,
    procedural sine phase, and a bind-pose reset every model frame. Portrait owns spring timing.
    Local motor integration should remain local, but semantic onset, synchronization, commitment,
    interruption, and completion must be central and inspectable.

12. **Resource conflicts are decided by overwrite/update order.** Idle applies first, semantic arm
    pose applies second, and visible dialogue sessions update one portrait controller in sequence.
    These are not arbitration policies.

13. **Cleanup does not use current physical state.** Clearing a pose simply returns the next model
    frame to bind plus idle. There is no persistent velocity or canonical state from which to
    retract, transfer, or settle after interruption.

14. **Incremental repair is not a behavior revision model.** Compatible expression prefixes
    preserve prepared beats, but an incompatible rebuild replaces the track while preserving only
    coarse track metadata. Active/committed behavior state is not preserved. That is acceptable for
    current portrait classification recovery; it cannot govern physical phase replay.

15. **Failure transactions are too narrow.** The arm solver rolls back four joint rotations if
    either arm fails, which is a useful precedent. It does not protect a whole canonical candidate,
    velocities, gaze, expression, or model projection. Model failure falls back to newly generated
    idle, not the last valid physical state.

16. **The thesis schema must not become an accidental durable ABI.** Current pose arrays and runtime
    structs are unversioned compiled data, while motion/user configuration correctly uses explicit
    versions and transactional swap. First-slice plans/programs are ephemeral. Any future authored
    behavior format requires a separate versioned loader and migration policy.

17. **Documented body-capability projection has no runtime owner.** `humanoid_ready` is a binary
    flag. EPR needs a typed profile that distinguishes required humanoid bones from optional eyes,
    look-at, expression presets, upper chest, shoulders, and other local degradations.

18. **No compile-time dependency cycle was found in the implicated modules.** The problem is a
    central app orchestrator with mixed ownership, not a literal include/link cycle. EPR should not
    invent a cycle-breaking abstraction where none exists; it should remove behavior state from
    app and model through one-way immutable publication.

19. **Presentation state is not yet inside EPR, and must stay out.** The existing final handoff is
    appropriately narrow: model produces a texture; SDL legacy draws it; scene/presentation owns
    geometry, input, hosts, targets, cadence, and commits. EPR may publish body-control and optional
    body-geometry metadata to the body renderer, never scene or native-target state.

### Thesis concepts deliberately deferred

- contact graphs, planted-foot constraints, balance support polygons, locomotion and root
  translation;
- fingers, two-arm coordinated gestures, props and environment contacts;
- VRM spring-bone authority beyond declaring update order and not double-owning joints;
- authored graph loading, behavior marketplaces, neural generators, and online learning;
- general non-VRM humanoid compatibility;
- DirectComposition 3D and native swapchain/target work.

Deferral means the first contracts leave these extensions possible; it does not mean implementing
empty interfaces for them now.

## 4. A2 integration boundary

### Ownership

A2 owns:

- configured `source_id` and the distinction between source instance and adapter kind;
- `(source_id, session_id)` identity, reconnect and restart continuity;
- accepted source-local ordering/revision and stale-event rejection;
- source connection/failure, session lifecycle, turn, response, interruption, completion, and error
  truth;
- transcript recovery semantics;
- the durable correlation fields from which message/response provenance is formed.

EPR owns:

- the body-performance consequences of already accepted truth;
- plan generations and behavioral commitments;
- resource grants, physical realization, interruption cleanup, settling, and trace;
- retention of provenance on every causal decision.

The shared-character performance-subject selection must be explicit at the integration boundary. It
may be supplied by A2/session coordination or by a separate product policy agreed with the A2
owner. EPR will consume an explicit subject/lease transition and will never infer the owner from
session array order, visibility order, or adapter kind.

### Logical ingress envelope

The A2 representation is intentionally not guessed. EPR requires the following logical fields:

```text
accepted truth revision
evidence kind
logical/observed time
operational payload or semantic payload
performance-subject lease/focus, when applicable

provenance:
    source_id              opaque stable A2 value
    session_id             opaque stable A2 value
    adapter_kind           diagnostic only; never identity
    turn_id                optional opaque A2 value
    response_id            optional opaque A2 value
    message_id             optional opaque A2 value
    semantic_beat_id       optional stable semantic producer value
    text span              optional original UTF-8 byte range
    user_interaction_id    optional stable interaction value
    causing truth revision
```

At runtime the boundary may intern opaque values into bounded handles for efficient equality and
copying. The interner remains an adapter-owned cache; handles are never persisted, hashed into a new
identity scheme, or interpreted by EPR. Structured traces serialize the original A2 values.

EPR consumes immutable accepted deltas or snapshots for:

- session becomes the explicit performance subject or loses that lease;
- listening/attention target changes;
- thinking/preparation begins or ends;
- response begins, extends, completes, is interrupted, or errors;
- semantic beat becomes provisional, stable, revised, or final;
- delivery/source-offset anchor becomes observed;
- user interaction changes attention or urgency.

Source connect/disconnect alone does not directly select a gesture. It matters only through truthful
operational state or an explicit performance-subject transition.

### Before A2 lands

The first vertical slice uses a deterministic synthetic evidence fixture with fully populated
fixture provenance. This is preferable to letting legacy identity assumptions harden in core
structures.

If live demonstration is needed before A2 is available, one optional
`legacy_performance_evidence_adapter` may translate current accepted normalized events and
per-dialogue tracks into the ingress envelope. Its constraints are:

- legacy `(provider, session_id)` appears only inside that adapter;
- its synthesized source key is marked ephemeral/compatibility in traces;
- no EPR plan, behavior, program, resource, control, or trace type gains a `provider` field;
- it does not claim restart identity or stale-revision guarantees the current path lacks;
- replacing it with the A2 adapter requires no EPR-core migration.

## 5. Proposed repository documentation

After approval, canonical design documentation should be created before or alongside code. Seven
documents are enough to keep ownership legible without one enormous specification.

| Proposed document | Owns | Must not own |
| --- | --- | --- |
| `docs/design/epr-overview.md` | EPR boundary, instance ownership, data/cadence flow, lifecycle of published plan/control snapshots, non-goals, integration map | detailed temporal algorithms, VRM fields, presentation or session truth |
| `docs/design/epr-performance-intent.md` | immutable intent/evidence vocabulary, stability/revision semantics, A2 provenance requirements, synthetic/legacy adapters | source identity allocation, classifier implementation, behavior selection |
| `docs/design/epr-behavior-plan.md` | behavior-unit lifecycle, plan generations, incremental temporal constraints, dispatcher, interruption/revision/retirement | physical curves, body mappings, rendering |
| `docs/design/epr-body-resources.md` | semantic resource hierarchy, claim modes, deterministic ranking, grants, preemption, transfer, co-articulation compatibility, cleanup obligations | gesture choice, IK math, GPU joints |
| `docs/design/epr-realization-program.md` | modality realizer contract, versioned ephemeral IR, controller composition, canonical control state, transactional solve order and bounded feedback | VRM extension parsing, authored persistence format, presentation |
| `docs/design/vrm-body-runtime.md` | VRM 1.0 validation, humanoid/expression/look-at mapping, capability profile, model-local projection, missing-capability behavior, physics ownership/update order | semantic behavior selection, source/session truth, SDL/D3D/DirectComposition ownership |
| `docs/design/epr-tracing-validation.md` | deterministic trace schema, causal reconstruction, hashes/snapshots, fault injection, static boundary tests, owner-evaluation boundary | product telemetry policy, artistic acceptance decisions |

The following existing documents then receive focused amendments rather than duplicated prose:

- `docs/architecture.md`: insert EPR between accepted performance evidence and body renderers;
- `docs/design/expression-performance.md`: define expression beats as upstream evidence and preserve
  portrait compatibility;
- `docs/design/procedural-motion.md`: move semantic scheduling/resource decisions to EPR while
  retaining bounded motor integration in realizers;
- `docs/design/body-capabilities.md`: link the generic capability contract to VRM projection;
- `docs/design/session-dialogue.md`: name the explicit performance-subject owner/lease and remove
  array-order ambiguity;
- `docs/assets.md` and `docs/configuration.md`: document the legal VRM package and only the
  configuration actually implemented;
- `docs/project-state.md` and the roadmap: update only after evidence establishes the new gate state.

The thesis, corpus, schema, and this reconciliation remain research/decision artifacts in
`docs/epr`; they do not silently become canonical production contracts.

## 6. First vertical slice

### Scenario

One deterministic fixture exercises the complete path through the existing SDL 3D renderer:

```text
synthetic accepted session evidence
        |
        v
versioned Performance Intent
        |
        v
incremental Behavior Plan + temporal constraints
        |
        v
explicit resource grants / transfer
        |
        v
posture + gaze + right-arm gesture realizers
        |
        v
Realization Programs + canonical controller composition
        |
        v
transactional constrained solve
        |
        v
VRM humanoid + look-at + optional expression projection
        |
        v
model renderer texture
        |
        v
existing SDL legacy 3D presentation
```

The fixture uses a fixed logical clock, fixed configuration, and fixed seed. Illustrative logical
times below become fixture constants only after implementation review.

| Time | Accepted evidence and plan change | Visible behavior and resource result |
| --- | --- | --- |
| 0 ms | no active session; plan generation 1 | neutral idle; quiet breath/sway residuals; stable ground posture |
| 400 ms | fixture session gains explicit performance lease; user attention observed | attentive/listening posture begins; eyes lead toward user target, head follows after a constrained delay |
| 1100 ms | thinking/preparation truth enters generation 2 | one long-lived contained attentive posture; gaze softens; idle remains only on compatible residual channels |
| 2000 ms | streamed response begins; generation 3 | posture opens slightly; gaze moves toward response/bubble target with eye-first/head-follow timing |
| 3000 ms | a stable semantic contrast beat and UTF-8 span enter generation 4 | restrained right-arm contrast gesture is planned; preparation, stroke peak, recovery, and source anchor are explicit |
| 3200–3570 ms | gesture prepares and executes; peak is constrained to the contrast anchor | posture keeps torso/left arm; gesture receives right-arm-chain override; gaze retains eyes/head under cooperative claims |
| 3600 ms | interruption truth supersedes uncommitted future work in generation 5, just after the gesture peak and during execution/recovery | delivered preparation/stroke phases retire and cannot replay; old recovery is cancelled; attention snaps eye-first to the interrupt source |
| 3600–3920 ms | arbiter transfers the right arm from gesture to interruption-settle program | cleanup begins from the actual sampled hand/joint state, not bind or authored ground; head follows eyes; posture becomes attentive/guarded |
| after 3920 ms | no new evidence; generation 5 remains current | resources release deterministically into a stable continued-presence posture; bounded idle resumes only where compatible |

This exercises one gesture, not a catalogue. The gesture peak is actually delivered and synchronized;
interruption then proves that completed phases do not replay and abandoned future motion cleans up
from current state.

### Behavior units and lifecycle

The first slice needs these behavior units:

- `presence.neutral_idle`: long-lived lowest-priority residual motion;
- `posture.attentive`: long-lived base posture for listening/responding;
- `posture.thinking_contained`: revisable long-lived thinking posture;
- `gaze.attention`: target plus eye-lead/head-follow constraints;
- `gesture.contrast_right_restrained`: one right-arm preparation/stroke/recovery unit tied to a
  semantic beat;
- `posture.interrupted_guarded`: urgent replacement posture;
- `settle.right_arm_to_guarded`: generated transactionally from current canonical state.

Every unit has a stable behavior id, provenance, plan generation, state, anchors, claims, selected
realizer, and terminal reason. Lifecycle states are:

```text
proposed -> scheduled -> committed -> executing -> retired(completed)
                    \            \-> retired(cancelled/interrupted/failed)
                     \-> retired(revised/denied)
```

Only uncommitted future behavior may be freely replaced. An executing unit may be interrupted only
through its declared policy. Completed phases and observed anchors are immutable facts in later plan
generations.

### Temporal contract

- Time is integer logical ticks; wall-clock sampling is an adapter concern.
- The planner incrementally inserts difference constraints between observed source anchors and
  behavior phase anchors.
- Every insertion validates the whole affected component before publication.
- The dispatcher commits only inside a configured horizon or after an observed anchor makes a phase
  unavoidable.
- A new plan generation references its predecessor and records preserved, added, revised, and
  retired nodes.
- The frame/control path reads one immutable published generation and performs bounded sampling; it
  never segments text, calls a classifier, parses configuration, or mutates the graph.
- Stale intent or plan generations are rejected before any resource or control mutation.

### Body-resource contract

The first resource hierarchy is deliberately small:

```text
body
├── torso
├── head
├── eyes
├── face_expression
├── left_arm_chain
└── right_arm_chain
```

Claims support:

- `base`: establishes a reference posture;
- `additive`: bounded residual compatible with the current base;
- `cooperative`: combines through a named composition rule, such as head aim over posture;
- `override`: exclusive task control for the interval.

The arbiter ranks requests by explicit urgency/priority, committed phase, anchor time, provenance
class, and stable behavior id. It never uses pointer, array, hash-bucket, registration, or iteration
order. Every decision emits a trace record.

For the fixture:

- attentive/thinking/guarded posture owns the torso and supplies base arm states;
- idle receives additive residual grants only on compatible torso/head channels;
- gaze receives eyes plus cooperative head control;
- the contrast gesture receives a timed right-arm override;
- interruption preempts future gesture recovery, then performs an explicit right-arm transfer to a
  settle program created from the current canonical state;
- the settle program releases the arm only after its terminal ground and velocity tolerances pass.

### Realization Programs and control

Each deterministic realizer emits a bounded, versioned program:

- posture: semantic base joint targets, weights, onset/settle curves;
- gaze: semantic target, eye/head contributions, lead/follow delay, range/fallback policy;
- gesture: task-space hand path, elbow-pole policy, wrist orientation, phase anchors;
- expression: neutral/focused VRM expression weights when supported;
- idle: seeded residual channels with bounded amplitude and phase.

Programs refer only to semantic resources, anchors, normalized body measures, and capability names.
They contain no SDL, D3D11, DirectComposition, Windows, glTF node index, or VRM JSON field.

At a fixed logical control tick:

1. read one immutable plan generation and its current grants;
2. sample active programs;
3. compose base, cooperative, additive, and override channels in specified order;
4. clone the last valid canonical state into a candidate;
5. solve gaze, posture, and arm constraints against the body profile;
6. validate finite values, joint/range limits, continuity, and required capability use;
7. atomically commit the complete candidate or retain the last valid state;
8. publish an immutable control snapshot and deterministic hash;
9. pass the snapshot to the VRM adapter; the model renderer samples the latest complete projection.

A realizer or solve failure cannot leave half of an arm, stale expression weights, or a partially
updated gaze in the current state.

### VRM 1.0 projection

The first VRM adapter must:

- parse and validate VRM 1.0 humanoid metadata rather than infer VRM roles from names;
- load the skinned glTF hierarchy using reusable geometry/texture/GPU plumbing;
- publish normalized body axes, dimensions, reachable arm lengths, relevant joint limits or safe
  defaults, and optional capabilities;
- project semantic posture and arm controls into the VRM humanoid;
- use VRM look-at for the eye component while head/torso orientation remains canonical control;
- map neutral/focused expression weights if the body supplies suitable expression binds;
- leave unsupported optional expression/look-at channels neutral and trace one local degradation;
- declare spring-bone/node-constraint update ownership even though active spring control is deferred;
- reject invalid required humanoid structure transactionally without affecting portrait/session
  operation.

The visible body asset must be a legally usable VRM 1.0 file with compatible license metadata.
Existing Rio GLB proves the renderer but is neither the VRM contract nor the first shippable VRM
body. Asset selection/acquisition is a Phase 2 input and any external download remains separately
approval-controlled.

### Deterministic trace

Trace output is structured, bounded, and event/decision oriented rather than a per-frame log flood.
A representative causal chain is:

```json
{"tick":3000,"plan_generation":4,"event":"intent.accepted","cause":"semantic_beat","beat_id":"contrast-1"}
{"tick":3000,"plan_generation":4,"event":"behavior.selected","behavior_id":"gesture-contrast-1","realizer":"gesture.contrast_right_restrained"}
{"tick":3000,"plan_generation":4,"event":"resource.granted","behavior_id":"gesture-contrast-1","resource":"right_arm_chain","mode":"override"}
{"tick":3510,"plan_generation":4,"event":"anchor.observed","behavior_id":"gesture-contrast-1","anchor":"stroke_peak","cause_beat_id":"contrast-1"}
{"tick":3600,"plan_generation":5,"event":"plan.revised","cause":"response.interrupted","preserved":["gesture-contrast-1:stroke_peak"],"retired":["gesture-contrast-1:recovery"]}
{"tick":3600,"plan_generation":5,"event":"resource.transferred","resource":"right_arm_chain","from":"gesture-contrast-1","to":"settle-right-arm-1"}
{"tick":3920,"plan_generation":5,"event":"behavior.retired","behavior_id":"settle-right-arm-1","reason":"settled"}
```

Production fields additionally retain the opaque A2 provenance described above and the canonical
control hash. Tests reconstruct the decision chain from trace records alone.

## 7. Acceptance criteria

### Automated evidence

Structural correctness is established by `make check` targets and deterministic fixtures, not by
owner log inspection.

1. **Determinism:** identical accepted evidence, configuration, body profile, logical tick sequence,
   and seed produce byte-identical normalized trace records and canonical control snapshots/hashes.
2. **Order independence:** permuting insertion, storage, hash, and iteration order produces the same
   selected behaviors and grants. Static review plus permutation tests reject implicit first/last
   wins behavior.
3. **Temporal validity:** every incremental insertion preserves all network constraints; rejected
   inconsistent insertions leave the published generation unchanged.
4. **Phase monotonicity:** interruption/revision cannot replay a completed or observed phase, and a
   phase cannot move backward through lifecycle states.
5. **Explicit resources:** every non-neutral control contribution has a live grant; conflicts,
   preemption, transfers, cleanup obligations, and release are traceable and deterministic.
6. **Stale revision safety:** an older truth/intent/plan generation cannot mutate current plan,
   grants, canonical state, or VRM projection.
7. **Transactional realization:** injected realizer, composition, constraint, or projection failure
   retains the last complete valid state and records one typed failure/fallback.
8. **Local capability degradation:** fixtures independently remove optional eyes/look-at and
   expressions; only the affected channel degrades while posture, arm gesture, dialogue, and
   session observation continue.
9. **Required capability failure isolation:** invalid VRM initialization prevents that body from
   becoming active, preserves portrait/default operation, and does not stop source/session
   observation.
10. **Renderer/presentation neutrality:** EPR core headers/sources fail a static dependency check if
    they include or link SDL, D3D11, DirectComposition, Win32, HWND, swapchain, presentation, or
    scene ownership.
11. **Portrait/session regression isolation:** app-level fault-injection tests prove portrait and
    normalized session observation still operate when EPR construction, VRM loading, realization,
    or solve fails.
12. **Causal reconstruction:** trace tests can answer which evidence and semantic beat selected a
    visible behavior, which temporal rule scheduled it, why each resource was granted or denied,
    what interruption revised, and which fallback produced the final control.
13. **Bounded frame path:** tests/static validation prove no classifier, text segmentation, file
    parse, graph allocation, or unbounded plan loop occurs in the control/render sampling path.
14. **Snapshot separation:** canonical trace/control snapshots are automated golden data with
    semantic diffs. Existing hidden PNG snapshots remain useful render evidence but are not the sole
    structural oracle.
15. **SDL vertical slice:** a deterministic integration fixture reaches the current SDL 3D model
    texture path with monotonically consumed control revisions while the configured default remains
    portrait.
16. **No normal manual verification:** one `make check`-reachable command produces the structural,
    deterministic, fault, and boundary evidence. The user is not asked to inspect routine logs or
    reproduce assertions manually.

### Owner-controlled evidence

Owner judgment is limited to:

- approving or revising this architecture blueprint;
- judging the final SDL 3D sequence for readable attention, thinking, streamed-response continuity,
  restrained contrast, interruption, cleanup, settling, and continued presence;
- deciding whether the chosen VRM body and motion tuning feel alive and artistically appropriate.

Owner evaluation does not replace deterministic tests and is not required to diagnose ordinary
structural failures.

## Implementation sequence after approval

1. **Canonical contracts:** create the seven proposed design documents and amend the existing
   architecture/expression/motion/body/session documents. Freeze first-slice non-goals and trace
   vocabulary.
2. **Headless evidence and plan core:** add the synthetic fixture, immutable intent ingress, plan
   generations, bounded temporal network, behavior lifecycle, revisions, and trace.
3. **Resource core:** implement the semantic resource hierarchy, explicit ranking/grants,
   preemption, transfer, cleanup obligations, and permutation tests.
4. **Realization and canonical control:** implement idle, posture, eye-first/head-follow gaze, the
   one restrained right-arm gesture, programs, controller composition, and whole-state
   transactional solve. Reuse verified IK math where it satisfies the new contract.
5. **VRM 1.0 body:** validate one legally usable VRM, publish its body profile/capabilities, map
   humanoid/look-at/optional expression channels, and prove local degradation.
6. **Existing renderer integration:** make the current model renderer consume immutable solved pose
   revisions while retaining its GPU loading/skinning/camera/texture responsibilities. Keep the SDL
   legacy 3D presentation handoff unchanged.
7. **Complete fixture and fault suite:** run the entire timeline through SDL 3D, produce
   deterministic trace/control artifacts, exercise stale revisions and failures, and run the
   ordinary regression suite.
8. **Optional pre-A2 compatibility:** only if useful for owner evaluation, add the isolated legacy
   evidence adapter. Otherwise wait for A2 and integrate its accepted contract directly.
9. **Owner feel gate:** ask only for artistic/performance evaluation and tune bounded realization
   parameters without changing ownership.

No step enables DirectComposition 3D, changes the default body, migrates source identity, or expands
the gesture catalogue. Those remain later gates in the stated trajectory.

## Approval gate

Production work stops here. Owner approval may accept this blueprint as written or request specific
changes. Approval authorizes Phase 2 documentation and the first SDL 3D / VRM vertical slice; it
does not authorize pushing, merging, native DirectComposition 3D, or making 3D the default.
