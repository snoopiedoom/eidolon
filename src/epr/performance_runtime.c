#include "epr/performance_runtime.h"

#include "ik.h"

#include <math.h>
#include <string.h>

#define EIDOLON_EPR_PI 3.14159265358979323846F

static bool finite_vector(const float *values, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

static float smooth01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0F - 2.0F * t);
}

static float mixf(float from, float to, float weight) {
    return from + (to - from) * weight;
}

static void mix3(const float from[3], const float to[3], float weight, float result[3]) {
    for (size_t index = 0; index < 3U; ++index) {
        result[index] = mixf(from[index], to[index], weight);
    }
}

static EidolonEprTraceRecord trace_record(const EidolonPerformanceRuntime *runtime,
                                          EidolonEprTick tick,
                                          EidolonEprTraceEvent event,
                                          EidolonEprTraceReason reason) {
    const EidolonEprTraceRecord record = {
        .tick = tick,
        .intent_revision = runtime->has_intent ? runtime->intent.revision : 0U,
        .plan_generation = runtime->has_plan ? runtime->plan.generation : 0U,
        .event = event,
        .reason = reason,
    };
    return record;
}

static void emit(EidolonPerformanceRuntime *runtime, EidolonEprTraceRecord record) {
    (void)eidolon_epr_trace_emit(&runtime->trace, record);
}

EidolonEprBodyProfile eidolon_epr_default_body_profile(void) {
    const EidolonEprBodyProfile profile = {
        .version = EIDOLON_EPR_BODY_PROFILE_VERSION,
        .fingerprint = UINT64_C(0x4550522d56495254),
        .shoulder = {0.22F, 1.42F, 0.0F},
        .right_upper_arm_length = 0.30F,
        .right_lower_arm_length = 0.28F,
        .maximum_reach_ratio = 0.98F,
        .shoulder_limit_radians = 2.60F,
        .elbow_limit_radians = 2.70F,
        .has_required_humanoid = true,
        .has_right_arm = true,
        .has_eyes = true,
        .has_expression = true,
    };
    return profile;
}

static bool body_profile_valid(const EidolonEprBodyProfile *body) {
    return body != NULL && body->version == EIDOLON_EPR_BODY_PROFILE_VERSION &&
           body->has_required_humanoid && body->has_right_arm &&
           isfinite(body->right_upper_arm_length) && body->right_upper_arm_length > 0.0F &&
           isfinite(body->right_lower_arm_length) && body->right_lower_arm_length > 0.0F &&
           isfinite(body->maximum_reach_ratio) && body->maximum_reach_ratio > 0.0F &&
           body->maximum_reach_ratio <= 1.0F && finite_vector(body->shoulder, 3U);
}

