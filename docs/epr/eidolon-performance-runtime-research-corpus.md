# Eidolon Performance Runtime research corpus

## Systematic source notes, design lineage, caveats, and adoption decisions

**Date:** 2026-07-24  
**Project:** Eidolon  
**Target system:** Eidolon Performance Runtime (EPR)  
**Central subsystem:** Behavior Realizer  
**First embodiment contract:** VRM 1.0

This document is the evidence companion to **Eidolon Performance Runtime V1 — A theoretical design for a continuously alive, deterministic VRM embodiment**.

It deliberately separates four things that are too often blended together:

- **fact:** what a specification, paper, or implementation actually establishes;
- **inference:** what follows when that work is applied to Eidolon;
- **adoption decision:** what EPR should inherit, reinterpret, or reject;
- **uncertainty:** what remains insufficiently documented, obsolete, visually subjective, or dependent on implementation inspection.

The goal is not to preserve a museum catalogue of embodied-agent projects. The goal is to retain the hard-won abstractions, failure cases, and licensing facts needed to build one coherent modern system.

---

## 1. Research conclusion

No examined open system already implements the Eidolon Performance Runtime.

Several systems solve important slices:

- **SAIBA / BML** define a clean boundary between communicative intent, behavior planning, physical realization, timing constraints, and feedback.
- **ASAPRealizer / Elckerlyc / BMLA** demonstrate incremental scheduling, symbolic synchronization anchors, co-articulation, graceful interruption, and revision of future behavior while earlier behavior is executing.
- **MURML / ACE** demonstrate form-based gesture descriptions and motor programs that generate connective motion from current state.
- **EMBR** inserts an explicit animation-realization layer of channels, constraints, poses, key poses, and timing profiles between high-level behavior and the final skeleton.
- **SmartBody** demonstrates a practical hierarchy of specialized controllers over mapped semantic joints.
- **Greta** separates communicative behavior from character baseline and expressive style; Greta 2.0 also demonstrates symbolic and neural realization paths coexisting.
- **MOSIM** demonstrates interchangeable heterogeneous motion generators behind a canonical intermediate skeleton and common service contracts.
- **BEAT, NVBG, and Cerebella** demonstrate rule- and evidence-driven selection of nonverbal behaviors from language, dialogue context, and communicative function.
- **VRM 1.0** supplies the strongest current portable stylized-avatar substrate: humanoid roles, expressions, look-at, node constraints, spring bones, animation transport, and embedded rights metadata.
- **modern animation runtimes and research** supply masked/additive composition, inertialization, analytic IK, FABRIK, position-based constraints, minimum-jerk trajectories, dynamic movement primitives, and learned motion generators.

What is missing is their synthesis under Eidolon's unusual conditions:

1. input arrives incrementally from a live working agent;
2. operational truth and language affect are separate evidence streams;
3. one persistent body has durable physical state;
4. several session streams may compete for that one body;
5. behavior must remain revisable without replaying old source offsets;
6. body parts, contacts, gaze, face, posture, delivery, and secondary response must share explicit ownership;
7. the runtime must be deterministic, inspectable, bounded, and suitable for an all-day native application;
8. authored, procedural, and learned realizers must remain interchangeable without bypassing semantics or physical validity.

The central original contribution of EPR is therefore not a new inverse-kinematics algorithm. It is a **continuously dispatched performance contract** joining:

- an incremental temporal network;
- a semantic body-resource and relationship graph;
- generative behavior schemas;
- modality-specific motor programs;
- a common Realization Program intermediate representation;
- a capability-aware VRM physical projection;
- transactional feedback, fallback, and deterministic replay.

---

## 2. Research method

### 2.1 Authority order

Sources were weighted in this order:

1. current official specifications;
2. original papers and theses;
3. official repositories and project documentation;
4. maintained reference implementations;
5. later surveys and project summaries;
6. secondary commentary only where primary material was unavailable.

Licensing conclusions use repository licences, specification licences, or official project statements—not search snippets or “open source” labels.

### 2.2 Evaluation questions

Every system was evaluated against the same questions:

- What semantic level does it represent?
- Is its plan static, incremental, or revisable?
- How does it represent time and synchronization?
- How does it represent gesture/posture form?
- Does it generate preparation and retraction from current state?
- How does it arbitrate simultaneous modalities and body resources?
- How does it interrupt or replace behavior?
- How does it expose prediction, progress, warning, and failure?
- Does it separate character style from communicative meaning?
- Does it support multiple motion-generation techniques behind one contract?
- Does it retarget across bodies?
- What does it assume about speech timing?
- Is source code available, maintained, and legally reusable?
- Which idea belongs in EPR, and which historical mechanism should be left behind?

### 2.3 Terminology discipline

This corpus uses:

- **planner** for choosing communicative and physical behaviors;
- **scheduler/dispatcher** for satisfying temporal constraints and starting due work;
- **realizer** for compiling a selected behavior into executable physical control;
- **motor program** for a stateful generator that advances a behavior from actual current body state;
- **controller** for a frame-rate producer or modifier of body-control values;
- **physical solver** for satisfying kinematic, contact, balance, or other geometric constraints;
- **body adapter** for projection into one loaded model format and capability set.

Historical projects use these terms inconsistently. EPR must not inherit that ambiguity.

---

## 3. Source index and status

