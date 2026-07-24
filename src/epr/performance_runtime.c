#include "epr/performance_runtime.h"

#include "epr/modality_realizers.h"
#include "ik.h"

#include <math.h>
#include <string.h>

static bool finite_vector(const float *values, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

static EidolonEprTraceRecord trace_record(const EidolonPerformanceRuntime *runtime,
                                          EidolonEprTick tick, EidolonEprTraceEvent event,
                                          EidolonEprTraceReason reason) {
    EidolonEprTraceRecord record = {
        .tick = tick,
        .intent_revision = runtime->has_intent ? runtime->intent.revision : 0U,
        .plan_generation = runtime->has_plan ? runtime->plan.generation : 0U,
        .event = event,
        .reason = reason,
    };
    if (runtime->has_intent) {
        record.provenance = runtime->intent.provenance;
    }
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
        .head = {0.0F, 1.70F, 0.0F},
        .right = {1.0F, 0.0F, 0.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .forward = {0.0F, 0.0F, 1.0F},
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
           body->maximum_reach_ratio <= 1.0F && finite_vector(body->shoulder, 3U) &&
           finite_vector(body->head, 3U) && finite_vector(body->right, 3U) &&
           finite_vector(body->up, 3U) && finite_vector(body->forward, 3U);
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
        const EidolonEprBehaviorUnit *previous_behavior =
            previous != NULL ? eidolon_epr_plan_find(previous, behavior->id) : NULL;
        const bool existed = previous_behavior != NULL && !previous_behavior->retired;
        if (!existed && !behavior->retired) {
            if (previous_behavior != NULL && previous_behavior->retired) {
                for (size_t state_index = 0; state_index < runtime->behavior_state_count;
                     ++state_index) {
                    EidolonEprBehaviorRuntimeState *state = &runtime->behavior_states[state_index];
                    if (state->behavior == behavior->id) {
                        state->state = EIDOLON_EPR_BEHAVIOR_PROPOSED;
                        state->highest_observed_phase = -1;
                        state->terminal_reason = EIDOLON_EPR_TERMINAL_NONE;
                        break;
                    }
                }
            }
            EidolonEprTraceRecord record =
                trace_record(runtime, runtime->intent.observed_tick,
                             EIDOLON_EPR_TRACE_REALIZER_SELECTED, EIDOLON_EPR_REASON_SELECTED);
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
    EidolonRealizationProgramSet candidate_programs;
    EidolonBehaviorPlan previous;
    const bool had_previous = runtime != NULL && runtime->has_plan;
    if (runtime == NULL || intent == NULL) {
        return false;
    }
    if (!eidolon_epr_intent_validate(intent)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_INTENT_REJECTED,
                         EIDOLON_EPR_REASON_INVALID_INTENT);
        record.provenance = intent->provenance;
        emit(runtime, record);
        return false;
    }
    if (!revisions_valid(runtime, intent)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_INTENT_REJECTED,
                         EIDOLON_EPR_REASON_STALE_REVISION);
        record.provenance = intent->provenance;
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
        record.provenance = intent->provenance;
        record.value = intent->revision;
        emit(runtime, record);
        return false;
    }
    if (!eidolon_epr_program_set_compile(&candidate, &candidate_programs)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_REALIZER_FAILED,
                         EIDOLON_EPR_REASON_INVALID_CANDIDATE);
        record.provenance = intent->provenance;
        record.value = candidate.generation;
        emit(runtime, record);
        return false;
    }
    if (intent->mode == EIDOLON_EPR_MODE_INTERRUPTED) {
        runtime->settle_start = runtime->control;
    }
    runtime->intent = *intent;
    runtime->has_intent = true;
    runtime->plan = candidate;
    runtime->programs = candidate_programs;
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

static EidolonEprBehaviorRuntimeState *runtime_state(EidolonPerformanceRuntime *runtime,
                                                     EidolonEprOpaqueId behavior) {
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
        const EidolonEprTick preparation = behavior->phase_ticks[EIDOLON_EPR_PHASE_PREPARATION];
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

static EidolonEprTraceReason terminal_trace_reason(EidolonEprTerminalReason terminal) {
    switch (terminal) {
    case EIDOLON_EPR_TERMINAL_COMPLETED:
        return EIDOLON_EPR_REASON_COMPLETED;
    case EIDOLON_EPR_TERMINAL_INTERRUPTED:
        return EIDOLON_EPR_REASON_INTERRUPTED;
    case EIDOLON_EPR_TERMINAL_REVISED:
        return EIDOLON_EPR_REASON_REVISED;
    case EIDOLON_EPR_TERMINAL_DENIED:
        return EIDOLON_EPR_REASON_PREEMPTED;
    case EIDOLON_EPR_TERMINAL_FAILED:
        return EIDOLON_EPR_REASON_INVALID_CANDIDATE;
    case EIDOLON_EPR_TERMINAL_NONE:
        return EIDOLON_EPR_REASON_NONE;
    }
    return EIDOLON_EPR_REASON_INVALID_CANDIDATE;
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
                EidolonEprTraceRecord record = trace_record(
                    runtime, tick, EIDOLON_EPR_TRACE_BEHAVIOR_TRANSITION,
                    desired == EIDOLON_EPR_BEHAVIOR_RETIRED ? terminal_trace_reason(terminal)
                                                            : EIDOLON_EPR_REASON_NONE);
                record.behavior = behavior->id;
                record.value = (uint64_t)desired;
                emit(runtime, record);
            }
        }
        if (desired == EIDOLON_EPR_BEHAVIOR_RETIRED && terminal != EIDOLON_EPR_TERMINAL_COMPLETED) {
            continue;
        }
        for (int phase = state->highest_observed_phase + 1;
             phase < (int)EIDOLON_EPR_BEHAVIOR_PHASE_COUNT; ++phase) {
            if (!behavior->has_phase[(size_t)phase] ||
                tick < behavior->phase_ticks[(size_t)phase]) {
                continue;
            }
            state->highest_observed_phase = phase;
            {
                EidolonEprTraceRecord record = trace_record(
                    runtime, tick, EIDOLON_EPR_TRACE_ANCHOR_OBSERVED, EIDOLON_EPR_REASON_NONE);
                record.behavior = behavior->id;
                record.cause = behavior->cause;
                record.value = (uint64_t)phase;
                emit(runtime, record);
            }
        }
    }
}

