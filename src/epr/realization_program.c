#include "epr/realization_program.h"

#include <math.h>
#include <string.h>

#define EIDOLON_EPR_REALIZATION_TARGET_LIMIT 2.0F
#define EIDOLON_EPR_REALIZATION_ANGLE_LIMIT 3.141593F

static uint32_t anchor_bit(EidolonEprPoseAnchorId anchor) {
    return UINT32_C(1) << (uint32_t)anchor;
}

static bool finite3(const float values[3]) {
    return isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]);
}

static bool bounded3(const float values[3], float limit) {
    return finite3(values) && fabsf(values[0]) <= limit && fabsf(values[1]) <= limit &&
           fabsf(values[2]) <= limit;
}

static bool anchor_valid(const EidolonEprPoseAnchor *anchor) {
    const uint32_t valid_resources = (UINT32_C(1) << EIDOLON_EPR_RESOURCE_COUNT) - 1U;
    return anchor != NULL && anchor->resource_mask != 0U &&
           (anchor->resource_mask & ~valid_resources) == 0U &&
           bounded3(anchor->torso_euler, EIDOLON_EPR_REALIZATION_ANGLE_LIMIT) &&
           bounded3(anchor->head_euler, EIDOLON_EPR_REALIZATION_ANGLE_LIMIT) &&
           bounded3(anchor->right_arm.hand_target, EIDOLON_EPR_REALIZATION_TARGET_LIMIT) &&
           bounded3(anchor->right_arm.elbow_pole, EIDOLON_EPR_REALIZATION_TARGET_LIMIT) &&
           bounded3(anchor->right_arm.wrist_euler, EIDOLON_EPR_REALIZATION_ANGLE_LIMIT) &&
           isfinite(anchor->right_arm.weight) && anchor->right_arm.weight >= 0.0F &&
           anchor->right_arm.weight <= 1.0F;
}

bool eidolon_epr_realization_profile_validate(const EidolonEprRealizationProfile *profile,
                                              const EidolonEprBodyProfile *body) {
    const uint32_t valid_anchors =
        (UINT32_C(1) << (uint32_t)EIDOLON_EPR_POSE_ANCHOR_COUNT) - 1U;
    if (profile == NULL || body == NULL ||
        profile->version != EIDOLON_EPR_REALIZATION_PROFILE_VERSION ||
        profile->body_fingerprint == 0U || profile->body_fingerprint != body->fingerprint ||
        (profile->anchor_mask & ~valid_anchors) != 0U ||
        (profile->anchor_mask & anchor_bit(EIDOLON_EPR_POSE_NEUTRAL)) == 0U) {
        return false;
    }
    for (size_t index = 0U; index < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++index) {
        if ((profile->anchor_mask & (UINT32_C(1) << (uint32_t)index)) != 0U &&
            !anchor_valid(&profile->anchors[index])) {
            return false;
        }
    }
    return (profile->anchors[EIDOLON_EPR_POSE_NEUTRAL].resource_mask &
            (UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN)) != 0U;
}

const EidolonEprPoseAnchor *
eidolon_epr_realization_anchor(const EidolonEprRealizationProfile *profile,
                               EidolonEprPoseAnchorId anchor) {
    if (profile == NULL || anchor < EIDOLON_EPR_POSE_NEUTRAL ||
        anchor >= EIDOLON_EPR_POSE_ANCHOR_COUNT ||
        (profile->anchor_mask & anchor_bit(anchor)) == 0U) {
        return NULL;
    }
    return &profile->anchors[(size_t)anchor];
}

static uint32_t resource_mask(const EidolonBehaviorPlan *plan, EidolonEprOpaqueId behavior) {
    uint32_t mask = 0U;
    for (size_t index = 0; index < plan->claim_count; ++index) {
        const EidolonEprResourceClaim *claim = &plan->claims[index];
        if (claim->behavior == behavior) {
            mask |= UINT32_C(1) << (uint32_t)claim->resource;
        }
    }
    return mask;
}

static bool configure_posture(const EidolonEprRealizationProfile *profile,
                              EidolonEprPoseAnchorId target,
                              EidolonRealizationProgram *program) {
    const EidolonEprPoseAnchor *neutral =
        eidolon_epr_realization_anchor(profile, EIDOLON_EPR_POSE_NEUTRAL);
    const EidolonEprPoseAnchor *selected = eidolon_epr_realization_anchor(profile, target);
    if (neutral == NULL) {
        return false;
    }
    program->modality = EIDOLON_EPR_MODALITY_POSTURE;
    program->poses[0] = *neutral;
    program->poses[1] = selected != NULL ? *selected : *neutral;
    program->pose_ids[0] = EIDOLON_EPR_POSE_NEUTRAL;
    program->pose_ids[1] = selected != NULL ? target : EIDOLON_EPR_POSE_NEUTRAL;
    program->pose_count = 2U;
    program->values[0] = 240.0F;
    if (selected == NULL) {
        program->missing_anchor_mask = anchor_bit(target);
    }
    return true;
}

static bool configure_gesture(const EidolonEprRealizationProfile *profile,
                              EidolonRealizationProgram *program) {
    static const EidolonEprPoseAnchorId anchors[EIDOLON_EPR_PROGRAM_POSE_CAPACITY] = {
        EIDOLON_EPR_POSE_CONTRAST_PREPARATION,
        EIDOLON_EPR_POSE_CONTRAST_PEAK,
        EIDOLON_EPR_POSE_CONTRAST_RECOVERY,
    };
    program->modality = EIDOLON_EPR_MODALITY_GESTURE;
    for (size_t index = 0U; index < EIDOLON_EPR_PROGRAM_POSE_CAPACITY; ++index) {
        const EidolonEprPoseAnchor *anchor = eidolon_epr_realization_anchor(profile, anchors[index]);
        const uint32_t right_arm = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
        if (anchor == NULL || (anchor->resource_mask & right_arm) == 0U) {
            program->missing_anchor_mask |= anchor_bit(anchors[index]);
            continue;
        }
        program->poses[index] = *anchor;
        program->pose_ids[index] = anchors[index];
    }
    if (program->missing_anchor_mask == 0U) {
        program->pose_count = EIDOLON_EPR_PROGRAM_POSE_CAPACITY;
    }
    return true;
}