| Family | Representative source | Status observed in 2026 | Licence / rights relevance | Primary EPR value |
|---|---|---:|---|---|
| SAIBA / BML | BML 1.0 specification | Stable historical standard | Specification; implementation-dependent | Intent/behavior/realization boundary, sync semantics, feedback |
| BMLA / ASAP | ASAPRealizer, Elckerlyc papers | Research lineage; repositories fragmented | Inspect per repository | Incremental plans, PegBoard anchors, co-articulation, interruption |
| SmartBody | USC ICT SmartBody | Historical; project site remains | LGPLv3 | Specialized controller hierarchy, semantic joint mapping |
| MURML / ACE | Kopp/Wachsmuth lineage | Historical research | Papers; source availability fragmented | Form constraints, motor programs, generated connective phases |
| EMBR | EMBR / EMBRScript papers | Historical research | Verify any recovered code separately | Explicit realization IR and channel/time-warp concepts |
| Greta | ISIR Greta / Greta 2.0 | Active research repository in 2025–2026 | GPLv3 current; old LGPL branch | Style separation, expressivity, symbolic/neural coexistence |
| BEAT / NVBG / Cerebella | original papers/project pages | Historical to later research | Varies | Rule architecture for behavior candidate selection |
| MOSIM | Mercedes-Benz MOSIM_Core | Archived, unmaintained | MIT | Motion-unit interface, intermediate skeleton, co-simulation contract |
| VRM | vrm-c/vrm-specification | Complete VRM 1.0 specs | Spec open; each avatar has own rights metadata | First body contract and projection substrate |
| ozz-animation | guillaumeblanc/ozz-animation | Maintained | MIT | Efficient sampling, masked/additive blending, analytic IK reference |
| Unreal animation systems | official documentation | Maintained proprietary engine | Learn concepts, do not copy engine code | Layering, inertialization, full-body IK controls |
| STN/STNU | Dechter et al.; Morris et al. | Mature temporal theory | Papers/algorithms | Dispatchable flexible time under revision/uncertainty |
| Motor primitives | Flash/Hogan; DMP literature | Mature research | Papers/implementations vary | Minimum-jerk, splines, DMPs as trajectory primitives |
| PBD/XPBD | Müller et al.; Macklin et al. | Mature simulation methods | Papers/implementations vary | Coupled contacts, balance, soft constraints |
| Neural motion | PFNN, GENEA, PRIMAL, motion matching | Active | Dataset/model rights vary sharply | Optional generators and residual style, not semantic authority |

---

## 4. SAIBA and Behavior Markup Language

### 4.1 Problem addressed

The SAIBA framework separates:

```text
communicative intent
        ↓
behavior planning
        ↓
behavior realization
```

BML is the behavior-plan/realizer boundary. It describes physically realizable multimodal behaviors and their synchronization rather than the underlying communicative intent.

Primary sources:

- BML 1.0 specification: https://www.mindmakers.org/projects/bml-1-0/wiki
- SAIBA framework overview and publications: https://www.mindmakers.org/projects/saiba/wiki
- Kopp et al., *Towards a Common Framework for Multimodal Generation: The Behavior Markup Language*.

### 4.2 Useful abstractions

#### Blocks and composition

BML requests group behaviors and define composition relative to existing behavior. Historical composition modes include merging with current behavior, appending after it, or replacing it.

The important EPR lesson is that a new request is not merely another animation. It has an explicit relationship to already scheduled and executing performance.

#### Synchronization points

Behaviors expose named semantic phase points such as:

- start;
- ready;
- stroke start;
- stroke;
- stroke end;
- relax;
- end.

Other behaviors can align against those points with offsets. This creates a graph of symbolic temporal relationships rather than a pile of absolute timestamps.

#### Bidirectional constraints

A behavior's own timing can be constrained by another behavior, and a behavior can become the reference for downstream behavior. The realizer must resolve the network and report whether the requested synchronization is possible.

#### Feedback

BML establishes the expectation that realizers report:

- prediction of timing;
- progress at sync points;
- completion;
- warning or failure.

This is essential for EPR diagnostics and for an upstream planner that may revise future performance.

#### Ground behavior versus temporary behavior

BML's distinction between persistent posture/face state and temporary behavior supports the EPR concepts of **ground state**, **shift**, and **temporary unit**.

### 4.3 Caveats

- BML is fundamentally a request language, not a complete runtime architecture.
- XML is inappropriate as EPR's frame-critical internal representation.
- Most BML behavior vocabularies are too coarse and implementation-specific to define Eidolon's physical vocabulary.
- BML does not itself solve body-resource ownership, contact, physical constraints, or shared-body arbitration.
- “Replace” semantics are often too global; an alive performer needs selective successor takeover and stranded-resource cleanup.
- Static blocks fit finished utterances better than continuously extending language.

### 4.4 EPR adoption decision

**Adopt:**

- the intent/planner/realizer boundary;
- named semantic phase points;
- symbolic synchronization constraints;
- prediction/progress/warning feedback;
- ground/shift/temporary semantics;
- explicit relation of new work to existing work.

**Reinterpret:**

- a BML block becomes a revisioned group of typed Behavior Units in one persistent Behavior Plan Graph;
- composition is resolved per semantic body resource and commitment horizon, not as one global block mode;
- required/optional behavior becomes capability-aware alternatives and degradation policies.

**Reject:**

- XML as internal state;
- behavior names as direct animation filenames;
- completed-utterance assumptions;
- all-or-nothing replacement of the entire performer.

---

## 5. BMLA, ASAPRealizer, and Elckerlyc

### 5.1 Problem addressed

ASAPRealizer and its Elckerlyc lineage focus on fluid, incremental multimodal realization: new behavior can be planned while earlier behavior is executing, timing can be updated, interruptions can be graceful, and adjacent gestures can co-articulate.

Representative sources:

- Kopp et al./van Welbergen et al., ASAPRealizer and Elckerlyc papers.
- van Welbergen et al., *An Incremental Multimodal Realizer for Behavior Co-Articulation and Coordination*.
- BMLA extension papers and ASAP documentation where available.

### 5.2 Useful abstractions

#### Symbolic time pegs / PegBoard

Plan units attach their sync points to shared symbolic anchors. When an anchor moves, dependent behaviors can be realigned without reconstructing every absolute time manually.

#### Incremental plan construction

A plan is not frozen when its first behavior begins. New units can be inserted; future timing can be shifted; already delivered phases remain stable.

#### Anticipators

An anticipator predicts external or future events and exposes timing anchors that behaviors may align to. This is relevant to:

- predicted dialogue reveal points;
- uncertain streaming completion;
- future speech timing;
- user interaction;
- source lifecycle transitions.

#### Graceful interruption

Interruption can preserve currently meaningful body state, transition to an interrupt target, or later resume, rather than globally snapping to neutral.

#### Co-articulation

Adjacent behaviors reuse current effector state:

- preparation can shorten when a hand is already near the next gesture space;
- a successor preparation can replace an obsolete retraction;
- cleanup motion is generated only for body resources left stranded;
- gesture chunks can be joined more naturally than independently realized blocks.

#### Grounded motor units

Lower-level motion units can be parameterized and scheduled while reporting their state upward.

### 5.3 Caveats

- The architecture and implementation lineage are academically sophisticated but fragmented across papers, old repositories, Java components, and extensions.
- Their resource-conflict model is not sufficient for Eidolon's one persistent body receiving several simultaneous semantic channels.
- Many examples assume speech with known or externally supplied timing rather than source-offset streaming reveal.
- The plan-unit APIs and terminology should not be copied blindly into a C17 runtime.
- Full generality in anticipators and block composition could create an overbuilt scheduler.