bool eidolon_epr_runtime_init(EidolonPerformanceRuntime *runtime, uint64_t seed,
                              const EidolonEprBodyProfile *body) {
    if (runtime == NULL || !body_profile_valid(body)) {
        return false;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->seed = seed;
    runtime->body = *body;
    runtime->control.version = EIDOLON_EPR_CONTROL_VERSION;
    runtime->control.valid = true;
    runtime->control.gaze_target[2] = 1.0F;
    runtime->control.revision = 1U;
    runtime->control.hash = eidolon_epr_control_hash(&runtime->control);
    runtime->last_tick = 0;
    eidolon_epr_trace_init(&runtime->trace);
    return true;
}

static bool revisions_valid(const EidolonPerformanceRuntime *runtime,
                            const EidolonPerformanceIntent *intent) {
    if (!runtime->has_intent) {
        return intent->predecessor_revision == 0U;
    }
    return intent->revision > runtime->intent.revision &&
           intent->predecessor_revision == runtime->intent.revision &&
           intent->provenance.source == runtime->intent.provenance.source &&
           intent->provenance.session == runtime->intent.provenance.session;
}

static void trace_new_realizers(EidolonPerformanceRuntime *runtime,
                                const EidolonBehaviorPlan *previous) {
    for (size_t index = 0; index < runtime->plan.behavior_count; ++index) {
        const EidolonEprBehaviorUnit *behavior = &runtime->plan.behaviors[index];
        const bool existed =
            previous != NULL && eidolon_epr_plan_find(previous, behavior->id) != NULL;
        if (!existed && !behavior->retired) {
            EidolonEprTraceRecord record =
                trace_record(runtime, runtime->intent.observed_tick,
                             EIDOLON_EPR_TRACE_REALIZER_SELECTED,
                             EIDOLON_EPR_REASON_SELECTED);
            record.behavior = behavior->id;
            record.cause = behavior->cause;
            record.value = (uint64_t)behavior->kind;
            emit(runtime, record);
        }
    }
}

bool eidolon_epr_runtime_accept(EidolonPerformanceRuntime *runtime,
                                const EidolonPerformanceIntent *intent) {
    EidolonBehaviorPlan candidate;
    EidolonBehaviorPlan previous;
    const bool had_previous = runtime != NULL && runtime->has_plan;
    if (runtime == NULL || intent == NULL) {
        return false;
    }
    if (!eidolon_epr_intent_validate(intent)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_INTENT_REJECTED,
                         EIDOLON_EPR_REASON_INVALID_INTENT);
        emit(runtime, record);
        return false;
    }
    if (!revisions_valid(runtime, intent)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_INTENT_REJECTED,
                         EIDOLON_EPR_REASON_STALE_REVISION);
        record.value = intent->revision;
        emit(runtime, record);
        return false;
    }
    if (had_previous) {
        previous = runtime->plan;
    } else {
        eidolon_epr_plan_init(&previous);
    }
    if (!eidolon_epr_plan_apply(had_previous ? &previous : NULL, intent, &candidate)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_PLAN_REJECTED,
                         EIDOLON_EPR_REASON_TEMPORAL_CONFLICT);
        record.value = intent->revision;
        emit(runtime, record);
        return false;
    }
    if (intent->mode == EIDOLON_EPR_MODE_INTERRUPTED) {
        runtime->settle_start = runtime->control;
    }
    runtime->intent = *intent;
    runtime->has_intent = true;
    runtime->plan = candidate;
    runtime->has_plan = true;

    {
        EidolonEprTraceRecord accepted =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_INTENT_ACCEPTED,
                         EIDOLON_EPR_REASON_NONE);
        accepted.cause = intent->provenance.message;
        accepted.value = (uint64_t)intent->mode;
        emit(runtime, accepted);
    }
    {
        EidolonEprTraceRecord published =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_PLAN_PUBLISHED,
                         EIDOLON_EPR_REASON_NONE);
        published.value = runtime->plan.predecessor_generation;
        emit(runtime, published);
    }
    trace_new_realizers(runtime, had_previous ? &previous : NULL);
    return true;
}

static EidolonEprBehaviorRuntimeState *
runtime_state(EidolonPerformanceRuntime *runtime, EidolonEprOpaqueId behavior) {
    for (size_t index = 0; index < runtime->behavior_state_count; ++index) {
        if (runtime->behavior_states[index].behavior == behavior) {
            return &runtime->behavior_states[index];
        }
    }
    if (runtime->behavior_state_count >= EIDOLON_EPR_BEHAVIOR_CAPACITY) {
        return NULL;
    }
    {
        EidolonEprBehaviorRuntimeState *state =
            &runtime->behavior_states[runtime->behavior_state_count];
        memset(state, 0, sizeof(*state));
        state->behavior = behavior;
        state->state = EIDOLON_EPR_BEHAVIOR_PROPOSED;
        state->highest_observed_phase = -1;
        runtime->behavior_state_count += 1U;
        return state;
    }
}

