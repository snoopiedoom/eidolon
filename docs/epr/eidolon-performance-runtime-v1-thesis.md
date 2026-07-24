# Eidolon Performance Runtime V1

## A theoretical design for a continuously alive, deterministic VRM embodiment

**Status:** research thesis and architecture proposal  
**Date:** 2026-07-24  
**Project:** Eidolon  
**System name:** **Eidolon Performance Runtime (EPR)**  
**Central subsystem:** **Behavior Realizer**

> **An alive body is not a stream of poses. It is a continuously revised, resource-constrained multimodal performance whose semantic commitments, physical state, and temporal obligations remain coherent under partial information.**

---

## 1. Thesis

Eidolon should not map emotions to animations, language-model tokens to joints, or semantic labels to pose presets. Those approaches can create movement, but not a body that feels inhabited.

The Eidolon Performance Runtime should instead maintain one persistent physical performer. That performer has:

- an actual current body state rather than an assumed neutral origin;
- durable posture, attention, facial and movement-quality state;
- pending semantic and operational obligations;
- multiple simultaneously active behaviors;
- explicit ownership and composition rules for shared body resources;
- temporal commitments that can still be revised while language is streaming;
- motor programs that generate preparation, connection, stroke, hold, recovery and settling from the state that actually exists;
- deterministic fallbacks whenever a requested capability, timing, contact or solve is unavailable.

The machine is therefore neither a conventional animation state machine nor a single inverse-kinematics controller. It is an **incremental multimodal performance compiler and dispatcher**.

Its inputs are truthful operational state, semantic message beats, continuous affect, attention, delivery timing and character style. Its output is a physically valid VRM control state: humanoid pose, expression weights, gaze, node constraints and secondary response.

```text
truthful session activity + stable streamed language evidence
                            ↓
                 renderer-neutral performance intent
                            ↓
                     behavior planning
                            ↓
              incrementally revised behavior plan
                            ↓
        temporal dispatch + body-resource arbitration
                            ↓
          modality-specific realization compilation
                            ↓
               controller and constraint composition
                            ↓
                   canonical humanoid control state
                            ↓
              VRM projection and physical realization
                            ↓
                     final visible performance
```

The design intentionally joins ideas that previous systems usually handled separately:

- SAIBA and BML's separation between intent, behavior choice and realization;
- ASAP/Elckerlyc's mutable plans, symbolic synchronization anchors, anticipators, graceful interruption and co-articulation;
- MURML and ACE's form-based gesture descriptions and executable motor programs;
- EMBR's explicit animation-realization layer between behavior and rendering;
- SmartBody's specialized controller hierarchy and mapped semantic joints;
- Greta's separation of communicative behavior from character style and expressive qualities;
- MOSIM's interchangeable motion-unit boundary and canonical intermediate skeleton;
- modern animation runtimes' masking, additive layers, inertialization and constrained IK;
- VRM 1.0's standardized humanoid, expression, look-at, node-constraint, spring-bone and rights substrate.

No examined system combines all of those with Eidolon's particular upstream evidence: live agent truth, stable streamed semantic prefixes, source-offset delivery timing, one persistent body shared across concurrent agent sessions, and a low-level native runtime expected to stay open all day.

The novel object is not a new solver. It is the **contract between semantics, time, resources, motor programs and embodiment**.

---

## 2. Scope of V1

“V1” means the first architecturally complete performance machine. It does not mean a cheap minimum feature set.

V1 shall define and support:

1. incremental performance planning from stable and provisional message evidence;
2. persistent ground state and temporary behavior;
3. posture, gesture, gaze, head motion, face, delivery motion, autonomic motion, contact and secondary response;
4. symbolic synchronization between modalities and streamed dialogue;
5. temporal prediction, revision, dispatch and execution feedback;
6. explicit semantic body-resource claims and deterministic conflict resolution;
7. co-articulation between consecutive and concurrent behaviors;
8. graceful interruption, replacement, successor takeover and local cleanup;
9. authored, procedural and optional learned generators behind one realization contract;
10. task-space targets, contacts, joint constraints, IK and physically coherent fallback;
11. projection onto VRM 1.0 humanoid, expressions, look-at, node constraints and spring bones;
12. deterministic replay and reconstructable diagnostics;
13. capability-aware degradation without breaking session truth or dialogue;
14. a versioned, human-readable authoring format and a compact runtime representation.

V1 does **not** require world locomotion, navigation, object manipulation, audio speech, phoneme-perfect lip synchronization, arbitrary non-VRM rigs, or a public plug-in ABI. The architecture must leave clean boundaries for them, but the desktop embodiment's defining problem is expressive continuous acting around real work.

---

## 3. Non-negotiable invariants

### 3.1 Operational truth is upstream authority

The Performance Runtime never infers what the worker is doing from sentiment or body motion. It receives normalized operational truth and may style it, but may not contradict it.

A blocked approval state can look confident, apologetic, irritated or uncertain. It cannot look idle merely because the sentence sounds calm.

### 3.2 Semantic intent never writes bones

Language, affect, operational state and persona style select behavior schemas, motif parameters and timing. They do not name exported bones, write model-local rotations or directly invoke animation filenames.

### 3.3 The current physical state is authoritative

Every new behavior begins from the body that actually exists at dispatch time, including current position, velocity, active contacts, held posture and occupied resources. It never assumes that the body first returned to neutral.

### 3.4 Meaningful phases are commitments; connective phases are generated

The planner primarily commits semantic nuclei: a gaze acquisition, gesture stroke, held posture, contact or expressive apex. Preparation, transition, retraction and settling should be generated from current and successor state unless authored shape is itself semantically important.

### 3.5 Time remains flexible until it must not

A stable source beat may commit semantic meaning before exact performance timing is known. The scheduler preserves intervals and constraints, dispatching exact times only as execution approaches or external evidence resolves.

### 3.6 Body resources are semantic, hierarchical and compositional

The runtime arbitrates claims such as “right hand contact,” “head orientation,” “eyes gaze,” “upper torso additive accent” and “global balance.” It does not merely lock bone indices.

### 3.7 Missing capability fails locally

A model without eye bones can use expression-based look-at or head attention. A model without fingers can use a relaxed hand preset. A failed arm contact can degrade to a guarded non-contact posture. Dialogue and session presence continue.

### 3.8 Real-time execution is bounded and deterministic

Language-scale planning may be asynchronous. Frame-rate execution cannot wait on a model, allocate without bound, retry forever or produce different results from identical accepted events, configuration and seed.

### 3.9 Every visible decision is explainable

For any movement, diagnostics must identify:

- the source session and semantic beat;
- the operational and affect evidence;
- the selected behavior schema;
- timing constraints and revisions;
- resource claims and conflict decisions;
- capability projection and fallback;
- realization program and solve status.

### 3.10 Character style transforms realization, not truth

A character profile may affect gesture frequency, amplitude, eye/head participation, asymmetry, tension, preparation, recovery, preferred hand and idle temperament. It may not invent unsupported work state or silently replace the message's semantic structure.

---

## 4. Terminology

### Performance intent

A renderer-neutral description of what should be communicated physically. It includes truthful state, semantic function, affect, delivery and attention but no joints.

### Behavior schema

A reusable declarative pattern describing when a behavior is eligible, what semantic function it can realize, what resources and capabilities it requires, its meaningful form, phases, alternatives, style parameters and fallbacks.

### Behavior unit

One selected and parameterized instance of a behavior schema, grounded in specific source evidence and represented in the live plan.

### Behavior Plan Graph

The revisioned graph of behavior units, temporal points, semantic dependencies, resource claims, alternatives and execution state.

### Ground state

The durable state to which temporary behaviors resolve: standing posture, weight preference, arm rest configuration, head attitude, face baseline, attention mode and movement-quality baseline. Ground state is not necessarily neutral.

### Shift

A behavior that intentionally changes ground state after completion, such as adopting crossed arms or transferring gaze focus.

### Temporary behavior

A behavior that overlays ground state and eventually releases its resources without permanently changing the baseline.

### Semantic nucleus

The physically meaningful portion that carries the behavior's communicative identity: a gesture stroke, contact, gaze acquisition, nod apex or posture hold.

### Connective tissue

Preparation, transition, co-articulated hand travel, recovery and settling generated from actual current state and future commitments.

### Realization program

An executable, modality-neutral intermediate representation containing phase-local target trajectories, constraints, masks, contacts, qualities and generator selections.

### Motor program

A stateful executable controller that advances a realization program from current body state and emits canonical targets, constraints or additive outputs.

### Resource claim

A declaration of how a behavior uses a semantic body resource: exclusively, additively, as a constraint, cooperatively, as a baseline, or observationally.

### Commitment horizon

The frontier before which changing a plan would violate already delivered semantics, temporal promises, physical continuity or active contact.

---

## 5. System context inside Eidolon

The Performance Runtime begins after Eidolon's existing truth and semantic systems have produced stable evidence. It does not absorb session adapters, dialogue text, classifier ownership or native presentation.