### 5.4 EPR adoption decision

**Adopt:**

- shared symbolic anchors;
- revisioned future plan;
- incremental insertion;
- co-articulation based on current effector state;
- successor takeover;
- graceful interruption;
- cleanup only for stranded resources;
- detailed timing and progress feedback.

**Strengthen for Eidolon:**

- every plan unit carries semantic provenance and source revision;
- body-resource claims are first-class and hierarchical;
- commitment horizons distinguish semantic, temporal, motor, contact, and attention commitments;
- source-offset activation cannot replay after stream repair;
- dynamic dispatchability is verified through a bounded temporal network.

**Reject:**

- a monolithic mutable timestamp table without formal constraint semantics;
- implicit resource ownership inferred from controller order;
- dependencies on speech engines or Java object graphs.

---

## 6. MURML and ACE

### 6.1 Problem addressed

MURML describes gesture form below the coarse behavior-name level. ACE-style animation realization turns those descriptions into local motor programs that adapt to the character's actual movement state.

Representative sources:

- Kopp and Wachsmuth, MURML and gesture-generation publications.
- ACE / Articulated Communicator Engine publications.
- comparative work on BML realizer gesture-description languages.

### 6.2 Useful abstractions

#### Semantic nucleus versus connective motion

A gesture description primarily specifies the meaningful part:

- location;
- orientation;
- hand shape;
- trajectory;
- symmetry;
- repetition;
- stroke timing.

Preparation and retraction can be generated from current state rather than being authored into every behavior.

#### Dynamic spatial constraints

A hand target can be described in body- or task-relative space rather than as model-local joint rotations.

#### Composition operators

MURML supports forms built from sequence, parallelism, symmetry, repetition, and static/dynamic constraints.

#### Motor programs

A motor program is stateful. It does not merely sample one precomputed clip; it continually advances toward a goal while observing current conditions.

### 6.3 Caveats

- Tree-structured descriptions can represent the same physical behavior in multiple equivalent ways, complicating canonicalization and comparison.
- MURML's contact representation is inadequate for Eidolon's self-contact and crossed-arm goals.
- Arbitrarily nested form languages become difficult to validate, author, version, and optimize.
- Historical implementations focus strongly on hand/arm gesture and less on one integrated whole-body performer.

### 6.4 EPR adoption decision

**Adopt:**

- semantic nucleus;
- task/body-relative form constraints;
- generated connective phases;
- stateful motor programs;
- explicit symmetry, repetition, trajectory, hand orientation, and hand shape.

**Reformulate:**

- use a flat, typed, versioned graph of targets, trajectories, contacts, phases, and constraints rather than unrestricted recursive XML trees;
- model contact as both a resource claim and a physical relationship;
- combine gesture form with posture, gaze, face, and movement-quality envelopes.

**Reject:**

- raw tree syntax as the canonical interchange;
- authoring complete prep/stroke/retract clips for every semantic behavior;
- gesture-only ownership of torso and gaze effects.

---

## 7. EMBR and the explicit realization layer

### 7.1 Problem addressed

EMBR recognizes that high-level behaviors are too abstract to send directly to an animation engine. EMBRScript describes channels, poses, key poses, spatial constraints, and temporal envelopes in an intermediate animation layer.

Representative source:

- Heloir and Kipp, *Real-time animation of interactive agents: Specification and realization* / EMBR publications.

### 7.2 Useful abstractions

- channel-specific control;
- key poses and constraint targets;
- time-warp/envelope functions;
- autonomous motion such as breathing;
- skeletal and non-skeletal outputs, including morphs and material effects;
- realization commands that are more executable than BML yet more semantic than final joint matrices.

### 7.3 Caveats

- EMBRScript is another historical text language, not a modern runtime IR.
- A channel is not the same as semantic resource ownership; two channels may still conflict physically.
- Time-warping arbitrary authored animation does not guarantee contact, balance, or anatomical validity.
- The system does not solve Eidolon's incremental stream-revision semantics.

### 7.4 EPR adoption decision

The strongest inheritance is architectural:

> EPR requires a first-class **Realization Program IR** between Behavior Units and final skeleton control.

That IR should contain:

- semantic-resource masks;
- trajectory generators;
- target frames;
- contact and balance constraints;
- expression/gaze channels;
- timing envelopes;
- phase state;
- predicted completion;
- interruption and takeover hooks;
- fallback branches;
- provenance.

The IR is compiled and bounded. It is not parsed from XML every frame.

---

## 8. SmartBody

### 8.1 Problem addressed

SmartBody is a practical embodied-character runtime combining BML realization with locomotion, steering, gaze, reaching/manipulation, speech and lip synchronization, nonverbal behavior, physics, animation blending, and runtime retargeting.

Primary project source:

- https://smartbody.ict.usc.edu/

Licence:

- LGPLv3 according to official project material.

### 8.2 Useful abstractions

- semantic joint mapping across character rigs;
- specialized controllers rather than one universal animation controller;
- hierarchical controller composition;
- procedural gaze and reaching beside authored clips;
- controller masks and priorities;
- retargeting as a body-adaptation layer;
- practical integration of speech, face, gaze, posture, locomotion, and gesture.

### 8.3 Caveats

- Large historical engine with assumptions irrelevant to Eidolon's renderer and presentation stack.
- LGPL incorporation would require deliberate dependency/distribution policy.
- “Supports BML and many controllers” does not imply the explicit resource and streaming-revision model EPR needs.
- Its broad feature surface should not become an excuse to import an obsolete architecture wholesale.

### 8.4 EPR adoption decision

**Learn from:**

- controller decomposition;
- semantic skeleton maps;
- procedural/authored coexistence;
- controller-tree composition;
- practical failure modes around gaze, reach, locomotion, and speech.

**Do not adopt wholesale:**

- engine ownership;
- rendering or platform layers;
- global controller ordering as conflict policy;
- large LGPL dependency before a measured value case exists.

---

## 9. Greta and Greta 2.0

### 9.1 Problem addressed

Greta is an expressive embodied-conversational-agent platform. Its lineage separates communicative intention, selected behavior, and expressive realization, with a character baseline modifying qualities such as extent, speed, strength, fluidity, and timing.

Current repository:

- https://github.com/isir/greta

Observed licensing:

- current mainline: GPLv3;
- historical `master-lgpl` branch: LGPLv3.