static bool grant_equal(const EidolonEprResourceGrant *left, const EidolonEprResourceGrant *right) {
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

static bool resolution_denied_has(const EidolonEprResourceResolution *resolution,
                                  const EidolonEprResourceDenial *denial) {
    for (size_t index = 0; index < resolution->denied_count; ++index) {
        const EidolonEprResourceDenial *existing = &resolution->denied[index];
        if (existing->behavior == denial->behavior && existing->resource == denial->resource &&
            existing->mode == denial->mode) {
            return true;
        }
    }
    return false;
}

static void trace_resource_changes(EidolonPerformanceRuntime *runtime,
                                   const EidolonEprResourceResolution *next, EidolonEprTick tick) {
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
                EidolonEprTraceRecord released = trace_record(
                    runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_RELEASED, EIDOLON_EPR_REASON_NONE);
                released.behavior = old->behavior;
                released.resource = (uint32_t)old->resource;
                emit(runtime, released);
            }
        }
    }
    for (size_t index = 0; index < next->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &next->grants[index];
        if (!runtime->has_resources || !resolution_has(&runtime->resources, grant)) {
            EidolonEprTraceRecord granted = trace_record(
                runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_GRANTED, EIDOLON_EPR_REASON_SELECTED);
            granted.behavior = grant->behavior;
            granted.resource = (uint32_t)grant->resource;
            granted.value = (uint64_t)grant->mode;
            emit(runtime, granted);
        }
    }
    for (size_t index = 0; index < next->denied_count; ++index) {
        const EidolonEprResourceDenial *denial = &next->denied[index];
        if (runtime->has_resources && resolution_denied_has(&runtime->resources, denial)) {
            continue;
        }
        EidolonEprTraceRecord denied = trace_record(
            runtime, tick, EIDOLON_EPR_TRACE_RESOURCE_DENIED, EIDOLON_EPR_REASON_PREEMPTED);
        denied.behavior = denial->behavior;
        denied.resource = (uint32_t)denial->resource;
        denied.value = (uint64_t)denial->mode;
        emit(runtime, denied);
    }
}