```text
session registry / presence contract
              ↓
expression director + delivery compiler + attention policy
              ↓
EidolonPerformanceIntent events
              ↓
Eidolon Performance Runtime
              ↓
VRM body renderer
              ↓
renderer-neutral body geometry and pixels
              ↓
presentation backend
```

The existing Expression Director should evolve into an upstream performance-evidence producer rather than become the skeleton controller. Its beat segmentation, continuous affect axes, source offsets, stable-prefix semantics and deterministic fallback remain valuable inputs.

The runtime has three cadence domains:

1. **Evidence cadence:** event-driven; operational changes and semantic beat updates.
2. **Planning cadence:** event-driven and typically low frequency; schema selection, plan revision and temporal/resource resolution.
3. **Motor cadence:** frame rate or a stable fixed internal step; realization programs, constraints, IK and secondary response.

Slow planning never blocks motor execution. The body continues its last valid ground state, attention and bounded autonomous motion while future behavior is unavailable.


## 6. End-to-end architecture

```text
┌────────────────────────────────────────────────────────────┐
│                 Upstream Eidolon evidence                  │
│ truth · semantic beats · affect · delivery · attention     │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 1. Performance Intent Normalizer                           │
│ stable/provisional evidence · provenance · confidence      │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 2. Behavior Planner                                       │
│ schemas · candidate generation · deterministic selection   │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 3. Behavior Plan Graph                                    │
│ behavior units · sync points · dependencies · alternatives │
└───────────────┬─────────────────────────────┬──────────────┘
                ↓                             ↓
┌───────────────────────────┐   ┌────────────────────────────┐
│ 4. Temporal Dispatcher    │   │ 5. Body Resource Arbiter  │
│ STN/STNU-like constraints │   │ claims · conflicts · swaps │
│ commit and revision       │   │ composition · degradation  │
└───────────────┬───────────┘   └──────────────┬─────────────┘
                └───────────────┬──────────────┘
                                ↓
┌────────────────────────────────────────────────────────────┐
│ 6. Modality Realizers                                     │
│ posture · gesture · gaze · head · face · delivery · idle   │
│ contact · hand shape · secondary                           │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 7. Realization Program IR                                 │
│ phase time · targets · paths · contacts · masks · quality  │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 8. Controller Compositor                                  │
│ ground state · shifts · temporary layers · takeover        │
│ additive motion · inertial continuity · constraint merge   │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 9. Canonical Physical Realizer                            │
│ anatomical frames · limits · IK · contact · balance        │
└────────────────────────────┬───────────────────────────────┘
                             ↓
┌────────────────────────────────────────────────────────────┐
│ 10. VRM Body Adapter                                      │
│ humanoid · lookAt · expressions · node constraints         │
│ spring bones · capabilities · package calibration          │
└────────────────────────────┬───────────────────────────────┘
                             ↓
                    final model-local pose
```

---

## 7. Performance Intent contract

The input should be a compact versioned value, not BML, not text, and not a bag of classifier labels.

Conceptually:

```c
typedef struct EidolonPerformanceIntent {
    uint64_t intent_id;
    uint64_t revision;
    uint64_t source_id;
    uint64_t session_id;
    uint64_t semantic_beat_id;

    EidolonEvidenceState evidence_state;   /* provisional | stable | final */
    EidolonOperationalState operation;     /* truthful source state */
    EidolonCommunicativeFunction functions[EIDOLON_MAX_FUNCTIONS];
    EidolonAffectAxes affect;
    EidolonMovementQuality quality;
    EidolonDeliveryWindow delivery;
    EidolonAttentionRequest attention;

    float semantic_confidence;
    float intensity;
    uint64_t source_begin;
    uint64_t source_end;
    uint64_t deterministic_seed;
} EidolonPerformanceIntent;
```

### 7.1 Communicative-function vocabulary

The vocabulary should be small enough to remain inspectable and physicalizable:

```text
acknowledge       answer             explain
contrast          correct            question
offer             indicate           emphasize
insist            refuse             warn
hesitate           concede            reassure
celebrate          apologize          resolve
invite_attention   request_action     yield_turn
```

Functions may coexist. A sentence can `correct + reassure`; a beat can `contrast + emphasize`; an approval prompt can `request_action + hesitate`.

This vocabulary does not select a specific gesture. It constrains candidate schemas and movement qualities.

### 7.2 Evidence stability

- **Provisional:** may influence anticipatory low-cost behavior but cannot commit an expensive semantic nucleus.
- **Stable:** source prefix will not normally change; semantic behavior may be planned and committed.
- **Final:** tail repaired and message complete; future holds, resolution and settling may be finalized.

Operational truth bypasses this language stability distinction. A real approval request, interruption or tool transition may immediately revise attention and posture even while text remains provisional.

### 7.3 Provenance

Every intent must retain source offsets and semantic-beat identity so activation can occur before the first relevant glyph and logs can reconstruct why it happened. This preserves Eidolon's existing source-offset timing model.

---

## 8. Behavior schemas and planning

### 8.1 A schema is generative, not a complete animation

A schema describes a family of performances.

```text
schema: gesture.present
eligible functions: explain, offer, indicate
semantic nucleus: one hand opens toward presentation target
resources: preferred arm, hand, optional torso, optional gaze
parameters: hand, extent, height, direction, firmness, asymmetry
phases: prepare → stroke → optional hold → successor/retract
style mapping: power, speed, fluidity, directness
capabilities: arm IK required; fingers optional
alternatives: opposite hand, two-hand open, head-only indicate
fallback: gaze + torso orientation
```

The schema owns no VRM node indices and no exact model-local keyframes.

### 8.2 Candidate generation

For each intent, the planner queries schemas by:

- communicative function;
- operational state;
- affect and movement quality;
- current ground state;
- body capabilities;
- current and expected resource occupancy;
- repetition history;
- character style;
- semantic importance and timing opportunity.

A schema can emit several candidates with alternative effectors, amplitudes or forms.

### 8.3 Deterministic selection

Candidate scoring should be explicit and reproducible:

```text
score = semantic_fit
      + operational_fit
      + character_preference
      + continuity_with_current_state
      + successor_compatibility
      + capability_quality
      - resource_conflict_cost
      - repetition_cost
      - transition_cost
      - timing_risk
      - fallback_penalty
```

Random variation is permitted only through a recorded deterministic seed and only among candidates inside a small score tolerance.

### 8.4 Repetition and behavioral memory

The planner keeps bounded history by schema family, effector and visible form. Repetition pressure should reduce mechanical loops without forbidding deliberate rhetorical repetition.

History includes:

- last use time and semantic function;
- chosen hand or side;
- amplitude and spatial zone;
- whether the behavior completed, was interrupted or degraded;
- current audience-visible posture residue.

### 8.5 Character style

The character profile is a transform over candidate choice and realization, not a duplicate behavior library.

Example dimensions:

```text
gesture_frequency
preferred_hand
spatial_extent
vertical_extent
movement_speed
movement_power
fluidity
hold_tendency
asymmetry
head_mover_tendency
gaze_directness
gaze_aversion
posture_openness
baseline_tension
idle_activity
recovery_speed
contact_comfort
```

These values establish priors and envelopes. Current affect and communicative function modulate them.

---

## 9. Behavior Plan Graph

The plan is a revisioned graph, not a flat queue and not an animation state machine.

### 9.1 Contents

The graph contains:

- behavior units;
- semantic dependencies and provenance;
- named sync points;
- metric temporal constraints;
- contingent or predicted external events;
- body-resource claims;
- capability requirements and alternatives;
- ground-state effects;
- interruption and takeover policies;
- realization predictions;
- execution feedback.

### 9.2 Behavior lifecycle

```text
proposed
   ↓
selected
   ↓
scheduled
   ↓
committed
   ↓
executing
   ↓
delivered ──→ settling ──→ completed
   │
   ├──→ interrupted ──→ transferred | recovering | completed
   ├──→ degraded
   ├──→ canceled
   └──→ failed
```

Definitions:

- **selected:** schema and primary parameters chosen.
- **scheduled:** temporal/resource solution exists but may remain flexible.
- **committed:** changing the semantic nucleus would now violate a visible or temporal promise.
- **executing:** at least one motor program has begun.
- **delivered:** semantic nucleus completed; connective cleanup may remain.
- **settling:** resources return or transfer toward ground/successor state.

### 9.3 Revisions

A new revision never mutates already delivered facts. It may:

- add future units;
- tighten or relax future time windows;
- replace a provisional candidate;
- change continuous parameters through a trajectory;
- transfer resources from predecessor to successor;
- interrupt or cancel uncommitted units;
- degrade a unit when capability or timing becomes impossible.

Every revision carries a monotonic sequence. Late planner output for an older revision is rejected.

### 9.4 Ground state as graph state

Persistent shifts are graph commitments, not invisible mutable globals. A shift produces a new ground-state revision after its semantic nucleus is accepted.

For example:

```text
postureShift: arms.crossed.soft
    prepare: current hand state → cross configuration
    commit: contact established
    effect: ground.arms = crossed.soft
```

A later temporary gesture may borrow the right hand, release one contact, perform the stroke and return into the crossed-arm ground state without rebuilding the entire posture from neutral.

---

## 10. Temporal model

### 10.1 Why a temporal network