Greta 2.0 material describes symbolic SAIBA-style processing alongside frame-by-frame neural generation and blending.

### 9.2 Useful abstractions

- character baseline/personality separate from momentary communicative intent;
- expressive parameter transformation rather than separate clips for every style;
- modality synchronization;
- facial and gesture expressivity;
- symbolic and neural generation under one broader realization system.

### 9.3 Caveats

- GPL code is unsuitable for direct inclusion in Eidolon's current licensing posture.
- Psychological interpretation of expressive dimensions must not be treated as universal fact.
- Style parameters can create physically invalid or semantically exaggerated motion unless constrained.
- Parallel neural/symbolic output still requires one authority for conflicts and final validity.

### 9.4 EPR adoption decision

**Adopt:**

- static character style profile;
- dynamic expressive-quality envelope;
- transformation of trajectories/timing/amplitude rather than behavior identity;
- interchangeable symbolic and learned generator implementations.

**Constrain:**

- operational truth and communicative function remain unchanged by style;
- style transformations are bounded by character capability, anatomy, contact, and readability;
- neural output enters through the same Realization Program contract.

---

## 10. BEAT, NVBG, and Cerebella

### 10.1 Problem addressed

These systems infer or select nonverbal behavior from text, dialogue structure, communicative function, prosody, and conversational context.

Representative sources:

- Cassell et al., BEAT: the Behavior Expression Animation Toolkit.
- USC ICT Nonverbal Behavior Generator publications.
- Cerebella publications on multimodal behavior generation.

### 10.2 Useful abstractions

- staged linguistic analysis;
- annotated conversational rules;
- candidate generation followed by filtering and scheduling;
- separation between evidence extraction and realization;
- behavior lexicons tied to discourse and communicative function;
- probabilistic or rule-based variation without surrendering the final realizer contract.

### 10.3 Caveats

- Many systems assume complete text and known speech timing.
- Their language analysis predates modern LLM semantic evidence and may be unnecessary upstream complexity for Eidolon.
- Rule sets can become brittle if they directly encode animations instead of physical schemas.
- Gesture frequency and cultural appropriateness require character- and context-specific authoring.

### 10.4 EPR adoption decision

Eidolon already supplies stronger upstream evidence:

- stable streamed semantic prefixes;
- source-offset semantic beats;
- deterministic delivery marks;
- continuous affect axes;
- truthful operational state.

EPR should inherit the **candidate-rule architecture**, not duplicate legacy NLP pipelines.

Rules should produce Behavior Schema candidates with provenance and confidence, never final joint commands.

---

## 11. MOSIM and Motion Model Units

### 11.1 Problem addressed

MOSIM is a modular framework for combining heterogeneous motion-generation techniques. Motion Model Units can be implemented in different languages/tools, operate through an intermediate skeleton, use services such as IK and path planning, and have their outputs merged by a co-simulator.

Official repository:

- https://github.com/mercedes-benz/MOSIM_Core

Observed status:

- archived and no longer maintained;
- MIT licensed.

### 11.2 Useful abstractions

- one explicit motion-generator interface;
- common simulation state in and result out;
- intermediate skeleton independent from target engine;
- co-simulation of multiple generators;
- reusable services for IK, blending, path planning, collision, and scene access;
- high-level task instructions separated from motion implementation.

### 11.3 Caveats

- Distributed Apache Thrift services are far too heavy for Eidolon's in-process native runtime.
- Network/distributed ownership creates latency, failure, and lifecycle complexity irrelevant to the product.
- Industrial task simulation emphasizes goal completion more than communicative acting and continuous semantic revision.
- The project is archived.

### 11.4 EPR adoption decision

Define an in-process, statically linked **Generator / Motion Unit ABI** with:

- immutable input snapshot;
- bounded output buffers;
- explicit resource mask;
- capability declaration;
- deterministic seed;
- prediction/progress/failure result;
- no ownership of global time, body state, or final skeleton;
- no dynamic allocation in frame-critical execution.

Procedural, authored, motion-matching, DMP, and learned generators can all implement this contract.

Reject MOSIM's deployment architecture while retaining its modularity principle.

---

## 12. Temporal reasoning: STN and STNU

### 12.1 Problem addressed

A performance plan contains flexible temporal relationships:

- a gaze shift should begin shortly before a gesture stroke;
- a gesture stroke must land near a semantic source offset;
- preparation may compress within a legal range;
- a hold may extend while text continues;
- an interruption may occur at an externally determined time;
- some future events are predicted but not controlled by EPR.

Simple absolute timestamps are brittle under incremental revision.

Primary theory:

- Dechter, Meiri, and Pearl, *Temporal Constraint Networks*;
- Morris, Muscettola, and Vidal on Simple Temporal Networks with Uncertainty;
- work on dispatchable forms and dynamic controllability.

### 12.2 Useful abstractions

#### Simple Temporal Network

Represent behavior sync points as time-point nodes and min/max difference constraints as edges:

```text
minimum ≤ t_b - t_a ≤ maximum
```

An STN compactly represents flexible legal timing and can be compiled into a dispatchable form.

#### Contingent intervals

Some time points are observed rather than controlled. STNU concepts distinguish executable decisions from contingent events and ask whether a strategy can remain valid as those events are revealed.

#### Dynamic dispatch

At runtime, choose an execution time using only events observed so far while preserving feasibility for future constraints.

### 12.3 Caveats

- Full STNU or conditional temporal-network theory can become an academic subsystem larger than the product need.
- Maintaining dynamic controllability under arbitrary revisions is complex.
- Text reveal is partly controlled by Eidolon, while source arrival is contingent; treating both identically would be wrong.
- Temporal feasibility does not resolve body-resource or physical feasibility.

### 12.4 EPR adoption decision

Use a bounded **incremental STN-like Behavior Plan Graph**:

- named phase time points;
- lower/upper bounds;
- equality/offset constraints;
- controlled versus contingent points;
- explicit plan revisions;
- commit horizons;
- dispatchable earliest/latest windows;
- deterministic conflict and relaxation policy.

V1 need not expose a general-purpose STNU solver. Its types should preserve the distinction between controlled and contingent events so later dynamic-controllability checks can be added without replacing the plan format.

Reject full Allen interval algebra as the central runtime representation. It is expressive but unnecessarily general and can make consistency reasoning intractable.

---

## 13. Gesture-phase theory

### 13.1 Research lineage

Kendon and McNeill describe gesture as phased activity rather than one undifferentiated animation. Commonly useful phases include:

