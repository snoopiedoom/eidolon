#include "epr/realization_program.h"

#include <string.h>

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

static bool configure_program(const EidolonBehaviorPlan *plan,
                              const EidolonEprBehaviorUnit *behavior,
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
        program->modality = EIDOLON_EPR_MODALITY_POSTURE;
        program->targets[0][0] = 0.40F;
        program->targets[0][1] = -0.70F;
        program->targets[0][2] = 0.08F;
        program->values[0] = 0.035F;
        program->values[1] = -0.025F;
        program->values[2] = 240.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING:
        program->modality = EIDOLON_EPR_MODALITY_POSTURE;
        program->targets[0][0] = 0.25F;
        program->targets[0][1] = -0.55F;
        program->targets[0][2] = 0.30F;
        program->values[0] = 0.075F;
        program->values[1] = 0.11F;
        program->values[2] = 240.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING:
        program->modality = EIDOLON_EPR_MODALITY_POSTURE;
        program->targets[0][0] = 0.50F;
        program->targets[0][1] = -0.65F;
        program->targets[0][2] = 0.12F;
        program->values[0] = -0.025F;
        program->values[1] = -0.015F;
        program->values[2] = 240.0F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED:
        program->modality = EIDOLON_EPR_MODALITY_POSTURE;
        program->targets[0][0] = 0.18F;
        program->targets[0][1] = -0.45F;
        program->targets[0][2] = 0.32F;
        program->values[0] = 0.055F;
        program->values[1] = 0.015F;
        program->values[2] = 240.0F;
        return true;
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
        program->modality = EIDOLON_EPR_MODALITY_GESTURE;
        program->targets[0][0] = 0.35F;
        program->targets[0][1] = -0.42F;
        program->targets[0][2] = 0.35F;
        program->targets[1][0] = 0.70F;
        program->targets[1][1] = -0.20F;
        program->targets[1][2] = 0.45F;
        program->targets[2][0] = 0.40F;
        program->targets[2][1] = -0.42F;
        program->targets[2][2] = 0.28F;
        program->values[0] = -0.12F;
        program->values[1] = 0.18F;
        return true;
    case EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM:
        program->modality = EIDOLON_EPR_MODALITY_SETTLE;
        return true;
    }
    return false;
}

bool eidolon_epr_program_set_compile(const EidolonBehaviorPlan *plan,
                                     EidolonRealizationProgramSet *programs) {
    EidolonRealizationProgramSet candidate;
    if (plan == NULL || programs == NULL || plan->behavior_count > EIDOLON_EPR_PROGRAM_CAPACITY) {
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
            !configure_program(plan, behavior, &candidate.programs[candidate.count])) {
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