BML's bidirectional synchronization references and ASAP's movable time pegs are naturally represented as a **Simple Temporal Network (STN)**: time points connected by minimum and maximum difference constraints.

Each behavior exposes meaningful points:

```text
start
ready
stroke_start
stroke_peak or stroke
stroke_end
relax
end
contact_acquire
contact_release
settle
```

Example constraints:

```text
gaze.acquire ≤ gesture.stroke_start - 80 ms
gesture.stroke_peak ∈ [word_onset - 80 ms, word_onset + 100 ms]
head.nod.stroke = contrast_mark
posture.hold_end ≥ response.completed + 300 ms
```

Intervals are derived from point pairs. Full Allen interval algebra is unnecessary and risks nondeterministic or expensive disjunction. The runtime should support a deliberately tractable set of metric point constraints.

### 10.2 Uncertainty and contingent links

Some events are externally determined or predicted:

- the final duration of a streamed phrase;
- arrival of a stable semantic beat;
- response completion;
- user interaction;
- TTS word timing in a future audio path;
- contact acquisition when a solver is converging.

These can be represented as contingent intervals with known bounds and observed completion, borrowing the useful subset of STN-with-uncertainty theory. The goal is dynamic dispatchability: choose future controllable times based only on evidence already observed.

The implementation need not begin with a general theorem prover. The architecture should nevertheless preserve the distinction between:

- controllable time points;
- externally observed contingent points;
- predictions that may be revised;
- hard constraints;
- preferred soft windows.

### 10.3 Scheduling layers

1. **Hard temporal constraints:** semantic synchrony, ordering and minimum physical duration.
2. **Dispatch windows:** latest/earliest safe execution times.
3. **Soft preferences:** preferred phrase rhythm, character timing and co-articulation.
4. **Motor timing:** the exact trajectory chosen after dispatch.

A soft preference may be violated with a diagnostic. A hard constraint requires alternative selection, degradation or rejection.

### 10.4 Commitment horizons

The runtime tracks several frontiers rather than one binary “started” flag:

- **semantic horizon:** meaning already shown to the user;
- **temporal horizon:** time points too near to move safely;
- **motor horizon:** trajectory portion already executed;
- **contact horizon:** physical relationship that cannot be broken invisibly;
- **attention horizon:** target already visibly acquired.

This enables precise modification. A gesture may retain its already executed preparation while replacing an uncommitted stroke; or retain a delivered stroke while skipping its retraction because the successor uses the same hand.

### 10.5 Dispatch

At each planning or motor tick:

1. incorporate new evidence and observed contingent points;
2. reject stale revisions;
3. propagate time constraints;
4. determine newly executable points;
5. ensure required resource claims can be granted;
6. commit only the nearest necessary frontier;
7. instantiate or revise motor programs;
8. publish prediction feedback.

The plan remains maximally flexible beyond the commitment horizon.


## 11. Body Resource Arbiter

This is the most important Eidolon-specific subsystem.

Prior realizers commonly schedule behaviors by modality or select controllers by priority. That is insufficient when one persistent body must simultaneously:

- hold a posture;
- gaze toward a bubble;
- nod on a contrast;
- lift one hand for a presenting gesture;
- preserve the other hand's body contact;
- breathe and sway;
- respond to an interruption;
- hand performance ownership between concurrent sessions.

### 11.1 Semantic resource hierarchy

```text
body
├── global_root
├── balance
│   ├── pelvis
│   ├── support_left
│   └── support_right
├── torso
│   ├── lower_spine
│   ├── chest
│   └── upper_chest
├── left_arm
│   ├── shoulder
│   ├── elbow
│   ├── wrist
│   ├── hand
│   └── fingers
├── right_arm
│   └── ...
├── head_system
│   ├── neck
│   ├── head_orientation
│   ├── eyes
│   ├── jaw
│   └── blink
├── face
│   ├── emotion
│   ├── mouth
│   ├── gaze_expression
│   └── custom_channels
├── attention
├── contacts
└── secondary_chains
```

The hierarchy is semantic. A claim on `right_arm` contains the hand and elbow unless explicitly narrowed. A gaze behavior can claim `eyes` exclusively while sharing `head_orientation` additively with a nod.

### 11.2 Claim modes

Every behavior declares one mode per resource:

- **exclusive:** only one owner can determine the resource output;
- **baseline:** supplies the value used when no stronger claim exists;
- **additive:** contributes an offset or impulse around the composed base;
- **masked override:** replaces selected semantic axes or subresources;
- **constraint:** contributes a condition to be jointly solved rather than a direct pose;
- **cooperative:** multiple owners negotiate one shared goal;
- **observational:** reads state without controlling it;
- **passive:** declares that the resource must be preserved but emits no new target.

A simple bone-lock model cannot express these cases.

### 11.3 Claim fields

```c
typedef struct EidolonResourceClaim {
    EidolonResourceId resource;
    EidolonClaimMode mode;
    EidolonClaimStrength strength;
    float priority;
    float blend_weight;
    uint64_t behavior_id;
    uint64_t phase_mask;
    EidolonPreemptionPolicy preemption;
    EidolonCompositionPolicy composition;
    EidolonAlternativeSet alternatives;
} EidolonResourceClaim;
```

Claim strength distinguishes:

- hard semantic commitment;
- required physical constraint;
- preferred realization;
- optional expressive decoration.

### 11.4 Conflict resolution

Resolve in this order:

1. **Preserve truth and user interaction.** Operational attention and direct user actions outrank decorative behavior.
2. **Preserve delivered commitments.** Do not invalidate already visible meaning.
3. **Preserve hard physical constraints.** Active support and committed contacts outrank soft posture style.
4. **Compose compatible claims.** Nod plus gaze, breathing plus posture, delivery impulse plus held gesture.
5. **Use declared alternatives.** Swap hand, reduce torso participation, use head-only indication.
6. **Phase-shift within temporal tolerance.** Delay a low-priority beat until the hand is available.
7. **Transfer ownership.** A successor may absorb predecessor recovery.
8. **Degrade capability.** Replace contact with non-contact guarded posture.
9. **Preempt gracefully.** Finish nucleus or generate local retreat.
10. **Drop with feedback.** Never silently pretend it occurred.

### 11.5 Priority is contextual

Priority is not one static integer. The arbiter computes effective priority from:

```text
semantic importance
+ operational urgency
+ explicit user interaction
+ phase commitment
+ physical/contact commitment
+ current visibility
+ session performance ownership
- interruptibility
- fallback quality
- timing flexibility
```

A low-priority gesture that has already acquired hand contact can temporarily outrank a newly proposed decorative beat because breaking the contact would be visibly worse.

### 11.6 Shared axes and joint behaviors

Some behaviors must not own whole joints:

- gaze primarily controls eye direction and a preferred head component;
- nod controls head pitch trajectory;
- posture controls a slower head attitude baseline;
- delivery may add a tiny head impulse;
- recoil may constrain head/torso relation.

The compositor therefore uses semantic axes and constraint contributions where possible. A nod can occupy head pitch while gaze controls yaw and eye residual, with joint limits coordinating the final result.

### 11.7 Contacts are resources and constraints

A contact claim names:

- source effector or surface;
- destination semantic surface;
- acquisition window;
- required position/orientation tolerance;
- firmness/compliance;
- break conditions;
- whether sliding is allowed;
- ownership and successor transfer policy.

Contacts survive through breathing and posture motion by being solved as relationships, not frozen world positions.

---

## 12. Co-articulation and successor takeover

Natural performance is dominated by what happens **between** named behaviors.

### 12.1 Co-articulation rules

When a successor becomes known, the realizer may:

- shorten predecessor retraction;
- shorten successor preparation;
- route the hand directly between nuclei;
- retain compatible hand shape or orientation;
- reuse a held spatial zone;
- preserve torso momentum;
- transfer a contact;
- merge repeated beats into one phrase;
- change ground state without passing through neutral.

### 12.2 Semantic nucleus preservation

Co-articulation may change preparation and recovery freely inside constraints. It may alter the nucleus only when the schema explicitly permits shape variation.

A deictic point must still acquire the intended direction. An emphatic beat must still land on the intended delivery mark. A soft crossed-arm self-comfort contact must not become a firm confrontational lock merely because the successor is tense.

### 12.3 Takeover types

- **release:** predecessor returns resource to ground state;
- **direct successor:** successor starts from predecessor's current terminal state;
- **nucleus chaining:** several strokes share one preparation and final retraction;
- **contact transfer:** new behavior inherits an existing contact;
- **baseline adoption:** temporary endpoint becomes a persistent shift;
- **partial takeover:** successor takes hand trajectory while predecessor retains torso hold.

### 12.4 Stranded-resource cleanup

Interruption or plan replacement may leave resources outside any valid owner. Cleanup is generated only for those stranded resources.

The runtime must not reset the whole body because one gesture was canceled. If the right arm is interrupted while gaze and posture remain valid, only the right-arm chain receives recovery or successor takeover.

---

## 13. Modality realizers

Each modality realizer converts behavior units into Realization Programs. It does not directly mutate the final skeleton.

### 13.1 Posture and stance realizer