- preparation;
- optional pre-stroke hold;
- stroke;
- optional post-stroke hold;
- retraction/recovery.

The stroke carries the primary communicative meaning. Holds and connective phases coordinate timing and continuity.

### 13.2 EPR interpretation

A Behavior Schema should distinguish:

- **semantic nucleus:** the stroke or held physical relationship that must remain recognizable;
- **connective tissue:** preparation, transitions, recovery, and settling generated from current state;
- **temporal flexibility:** which phases may compress, extend, overlap, or be replaced;
- **delivery commitment:** whether the meaning has already been visibly delivered.

### 13.3 Caveats

- Not every behavior is a hand gesture; posture shifts, gaze, facial behavior, and operational attention have different phase structures.
- “Stroke” should therefore be a general semantic nucleus, not a mandatory hand-motion event.
- A long-lived posture motif may have acquire, establish, hold, release, and settle phases.

### 13.4 Adoption decision

Each modality owns a phase vocabulary compatible with a shared lifecycle and common temporal anchors. EPR should not force every behavior into one exact gesture template.

---

## 14. Expressive movement quality: Laban, EMOTE, and Greta

### 14.1 Problem addressed

The same behavior form can feel different through timing and dynamics:

- direct versus wandering;
- sudden versus sustained;
- strong versus light;
- bound versus free;
- expanded versus contracted;
- symmetric versus asymmetric.

EMOTE and related systems operationalize selected Laban-inspired qualities into animation parameters.

### 14.2 Useful abstractions

EPR can use a restrained computational envelope:

- amplitude/extent;
- speed;
- acceleration sharpness;
- continuity/fluidity;
- tension/stiffness;
- overshoot;
- damping/settling;
- directness/curvature;
- asymmetry;
- preparation ratio;
- hold ratio;
- recoil/approach bias.

### 14.3 Caveats

- Laban terminology is descriptive practice, not a guaranteed universal psychological decoder.
- Mapping “anger” directly to “strong/direct/sudden” would repeat the emotion-to-animation mistake.
- Quality transforms can destroy contact, readability, balance, or joint safety.

### 14.4 EPR adoption decision

Movement quality is a transform applied after behavior identity is chosen and before/within trajectory realization. It is constrained by:

- semantic nucleus;
- physical validity;
- character style;
- body capability;
- current operational truth;
- per-schema bounds.

---

## 15. Gaze, head, and torso coordination

### 15.1 Research findings

Human orienting distributes target acquisition across eyes, head, and sometimes torso. The distribution depends on:

- target eccentricity;
- movement urgency;
- current task;
- social purpose;
- individual eye-mover/head-mover tendency;
- physical range and comfort.

Eyes generally initiate fast orienting, head follows for larger or sustained shifts, and torso participates when target angle or engagement demands it. Blinks and saccades interact with attention but should not become constant random noise.

### 15.2 VRM relevance

VRM look-at supports either:

- eye-bone local rotation; or
- expression-based look directions.

It defines a head-relative look-at space, range maps, and one shared line-of-sight direction for both eyes. It does not represent independent vergence or cross-eyed targets.

### 15.3 EPR adoption decision

The Gaze Realizer should own a semantic attention target and compile it into:

1. eye target acquisition;
2. head-follow trajectory;
3. optional torso-follow request;
4. dwell and aversion policy;
5. blink/saccade scheduling;
6. capability fallback.

The resource arbiter must coordinate gaze with head nods, posture, face, and user-driven model rotation.

Character style includes an eye/head-mover parameter rather than hard-coding one human average.

---

## 16. VRM 1.0 as the first body contract

### 16.1 Why VRM

VRM provides a popular, stylized, glTF-based humanoid ecosystem with explicit semantics for:

- humanoid bones;
- expressions;
- look-at;
- spring bones;
- node constraints;
- avatar metadata and usage permissions;
- separate portable VRM Animation files.

Official specification repository:

- https://github.com/vrm-c/vrm-specification

### 16.2 Humanoid

VRM 1.0 requires a fixed semantic core including hips, spine, head, upper/lower arms, hands, upper/lower legs, and feet. Chest, upper chest, neck, shoulders, eyes, jaw, toes, and finger chains are optional under defined parent relationships.

Important constraints:

- humanoid bone assignments are unique;
- positive scale is required;
- non-humanoid nodes may exist between humanoid nodes;
- the hierarchy is semantically fixed even when optional intermediates are absent.

### 16.3 Expressions

VRM expressions are semantic groups of:

- morph-target binds;
- material-color binds;
- texture-transform binds.

Preset emotions, viseme-like mouth shapes, blink, and expression-based gaze are optional. VRM deliberately does not prescribe exact facial deformation for `happy`, `sad`, and similar labels.

Procedural override metadata allows non-procedural expressions to block or attenuate mouth, blink, and look-at channels to avoid broken combinations.

### 16.4 Look-at

VRM defines:

- head-relative gaze origin;
- +Z forward in model space;
- yaw/pitch conversion;
- separate inner/outer and up/down range maps;
- bone- or expression-based realization.

Both eyes share a direction; independent vergence is outside the format.

### 16.5 Node constraints

VRM node constraints include:

- roll transfer, intended especially for twist bones;
- aim constraints;
- local-local rotation copy.

They are useful package-level mechanics after EPR has produced the humanoid pose. They are not replacements for EPR's semantic contact or full-body solve.

### 16.6 Spring bones

VRM spring bones define inertial chains with stiffness, drag/deceleration, gravity, colliders, and optional center-relative evaluation. They target hair, costumes, and accessories.

Caveats:

- branching-chain execution order can be undefined;
- duplicate participation is prohibited;
- EPR must not also procedurally own the same nodes;
- spring update belongs after humanoid, look-at, expressions, and node constraints.

### 16.7 VRM Animation

VRMA maps glTF nodes to humanoid roles, expressions, and gaze so one animation file may be applied to compatible VRM avatars. Humanoid animation uses rotations, with translation normally restricted to hips. A VRM T-pose is the reference.

VRMA is valuable as:

- authored motion interchange;
- a test vector for retargeting;
- a source of semantic animation clips behind EPR's generator contract.

It is not the Behavior Plan format and should not own meaning or scheduling.

### 16.8 Recommended update order

The official VRM 1.0 order is:

1. resolve humanoid bones;
2. resolve look-at;
3. set expression weights;
4. apply expressions;
5. resolve node constraints;
6. resolve spring bones.

