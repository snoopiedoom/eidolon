#ifndef EIDOLON_EPR_TASK_TARGET_H
#define EIDOLON_EPR_TASK_TARGET_H

#include "epr/performance_intent.h"

#include <stdbool.h>
#include <stdint.h>

#define EIDOLON_EPR_RIGHT_ARM_TASK_TARGET_VERSION 1U
#define EIDOLON_EPR_RIGHT_ARM_TASK_TARGET_MAX_LIFETIME_MS 1000

typedef struct EidolonEprRightArmTaskTarget {
    uint32_t version;
    uint64_t revision;
    uint64_t predecessor_revision;
    uint64_t plan_generation;
    EidolonEprOpaqueId producer;
    EidolonEprOpaqueId target;
    EidolonEprOpaqueId behavior;
    EidolonEprTick sample_tick;
    EidolonEprTick valid_until_tick;
    /* Shoulder-relative anatomical coordinates in whole-arm units. */
    float hand_target[3];
    float elbow_pole[3];
    /* Anatomical pitch, yaw, and roll in radians. */
    float wrist_euler[3];
    float weight;
} EidolonEprRightArmTaskTarget;

bool eidolon_epr_right_arm_task_target_validate(const EidolonEprRightArmTaskTarget *target);

#endif