Responsibilities:

- persistent ground posture;
- stance and weight distribution;
- pelvis and spine attitude;
- chest openness and guardedness;
- long-lived arm configurations;
- approach/recoil and engagement;
- local balance and planted support;
- transition between posture shifts.

Posture is represented by orthogonal dimensions and recognizable motifs rather than full emotion poses.

Continuous dimensions:

```text
weight_left_right
weight_forward_back
pelvis_height
pelvis_yaw
spine_lean
spine_curve
chest_openness
shoulder_tension
body_width
approach_recoil
asymmetry
```

Initial motifs:

```text
stance.centered
stance.contrapposto
posture.open
posture.guarded
posture.attentive
posture.relaxed
arms.resting
arms.crossed.soft
arms.crossed.firm
arms.behind_back
arms.self_contact
arms.akimbo
```

A motif is parameterized and may claim contacts.

### 13.2 Gesture realizer

Gesture families:

- beat;
- deictic/indicating;
- presenting/offering;
- iconic spatial depiction;
- emblematic gesture;
- regulator/turn management;
- self-adaptor/self-contact;
- acknowledgement;
- recoil/rejection;
- resolution/settling.

The gesture schema specifies the meaningful form using:

- semantic hand or both hands;
- hand shape class;
- wrist location and orientation;
- trajectory primitive;
- spatial zone;
- symmetry/asymmetry;
- repetition;
- contact;
- speech/delivery affiliate;
- required torso or gaze coordination.

Gesture phases follow observed human annotation practice:

```text
optional preparation
optional pre-stroke hold
required stroke/nucleus
optional post-stroke hold
optional retraction or successor transfer
```

The realizer generates missing connective phases from current state.

### 13.3 Gaze and attention realizer

VRM look-at controls line of sight; Eidolon must control the full attention performance.

The realizer owns:

- target selection received from attention policy;
- fixation, glance and aversion behaviors;
- eye-first/head-follow/torso-follow timing;
- eye/head contribution based on angular distance, eye-in-head comfort and character tendency;
- saccadic transitions and small fixation variation;
- gaze holds and return;
- coordination with user, bubble and session targets;
- suppression or alteration during blink and facial overrides.

Conceptual solve:

1. calculate target direction in head-relative look-at space;
2. allocate a comfortable eye contribution;
3. schedule eye saccade first;
4. follow with head when target angle, duration or social function warrants it;
5. recruit torso for larger or sustained reorientation;
6. counter-rotate eyes as head settles to preserve fixation;
7. release or transfer attention according to plan.

Character style includes an eye-mover/head-mover tendency.

VRM's look-at target cannot express vergence; V1 accepts shared eye direction and treats convergence as unavailable capability.

### 13.4 Head behavior realizer

Head behaviors include:

- nod;
- shake;
- tilt;
- recoil;
- forward acknowledgement;
- orientation shifts;
- micro-accent.

Head motion must compose with gaze. Nods and shakes operate as phase trajectories around the current gaze/posture orientation, while the gaze system may redistribute direction into eyes or torso to preserve the target.

### 13.5 Face realizer

The face has four logical layers:

1. persistent baseline or shift;
2. semantic affect expression;
3. temporary facial action/accent;
4. procedural mouth, blink and expression-based gaze.

The realizer outputs semantic VRM expression weights, not morph target indices.

It must respect VRM's procedural override rules:

- an emotion expression may block or attenuate mouth, blink or gaze expression channels;
- binary expressions use their binary output for override evaluation;
- custom-expression interactions must be declared by the package because VRM does not fully standardize them.

Facial transitions use attack, sustain, release and optional overshoot envelopes. A hard semantic reversal can remain immediate, while ordinary changes preserve continuity.

### 13.6 Delivery-motion realizer

Delivery motion is separate from semantic gesture.

It consumes deterministic source-offset marks for:

- phrase onset;
- contrast;
- hesitation;
- question;
- emphasis;
- punctuation accent;
- landing/resolution.

It emits bounded additive impulses to torso, head, shoulders, hands or whole-body presentation transform according to available resources. It cannot seize a hand already committed to a semantic contact or create a new semantic gesture.

### 13.7 Hand-shape realizer

Hand shape strongly affects perceived quality even when full finger articulation is not semantically central.

V1 should provide:

```text
relaxed
open
soft_open
flat
point
pinch
fist
cupped
self_contact
presentation
```

Finger-capable VRMs receive procedural or authored finger targets. Models without fingers retain wrist/palm realization and degrade locally.

### 13.8 Contact realizer

The contact realizer compiles semantic surface relationships into constraints and maintains them while parent body parts move.

Initial surfaces:

```text
upper_arm_outer
upper_arm_inner
forearm_outer
chest_upper
chest_side
waist_side
opposite_hand
opposite_wrist
chin_or_cheek_optional
```

Surfaces are package-calibrated volumes or parametric segments, not mesh-triangle references in behavior schemas.

### 13.9 Autonomic and idle realizer

Idle life is not a looping sine wave.

The realizer combines:

- breathing with inhale/exhale asymmetry;
- low-frequency weight drift;
- state-dependent postural settling;
- blink scheduling;
- attention maintenance;
- bounded micro-adjustments after a gesture;
- fatigue/tension-like style only where explicitly configured.

Rules:

- autonomous motion yields to semantic constraints;
- amplitude depends on operational activity and character style;
- phase continues across most behaviors rather than restarting;
- motion settles and varies through deterministic low-discrepancy or seeded processes;
- no random gesture is introduced without semantic justification.

### 13.10 Secondary-motion realizer

VRM SpringBone owns hair, clothing and accessory chains after the primary skeleton and node constraints are resolved.

Eidolon may modulate or inject physical impulses through supported runtime parameters, but must not also rotate spring-owned joints directly. The body adapter declares ownership and update order.

---

## 14. Realization Program intermediate representation

The Realization Program is the missing layer between behavior and skeleton.

### 14.1 Requirements

It must be:

- independent from model-local nodes;
- independent from one solver;
- able to express authored and procedural motion;
- phase-aware and temporally revisable;
- task-space and contact capable;
- maskable and composable;
- executable from arbitrary current state;
- predictable enough to report expected sync times;
- serializable for authoring and diagnostics;
- compiled into bounded runtime data.

### 14.2 Core structure

```c
typedef struct EidolonRealizationProgram {
    uint64_t program_id;
    uint64_t behavior_id;
    uint64_t revision;

    EidolonPhase phases[EIDOLON_MAX_PHASES];
    EidolonChannel channels[EIDOLON_MAX_CHANNELS];
    EidolonTarget targets[EIDOLON_MAX_TARGETS];
    EidolonConstraint constraints[EIDOLON_MAX_CONSTRAINTS];
    EidolonTrajectory trajectories[EIDOLON_MAX_TRAJECTORIES];
    EidolonQualityEnvelope quality;
    EidolonProgramFallback fallbacks[EIDOLON_MAX_FALLBACKS];
} EidolonRealizationProgram;
```

### 14.3 Coordinate spaces

Targets explicitly identify space:

- world;
- presentation host;
- character root;
- pelvis;
- chest frame;
- shoulder frame;
- head/look-at frame;
- semantic body surface;
- opposite effector;
- normalized anatomical frame.

No vector is interpreted without a space and unit.

### 14.4 Trajectory primitives

The IR should support:

- hold;
- linear only for deliberate mechanical motion;
- cubic/quintic minimum-jerk transition;
- Hermite spline with velocity boundary conditions;
- bounded damped spring;
- time-warped authored canonical clip or VRMA;
- dynamic movement primitive;
- target-following motor program;
- inertial continuation offset;
- stochastic micro-motion under deterministic seed.

A schema selects the semantic shape; the motor program chooses or parameterizes the appropriate primitive.

### 14.5 Expressive quality envelope

Borrow a computational subset from Greta/EMOTE/Laban-inspired systems:

```text
spatial_extent
vertical_extent
temporal_extent
speed
power
fluidity
directness
tension
acceleration_profile
hold_emphasis
repetition
whole_body_recruitment
```

These parameters modify trajectory and recruitment, not semantic identity.

For example:

- greater power shortens acceleration time and increases whole-body recruitment;
- greater fluidity reduces abrupt curvature and encourages successor blending;
- directness favors straighter task-space paths;
- tension increases co-contraction-like stiffness and reduces idle spill;
- hold emphasis lengthens post-stroke stability.

Mappings must remain explicit and empirically tuned. The system should not claim faithful psychological Laban interpretation merely because it uses similar dimensions.

### 14.6 Program prediction

Before execution, a motor program reports:

- minimum, preferred and maximum phase durations;
- reachable target estimate;
- expected contact acquisition;
- required resources;
- degradation alternatives;
- confidence and failure reasons.

This feedback lets the temporal dispatcher adjust without waiting for a visible miss.


## 15. Controller Compositor

The compositor combines realization outputs into one canonical control state. It is a graph of semantic controllers, not merely a normalized weighted average of poses.

### 15.1 Composition stages

A recommended canonical order is:

