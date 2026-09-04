#ifndef EIDOLON_EPR_REALIZATION_PROGRAM_H
#define EIDOLON_EPR_REALIZATION_PROGRAM_H

#include "epr/behavior_plan.h"
#include "epr/canonical_control.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_PROGRAM_VERSION 4U
#define EIDOLON_EPR_MOTION_GENERATOR_REFERENCE_VERSION 1U
#define EIDOLON_EPR_PROGRAM_CAPACITY EIDOLON_EPR_BEHAVIOR_CAPACITY
#define EIDOLON_EPR_REALIZATION_PROFILE_VERSION 1U
#define EIDOLON_EPR_PROGRAM_POSE_CAPACITY 3U

typedef struct EidolonEprArmAnchor {
    /* Shoulder-relative anatomical coordinates in whole-arm units. */
    float hand_target[3];
    float elbow_pole[3];
    float wrist_euler[3];
    float weight;
} EidolonEprArmAnchor;

typedef struct EidolonEprPoseAnchor {
    uint32_t resource_mask;
    float torso_euler[3];
    float head_euler[3];
    EidolonEprArmAnchor right_arm;
} EidolonEprPoseAnchor;

typedef struct EidolonEprRealizationProfile {
    uint32_t version;
    uint64_t body_fingerprint;
    uint32_t anchor_mask;
    EidolonEprPoseAnchor anchors[EIDOLON_EPR_POSE_ANCHOR_COUNT];
} EidolonEprRealizationProfile;

typedef enum EidolonEprModality {
    EIDOLON_EPR_MODALITY_IDLE = 0,
    EIDOLON_EPR_MODALITY_POSTURE,
    EIDOLON_EPR_MODALITY_GAZE,
    EIDOLON_EPR_MODALITY_GESTURE,
    EIDOLON_EPR_MODALITY_SETTLE,
    EIDOLON_EPR_MODALITY_EXPRESSION
} EidolonEprModality;

typedef enum EidolonEprCapabilityRequirement {
    EIDOLON_EPR_CAPABILITY_NONE = 0,
    EIDOLON_EPR_CAPABILITY_EYES = 1U << 0U,
    EIDOLON_EPR_CAPABILITY_EXPRESSION = 1U << 1U
} EidolonEprCapabilityRequirement;

typedef enum EidolonEprMotionGeneratorId {
    EIDOLON_EPR_MOTION_GENERATOR_NONE = 0,
    EIDOLON_EPR_MOTION_IDLE_NEUTRAL,
    EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE,
    EIDOLON_EPR_MOTION_POSTURE_THINKING,
    EIDOLON_EPR_MOTION_POSTURE_RESPONDING,
    EIDOLON_EPR_MOTION_POSTURE_INTERRUPTED_GUARDED,
    EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT,
    EIDOLON_EPR_MOTION_SETTLE_RIGHT_ARM,
    EIDOLON_EPR_MOTION_GENERATOR_COUNT
} EidolonEprMotionGeneratorId;

typedef enum EidolonEprMotionTakeoverPolicy {
    EIDOLON_EPR_MOTION_TAKEOVER_NONE = 0,
    EIDOLON_EPR_MOTION_TAKEOVER_BASE,
    EIDOLON_EPR_MOTION_TAKEOVER_ADDITIVE,
    EIDOLON_EPR_MOTION_TAKEOVER_COOPERATIVE,
    EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE,
    EIDOLON_EPR_MOTION_TAKEOVER_COUNT
} EidolonEprMotionTakeoverPolicy;

typedef struct EidolonEprMotionGeneratorReference {
    uint32_t version;
    EidolonEprMotionGeneratorId generator;
    EidolonEprMotionTakeoverPolicy takeover;
    uint32_t resource_mask;
    uint64_t humanoid_rotation_mask;
    float blend_weight;
    float intensity;
    float playback_rate;
    bool owns_hips_translation;
} EidolonEprMotionGeneratorReference;

typedef struct EidolonRealizationProgram {
    uint32_t version;
    EidolonEprOpaqueId id;
    EidolonEprOpaqueId behavior;
    EidolonEprOpaqueId cause;
    uint64_t plan_generation;
    EidolonEprBehaviorKind behavior_kind;
    EidolonEprModality modality;
    uint32_t resource_mask;
    uint32_t capability_mask;
    EidolonEprTick phase_ticks[EIDOLON_EPR_BEHAVIOR_PHASE_COUNT];
    bool has_phase[EIDOLON_EPR_BEHAVIOR_PHASE_COUNT];
    EidolonEprMotionGeneratorReference motion;
    EidolonEprPoseAnchor poses[EIDOLON_EPR_PROGRAM_POSE_CAPACITY];
    EidolonEprPoseAnchorId pose_ids[EIDOLON_EPR_PROGRAM_POSE_CAPACITY];
    size_t pose_count;
    /* An anchor bit is set when the anchor is absent or incomplete for this behavior. */
    uint32_t missing_anchor_mask;
    /* Union of the claimed resources that fall back because of those anchor gaps. */
    uint32_t missing_resource_mask;
    float targets[3][3];
    float values[8];
} EidolonRealizationProgram;

typedef struct EidolonRealizationProgramSet {
    uint64_t plan_generation;
    EidolonRealizationProgram programs[EIDOLON_EPR_PROGRAM_CAPACITY];
    size_t count;
} EidolonRealizationProgramSet;

const char *eidolon_epr_motion_generator_name(EidolonEprMotionGeneratorId generator);
bool eidolon_epr_motion_generator_reference_validate(
    const EidolonEprMotionGeneratorReference *reference);
bool eidolon_epr_realization_profile_validate(const EidolonEprRealizationProfile *profile,
                                              const EidolonEprBodyProfile *body);
const EidolonEprPoseAnchor *
eidolon_epr_realization_anchor(const EidolonEprRealizationProfile *profile,
                               EidolonEprPoseAnchorId anchor);
bool eidolon_epr_program_set_compile(const EidolonBehaviorPlan *plan,
                                     const EidolonEprRealizationProfile *profile,
                                     EidolonRealizationProgramSet *programs);
const EidolonRealizationProgram *
eidolon_epr_program_find(const EidolonRealizationProgramSet *programs, EidolonEprOpaqueId behavior);

#endif