static EidolonEprBehaviorState desired_state(const EidolonEprBehaviorUnit *behavior,
                                             EidolonEprTick tick,
                                             EidolonEprTerminalReason *terminal) {
    if (behavior->retired) {
        *terminal = behavior->terminal_reason;
        return EIDOLON_EPR_BEHAVIOR_RETIRED;
    }
    if (behavior->has_phase[EIDOLON_EPR_PHASE_COMPLETION] &&
        tick >= behavior->phase_ticks[EIDOLON_EPR_PHASE_COMPLETION]) {
        *terminal = EIDOLON_EPR_TERMINAL_COMPLETED;
        return EIDOLON_EPR_BEHAVIOR_RETIRED;
    }
    if (behavior->has_phase[EIDOLON_EPR_PHASE_SETTLE] &&
        tick >= behavior->phase_ticks[EIDOLON_EPR_PHASE_SETTLE]) {
        *terminal = EIDOLON_EPR_TERMINAL_COMPLETED;
        return EIDOLON_EPR_BEHAVIOR_RETIRED;
    }
    if (behavior->has_phase[EIDOLON_EPR_PHASE_PREPARATION]) {
        const EidolonEprTick preparation =
            behavior->phase_ticks[EIDOLON_EPR_PHASE_PREPARATION];
        if (tick < preparation - 200) {
            return EIDOLON_EPR_BEHAVIOR_SCHEDULED;
        }
        if (tick < preparation) {
            return EIDOLON_EPR_BEHAVIOR_COMMITTED;
        }
        return EIDOLON_EPR_BEHAVIOR_EXECUTING;
    }
    if (behavior->has_phase[EIDOLON_EPR_PHASE_ONSET] &&
        tick >= behavior->phase_ticks[EIDOLON_EPR_PHASE_ONSET]) {
        return EIDOLON_EPR_BEHAVIOR_EXECUTING;
    }
    if (behavior->has_phase[EIDOLON_EPR_PHASE_INTERRUPT] &&
        tick >= behavior->phase_ticks[EIDOLON_EPR_PHASE_INTERRUPT]) {
        return EIDOLON_EPR_BEHAVIOR_EXECUTING;
    }
    return EIDOLON_EPR_BEHAVIOR_SCHEDULED;
}

static void advance_behavior_states(EidolonPerformanceRuntime *runtime, EidolonEprTick tick) {
    for (size_t index = 0; index < runtime->plan.behavior_count; ++index) {
        const EidolonEprBehaviorUnit *behavior = &runtime->plan.behaviors[index];
        EidolonEprBehaviorRuntimeState *state = runtime_state(runtime, behavior->id);
        EidolonEprTerminalReason terminal = EIDOLON_EPR_TERMINAL_NONE;
        const EidolonEprBehaviorState desired = desired_state(behavior, tick, &terminal);
        if (state == NULL) {
            continue;
        }
        if (state->state != EIDOLON_EPR_BEHAVIOR_RETIRED && desired > state->state) {
            state->state = desired;
            state->terminal_reason = terminal;
            {
                EidolonEprTraceRecord record =
                    trace_record(runtime, tick, EIDOLON_EPR_TRACE_BEHAVIOR_TRANSITION,
                                 desired == EIDOLON_EPR_BEHAVIOR_RETIRED
                                     ? (terminal == EIDOLON_EPR_TERMINAL_INTERRUPTED
                                            ? EIDOLON_EPR_REASON_INTERRUPTED
                                            : EIDOLON_EPR_REASON_COMPLETED)
                                     : EIDOLON_EPR_REASON_NONE);
                record.behavior = behavior->id;
                record.value = (uint64_t)desired;
                emit(runtime, record);
            }
        }
        for (int phase = state->highest_observed_phase + 1;
             phase < (int)EIDOLON_EPR_BEHAVIOR_PHASE_COUNT; ++phase) {
            if (!behavior->has_phase[(size_t)phase] ||
                tick < behavior->phase_ticks[(size_t)phase]) {
                continue;
            }
            state->highest_observed_phase = phase;
            {
                EidolonEprTraceRecord record =
                    trace_record(runtime, tick, EIDOLON_EPR_TRACE_ANCHOR_OBSERVED,
                                 EIDOLON_EPR_REASON_NONE);
                record.behavior = behavior->id;
                record.cause = behavior->cause;
                record.value = (uint64_t)phase;
                emit(runtime, record);
            }
        }
    }
}