1. canonical rest and package normalization;
2. persistent ground-state shifts;
3. primary stance and posture goals;
4. authored/canonical FK contributions;
5. semantic task-space targets and contacts;
6. head/gaze coordination goals;
7. face semantic and procedural layers;
8. delivery and other bounded additive accents;
9. coupled physical solve and joint-limit projection;
10. inertial continuity and per-resource settling where compatible;
11. package-local corrections and VRM projection;
12. VRM node constraints;
13. VRM spring bones.

The exact internal implementation may interleave stages where mathematics requires it. Ownership must remain explicit.

### 15.2 Composition operators

- **replace:** one controller supplies the complete semantic resource state;
- **weighted blend:** appropriate for alternative complete poses;
- **additive local rotation/translation:** breathing, nod residual, delivery accent;
- **masked blend:** partial body or semantic-axis control;
- **constraint merge:** several goals solved together;
- **successor transfer:** controller state migrates without a neutral gap;
- **inertialized switch:** outgoing velocity is preserved while offset decays;
- **baseline fallback:** uncovered channels resolve to persistent ground state.

### 15.3 Why ordinary pose blending is insufficient

Linear or quaternion blending cannot reliably preserve:

- hand-on-body contact;
- planted feet;
- gaze target;
- semantic trajectory shape;
- joint limits;
- asymmetric resource ownership;
- successor takeover.

Blending is useful for compatible pose alternatives and authored animation layers. Constraints and motor state handle relational commitments.

### 15.4 Inertialization

When a controller or target changes abruptly, the runtime records the difference between the old moving state and the new target state, then decays that offset while no longer evaluating the old controller.

Use per-resource inertialization with:

- pose, angular and linear velocity offsets;
- maximum duration;
- filtered resources that must switch immediately;
- deficit tracking when repeated switches occur;
- constraint projection after continuity correction.

Inertialization is not a substitute for planned co-articulation. It is the safety net for unavoidable or interactive switches.

---

## 16. Canonical physical realization

### 16.1 Canonical humanoid state

The shared state should contain:

- canonical humanoid local rotations;
- hips translation and root/presentation transform;
- semantic anatomical frames;
- task-space effector targets;
- preferred bend/pole directions;
- hand shapes;
- contact and support constraints;
- expression weights;
- gaze target and eye/head allocation;
- linear and angular velocities;
- validity and ownership metadata.

### 16.2 Anatomical frames

For every major region, the character package supplies or derives stable semantic frames:

- pelvis forward/up/left;
- chest forward/up/left;
- shoulder span and local shoulder frames;
- upper/lower limb primary and twist axes;
- palm forward/up and finger direction;
- foot forward/up and sole plane;
- head forward/up;
- eye/look-at origin.

These frames insulate behavior data from arbitrary exported local axes.

### 16.3 Solver hierarchy

No one IK algorithm owns the body.

#### Analytic two-bone solve

Use for ordinary arms and legs because it is fast, predictable and directly exposes preferred bend, pole vector, twist, reachability and soft extension.

#### FABRIK or Jacobian/DLS solve

Use for unusual variable-length chains, spine-like chains or effectors where analytic structure is unavailable. Constraints and orientations must be layered explicitly.

#### Position-based / XPBD-like coupled solve

Use for full-body relations:

- both hands contacting the body;
- planted feet while pelvis moves;
- hand-hand contact;
- balance and reach interaction;
- multiple weighted effectors;
- compliant contact.

XPBD-style compliance gives constraint stiffness that is less dependent on iteration count and timestep than ordinary PBD.

#### Direct FK and authored clips

Use where an authored rotational shape is itself the desired motion, then project hard constraints and package corrections.

### 16.4 Hard and soft constraints

Hard constraints:

- joint anatomical limits;
- non-penetration for declared critical surfaces;
- active support foot;
- committed contact within tolerance;
- required gaze target when capability exists;
- no invalid hierarchy or NaN transform.

Soft constraints:

- preferred hand target;
- preferred elbow pole;
- posture style;
- center-of-mass preference;
- shoulder comfort;
- minimum movement from current state;
- semantic target approximation;
- symmetry/asymmetry preference.

The solver uses lexicographic or strongly weighted priorities so decorative quality cannot violate hard physical validity.

### 16.5 Progressive relaxation

When a solve is impossible, relax in a declared order:

1. reduce decorative torso participation;
2. reduce exact orientation while preserving position/contact;
3. reduce target extent;
4. move pelvis/root within allowed envelope;
5. soften contact;
6. select schema alternative;
7. substitute fallback behavior;
8. retain the last valid state and report failure.

Never emit a partially corrupted hierarchy.

### 16.6 Joint limits and twist

The package supplies:

- swing cones or axis limits;
- preferred bend direction;
- twist range;
- twist distribution across optional auxiliary bones;
- stiffness/compliance;
- neutral comfort region.

VRM Node Constraint roll constraints may own exported twist bones after humanoid pose projection. Eidolon's solver should control the semantic limb roll, then allow declared node constraints to distribute it.

### 16.7 Balance

For the desktop-standing body, V1 needs quasi-static balance rather than full physics locomotion.

The balance controller tracks:

- support polygon from planted feet;
- projected center-of-mass estimate;
- pelvis shift limits;
- knee and ankle comfort;
- torso counterbalance;
- whether a stance is decorative or physically committed.

If a model is framed above the legs or declares no planted-foot capability, lower-body solve may degrade to a calibrated stance without claiming physical grounding.

---

## 17. VRM 1.0 body adapter

VRM is the first embodiment substrate. It is not the behavior language and not the planner.

### 17.1 Load-time validation

The adapter validates:

- VRM 1.0 and required glTF structures;
- required unique humanoid bones;
- fixed parent relationships, allowing non-humanoid intermediary nodes;
- positive humanoid scales;
- T-pose compatibility and rest transforms;
- expressions and their procedural override rules;
- look-at type and range maps;
- node-constraint dependency graph and absence of cycles;
- spring-bone chain validity and ownership;
- model metadata and rights fields;
- renderer limits for materials, morphs and draw complexity.

### 17.2 Required and optional capability profile

Required VRM humanoid roles provide the first body baseline:

```text
hips, spine, head
left/right upper leg, lower leg, foot
left/right upper arm, lower arm, hand
```

Optional but highly valuable:

```text
chest, upperChest, neck, shoulders
eyes, jaw, toes, all finger chains
semantic expressions
lookAt
node constraints
spring bones
```

The adapter publishes capabilities, not assumptions.

### 17.3 Rest normalization

VRM Animation requires a VRM T-pose but permits arbitrary rotations in the T-pose hierarchy. Eidolon computes canonical anatomical correction transforms from package rest state to its internal frames.

The runtime should preserve the actual model hierarchy and skin bind. It does not rebind or rewrite the asset every launch.

### 17.4 Humanoid projection

Canonical humanoid deltas are projected onto mapped VRM humanoid bones. As with VRM Animation:

- ordinary humanoid motion uses rotations;
- hips may translate;
- humanoid scale animation is prohibited;
- model/root placement remains separate from hips motion.

### 17.5 Look-at projection

VRM look-at accepts a target direction in head-relative look-at space and maps it to either eye-bone rotations or expression weights.

Eidolon supplies:

- world/presentation target;
- eye-first/head-follow/torso-follow plan;
- character head-mover tendency;
- comfortable range and aversion behavior.

The adapter supplies only the final eye component through VRM look-at. Head and torso are already part of the humanoid pose.

### 17.6 Expression projection

The face compositor sets semantic preset and custom expression weights, then applies VRM procedural override semantics for mouth, blink and expression-based look-at.

Because every preset expression is optional and deformation is unspecified, packages may add:

- affect target calibration;
- supported intensity range;
- visual conflict annotations;
- custom-expression semantic aliases;
- whether a preset is binary;
- fallback combinations.

### 17.7 Exact update order

The runtime follows the VRM-recommended order:

1. resolve humanoid bones;
2. resolve look-at after head position is known;
3. set external emotion, mouth, blink and expression-look-at weights;
4. apply expressions and their overrides;
5. resolve VRM node constraints;
6. resolve VRM spring bones.

Any Eidolon controller that writes a node after its declared owner stage risks double ownership and must be rejected by validation.

### 17.8 Rights metadata

VRM metadata includes authorship, references, commercial-use policy, credit, redistribution and modification permissions. Eidolon packages should preserve those fields and add a stricter normalized rights record for distribution tooling.

A package is not distributable merely because it is valid VRM.

---

## 18. Authored motion and VRM Animation

VRM Animation is a useful portable authored-motion source because it maps humanoid roles, expressions and look-at in a separate glTF animation file.

Eidolon may use VRMA for:

- authored gesture nuclei;
- calibrated hand shapes;
- showcase motion;
- regression fixtures;
- comparison against procedural realization;
- authoring previews.

But a behavior schema should reference a semantic realization asset, not expose a VRMA filename as the upstream semantic protocol.

At import time, a VRMA clip can be compiled into:

- canonical FK curves;
- phase annotations;
- resource masks;
- optional task-space contact annotations;
- timing and quality metadata;
- fallback requirements.

The runtime may time-warp and mask it, but should not pretend an unannotated clip preserves contacts or semantics under arbitrary blending.