EPR's VRM adapter should preserve this order exactly unless a documented compatibility reason requires otherwise.

### 16.9 Rights metadata

VRM metadata records:

- authors and copyright information;
- third-party licences;
- avatar permission;
- commercial usage;
- credit requirements;
- redistribution;
- modification and modified redistribution;
- additional licence URL.

The format is open; each model is not automatically open. EPR package validation must treat restrictive defaults as restrictive and preserve the source rights record.

### 16.10 EPR adoption decision

VRM is the first **projection and capability substrate**, not the internal behavior language.

EPR adds package-local calibration for:

- anatomical frames;
- joint limits and preferred bends;
- body measurements;
- contact surfaces;
- expression targets and conflicts;
- gaze/head-follow style;
- hand-shape presets;
- known deformation limits;
- fallback policies.

---

## 17. Animation composition and controller runtimes

### 17.1 ozz-animation

Official repository:

- https://github.com/guillaumeblanc/ozz-animation

Licence:

- MIT.

Capabilities relevant to EPR:

- renderer- and engine-agnostic skeletal sampling;
- data-oriented runtime structures;
- normal and additive pose layers;
- per-joint blend weights;
- rest-pose fallback;
- analytic two-bone IK with pole vector, twist, soften, blend weight, and reachability output;
- offline asset conversion.

Caveats:

- C++17, while Eidolon core is C17-first;
- no complete semantic system;
- no general retargeting solution in current public runtime;
- pose blending alone does not preserve contacts or semantic relationships.

Decision:

- use as a reference and possible quarantined dependency only after measuring integration cost;
- EPR owns semantics, resources, plans, and constraint composition regardless of low-level runtime choice.

### 17.2 Masked and additive layers

Modern engines establish useful operators:

- normal weighted blend;
- per-joint mask;
- additive local-space offset;
- mesh/model-space additive correction;
- slot or region ownership;
- inertialized transition.

EPR needs these, but with semantic ownership above them. A hand contact cannot be maintained by normalized pose blending alone.

### 17.3 Inertialization

Inertialization computes a decaying offset from outgoing pose/velocity into an incoming target, avoiding a long crossfade that evaluates both sources.

Useful for:

- generator switches;
- fallback replacement;
- abrupt target changes;
- clip-to-procedural transition;
- non-contact joints after interruption.

Caveat:

- inertialization is not valid across every contact or constraint transfer. Contact-preserving takeover needs explicit constrained transition.

---

## 18. Physical solving

### 18.1 Analytic two-bone IK

Best default for ordinary arms and legs:

- deterministic;
- constant-time;
- explicit preferred bend/pole;
- easy reach and soften diagnostics;
- simple to blend.

Limitations:

- chain-local;
- does not solve simultaneous hand contacts, pelvis response, balance, or shoulder recruitment.

### 18.2 FABRIK

FABRIK iteratively places joints using forward/backward reaching. It is fast, intuitive, and suitable for longer or unusual chains.

Use cases:

- non-standard accessories;
- multi-joint tails or tentacles;
- approximate variable-length chains;
- some authoring tools.

Limitations:

- target and pole choice remain upstream problems;
- joint limits and orientation need additional treatment;
- coupled whole-body contacts remain outside the basic algorithm.

### 18.3 Jacobian / damped least squares

Useful when several end-effectors or orientations must be solved together and a differentiable local solve is acceptable.

Limitations:

- tuning and singularity behavior;
- iterative cost;
- less predictable artistic control than analytic limbs.

### 18.4 Position-Based Dynamics and XPBD

PBD/XPBD solve constraints over positions or generalized coordinates iteratively. XPBD improves stiffness behavior across timestep/iteration changes.

Potential EPR use:

- hand/body and hand/hand contact;
- planted feet;
- pelvis/torso coupling;
- balance and reach redistribution;
- soft posture relationships;
- secondary constrained motion.

Caveats:

- a physics-like constraint solve can feel mushy without priorities/compliance design;
- rotations and anatomical limits need careful formulation;
- iteration order and convergence must remain deterministic;
- it should refine a semantic target, not invent behavior.

### 18.5 Solver hierarchy decision

EPR should select the smallest solver that satisfies each accepted Realization Program:

1. direct FK / authored sample;
2. analytic two-bone solve;
3. FABRIK or DLS for special chains;
4. coupled PBD/XPBD-like solve for contacts, planted feet, and full-body redistribution;
5. progressive relaxation or fallback if infeasible.

No one solver becomes the architecture.

---

## 19. Trajectory and motor primitives

### 19.1 Minimum-jerk trajectories

Minimum-jerk movement provides a useful smooth point-to-point baseline and a perceptually plausible default for deliberate reaching.

Use for:

- simple preparation/recovery;
- gaze/head transitions after saccade component;
- direct hand and torso shifts;
- deterministic tests.

It does not model every expressive motion or obstacle path.

### 19.2 Splines and piecewise curves

Cubic/quintic Hermite or Bézier curves provide:

- endpoint position/velocity control;
- authored path shape;
- deterministic interpolation;
- easy phase segmentation.

Use for explicit curved gestures and spatially authored strokes.

### 19.3 Damped springs

Use for:

- settling;
- restrained delivery impulses;
- attention lag;
- small inertial follow;
- recovery after target change.

Permanent sine-wave motion is not a substitute for dynamics.

### 19.4 Dynamic Movement Primitives

DMPs represent attractor-based trajectories with a learned or authored forcing term and can adapt to new goals and durations.

Potential use:

- authored gesture strokes retargeted to new endpoints;
- reusable approach/retract shape;
- motion-library compression;
- learned expressive trajectory form behind deterministic goals.

Caveats:

- standard DMPs do not inherently preserve obstacle/contact constraints;
- timing scaling can distort expressive intent;
- a DMP remains a motor implementation, not a behavior definition.

### 19.5 EPR trajectory primitive set

V1 IR should support at least:

- hold;
- minimum-jerk point-to-point;
- Hermite/quintic spline;
- damped spring;
- authored sampled curve;
- DMP-like attractor primitive;
- target-follow controller;
- constrained/contact-maintaining controller;
- inertialized takeover.

---

## 20. Learned motion and motion matching

### 20.1 Phase-Functioned Neural Networks

PFNN demonstrates that learned locomotion/control policies can execute interactively in milliseconds and with relatively small runtime weights. It establishes feasibility of compact neural motion control, not semantic correctness for conversation.

### 20.2 Motion matching and learned motion matching