static bool grant_equal(const EidolonEprResourceGrant *left,
                        const EidolonEprResourceGrant *right) {
    return left->behavior == right->behavior && left->resource == right->resource &&
           left->mode == right->mode && left->composition_rule == right->composition_rule;
}

static bool resolution_has(const EidolonEprResourceResolution *resolution,
                           const EidolonEprResourceGrant *grant) {
    for (size_t index = 0; index < resolution->grant_count; ++index) {
        if (grant_equal(&resolution->grants[index], grant)) {
            return true;
        }
    }
    return false;
}

static void trace_resource_changes(EidolonPerformanceRuntime *runtime,
                                   const EidolonEprResourceResolution *next,
                                   EidolonEprTick tick) {
    if (runtime->has_resources) {
        const EidolonEprOpaqueId old_right = eidolon_epr_resource_override_owner(
            &runtime->resources, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
        const EidolonEprOpaqueId new_right =
            eidolon_epr_resource_override_owner(next, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
        if (old_right != 0U && new_right != 0U && old_right != new_right) {
            EidolonEprTraceRecord transfer =
                trace_record(runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_TRANSFERRED,
                             EIDOLON_EPR_REASON_PREEMPTED);
            transfer.behavior = new_right;
            transfer.cause = old_right;
            transfer.resource = (uint32_t)EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
            emit(runtime, transfer);
        }
        for (size_t index = 0; index < runtime->resources.grant_count; ++index) {
            const EidolonEprResourceGrant *old = &runtime->resources.grants[index];
            if (!resolution_has(next, old)) {
                EidolonEprTraceRecord released =
                    trace_record(runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_RELEASED,
                                 EIDOLON_EPR_REASON_NONE);
                released.behavior = old->behavior;
                released.resource = (uint32_t)old->resource;
                emit(runtime, released);
            }
        }
    }
    for (size_t index = 0; index < next->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &next->grants[index];
        if (!runtime->has_resources || !resolution_has(&runtime->resources, grant)) {
            EidolonEprTraceRecord granted =
                trace_record(runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_GRANTED,
                             EIDOLON_EPR_REASON_SELECTED);
            granted.behavior = grant->behavior;
            granted.resource = (uint32_t)grant->resource;
            granted.value = (uint64_t)grant->mode;
            emit(runtime, granted);
        }
    }
    for (size_t index = 0; index < next->denied_count; ++index) {
        EidolonEprTraceRecord denied =
            trace_record(runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_DENIED,
                         EIDOLON_EPR_REASON_PREEMPTED);
        denied.behavior = next->denied[index];
        emit(runtime, denied);
    }
}

static const EidolonEprBehaviorUnit *
behavior_for_owner(const EidolonPerformanceRuntime *runtime, EidolonEprOpaqueId owner) {
    return eidolon_epr_plan_find(&runtime->plan, owner);
}

static void base_posture(const EidolonEprBehaviorUnit *posture, EidolonEprTick tick,
                         EidolonCanonicalControl *candidate, float arm_target[3]) {
    static const float neutral_arm[3] = {0.36F, 1.03F, 0.03F};
    float target_pitch = 0.0F;
    float target_head_pitch = 0.0F;
    float target_arm[3] = {neutral_arm[0], neutral_arm[1], neutral_arm[2]};
    float weight = 1.0F;
    if (posture != NULL) {
        const EidolonEprTick onset = posture->has_phase[EIDOLON_EPR_PHASE_ONSET]
                                         ? posture->phase_ticks[EIDOLON_EPR_PHASE_ONSET]
                                         : tick;
        weight = smooth01((float)(tick - onset) / 240.0F);
        switch (posture->kind) {
        case EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE:
            target_pitch = 0.035F;
            target_head_pitch = -0.025F;
            target_arm[0] = 0.34F;
            target_arm[1] = 1.05F;
            target_arm[2] = 0.02F;
            break;
        case EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING:
            target_pitch = 0.075F;
            target_head_pitch = 0.11F;
            target_arm[0] = 0.31F;
            target_arm[1] = 1.10F;
            target_arm[2] = 0.10F;
            break;
        case EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING:
            target_pitch = -0.025F;
            target_head_pitch = -0.015F;
            target_arm[0] = 0.39F;
            target_arm[1] = 1.08F;
            target_arm[2] = 0.04F;
            break;
        case EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED:
            target_pitch = 0.055F;
            target_head_pitch = 0.015F;
            target_arm[0] = 0.30F;
            target_arm[1] = 1.14F;
            target_arm[2] = 0.12F;
            break;
        default:
            break;
        }
    }
    candidate->torso_pitch = target_pitch * weight;
    candidate->head_pitch = target_head_pitch * weight;
    mix3(neutral_arm, target_arm, weight, arm_target);
}

static void apply_idle(const EidolonPerformanceRuntime *runtime, EidolonEprTick tick,
                       EidolonCanonicalControl *candidate) {
    const float seconds = (float)tick / 1000.0F;
    const float phase =
        (float)(runtime->seed % UINT64_C(997)) * (2.0F * EIDOLON_EPR_PI / 997.0F);
    candidate->torso_pitch += 0.008F * sinf(seconds * 1.17F + phase);
    candidate->torso_roll += 0.006F * sinf(seconds * 0.43F + phase * 0.73F);
    candidate->head_roll -= candidate->torso_roll * 0.45F;
}

static void apply_gaze(EidolonPerformanceRuntime *runtime,
                       const EidolonEprBehaviorUnit *gaze, EidolonEprTick tick,
                       EidolonCanonicalControl *candidate) {
    float target[3] = {0.0F, 1.55F, 1.0F};
    float yaw = 0.0F;
    float pitch = 0.0F;
    float eye_weight;
    float head_weight;
    EidolonEprTick onset;
    if (gaze == NULL) {
        candidate->gaze_target[0] = target[0];
        candidate->gaze_target[1] = target[1];
        candidate->gaze_target[2] = target[2];
        return;
    }
    onset = gaze->has_phase[EIDOLON_EPR_PHASE_ONSET]
                ? gaze->phase_ticks[EIDOLON_EPR_PHASE_ONSET]
                : tick;
    switch (gaze->kind) {
    case EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION:
        target[0] = -0.22F;
        yaw = -0.18F;
        pitch = -0.03F;
        break;
    case EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE:
        target[0] = 0.20F;
        target[1] = 1.65F;
        yaw = 0.15F;
        pitch = -0.05F;
        break;
    case EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED:
        target[0] = -0.36F;
        target[1] = 1.60F;
        yaw = -0.28F;
        pitch = -0.02F;
        break;
    default:
        break;
    }
    eye_weight = smooth01((float)(tick - onset) / 100.0F);
    head_weight = smooth01((float)(tick - onset - 80) / 240.0F);
    if (!runtime->body.has_eyes) {
        eye_weight = 0.0F;
        head_weight = smooth01((float)(tick - onset) / 200.0F);
        candidate->eyes_degraded = true;
        if (!runtime->eyes_degradation_traced) {
            EidolonEprTraceRecord degraded =
                trace_record(runtime, tick, EIDOLON_EPR_TRACE_CAPABILITY_DEGRADED,
                             EIDOLON_EPR_REASON_OPTIONAL_MISSING);
            degraded.resource = (uint32_t)EIDOLON_EPR_RESOURCE_EYES;
            degraded.behavior = gaze->id;
            emit(runtime, degraded);
            runtime->eyes_degradation_traced = true;
        }
    }
    memcpy(candidate->gaze_target, target, sizeof(target));
    candidate->eye_yaw = yaw * eye_weight;
    candidate->eye_pitch = pitch * eye_weight;
    candidate->eye_weight = eye_weight;
    candidate->head_yaw += yaw * head_weight * 0.72F;
    candidate->head_pitch += pitch * head_weight * 0.50F;
    candidate->head_gaze_weight = head_weight;
}

static void gesture_target(const EidolonEprBehaviorUnit *gesture, EidolonEprTick tick,
                           const float rest[3], float target[3], float wrist[3]) {
    static const float preparation_target[3] = {0.40F, 1.15F, 0.10F};
    static const float peak_target[3] = {0.57F, 1.28F, 0.22F};
    static const float recovery_target[3] = {0.42F, 1.15F, 0.08F};
    const EidolonEprTick preparation =
        gesture->phase_ticks[EIDOLON_EPR_PHASE_PREPARATION];
    const EidolonEprTick onset = gesture->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
    const EidolonEprTick peak = gesture->phase_ticks[EIDOLON_EPR_PHASE_PEAK];
    const EidolonEprTick recovery = gesture->phase_ticks[EIDOLON_EPR_PHASE_RECOVERY];
    const EidolonEprTick completion = gesture->phase_ticks[EIDOLON_EPR_PHASE_COMPLETION];
    if (tick < preparation) {
        memcpy(target, rest, sizeof(float) * 3U);
        return;
    }
    if (tick < onset) {
        mix3(rest, preparation_target,
             smooth01((float)(tick - preparation) / (float)(onset - preparation)), target);
    } else if (tick < peak) {
        mix3(preparation_target, peak_target,
             smooth01((float)(tick - onset) / (float)(peak - onset)), target);
    } else if (tick < recovery) {
        mix3(peak_target, recovery_target,
             smooth01((float)(tick - peak) / (float)(recovery - peak)), target);
    } else {
        mix3(recovery_target, rest,
             smooth01((float)(tick - recovery) / (float)(completion - recovery)), target);
    }
    wrist[0] = -0.12F * smooth01((float)(tick - onset) / (float)(peak - onset));
    wrist[1] = 0.18F * smooth01((float)(tick - onset) / (float)(peak - onset));
}

static void right_arm_target(EidolonPerformanceRuntime *runtime,
                             const EidolonEprBehaviorUnit *owner, EidolonEprTick tick,
                             const float posture_target[3],
                             EidolonCanonicalControl *candidate) {
    static const float pole[3] = {0.48F, 1.38F, -0.24F};
    memcpy(candidate->right_hand_target, posture_target, sizeof(float) * 3U);
    memcpy(candidate->right_elbow_pole, pole, sizeof(pole));
    if (owner == NULL) {
        return;
    }
    if (owner->kind == EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT) {
        gesture_target(owner, tick, posture_target, candidate->right_hand_target,
                       candidate->right_wrist_euler);
    } else if (owner->kind == EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM) {
        const EidolonEprTick start = owner->phase_ticks[EIDOLON_EPR_PHASE_INTERRUPT];
        const EidolonEprTick end = owner->phase_ticks[EIDOLON_EPR_PHASE_SETTLE];
        const float weight = smooth01((float)(tick - start) / (float)(end - start));
        mix3(runtime->settle_start.right_hand_position, posture_target, weight,
             candidate->right_hand_target);
        for (size_t index = 0; index < 3U; ++index) {
            candidate->right_wrist_euler[index] =
                mixf(runtime->settle_start.right_wrist_euler[index], 0.0F, weight);
        }
    }
}

static bool solve_right_arm(const EidolonEprBodyProfile *body,
                            EidolonCanonicalControl *candidate) {
    EidolonIkTwoBoneInput input;
    EidolonIkTwoBoneSolution solution;
    memset(&input, 0, sizeof(input));
    memcpy(input.root, body->shoulder, sizeof(input.root));
    memcpy(input.target, candidate->right_hand_target, sizeof(input.target));
    memcpy(input.pole, candidate->right_elbow_pole, sizeof(input.pole));
    input.fallback_direction[0] = 0.4F;
    input.fallback_direction[1] = -0.8F;
    input.fallback_direction[2] = 0.1F;
    input.upper_length = body->right_upper_arm_length;
    input.lower_length = body->right_lower_arm_length;
    input.soften_ratio = body->maximum_reach_ratio;
    if (!eidolon_ik_solve_two_bone(&input, &solution)) {
        return false;
    }
    memcpy(candidate->right_elbow_position, solution.mid, sizeof(solution.mid));
    memcpy(candidate->right_hand_position, solution.end, sizeof(solution.end));
    return finite_vector(candidate->right_elbow_position, 3U) &&
           finite_vector(candidate->right_hand_position, 3U);
}

static bool canonical_valid(const EidolonPerformanceRuntime *runtime,
                            const EidolonCanonicalControl *candidate) {
    const float angles[] = {
        candidate->torso_pitch, candidate->torso_yaw, candidate->torso_roll,
        candidate->head_pitch,  candidate->head_yaw,  candidate->head_roll,
        candidate->eye_yaw,     candidate->eye_pitch,
    };
    return finite_vector(angles, sizeof(angles) / sizeof(angles[0])) &&
           finite_vector(candidate->gaze_target, 3U) &&
           finite_vector(candidate->right_hand_target, 3U) &&
           finite_vector(candidate->right_elbow_position, 3U) &&
           finite_vector(candidate->right_hand_position, 3U) &&
           finite_vector(candidate->right_wrist_euler, 3U) &&
           fabsf(candidate->torso_pitch) <= 0.7F && fabsf(candidate->torso_yaw) <= 0.7F &&
           fabsf(candidate->torso_roll) <= 0.7F &&
           fabsf(candidate->head_pitch) <= runtime->body.shoulder_limit_radians &&
           fabsf(candidate->head_yaw) <= runtime->body.shoulder_limit_radians;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    for (size_t index = 0; index < 8U; ++index) {
        hash ^= (unsigned char)(value & UINT64_C(0xff));
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static int64_t quantize(float value) {
    return (int64_t)llroundf(value * 1000000.0F);
}

uint64_t eidolon_epr_control_hash(EidolonCanonicalControl *control) {
    uint64_t hash = UINT64_C(1469598103934665603);
    const float *vectors[] = {
        &control->torso_pitch,
        control->gaze_target,
        &control->eye_yaw,
        control->right_hand_target,
        control->right_elbow_pole,
        control->right_elbow_position,
        control->right_hand_position,
        control->right_wrist_euler,
        control->right_arm_velocity,
        &control->focused_expression_weight,
    };
    const size_t counts[] = {6U, 3U, 4U, 3U, 3U, 3U, 3U, 3U, 3U, 1U};
    hash = hash_u64(hash, control->version);
    hash = hash_u64(hash, control->revision);
    hash = hash_u64(hash, control->plan_generation);
    hash = hash_u64(hash, (uint64_t)control->tick);
    for (size_t vector = 0; vector < sizeof(vectors) / sizeof(vectors[0]); ++vector) {
        for (size_t index = 0; index < counts[vector]; ++index) {
            hash = hash_u64(hash, (uint64_t)quantize(vectors[vector][index]));
        }
    }
    hash = hash_u64(hash, control->valid ? 1U : 0U);
    hash = hash_u64(hash, control->eyes_degraded ? 1U : 0U);
    hash = hash_u64(hash, control->expression_degraded ? 1U : 0U);
    control->hash = hash;
    return hash;
}

bool eidolon_epr_runtime_step(EidolonPerformanceRuntime *runtime, EidolonEprTick tick) {
    EidolonEprResourceResolution resolution;
    EidolonCanonicalControl candidate;
    const EidolonEprBehaviorUnit *posture;
    const EidolonEprBehaviorUnit *gaze;
    const EidolonEprBehaviorUnit *right_arm;
    EidolonEprOpaqueId posture_owner;
    EidolonEprOpaqueId gaze_owner;
    EidolonEprOpaqueId right_arm_owner;
    float posture_arm_target[3];
    if (runtime == NULL || !runtime->has_plan || (runtime->has_tick && tick < runtime->last_tick) ||
        !eidolon_epr_resource_resolve(runtime->plan.claims, runtime->plan.claim_count, tick,
                                     &resolution)) {
        return false;
    }
    trace_resource_changes(runtime, &resolution, tick);
    runtime->resources = resolution;
    runtime->has_resources = true;
    advance_behavior_states(runtime, tick);

    posture_owner =
        eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_TORSO);
    gaze_owner = eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_EYES);
    right_arm_owner =
        eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    posture = behavior_for_owner(runtime, posture_owner);
    gaze = behavior_for_owner(runtime, gaze_owner);
    right_arm = behavior_for_owner(runtime, right_arm_owner);

    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_CONTROL_VERSION;
    candidate.revision = runtime->control.revision + 1U;
    candidate.plan_generation = runtime->plan.generation;
    candidate.tick = tick;
    candidate.valid = true;
    base_posture(posture, tick, &candidate, posture_arm_target);
    if (eidolon_epr_resource_is_granted(
            &resolution, eidolon_epr_behavior_id(EIDOLON_EPR_BEHAVIOR_IDLE, 1U),
            EIDOLON_EPR_RESOURCE_TORSO)) {
        apply_idle(runtime, tick, &candidate);
    }
    apply_gaze(runtime, gaze, tick, &candidate);
    right_arm_target(runtime, right_arm, tick, posture_arm_target, &candidate);

    if (runtime->body.has_expression) {
        candidate.focused_expression_weight =
            runtime->plan.mode == EIDOLON_EPR_MODE_THINKING ? 0.45F : 0.0F;
    } else {
        candidate.expression_degraded = true;
        if (!runtime->expression_degradation_traced) {
            EidolonEprTraceRecord degraded =
                trace_record(runtime, tick, EIDOLON_EPR_TRACE_CAPABILITY_DEGRADED,
                             EIDOLON_EPR_REASON_OPTIONAL_MISSING);
            degraded.resource = (uint32_t)EIDOLON_EPR_RESOURCE_FACE_EXPRESSION;
            emit(runtime, degraded);
            runtime->expression_degradation_traced = true;
        }
    }
    if (runtime->has_tick && tick > runtime->control.tick) {
        const float seconds = (float)(tick - runtime->control.tick) / 1000.0F;
        for (size_t index = 0; index < 3U; ++index) {
            candidate.right_arm_velocity[index] =
                (candidate.right_hand_target[index] - runtime->control.right_hand_position[index]) /
                seconds;
        }
    }
    if (runtime->inject_next_solve_failure || !solve_right_arm(&runtime->body, &candidate) ||
        !canonical_valid(runtime, &candidate)) {
        EidolonEprTraceRecord rejected =
            trace_record(runtime, tick, EIDOLON_EPR_TRACE_SOLVE_REJECTED,
                         runtime->inject_next_solve_failure
                             ? EIDOLON_EPR_REASON_INJECTED_FAILURE
                             : EIDOLON_EPR_REASON_INVALID_CANDIDATE);
        rejected.control_hash = runtime->control.hash;
        emit(runtime, rejected);
        runtime->inject_next_solve_failure = false;
        runtime->last_tick = tick;
        runtime->has_tick = true;
        return false;
    }
    runtime->inject_next_solve_failure = false;
    (void)eidolon_epr_control_hash(&candidate);
    runtime->control = candidate;
    runtime->last_tick = tick;
    runtime->has_tick = true;
    {
        EidolonEprTraceRecord committed =
            trace_record(runtime, tick, EIDOLON_EPR_TRACE_SOLVE_COMMITTED,
                         EIDOLON_EPR_REASON_NONE);
        committed.control_hash = candidate.hash;
        emit(runtime, committed);
    }
    {
        EidolonEprTraceRecord published =
            trace_record(runtime, tick, EIDOLON_EPR_TRACE_CONTROL_PUBLISHED,
                         EIDOLON_EPR_REASON_NONE);
        published.control_hash = candidate.hash;
        published.value = candidate.revision;
        emit(runtime, published);
    }
    return true;
}

void eidolon_epr_runtime_inject_solve_failure(EidolonPerformanceRuntime *runtime) {
    if (runtime != NULL) {
        runtime->inject_next_solve_failure = true;
    }
}

const EidolonCanonicalControl *
eidolon_epr_runtime_control(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL ? &runtime->control : NULL;
}

const EidolonBehaviorPlan *eidolon_epr_runtime_plan(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL && runtime->has_plan ? &runtime->plan : NULL;
}

const EidolonEprTrace *eidolon_epr_runtime_trace(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL ? &runtime->trace : NULL;
}