---

## 19. Deterministic, authored and learned generators

The architecture does not force a false choice between procedural and neural generation.

### 19.1 Generator contract

Every generator receives:

- one behavior unit;
- current canonical physical state;
- current and future resource context;
- timing windows;
- character style;
- body capability profile;
- deterministic seed.

It returns one or more candidate Realization Programs plus predictions and confidence.

```c
typedef struct EidolonMotionGeneratorVTable {
    bool (*supports)(const EidolonBehaviorUnit*,
                     const EidolonBodyCapabilities*);
    EidolonGenerateResult (*generate)(const EidolonGenerateRequest*,
                                      EidolonProgramBuffer*);
    bool (*revise)(const EidolonReviseRequest*,
                   EidolonProgramBuffer*);
} EidolonMotionGeneratorVTable;
```

V1 can use a static registry rather than a binary plug-in ABI.

### 19.2 Deterministic generators

Preferred first implementations:

- form-based procedural motor programs;
- minimum-jerk target transitions;
- damped spring accents;
- authored canonical clips;
- dynamic movement primitives for adaptable expressive trajectories;
- motion matching over a small legally clean canonical library;
- rule-based gaze/head coordination;
- constrained posture/contact solvers.

### 19.3 Learned generators

A learned generator may later provide:

- candidate gesture forms;
- phase-duration predictions;
- motion-prior costs;
- retrieval embeddings for motion matching;
- canonical target trajectories;
- bounded residual motion;
- character-style adaptation.

It may **not** bypass:

- operational truth;
- semantic behavior selection;
- temporal commitment;
- resource arbitration;
- capability negotiation;
- hard contacts and joint constraints;
- deterministic fallback;
- provenance and diagnostics.

Its output is a candidate Realization Program or bounded canonical residual, never unconstrained final model-local joints.

### 19.4 Why this boundary matters

Modern gesture-generation evaluations show that human-like motion and speech-appropriate motion are distinct. A model can generate motion judged natural while barely selecting motion more appropriate to the actual speech than a mismatched sample.

Eidolon therefore keeps semantics explicit and allows learning to improve realization quality without becoming the authority on meaning.

### 19.5 Training and weight rights

Every learned generator requires a provenance manifest covering:

- source datasets;
- permitted training use;
- commercial use;
- redistribution of weights;
- derivative motion and generated output;
- required attribution;
- prohibited content or downstream restrictions.

Downloadability is not training permission.

---

## 20. Failure, interruption and recovery semantics

### 20.1 Interruption policies

Each behavior declares one:

- **immediate cancel:** safe for optional future behavior;
- **inertial cancel:** stop source and decay motion toward valid state;
- **finish nucleus:** complete already committed meaning, skip recovery;
- **graceful retreat:** generate local recovery from current state;
- **successor takeover:** transfer resources directly;
- **hold:** freeze semantic nucleus until released;
- **noninterruptible contact:** release only through declared break procedure.

### 20.2 Replacement is not global neutralization

BML's original `REPLACE` semantics return the agent to neutral before new behavior. Eidolon should not do that by default. Replacement is resource-local and ground-state-aware.

A new urgent behavior can:

- cancel uncommitted gestures;
- preserve persistent posture;
- redirect gaze immediately;
- finish or transfer active contacts;
- preserve unaffected autonomous motion.

### 20.3 Failure categories

- schema unavailable;
- temporal plan inconsistent;
- resource conflict unresolved;
- capability missing;
- target unreachable;
- hard constraint unsatisfied;
- stale revision;
- generator failure;
- numerical failure;
- package invalid;
- external contingent event missed.

Each has a declared fallback ladder and bounded diagnostic.

### 20.4 Transactional frame state

The motor step produces a candidate canonical state. Validation checks finite transforms, hierarchy, hard constraints and declared tolerances. Only a valid complete state replaces the previous accepted state.

On failure, retain the last valid pose, advance safe autonomous clocks if possible, and request degradation or replan. Never commit half a solved body.

---

## 21. Feedback and observability

The realizer publishes:

### Prediction feedback

- expected sync-point windows;
- expected resource release;
- selected alternative and fallback;
- reachability and contact confidence;
- revised future timing.

### Progress feedback

- behavior lifecycle;
- activated sync point;
- semantic nucleus delivered;
- contact acquired/released;
- resource ownership transferred;
- interrupted/degraded/completed.

### Shape feedback

- actual effector target reached;
- final amplitude and quality;
- fallback used;
- important constraint residuals.

### Warning feedback

- unsupported capability;
- missed timing window;
- dropped optional behavior;
- solver relaxation;
- stale revision;
- resource conflict;
- invalid package metadata.

### Diagnostics

Debug builds should expose:

- live temporal graph;
- resource-claim graph;
- controller stack per semantic resource;
- current ground state;
- target and contact geometry;
- solver residuals;
- VRM capability projection;
- semantic provenance.

Release logs omit private message content and retain IDs, offsets and categorical decisions.


## 22. Versioned data and authoring formats

The human-readable formats are authoring and package contracts. Runtime structures are compiled, validated and bounded.

### 22.1 Files

```text
character.vrm
character.eidolon.yaml
style.yaml
humanoid-calibration.yaml
contact-surfaces.yaml
joint-limits.yaml
behavior-library/
    posture/*.yaml
    gesture/*.yaml
    gaze/*.yaml
    face/*.yaml
motion/
    authored/*.vrma
    compiled/*.eprbin
rights.yaml
```

### 22.2 Character package extension

The Eidolon sidecar adds information VRM does not standardize:

- capability declarations;
- anatomical frames and measurements;
- semantic contact surfaces;
- joint limits and comfort regions;
- expression affect calibration;
- custom expression aliases and conflicts;
- preferred body framing and visible bounds;
- style defaults;
- solver tolerances;
- normalized rights conclusion and source evidence.

It does not duplicate VRM node mappings unnecessarily.

### 22.3 Behavior schema format

A schema file should describe:

- semantic eligibility;
- parameters and ranges;
- meaningful nucleus;
- phase topology;
- temporal flexibility;
- resource claims by phase;
- target/contact form;
- quality mapping;
- generator choices;
- capability requirements;
- alternatives and fallback;
- interruption/takeover policy;
- authoring and rights metadata for embedded motion.

### 22.4 Compilation

Offline validation performs:

- schema and version checks;
- reference resolution;
- temporal consistency;
- resource-claim consistency;
- capability and fallback completeness;
- VRMA inspection;
- coordinate-space validation;
- deterministic ID assignment;
- fixed-capacity sizing;
- rights metadata checks.

Runtime loading is transactional and retains the last valid package/library on failure.

---

## 23. C17 implementation architecture

The conceptual system is large. Ownership must remain boring.

### 23.1 Proposed modules

```text
performance_intent       input normalization and revisions
behavior_schema          immutable compiled schema library
behavior_planner         candidate generation and selection
behavior_plan            plan graph and lifecycle
performance_time         temporal network and dispatch
body_resources           resource taxonomy and claims
resource_arbiter         conflict resolution and transfers
realization_program      IR storage and validation
realizer_posture         posture and stance compiler
realizer_gesture         gesture compiler
realizer_gaze            gaze/head allocation
realizer_head            nod/shake/tilt
realizer_face            expression and procedural face layers
realizer_delivery        source-offset motion impulses
realizer_hand            hand-shape programs
realizer_contact         semantic surface constraints
realizer_idle            breathing, blink and micro-motion
controller_compositor    layer/constraint composition
motion_trajectory        minimum-jerk, spline, spring, DMP
physical_solver          limits, IK, contacts, balance
vrm_runtime              VRM model semantics and update order
performance_feedback     prediction/progress/warnings
performance_trace        deterministic diagnostics and replay
```

### 23.2 Ownership

- `performance_intent` owns accepted upstream evidence snapshots.
- `behavior_plan` owns behavior-unit lifecycle and provenance.
- `performance_time` owns temporal points, constraints and dispatch state.
- `resource_arbiter` owns grants and transfers, not motor output.
- modality realizers own program construction, not global plan state.
- motor programs own their private dynamic state.
- `controller_compositor` owns the canonical candidate state for one tick.
- `physical_solver` owns transactional projection to a valid canonical pose.
- `vrm_runtime` owns model-local mapping, expressions, constraints and spring state.

No module keeps a parallel shadow plan or skeleton.

### 23.3 Handles and revisions

Use stable opaque handles and generation counters. Cross-module references never borrow pointers to mutable arrays that can be invalidated by a plan revision.

### 23.4 Memory

- bounded plan capacity;
- bounded resource and constraint arrays;
- arenas or pools allocated during runtime initialization;
- no ordinary frame-path heap allocation;
- immutable compiled behavior assets;
- explicit overflow degradation and diagnostics.

### 23.5 Threading

A practical split:

- application/planning thread accepts intent and builds candidate plan revisions;
- motor/presentation thread consumes an immutable accepted plan snapshot and advances controllers;
- optional worker threads perform learned generation, authored-asset decoding or expensive validation;
- only completed results cross bounded queues;
- stale results are rejected by revision.

