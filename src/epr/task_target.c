#include "epr/task_target.h"

#include <limits.h>
#include <math.h>

#define EIDOLON_EPR_TASK_TARGET_POSITION_LIMIT 2.0F
#define EIDOLON_EPR_TASK_TARGET_ANGLE_LIMIT 3.141593F

static bool bounded3(const float values[3], float limit) {
    return isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]) &&
           fabsf(values[0]) <= limit && fabsf(values[1]) <= limit && fabsf(values[2]) <= limit;
}

bool eidolon_epr_right_arm_task_target_validate(const EidolonEprRightArmTaskTarget *target) {
    return target != NULL && target->version == EIDOLON_EPR_RIGHT_ARM_TASK_TARGET_VERSION &&
           target->revision != 0U && target->plan_generation != 0U && target->producer != 0U &&
           target->target != 0U && target->behavior != 0U &&
           target->sample_tick <= target->valid_until_tick &&
           target->sample_tick <= INT64_MAX - EIDOLON_EPR_RIGHT_ARM_TASK_TARGET_MAX_LIFETIME_MS &&
           target->valid_until_tick <=
               target->sample_tick + EIDOLON_EPR_RIGHT_ARM_TASK_TARGET_MAX_LIFETIME_MS &&
           bounded3(target->hand_target, EIDOLON_EPR_TASK_TARGET_POSITION_LIMIT) &&
           bounded3(target->elbow_pole, EIDOLON_EPR_TASK_TARGET_POSITION_LIMIT) &&
           bounded3(target->wrist_euler, EIDOLON_EPR_TASK_TARGET_ANGLE_LIMIT) &&
           isfinite(target->weight) && target->weight >= 0.0F && target->weight <= 1.0F;
}