static const EidolonRealizationProgram *program_for_owner(const EidolonPerformanceRuntime *runtime,
                                                          EidolonEprOpaqueId owner) {
    return eidolon_epr_program_find(&runtime->programs, owner);
}

static bool solve_right_arm(const EidolonEprBodyProfile *body, EidolonCanonicalControl *candidate) {
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
        candidate->torso_pitch, candidate->torso_yaw, candidate->torso_roll, candidate->head_pitch,
        candidate->head_yaw,    candidate->head_roll, candidate->eye_yaw,    candidate->eye_pitch,
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

static int64_t quantize(float value) { return (int64_t)llroundf(value * 1000000.0F); }

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
    const EidolonRealizationProgram *posture;
    const EidolonRealizationProgram *gaze;
    const EidolonRealizationProgram *right_arm;
    const EidolonRealizationProgram *idle;
    const EidolonRealizationProgram *expression;
    EidolonEprOpaqueId posture_owner;
    EidolonEprOpaqueId gaze_owner;
    EidolonEprOpaqueId right_arm_owner;
    EidolonEprOpaqueId expression_owner;
    float posture_arm_target[3];
    uint64_t decision_sequence;
    bool publish_checkpoint;
    if (runtime == NULL || !runtime->has_plan || (runtime->has_tick && tick < runtime->last_tick) ||
        !eidolon_epr_resource_resolve(runtime->plan.claims, runtime->plan.claim_count, tick,
                                      &resolution)) {
        return false;
    }
    decision_sequence = runtime->trace.next_sequence;
    trace_resource_changes(runtime, &resolution, tick);
    runtime->resources = resolution;
    runtime->has_resources = true;
    advance_behavior_states(runtime, tick);

    posture_owner = eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_TORSO);
    gaze_owner = eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_EYES);
    right_arm_owner =
        eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    expression_owner =
        eidolon_epr_resource_override_owner(&resolution, EIDOLON_EPR_RESOURCE_FACE_EXPRESSION);
    posture = program_for_owner(runtime, posture_owner);
    gaze = program_for_owner(runtime, gaze_owner);
    right_arm = program_for_owner(runtime, right_arm_owner);
    expression = program_for_owner(runtime, expression_owner);
    idle = program_for_owner(runtime, eidolon_epr_behavior_id(EIDOLON_EPR_BEHAVIOR_IDLE, 1U));

    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_CONTROL_VERSION;
    candidate.revision = runtime->control.revision + 1U;
    candidate.plan_generation = runtime->plan.generation;
    candidate.tick = tick;
    candidate.valid = true;
    eidolon_epr_realize_posture(&runtime->body, posture, tick, &candidate, posture_arm_target);
    if (eidolon_epr_resource_is_granted(&resolution,
                                        eidolon_epr_behavior_id(EIDOLON_EPR_BEHAVIOR_IDLE, 1U),
                                        EIDOLON_EPR_RESOURCE_TORSO)) {
        eidolon_epr_realize_idle(runtime->seed, idle, tick, &candidate);
    }
    if (!eidolon_epr_realize_gaze(&runtime->body, gaze, tick, &candidate) &&
        !runtime->eyes_degradation_traced) {
        EidolonEprTraceRecord degraded =
            trace_record(runtime, tick, EIDOLON_EPR_TRACE_CAPABILITY_DEGRADED,
                         EIDOLON_EPR_REASON_OPTIONAL_MISSING);
        degraded.resource = (uint32_t)EIDOLON_EPR_RESOURCE_EYES;
        degraded.behavior = gaze != NULL ? gaze->behavior : 0U;
        emit(runtime, degraded);
        runtime->eyes_degradation_traced = true;
    }
    eidolon_epr_realize_right_arm(&runtime->body, right_arm, tick, &runtime->settle_start,
                                  posture_arm_target, &candidate);
    if (!eidolon_epr_realize_expression(&runtime->body, expression, &candidate)) {
        if (!runtime->expression_degradation_traced) {
            EidolonEprTraceRecord degraded =
                trace_record(runtime, tick, EIDOLON_EPR_TRACE_CAPABILITY_DEGRADED,
                             EIDOLON_EPR_REASON_OPTIONAL_MISSING);
            degraded.resource = (uint32_t)EIDOLON_EPR_RESOURCE_FACE_EXPRESSION;
            degraded.behavior = expression != NULL ? expression->behavior : 0U;
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
                         runtime->inject_next_solve_failure ? EIDOLON_EPR_REASON_INJECTED_FAILURE
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
    publish_checkpoint = candidate.plan_generation != runtime->control.plan_generation ||
                         runtime->trace.next_sequence != decision_sequence;
    runtime->control = candidate;
    runtime->last_tick = tick;
    runtime->has_tick = true;
    if (publish_checkpoint) {
        EidolonEprTraceRecord committed =
            trace_record(runtime, tick, EIDOLON_EPR_TRACE_SOLVE_COMMITTED, EIDOLON_EPR_REASON_NONE);
        committed.control_hash = candidate.hash;
        emit(runtime, committed);
    }
    if (publish_checkpoint) {
        EidolonEprTraceRecord published = trace_record(
            runtime, tick, EIDOLON_EPR_TRACE_CONTROL_PUBLISHED, EIDOLON_EPR_REASON_NONE);
        published.control_hash = candidate.hash;
        published.value = candidate.revision;
        emit(runtime, published);
        runtime->projection_pending_after_revision = candidate.revision;
    }
    return true;
}

void eidolon_epr_runtime_inject_solve_failure(EidolonPerformanceRuntime *runtime) {
    if (runtime != NULL) {
        runtime->inject_next_solve_failure = true;
    }
}

void eidolon_epr_runtime_note_projection(EidolonPerformanceRuntime *runtime,
                                         uint64_t control_revision, bool committed,
                                         EidolonEprTraceReason reason) {
    EidolonEprTraceRecord record;
    if (runtime == NULL || !runtime->has_tick) {
        return;
    }
    if (committed && (runtime->projection_pending_after_revision == 0U ||
                      control_revision < runtime->projection_pending_after_revision)) {
        return;
    }
    record = trace_record(runtime, runtime->last_tick,
                          committed ? EIDOLON_EPR_TRACE_PROJECTION_COMMITTED
                                    : EIDOLON_EPR_TRACE_PROJECTION_REJECTED,
                          reason);
    record.value = control_revision;
    record.control_hash = runtime->control.hash;
    emit(runtime, record);
    if (committed) {
        runtime->projection_pending_after_revision = 0U;
    }
}

const EidolonCanonicalControl *
eidolon_epr_runtime_control(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL ? &runtime->control : NULL;
}

const EidolonBehaviorPlan *eidolon_epr_runtime_plan(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL && runtime->has_plan ? &runtime->plan : NULL;
}

const EidolonRealizationProgramSet *
eidolon_epr_runtime_programs(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL && runtime->has_plan ? &runtime->programs : NULL;
}

const EidolonEprTrace *eidolon_epr_runtime_trace(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL ? &runtime->trace : NULL;
}