The exact thread placement may follow Eidolon's existing main loop, but language/model work never becomes a frame dependency.

### 23.6 External libraries

`ozz-animation` is a technically credible MIT-licensed reference or optional dependency for data-oriented sampling, blending, local-to-model transforms and two-bone IK. Its C++17 runtime and immutable optimized structures conflict with Eidolon's C17-first preference and it does not solve retargeting, semantic planning, contacts or the Behavior Realizer. The architecture should therefore avoid depending conceptually on it. A measured dependency decision can be made after comparing its useful low-level jobs against Eidolon's existing skeleton runtime.

The older academic realizers are primarily design sources, not production dependencies:

- SmartBody: LGPLv3, large legacy C++ platform;
- current Greta: GPLv3, older branch LGPLv3, Java-centric;
- MOSIM Core: MIT but archived and distributed/service-heavy;
- Elckerlyc/ASAP: historically useful Java research stack with fragmented maintenance/distribution;
- EMBR and MURML assets: valuable research, uncertain modern reusable package surface.

Eidolon should reimplement the necessary contracts cleanly unless a permissive, small dependency provides measured value.

---

## 24. Initial V1 behavior vocabulary

The vocabulary is intentionally orthogonal. It should create many recognizable performances through composition rather than hundreds of monolithic animations.

### 24.1 Ground and posture

```text
stance.centered
stance.contrapposto
stance.weight_shift
posture.relaxed
posture.attentive
posture.open
posture.guarded
posture.approach
posture.recoil
arms.resting
arms.crossed.soft
arms.crossed.firm
arms.behind_back
arms.self_contact
arms.akimbo
```

### 24.2 Gestures

```text
gesture.beat
gesture.double_beat
gesture.present
gesture.offer
gesture.indicate
gesture.point
gesture.open_question
gesture.contrast
gesture.rejection
gesture.acknowledge
gesture.resolve
gesture.small_shrug
gesture.self_touch
gesture.withdraw
```

### 24.3 Head and gaze

```text
gaze.acquire
gaze.fixate
gaze.glance
gaze.avert
gaze.return
head.nod
head.double_nod
head.shake
head.tilt
head.recoil
head.forward_ack
```

### 24.4 Face

Use VRM semantic presets when supported:

```text
happy
angry
sad
relaxed
surprised
neutral/package default
```

Add package-local custom aliases only through explicit semantic calibration.

### 24.5 Movement-quality dimensions

```text
extent
speed
power
fluidity
directness
tension
asymmetry
hold_emphasis
repetition
whole_body_recruitment
```

### 24.6 Example compositions

#### Focused explanation

```text
posture.attentive
+ stance.weight_shift subtle
+ gaze.fixate(session bubble)
+ gesture.present on explanation nucleus
+ low-power delivery beats
+ reduced idle amplitude
```

#### Guarded uncertainty

```text
arms.crossed.soft shift
+ narrowed chest openness
+ slight weight back
+ gaze intermittent user/session
+ head tilt
+ high fluidity, low directness
```

#### Irritated correction

```text
arms.crossed.firm or one-arm release from crossed ground
+ chest tension
+ direct gaze acquisition
+ gesture.contrast with short preparation and hard stroke
+ restrained head shake
+ sharp but bounded delivery landing
```

#### Affectionate reassurance

```text
posture.open
+ soft self-contact or relaxed hands
+ slight approach
+ warm face if available
+ gentle head tilt
+ slower preparation and longer settle
```

#### Approval requested

```text
truth: approval.requested
+ attention.user mandatory
+ gaze acquire user
+ contained presenting gesture if hand available
+ confidence maps to direct/open vs hesitant/guarded realization
```

The crossed-arm motif never means one emotion by itself.

---

## 25. Authoring workflow

### 25.1 VRM package preparation

1. acquire or author a legally distributable VRM 1.0 model;
2. validate humanoid hierarchy, T-pose, expressions, look-at, node constraints and spring chains;
3. inspect deformation at shoulder, elbow, wrist, pelvis, knee and ankle extremes;
4. derive anatomical frames and body measurements;
5. author joint comfort and hard-limit data;
6. define semantic contact surfaces;
7. calibrate expression targets and conflicts;
8. preview canonical neutral and extreme validation poses;
9. save rights evidence and normalized package conclusion.

### 25.2 Behavior authoring

1. author the semantic nucleus in a canonical VRM mannequin or task-space editor;
2. annotate resources, contacts and phase points;
3. declare style dimensions and allowable variation;
4. declare alternatives and fallbacks;
5. test from several deliberately awkward incoming states;
6. test successor takeover into several outgoing behaviors;
7. inspect front, side and desktop framing;
8. compile and run deterministic scenario tests;
9. conduct owner visual review.

### 25.3 Tooling

The authoring tool should expose:

- live Behavior Plan Graph;
- temporal windows and sync anchors;
- resource claims and conflicts;
- canonical and model-local skeletons;
- target/contact handles;
- trajectory and velocity plots;
- joint limits and solver residuals;
- VRM expression and override state;
- front/side/three-quarter previews;
- deterministic scenario replay;
- copy/export of schema and calibration data.

Blender remains useful for asset repair, authored motion and visual QA. A lightweight in-Eidolon authoring/debug surface is necessary for runtime state, co-articulation and streaming scenarios that Blender cannot reproduce.

---

## 26. Validation philosophy

Naturalness, semantic appropriateness and physical correctness are separate axes.

A motion may be smooth and human-like but semantically wrong. It may be semantically clear but physically awkward. It may satisfy IK while reading poorly from the desktop camera.

### 26.1 Objective tests

- no invalid transforms or hierarchy corruption;
- hard constraints respected within tolerance;
- joint-limit violations absent;
- contacts maintained or explicitly released;
- planted support preserved;
- time constraints satisfied or diagnosed;
- identical accepted inputs and seed replay identically;
- no source-offset gesture replay after stream extension;
- bounded queues and memory;
- no frame-path dependency on language/model inference;
- capability failures remain local;
- stale revisions cannot mutate current performance.

### 26.2 Perceptual dimensions

Evaluate separately:

1. **semantic appropriateness:** does the body fit what is being communicated and done?
2. **temporal appropriateness:** does the meaningful phase land at the right moment?
3. **physical naturalness:** does motion look human rather than mechanically interpolated?
4. **continuity:** does the body remain one persistent physical entity across changes?
5. **character consistency:** does style remain recognizable without becoming repetitive?
6. **readability:** is intent legible from front, side and actual desktop framing?
7. **restraint:** does Eidolon stay alive without demanding attention continuously?

### 26.3 Owner review is authoritative

Automated metrics cannot determine whether soft crossed arms look comforting, embarrassed or broken. Owner-controlled visual review remains a release gate.

---

## 27. V1 acceptance scenarios

### Scenario A: streamed mixed-emotion explanation

A long response moves from uncertainty to correction to relief.

Acceptance:

- stable semantic beats are planned before reveal reaches them;
- uncertain tail does not commit a contradictory gesture;
- contrast stroke lands on the contrast source offset;
- posture and face evolve without rapid roulette;
- appending text never replays prior motion;
- final resolution settles naturally.

### Scenario B: successor co-articulation

Two consecutive explanatory gestures use the right hand.

Acceptance:

- first retraction and second preparation are shortened or removed;
- hand travels directly between semantic nuclei;
- speech synchronization remains valid;
- no neutral reset or velocity discontinuity occurs.

### Scenario C: crossed-arm interruption

The body holds soft crossed arms. A user-directed approval request requires attention and a presenting hand.

Acceptance:

- attention redirects immediately;
- one arm releases through a valid contact transition;
- the other preserves a plausible partial crossed posture;
- presenting gesture occurs;
- the hand returns to the crossed ground state or a new accepted posture;
- no whole-body reset occurs.

### Scenario D: head/gaze/nod composition

The body looks toward a session bubble while nodding on an acknowledgement.

Acceptance:

- eye fixation is preserved;
- nod pitch composes with head orientation;
- eyes counter-adjust as the head moves;
- joint limits and package look-at range are respected.

### Scenario E: resource conflict

A firm right-hand point overlaps a proposed right-hand delivery beat.

Acceptance:

- semantic point retains ownership;
- delivery beat shifts to head/torso, changes hand or is dropped according to policy;
- decision is deterministic and logged.

### Scenario F: semantic repair

A provisional beat is classified as acknowledgement, then repaired into refusal before its stroke commits.

Acceptance:

- low-cost anticipatory attention may remain;
- uncommitted acknowledgement stroke is replaced;
- existing preparation is reused or redirected if possible;
- no visible contradictory nucleus is delivered.

### Scenario G: late interruption

A gesture stroke has already delivered when the source session is interrupted.

Acceptance:

- delivered meaning is not rewritten;
- recovery is shortened or transferred into interrupted operational posture;
- interruption state takes attention priority.

### Scenario H: capability-poor VRM

The model lacks eye bones, fingers, custom expressions and spring bones.

Acceptance:

- gaze uses expression look-at or head-only fallback;
- hands use wrist/palm posture;
- affect uses supported face or body cues;
- no subsystem crashes or claims unsupported realization.

### Scenario I: impossible contact