static bool configure_program(const EidolonBehaviorPlan *plan,
                              const EidolonEprBehaviorUnit *behavior,
                              const EidolonEprRealizationProfile *profile,
                              EidolonRealizationProgram *program) {
    memset(program, 0, sizeof(*program));
    program->version = EIDOLON_EPR_PROGRAM_VERSION;
    program->id = behavior->id ^ UINT64_C(0x525049522d763100);
    if (program->id == 0U) {
        program->id = 1U;
    }
    program->behavior = behavior->id;
    program->cause = behavior->cause;
    program->plan_generation = plan->generation;
    program->behavior_kind = behavior->kind;
    program->resource_mask = resource_mask(plan, behavior->id);
    memcpy(program->phase_ticks, behavior->phase_ticks, sizeof(program->phase_ticks));
    memcpy(program->has_phase, behavior->has_phase, sizeof(program->has_phase));

    switch (behavior->kind) {
    case EIDOLON_EPR_BEHAVIOR_IDLE:
        program->modality = EIDOLON_EPR_MODALITY_IDLE;
        program->values[0] = 0.008F;
        program->values[1] = 1.17F;
        program->values[2] = 0.006F;
        program->values[3] = 0.43F;
        program->values[4] = 0.45F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE:
        return configure_posture(profile, EIDOLON_EPR_POSE_ATTENTIVE, program);
    case EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING:
        return configure_posture(profile, EIDOLON_EPR_POSE_THINKING, program);
    case EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING:
        return configure_posture(profile, EIDOLON_EPR_POSE_RESPONDING, program);
    case EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED:
        return configure_posture(profile, EIDOLON_EPR_POSE_INTERRUPTED_GUARDED, program);
    case EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION:
        program->modality = EIDOLON_EPR_MODALITY_GAZE;
        program->capability_mask = EIDOLON_EPR_CAPABILITY_EYES;
        program->targets[0][0] = -0.22F;
        program->targets[0][1] = 1.55F;
        program->targets[0][2] = 1.0F;
        program->values[0] = -0.18F;
        program->values[1] = -0.03F;
        program->values[2] = 100.0F;
        program->values[3] = 80.0F;
        program->values[4] = 240.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE:
        program->modality = EIDOLON_EPR_MODALITY_GAZE;
        program->capability_mask = EIDOLON_EPR_CAPABILITY_EYES;
        program->targets[0][0] = 0.20F;
        program->targets[0][1] = 1.65F;
        program->targets[0][2] = 1.0F;
        program->values[0] = 0.15F;
        program->values[1] = -0.05F;
        program->values[2] = 100.0F;
        program->values[3] = 80.0F;
        program->values[4] = 240.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED:
        program->modality = EIDOLON_EPR_MODALITY_GAZE;
        program->capability_mask = EIDOLON_EPR_CAPABILITY_EYES;
        program->targets[0][0] = -0.36F;
        program->targets[0][1] = 1.60F;
        program->targets[0][2] = 1.0F;
        program->values[0] = -0.28F;
        program->values[1] = -0.02F;
        program->values[2] = 100.0F;
        program->values[3] = 80.0F;
        program->values[4] = 240.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL:
        program->modality = EIDOLON_EPR_MODALITY_EXPRESSION;
        program->capability_mask = EIDOLON_EPR_CAPABILITY_EXPRESSION;
        program->values[0] = 0.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_EXPRESSION_FOCUSED:
        program->modality = EIDOLON_EPR_MODALITY_EXPRESSION;
        program->capability_mask = EIDOLON_EPR_CAPABILITY_EXPRESSION;
        program->values[0] = 0.45F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT:
        return configure_gesture(profile, program);
    case EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM:
        program->modality = EIDOLON_EPR_MODALITY_SETTLE;
        return true;
    }
    return false;
}

bool eidolon_epr_program_set_compile(const EidolonBehaviorPlan *plan,
                                     const EidolonEprRealizationProfile *profile,
                                     EidolonRealizationProgramSet *programs) {
    EidolonRealizationProgramSet candidate;
    if (plan == NULL || profile == NULL || programs == NULL ||
        plan->behavior_count > EIDOLON_EPR_PROGRAM_CAPACITY) {
        return false;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.plan_generation = plan->generation;
    for (size_t index = 0; index < plan->behavior_count; ++index) {
        const EidolonEprBehaviorUnit *behavior = &plan->behaviors[index];
        if (behavior->retired) {
            continue;
        }
        if (candidate.count >= EIDOLON_EPR_PROGRAM_CAPACITY ||
            !configure_program(plan, behavior, profile, &candidate.programs[candidate.count])) {
            return false;
        }
        candidate.count += 1U;
    }
    *programs = candidate;
    return true;
}

const EidolonRealizationProgram *
eidolon_epr_program_find(const EidolonRealizationProgramSet *programs,
                         EidolonEprOpaqueId behavior) {
    if (programs == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < programs->count; ++index) {
        if (programs->programs[index].behavior == behavior) {
            return &programs->programs[index];
        }
    }
    return NULL;
}
