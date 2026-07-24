#ifndef EIDOLON_EPR_PERFORMANCE_RUNTIME_H
#define EIDOLON_EPR_PERFORMANCE_RUNTIME_H

#include "epr/behavior_plan.h"
#include "epr/body_resources.h"
#include "epr/performance_intent.h"
#include "epr/performance_trace.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_BODY_PROFILE_VERSION 1U
#define EIDOLON_EPR_CONTROL_VERSION 1U

typedef struct EidolonEprBodyProfile {
    uint32_t version;
    uint64_t fingerprint;
    float shoulder[3];
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

typedef struct EidolonEprBehaviorRuntimeState {
    EidolonEprOpaqueId behavior;
    EidolonEprBehaviorState state;
    int highest_observed_phase;
    EidolonEprTerminalReason terminal_reason;
} EidolonEprBehaviorRuntimeState;

typedef struct EidolonPerformanceRuntime {
    uint64_t seed;
    EidolonPerformanceIntent intent;
    bool has_intent;
    EidolonBehaviorPlan plan;
    bool has_plan;
    EidolonEprBodyProfile body;
    EidolonCanonicalControl control;
    EidolonCanonicalControl settle_start;
    EidolonEprBehaviorRuntimeState behavior_states[EIDOLON_EPR_BEHAVIOR_CAPACITY];
    size_t behavior_state_count;
    EidolonEprResourceResolution resources;
    bool has_resources;
    EidolonEprTrace trace;
    EidolonEprTick last_tick;
    bool has_tick;
    bool eyes_degradation_traced;
    bool expression_degradation_traced;
    bool inject_next_solve_failure;
} EidolonPerformanceRuntime;

EidolonEprBodyProfile eidolon_epr_default_body_profile(void);
bool eidolon_epr_runtime_init(EidolonPerformanceRuntime *runtime, uint64_t seed,
                              const EidolonEprBodyProfile *body);
bool eidolon_epr_runtime_accept(EidolonPerformanceRuntime *runtime,
                                const EidolonPerformanceIntent *intent);
bool eidolon_epr_runtime_step(EidolonPerformanceRuntime *runtime, EidolonEprTick tick);
void eidolon_epr_runtime_inject_solve_failure(EidolonPerformanceRuntime *runtime);
const EidolonCanonicalControl *
eidolon_epr_runtime_control(const EidolonPerformanceRuntime *runtime);
const EidolonBehaviorPlan *eidolon_epr_runtime_plan(const EidolonPerformanceRuntime *runtime);
const EidolonEprTrace *eidolon_epr_runtime_trace(const EidolonPerformanceRuntime *runtime);
uint64_t eidolon_epr_control_hash(EidolonCanonicalControl *control);

#endif