Proportions prevent the requested crossed-arm contact.

Acceptance:

- progressive relaxation attempts package-calibrated alternatives;
- fallback guarded non-contact posture is used;
- last valid pose remains intact;
- failure is visible in diagnostics, not in broken wrists.

### Scenario J: concurrent sessions

Two bubbles reveal simultaneously.

Acceptance:

- one deterministic session owns body performance;
- non-owner text continues independently;
- ownership transfer occurs only on explicit rule/event;
- stale plan units cannot regain body resources by iteration order.

### Scenario K: hitch and recovery

The application misses several frames.

Acceptance:

- motion advances from monotonic time with bounded substeps;
- no catch-up gesture burst occurs;
- already skipped delivery marks coalesce;
- contacts and joint limits remain valid;
- latest valid plan state is presented.

### Scenario L: deterministic replay

A recorded event, plan revision and user-interaction stream is replayed.

Acceptance:

- selected schemas, alternatives, timings and motion match within numerical tolerance;
- diagnostics identify any platform-floating-point divergence.

---

## 28. Implementation sequence toward the complete machine

This sequence minimizes architectural rework. It does not reduce the final V1 scope.

### Phase 0: research fixtures and executable specification

- create deterministic intent streams and expected plan outcomes;
- create one legal VRM reference body plus deliberately capability-poor fixtures;
- build a headless plan/resource simulator;
- define versioned types and trace format.

**Gate:** the temporal and resource semantics can be tested without rendering.

### Phase 1: VRM semantic runtime

- humanoid validation and mapping;
- anatomical frames and rest normalization;
- expressions, look-at, node constraints and spring-bone execution order;
- package capabilities and rights metadata;
- canonical pose inspection.

**Gate:** known canonical states project deterministically and correctly.

### Phase 2: Behavior Plan Graph and temporal dispatcher

- behavior-unit lifecycle;
- STN constraints and incremental propagation;
- sync points, predictions and revisions;
- commitment horizons;
- deterministic dispatch and feedback.

**Gate:** streamed scenario plans revise without replay or inconsistent timing.

### Phase 3: Body Resource Arbiter

- semantic resource hierarchy;
- claim modes;
- conflict alternatives;
- local preemption and transfer;
- stranded-resource cleanup.

**Gate:** adversarial conflict scenarios resolve deterministically before real motion exists.

### Phase 4: Realization Program IR and motor runtime

- phase-local time;
- coordinate spaces;
- trajectories;
- masks and constraints;
- program prediction/revision;
- inertial continuity.

**Gate:** programs execute from arbitrary incoming states and expose correct predictions.

### Phase 5: upper-body physical foundation

- posture/spine goals;
- shoulder frames;
- analytic arm IK;
- wrist orientation and twist;
- hand shapes;
- transactional solve.

**Gate:** initial gesture and posture vocabulary is readable from front and side.

### Phase 6: co-articulation and contact

- successor takeover;
- generated preparation/retraction;
- semantic body surfaces;
- hand-body and hand-hand contact;
- coupled constraint solve.

**Gate:** crossed-arm, self-contact and chained gesture scenarios work without neutral reset.

### Phase 7: gaze, head and face

- eye/head/torso allocation;
- gaze shifts and aversion;
- nod/shake composition;
- VRM expression arbitration;
- blink and procedural overrides.

**Gate:** attention remains truthful and head behaviors compose with fixation.

### Phase 8: lower body and balance

- stance and weight distribution;
- planted-foot constraints;
- pelvis/knee/ankle coordination;
- balance preference and counter-motion.

**Gate:** upper-body performance does not make the body look ungrounded.

### Phase 9: delivery, idle and secondary life

- source-offset additive delivery;
- breathing and non-repeating bounded idle;
- state-aware blinking;
- post-gesture settling;
- VRM SpringBone impulses and validation.

**Gate:** an ordinary workday body feels alive but unobtrusive.

### Phase 10: authored motion and generator interchange

- VRMA import and phase annotation;
- authored/procedural generator parity;
- compact motion matching or DMP experimentation;
- learned generator interface and provenance policy.

**Gate:** swapping generator implementation cannot bypass plan, resources or physical validity.

### Phase 11: complete authoring and validation pipeline

- schema editor/debug UI;
- Blender/VRM export pipeline;
- scenario capture and replay;
- visual regression and performance profiling;
- package validation and rights tooling.

**Gate:** a new behavior and a new legal VRM package can be added without engine source edits.

---

## 29. Performance budget principles

No numerical budget is invented before measurement, but the architecture constrains cost:

- behavior planning scales with a small number of active/pending units, not message length per frame;
- temporal graph remains bounded and retires completed units;
- resource arbitration operates over semantic resources, not every bone pair;
- analytic solvers handle common limbs;
- coupled solve is limited to active constraints;
- static or held body state can sleep except for selected autonomic channels;
- expensive inactive generators and body capabilities do not initialize;
- learned generation is asynchronous and optional;
- native presentation and rendering remain separate workstreams.

The body must remain suitable for a persistent transparent desktop application.

---

## 30. Principal risks and unresolved decisions

### 30.1 Resource semantics can become another hidden god object

Mitigation: resource taxonomy, claim modes and conflict rules must remain data-driven and independently testable. The arbiter grants ownership; it does not generate motion.

### 30.2 Temporal theory can be overbuilt

A general STNU/CSTNU solver may be intellectually attractive but unnecessary for the bounded runtime. Begin with incremental STN dispatch plus explicit contingent predictions, while keeping types capable of later dynamic-controllability analysis.

### 30.3 Semantic vocabulary can become a brittle ontology

Keep communicative functions small, compositional and grounded in actual physical choice. Allow schema eligibility to use continuous affect and delivery evidence rather than inventing hundreds of labels.

### 30.4 Procedural motion can look sterile

Use authored nuclei, quality envelopes, current-state motor programs, co-articulation, small legal motion libraries, DMPs or learned priors behind the same contract. Do not weaken semantic and physical authority merely to gain surface naturalness.

### 30.5 Contact and shoulders are likely the hardest visible failures

Crossed arms, self-touch and expressive reaching combine proportion, deformation, sleeve geometry, shoulder recruitment, twist and occlusion. They need package calibration and visual review.

### 30.6 VRM standardization is not deformation standardization

A valid humanoid mapping cannot repair poor skin weights, collapsing shoulders or pathological proportions. Package validation must distinguish semantic compatibility from visual quality.

### 30.7 Face presets are semantically broad and visually inconsistent

VRM does not prescribe deformation for `happy`, `sad` and other expressions. Each public package needs affect-target calibration and conflict review.

### 30.8 Neural systems can produce convincing wrong motion

Semantic appropriateness must be evaluated independently from human-likeness. Learned output stays subordinate to explicit behavior and resource contracts.

### 30.9 Concurrency needs one visible owner

The plan is per shared body, not simply per session. Session performance arbitration must occur before behavior planning so two independent semantic plans do not fight at the resource layer.

### 30.10 “Alive” is partly artistic judgment

No formal architecture removes owner review. The system can prove continuity, constraints, timing and provenance; it cannot mathematically prove charm.

---

## 31. Final architectural decision

Build the **Eidolon Performance Runtime** as a continuously dispatched, deterministic multimodal performance system.

Its defining internal structures are:

1. **Performance Intent** — truthful, semantic and joint-free evidence;
2. **Behavior Schemas** — generative physical vocabulary;
3. **Behavior Plan Graph** — revisioned semantic and temporal commitments;
4. **Temporal Dispatcher** — flexible symbolic synchronization under uncertainty;
5. **Body Resource Arbiter** — explicit compositional ownership of one persistent body;
6. **Modality Realizers** — plural specialized behavior compilers;
7. **Realization Program IR** — the complete executable bridge between behavior and animation;
8. **Controller Compositor** — ground state, temporary layers, constraints and takeover;
9. **Canonical Physical Realizer** — trajectories, limits, IK, contact and balance;
10. **VRM Body Adapter** — standardized model projection and capability degradation;
11. **Feedback and Trace** — prediction, progress, failure and deterministic replay;
12. **Generator Contract** — procedural, authored and learned implementations under the same governors.

The heart of the machine is the combination of a **temporal contract** and a **body-resource contract**.

The temporal contract answers:

> What meaning has been promised, what may still move in time, and what must happen next?

The body-resource contract answers:

> Which behaviors may coexist in this one physical body, what relationships must be preserved, and how can ownership transfer without breaking continuity?

The motor system then answers the comparatively local question:

> Given those accepted commitments and this exact current body state, what trajectories and constraints realize them now?

That separation is what lets Eidolon become more than an avatar playing reactions. It becomes one continuous performer inhabiting real agent work.

---

## 32. Condensed thesis statement

> **Eidolon should model embodiment as an incremental, dynamically dispatched performance graph whose units carry semantic provenance, temporal constraints, body-resource claims and executable realization programs. A deterministic arbiter composes or revises those units against the body's actual current state, then projects the resulting canonical constraints through a capability-aware VRM physical realizer. Authored, procedural and learned motion are interchangeable realization strategies—not owners of meaning, truth or the body.**

