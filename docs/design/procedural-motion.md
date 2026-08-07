# Procedural motion specification

This specification owns the rigged-3D motion system. It does not own portrait face selection or
whole-image 2D motion. The systems may consume common body-neutral evidence and must coexist behind
the same selection, scene, dialogue, and presentation boundaries without sharing renderer-local
state.

## Intent

Eidolon should not choose animation clips directly from emotion labels. Language and session state
produce slow, inspectable behavior intent; a character-specific controller turns that intent into
pose goals, constrained movement, and secondary physical response.

```text
accepted Performance Intent
        ↓
versioned behavior plan + temporal dispatch
        ↓
explicit body-resource grants + modality Realization Programs
        ↓
canonical composition + transactional IK/joint limits, fixed control ticks
        ↓
experimental VRM/rigged-body capability projection
        ↓
bone matrices → GPU skinning
        ↓
shared GPU texture → transparent SDL composition
```

The presentation loop uses measured frame time and follows the configured VSync/FPS policy. Future
solver or secondary-physics stages may use a fixed internal timestep when their stability requires
one, without imposing that rate on presentation.

The VRM leg of this flow currently means the supported reference-avatar experiment described by the
[VRM reference-body contract](vrm-body-runtime.md), not general VRM 1.0 compatibility.

## Goals

- natural idling without one endlessly repeated authored clip;
- poses assembled from reusable semantic motifs rather than raw bone names;
- body-relative generation from measured segment lengths, proportions, and anatomical frames;
- a small user-approved semantic calibration vocabulary instead of model-specific guessed poses;
- character personality expressed through weights and constraints;
- continuous transitions with bounded velocity and acceleration;
- believable secondary response that settles;
- runtime calibration without recompiling.

## Semantic layers

**Behavior intent** contains affect, engagement, attention, intensity, and movement quality. It does
not address joints.

The [EPR contracts](epr-overview.md) now own semantic behavior selection, timing, interruption, and
resource conflicts. Modality realizers retain bounded motor integration and produce model-neutral
programs; renderers consume complete projected control rather than selecting motifs themselves.

**Motifs** describe stance, arm posture, spine attitude, gaze, and timing. A motif is not an emotion:
crossed arms may mean confrontation, concentration, or self-comfort; hands behind the back may mean
confidence, curiosity, playfulness, or formality. Context, intent, and character profile select and
blend motifs.

**Pose goals** are target-space positions/orientations in a semantic humanoid profile. VRM humanoid
metadata maps nodes to common roles before pose code runs. Goals use measured body axes and segment
lengths, so they are expressed in character-relative units rather than fixed metres or raw local
Euler angles.

**Calibration anchors** are user-approved examples of semantic endpoints such as neutral,
attentive, thinking, responding, contrast, and interrupted/guarded. They belong to a specific
measured anatomy fingerprint. Each anchor carries its resource mask, normalized task-space goals,
and optional model-local quaternion residuals. An anchor is evidence about how that character
should perform; it is not an EPR event, animation timeline, or global pose reset.

**Solvers** apply IK, reach limits, joint limits, and transactional failure. A failed solve leaves
the last valid pose rather than partially corrupting the hierarchy.

**Secondary physics** responds to acceleration and pose change through damped springs with anatomical
limits. Hair, clothing, accessories, and character-specific chest mass must settle; permanent
sine-wave bouncing is not physics.

## Current 3D foundation

- bind hierarchy evaluation and GPU linear-blend skinning;
- authoritative VRM 1.0 humanoid role mapping on the experimental reference-body path;
- measured bind positions and nearest-semantic-parent length for every mapped humanoid bone;
- measured skeleton height, shoulder width, torso length, bilateral arm/leg segments, and body axes;
- a stable anatomy fingerprint derived from quantized semantic bind measurements;
- a versioned, partial calibration-anchor profile with transactional parsing and validation;
- an in-runtime named-anchor session that deterministically rebuilds the real fixture control;
- a live task-space editor for torso, head, right hand, elbow pole, wrist, and arm weight;
- deterministic serialization and same-directory atomic sidecar replacement;
- automatic optional sidecar loading from `<model>.epr-calibration`, with
  `EIDOLON_VRM_CALIBRATION_PATH` as an explicit override;
- procedural breathing and slow weight sway;
- normalized shoulder-relative hand and elbow-pole targets;
- analytic two-bone arm IK with reach clamping;
- bind-space correction and complete scratch-pose commits for the EPR reference projection;
- strict transactional `config/motion.cfg` reload;
- yaw/pitch/roll inspection controls.

The old hard-coded EPR posture/gesture endpoints remain only in the explicit deterministic test
fixture. Ordinary VRM playback cannot select them. The measurement/profile, interactive authoring,
calibrated-program compilation, and ordinary-playback slices are implemented. `make vrm-calibrate
VRM_PATH=...` freezes the actual fixture at each named tick and projects every draft edit through
the normal scratch transaction. A matching `neutral` right-arm anchor enables calibrated playback;
approved state and gesture anchors then drive the corresponding programs. Missing anchors degrade
only their resource/behavior family and emit typed trace evidence.

## Calibration model

Calibration has three distinct stages:

1. **automatic structural measurement:** resolve humanoid roles, bind positions, authored axes,
   segment lengths, proportions, and capability presence;
2. **interactive semantic calibration:** pause the actual EPR scenario at a named anchor and let the
   user approve or adjust task-space handles and, only where needed, model-local correction deltas;
