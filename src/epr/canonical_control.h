#ifndef EIDOLON_EPR_CANONICAL_CONTROL_H
#define EIDOLON_EPR_CANONICAL_CONTROL_H

#include "epr/performance_intent.h"

#include <stdbool.h>
#include <stdint.h>

#define EIDOLON_EPR_BODY_PROFILE_VERSION 1U
#define EIDOLON_EPR_CONTROL_VERSION 1U

typedef struct EidolonEprBodyProfile {
    uint32_t version;
    uint64_t fingerprint;
    float shoulder[3];
    float head[3];
    float right[3];
    float up[3];
    float forward[3];
    float right_upper_arm_length;
    float right_lower_arm_length;
    float maximum_reach_ratio;
    float shoulder_limit_radians;
    float elbow_limit_radians;
    bool has_required_humanoid;
    bool has_right_arm;
    bool has_eyes;
    bool has_expression;
} EidolonEprBodyProfile;

typedef struct EidolonCanonicalControl {
    uint32_t version;
    uint64_t revision;
    uint64_t plan_generation;
    EidolonEprTick tick;
    float torso_pitch;
    float torso_yaw;
    float torso_roll;
    float head_pitch;
    float head_yaw;
    float head_roll;
    float gaze_target[3];
    float eye_yaw;
    float eye_pitch;
    float eye_weight;
    float head_gaze_weight;
    float right_hand_target[3];
    float right_elbow_pole[3];
    float right_elbow_position[3];
    float right_hand_position[3];
    float right_wrist_euler[3];
    float right_arm_velocity[3];
    float focused_expression_weight;
    uint64_t hash;
    bool valid;
    bool eyes_degraded;
    bool expression_degraded;
} EidolonCanonicalControl;

uint64_t eidolon_epr_control_hash(EidolonCanonicalControl *control);

#endif