Motion matching retrieves high-quality source motion based on current pose and trajectory features. Learned motion matching reduces database or lookup cost.

Potential EPR use:

- gesture connective motion;
- posture transitions;
- idling variation;
- later locomotion;
- authored high-quality strokes.

Caveats:

- database rights and memory;
- retrieved motion may be semantically inappropriate;
- contact and body-region conflicts remain;
- sparse stationary conversational datasets may limit quality.

### 20.3 GENEA Challenge evidence

GENEA evaluations separate:

- human-likeness/naturalness;
- semantic appropriateness to speech.

Several systems can produce motion approaching motion-capture naturalness while remaining only weakly better than chance at matching the actual speech. This is the most important neural warning for Eidolon.

### 20.4 Modern multimodal generators

Recent systems increasingly condition on text, audio, interlocutor state, style, or scene. They may improve variation and co-speech timing, but:

- model and dataset licences vary;
- text-only input lacks actual prosody;
- stochastic generation complicates deterministic replay;
- long-horizon semantic consistency remains difficult;
- a plausible gesture can still contradict operational truth.

### 20.5 EPR adoption decision

Learned systems may implement:

- a modality generator;
- a trajectory forcing term;
- a bounded canonical-space residual;
- motion-quality/style adaptation;
- candidate ranking.

They may not bypass:

- Performance Intent;
- Behavior Plan Graph;
- temporal commitments;
- Body Resource Arbiter;
- contacts and joint limits;
- capability projection;
- deterministic fallback;
- rights metadata.

A learned generator must support deterministic seed/replay and report confidence/capability/failure through the same interface as procedural generators.

---

## 21. Body-resource arbitration: the principal missing synthesis

### 21.1 Evidence from prior systems

Prior systems provide pieces:

- BML provides behavior timing and block composition;
- ASAP provides incremental scheduling and co-articulation;
- SmartBody provides controller hierarchies and masks;
- EMBR provides channels;
- MOSIM provides co-simulation;
- animation engines provide layer priorities and joint masks.

None examined provides a sufficiently explicit, semantic, hierarchical resource contract for Eidolon's case:

- one persistent body;
- several modalities;
- current-state motor programs;
- contacts spanning body parts;
- concurrent session evidence;
- streaming repair;
- semantic commitments already delivered;
- capability degradation.

### 21.2 Why joint masks are insufficient

A joint mask says which transforms a controller may write. It does not say:

- the left hand is in cooperative contact with the right upper arm;
- the head is serving both direct gaze and a nod;
- the torso is baseline posture plus an additive delivery accent;
- a gesture has delivered its semantic stroke but still owns recovery;
- the feet must remain planted while the pelvis redistributes reach;
- a successor can inherit the current hand state without neutralization.

### 21.3 Required EPR model

Resources are semantic and hierarchical:

```text
body
├── root / locomotion
├── balance / support
├── pelvis
├── torso
│   ├── spine
│   ├── chest
│   └── shoulders
├── left arm / hand / fingers
├── right arm / hand / fingers
├── head
├── gaze / eyes
├── face
├── mouth
├── left leg / foot
├── right leg / foot
└── secondary chains
```

Claims include modes such as:

- baseline;
- exclusive;
- additive;
- masked override;
- cooperative;
- constraint;
- observational;
- passive.

Relationships such as contact are first-class graph edges, not hidden side effects of bone writes.

### 21.4 Novelty claim, carefully stated

The novelty is not that no prior engine has priorities, masks, controllers, or constraints. The novel synthesis is:

> one revisioned semantic performance graph in which temporal commitments, semantic provenance, body-resource claims, physical relationships, and executable motor programs are jointly dispatched against a continuously changing body and streaming evidence.

That is the design center of EPR.

---

## 22. Determinism and observability

### 22.1 Deterministic requirements

For identical:

- input event order;
- source revisions;
- configuration;
- character package;
- deterministic seed;
- frame-time trace or fixed simulation steps;

the runtime should produce identical planning choices and equivalent body-control traces within defined floating-point tolerances.

### 22.2 Required trace

Every visible behavior should be reconstructable from:

- upstream evidence and source offsets;
- candidate schemas and scores;
- selected Behavior Unit;
- temporal constraints and dispatch window;
- resource claims, conflicts, and grants;
- generated Realization Program;
- active controller layers;
- physical solve result and relaxation;
- VRM capability projection;
- interruption/failure/fallback path.

### 22.3 Why this matters

Without this trace, tuning becomes folklore. A character that “felt weird once” cannot be repaired if the system cannot say whether the cause was:

- semantic planning;
- timing;
- resource conflict;
- motor generation;
- physical solve;
- package calibration;
- facial override;
- spring-bone interaction.

---

## 23. Licensing and code-reuse matrix

| System / asset family | Observed licence posture | Direct-code recommendation | Design-learning recommendation |
|---|---|---|---|
| Eidolon original work | Public-domain notice, subject to future project policy review | Native implementation | Authoritative project boundary |
| VRM specifications | Open specification repository; models retain individual rights | Implement spec | Strongly adopt contract |
| SmartBody | LGPLv3 | Avoid wholesale dependency unless deliberately accepted | Study controllers/retargeting |
| Greta current | GPLv3 | Do not incorporate into permissive/public-domain-origin runtime | Study planner/style/neural architecture |
| Greta historical branch | LGPLv3 | Still requires deliberate dynamic-link/distribution policy | Study older implementation |
| MOSIM_Core | MIT, archived | Selective code possible after relevance audit | Adopt motion-unit ideas |
| ozz-animation | MIT | Plausible quarantined C++ dependency after measurement | Strong low-level reference |
| Unreal Engine docs/code | Proprietary engine terms | Do not copy engine code | Learn inertialization/layer/FBIK concepts |
| Academic pseudocode | Copyrighted papers; algorithms may be implementable | Reimplement from concepts; inspect patents where relevant | Primary design evidence |
| Motion datasets | Highly variable | No training/use without explicit rights audit | Evaluate separately |
| VRM avatars | Per-model metadata and external licences | Bundle only with explicit redistribution/modification rights | Format openness is not asset openness |

### 23.1 Policy recommendation

Maintain a machine-readable provenance record for every:

- source code dependency;
- authored motion clip;
- VRMA file;
- character model;
- texture/material asset;
- training dataset;
- trained weight file;
- generated derivative.

“Open source” and “free download” are not rights conclusions.

---

## 24. Maintenance and technological-age caveats

### 24.1 Academic systems are design sources, not automatic dependencies