3. **runtime derivation:** use the measured profile and approved anchors to generate intensity,
   timing, transitions, interruption, settling, asymmetry, and compatible combinations.

Bone lengths make motion scale correctly, but they do not define acting. User-approved anchors
supply the aesthetic evidence that distinguishes an intentional pose from a merely reachable one.
Mesh/skinning-derived clearance volumes may later constrain hands against the torso, hair, and
clothing; those volumes complement rather than replace skeletal measurement.

The bounded first anchor vocabulary is:

- `neutral`;
- `attentive`;
- `thinking`;
- `responding`;
- `contrast_preparation`;
- `contrast_peak`;
- `contrast_recovery`;
- `interrupted_guarded`.

Calibration may be saved after any complete anchor. Missing anchors degrade locally and never fall
back to invented model-specific poses. Neutral plus a requested semantic anchor is sufficient for
the runtime to derive intermediate strength and transitions within that gesture family. A genuinely
new gesture family still requires another approved anchor or an optional authored generator.

### First sidecar format

The implemented first slice uses a strict line-oriented format so parsing stays bounded and
transactional. Angles are radians; hand and pole coordinates are shoulder-relative anatomical
`outward, up, forward` values measured in whole-arm units. Every declared anchor supplies all of
its fields; per-bone residual quaternions are optional.

```ini
version = 1
anatomy_fingerprint = 0x0123456789abcdef

anchor.neutral.resources = torso,head,left_arm,right_arm
anchor.neutral.torso = 0, 0, 0
anchor.neutral.head = 0, 0, 0
anchor.neutral.left.hand = 0.05, -0.80, 0.10
anchor.neutral.left.pole = 0.30, -0.35, -0.20
anchor.neutral.left.wrist = 0, 0, 0
anchor.neutral.left.weight = 1
anchor.neutral.right.hand = 0.05, -0.80, 0.10
anchor.neutral.right.pole = 0.30, -0.35, -0.20
anchor.neutral.right.wrist = 0, 0, 0
anchor.neutral.right.weight = 1
anchor.neutral.residual.rightHand = 0, 0, 0, 1
```

The numbers above demonstrate syntax only; they are not an approved DECAGRAMMATON pose.

## Authoring workflow

1. load the user's VRM and measure its semantic skeleton;
2. load a matching partial sidecar, or begin an empty calibration session;
3. run the real EPR scenario and freeze it at one named semantic anchor;
4. let the user adjust normalized right-hand/elbow/head/torso/wrist handles (bounded residual and
   left-arm editors are later slices);
5. validate and atomically save that anchor without recompiling Eidolon;
6. replay the complete behavior family and revise only the rejected anchor;
7. derive transitions and intensities from approved endpoints;
8. add a new anchor or optional authored motion only when the existing vocabulary cannot express a
   genuinely new form.

Raw `neutral.arm_lower_deg` and `neutral.elbow_add_deg` are bind-axis diagnostics, not a semantic
authoring format.

## Invariants

- pose code uses humanoid roles, not Rio-specific exported names;
- intent never writes bones directly;
- no model-specific semantic pose is accepted without an explicit calibration anchor;
- sidecars bind to measured anatomy, load transactionally, and may remain partial;
- uncalibrated behavior degrades locally rather than selecting guessed constants;
- 3D motion code never writes portrait expression or whole-image motion state;
- primary pose, speech accents, breathing, and physics have explicit composition order;
- all model-specific constants are calibratable or documented;
- solver failure is transactional;
- render target resolution and motion simulation cadence remain independent;
- authored clips may become optional motifs, but the controller never requires them for idle life.

## Milestones

1. extract and validate the semantic skeleton measurements and versioned partial sidecar — done;
2. add calibration-session state and anchor capture to the running EPR review harness — done;
3. add first-slice task-space editing controls and transactional sidecar save — done;
4. compile posture/gesture Realization Programs from approved anchors with minimum-jerk transition
   curves, exact resource-local fallback traces, and non-accumulating transactional residuals —
   done;
5. calibrate the selected local reference body and repeat the complete owner performance review;
6. derive mesh/skinning clearance volumes, joint comfort ranges, wrist orientation, and arm twist;
7. add planted-foot stance and lower-body IK;
8. add eye-first/head-follow attention, blinking, and bounded secondary physics;
9. retain VRMA as an optional generator for motion families that calibrated anchors cannot derive.

## Acceptance criteria

- every semantic pose is recognizable from front and side views;
- importing a proportionally different valid body rescales semantic targets from its own measured
  skeleton rather than inheriting DECAGRAMMATON distances;
- a stale sidecar is rejected when semantic bind anatomy changes and remains valid across
  texture-only changes;
- neutral plus one calibrated semantic anchor can generate weaker/stronger and interrupted
  transitions without another hand-authored frame sequence;
- transitions do not snap, overshoot anatomical limits, or accumulate drift;
- failed projection does not change the live rig or residual ownership, and removing a residual
  weight/calibration restores its target from the captured base pose;
- idle motion is non-repeating over short observation without looking restless;
- manual model rotation does not alter the authored pose state;
- secondary motion settles after an impulse;
- a missing or unmapped optional bone degrades locally rather than breaking the model.

Ordinary playback deliberately accepts partial progress under those degradation rules. Acceptance
is stricter: both the hidden runtime gate and visible performance review require all eight anchors
with the resources owned by their behavior family. The hidden gate additionally rejects dropped
trace records, realizer fallback/failure, solve/projection rejection, an incomplete five-second
clock, or a final control revision that was not projected.
