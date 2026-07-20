# Procedural motion specification

## Intent

Eidolon should not choose animation clips directly from emotion labels. Language and session state
produce slow, inspectable behavior intent; a character-specific controller turns that intent into
pose goals, constrained movement, and secondary physical response.

```text
language/session state
        ↓
behavior intent, 1–5 Hz
        ↓
procedural pose goals
        ↓
IK + joint limits, 60 Hz target
        ↓
secondary physics, 60 Hz target
        ↓
bone matrices → GPU skinning
        ↓
shared GPU texture → transparent SDL composition
```

The current presentation loop is 30 Hz; higher-rate solver/physics work is a design target and may
run independently when it materially improves motion.

## Goals

- natural idling without one endlessly repeated authored clip;
- poses assembled from reusable semantic motifs rather than raw bone names;
- character personality expressed through weights and constraints;
- continuous transitions with bounded velocity and acceleration;
- believable secondary response that settles;
- runtime calibration without recompiling.

## Semantic layers

**Behavior intent** contains affect, engagement, attention, intensity, and movement quality. It does
not address joints.

**Motifs** describe stance, arm posture, spine attitude, gaze, and timing. A motif is not an emotion:
crossed arms may mean confrontation, concentration, or self-comfort; hands behind the back may mean
confidence, curiosity, playfulness, or formality. Context, intent, and character profile select and
blend motifs.

**Pose goals** are target-space positions/orientations in a semantic humanoid profile. Character
aliases map exported bone names to common roles before pose code runs.

**Solvers** apply IK, reach limits, joint limits, and transactional failure. A failed solve leaves
the last valid pose rather than partially corrupting the hierarchy.

**Secondary physics** responds to acceleration and pose change through damped springs with anatomical
limits. Hair, clothing, accessories, and character-specific chest mass must settle; permanent
sine-wave bouncing is not physics.

## Current 3D foundation

- bind hierarchy evaluation and GPU linear-blend skinning;
- semantic humanoid role discovery from Rio/common aliases;
- procedural breathing and slow weight sway;
- normalized shoulder-relative hand and elbow-pole targets;
- analytic two-bone arm IK with reach clamping;
- runtime semantic-pose presets and target/pole calibration;
- strict transactional `config/motion.cfg` reload;
- yaw/pitch/roll inspection controls.

The bind/A calibration pose is the trustworthy baseline. Relaxed/open is directionally correct but
stiff. Guarded still crosses behind the body instead of in front. Attentive and playful remain
uncalibrated guesses.

## Authoring workflow

1. select a semantic pose under Character > 3D Model;
2. adjust normalized hand and elbow-pole targets;
3. copy the complete initializer;
4. review it visually with the user;
5. promote accepted values into `src/pose.c`;
6. add transition dynamics only after endpoints are correct.

Raw `neutral.arm_lower_deg` and `neutral.elbow_add_deg` are bind-axis diagnostics, not a semantic
authoring format.

## Invariants

- pose code uses humanoid roles, not Rio-specific exported names;
- intent never writes bones directly;
- primary pose, speech accents, breathing, and physics have explicit composition order;
- all model-specific constants are calibratable or documented;
- solver failure is transactional;
- render target resolution and motion simulation cadence remain independent;
- authored clips may become optional motifs, but the controller never requires them for idle life.

## Milestones

1. calibrate relaxed, guarded, attentive, and playful arm goals;
2. add velocity/acceleration-continuous pose transitions;
3. add wrist orientation and shoulder/forearm twist limits;
4. add planted-foot stance and lower-body IK;
5. add eye-first/head-follow attention and stochastic blinking;
6. add bounded hair, clothing, accessory, and body secondary springs;
7. connect language/session intent to motif weights.

## Acceptance criteria

- every semantic pose is recognizable from front and side views;
- transitions do not snap, overshoot anatomical limits, or accumulate drift;
- idle motion is non-repeating over short observation without looking restless;
- manual model rotation does not alter the authored pose state;
- secondary motion settles after an impulse;
- a missing or unmapped optional bone degrades locally rather than breaking the model.