Many canonical realizers were developed around:

- Java runtime stacks;
- XML messaging;
- complete speech schedules;
- centralized scene engines;
- heavyweight agent frameworks;
- desktop assumptions from the 2000s and 2010s.

Their conceptual solutions remain valuable. Their implementation boundaries may be actively harmful to Eidolon.

### 24.2 Modern engines solve lower layers, not EPR

Commercial/game engines now offer excellent:

- animation graphs;
- control rigs;
- full-body IK;
- motion matching;
- inertialization;
- runtime retargeting.

They generally assume that a designer or gameplay system has already chosen the behavior. They do not replace EPR's semantic, temporal, resource, and continuity architecture.

### 24.3 VRM is standardized but deliberately permissive

VRM aims for portable avatar semantics, not bit-identical realization. Implementations and assets may differ visibly. EPR needs validation and package calibration above the format.

---

## 25. Adopt / adapt / reject summary

### 25.1 Adopt directly as concepts

- intent → planner → realizer separation;
- semantic phase points and synchronization;
- incremental mutable plans;
- symbolic anchors;
- current-state motor programs;
- generated connective phases;
- modality-specific realizers;
- explicit realization IR;
- mapped semantic humanoid roles;
- style as bounded realization transform;
- interchangeable motion generators;
- prediction/progress/warning feedback;
- masked/additive/inertialized composition;
- capability-aware local degradation;
- transactional physical solve;
- VRM projection order and rights metadata.

### 25.2 Adapt substantially

- BML blocks → revisioned Behavior Plan Graph;
- PegBoard → typed STN-like temporal dispatcher;
- gesture channels → semantic hierarchical resource claims;
- controller hierarchy → resource-granted Controller Compositor;
- MURML trees → flat typed schema/IR graphs;
- MOSIM MMUs → in-process bounded generators;
- expressive dimensions → schema-bounded quality envelopes;
- neural frame generation → governed modality implementation/residual;
- VRMA → authored generator input, not behavior plan.

### 25.3 Reject

- emotion-to-animation lookup;
- LLM-to-joint generation;
- animation filename as semantic protocol;
- hidden controller-order ownership;
- global snap-to-neutral replacement;
- one universal IK solver;
- unrestricted recursive behavior languages;
- XML in the frame-critical runtime;
- distributed service architecture for local body realization;
- continuous CPU readback or presentation coupling;
- neural output as final semantic or physical authority;
- assuming VRM format openness grants model redistribution rights.

---

## 26. Open research questions retained for implementation experiments

### 26.1 Temporal dispatcher

- Is incremental STN propagation sufficient for all V1 scenarios?
- Which contingent events genuinely require STNU-style dynamic controllability?
- How large can the active plan graph become before compaction is necessary?
- What is the cleanest semantics for semantic-beat repair after partial commitment?

### 26.2 Resource arbiter

- What is the smallest stable semantic resource taxonomy?
- Which claims compose mathematically and which require schema-specific resolvers?
- How are symmetric cooperative behaviors represented without duplicated ownership?
- Should contact graphs be held by the arbiter or a separate relationship owner referenced by claims?

### 26.3 Motor programs

- Which trajectory primitives cover most conversational acting without looking sterile?
- How should motor programs expose feasible timing windows back to the scheduler?
- How much current-state adaptation can remain local before it changes semantic form?

### 26.4 Physical solve

- Is a custom XPBD-like generalized-coordinate solver justified, or can a smaller staged constraint solver achieve the required contact quality?
- How should shoulder recruitment and clavicle limits be calibrated across VRM assets?
- What balance model is sufficient for mostly stationary desktop characters?

### 26.5 Face and gaze

- How should continuous affect map onto highly inconsistent optional VRM expressions?
- How should expression override metadata combine with Eidolon's own modality-resource policy?
- Which blink and gaze statistics look alive without becoming visibly random?

### 26.6 Learned generation

- Which legal motion datasets permit training and redistribution of weights?
- Does a learned residual outperform authored variation after semantic and physical governors are applied?
- How should stochastic generators participate in deterministic replay?

---

## 27. Recommended executable research fixtures

The theory should be tested through adversarial fixtures rather than a single polished demo.

### Fixture 1: streamed contrast

A response begins calm, introduces uncertainty, then sharply corrects itself. Validate stable-prefix planning, provisional future motion, contrast stroke timing, and no replay after extension.

### Fixture 2: successor takeover

A right-hand presenting gesture is followed by another right-hand gesture. Validate preparation shortening and replacement of obsolete retraction.

### Fixture 3: crossed-arm interruption

A soft self-comfort crossed-arm posture is interrupted by approval-required direct attention. Validate contact release, gaze takeover, and no global neutral snap.

### Fixture 4: cooperative head use

Direct gaze, a semantic nod, delivery accent, and posture head attitude overlap. Validate resource composition rather than last-writer wins.

### Fixture 5: impossible contact

A VRM's proportions or sleeves make the desired crossed-arm contact infeasible. Validate progressive relaxation and declared fallback.

### Fixture 6: capability-poor VRM

No eye bones, no custom expressions, no fingers. Validate local degradation to head attention, body affect, and relaxed-hand fallback.

### Fixture 7: concurrent sessions

Two sessions stream simultaneously. Validate upstream shared-character ownership before Behavior Units enter the body plan.

### Fixture 8: late interruption

A semantic stroke is already delivered when the agent is interrupted. Validate preservation of delivered meaning and only necessary cleanup.

### Fixture 9: timing infeasibility

Requested stroke alignment is physically impossible without exceeding speed/acceleration limits. Validate warning, legal relaxation, and updated prediction.

### Fixture 10: deterministic replay

Record all input and timing evidence, replay headlessly, and compare selected plans, grants, trajectories, solve status, and VRM outputs.

---

## 28. Final research position

The Eidolon Performance Runtime should not be framed as an animation system that happens to receive AI output.

It is a **symbolic-to-physical performance machine**:

- symbolic enough to preserve truth, meaning, provenance, and temporal obligations;
- physical enough to understand current body state, contacts, limits, balance, and continuity;
- modular enough to incorporate authored, procedural, and learned motion;
- deterministic enough to debug and trust;
- portable enough to begin with VRM and later support other body packages;
- low-level enough to remain resident without inheriting an academic distributed stack or a commercial game engine.

The best prior systems prove that each individual layer is possible. Eidolon's opportunity is to make their missing composition into one coherent native runtime.

