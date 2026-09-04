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

static void trace_right_arm_task_target(EidolonPerformanceRuntime *runtime,
                                        const EidolonEprRightArmTaskTarget *target,
                                        EidolonEprTick tick, EidolonEprTraceEvent event,
                                        EidolonEprTraceReason reason) {
    EidolonEprTraceRecord record = trace_record(runtime, tick, event, reason);
    record.resource = (uint32_t)EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    if (target != NULL) {
        record.behavior = target->behavior;
        record.cause = target->target;
        record.value = target->revision;
    }
    emit(runtime, record);
}

static void release_right_arm_task_target(EidolonPerformanceRuntime *runtime, EidolonEprTick tick,
                                          EidolonEprTraceReason reason) {
    if (runtime == NULL || !runtime->has_right_arm_task_target) {
        return;
    }
    trace_right_arm_task_target(runtime, &runtime->right_arm_task_target, tick,
                                EIDOLON_EPR_TRACE_TASK_TARGET_RELEASED, reason);
    memset(&runtime->right_arm_task_target, 0, sizeof(runtime->right_arm_task_target));
    runtime->has_right_arm_task_target = false;
}

static bool plan_claims_right_arm_task_target(const EidolonPerformanceRuntime *runtime,
                                              const EidolonEprRightArmTaskTarget *target) {
    const EidolonEprBehaviorUnit *behavior;
    if (runtime == NULL || target == NULL || !runtime->has_plan ||
        target->plan_generation != runtime->plan.generation) {
        return false;
    }
    behavior = eidolon_epr_plan_find(&runtime->plan, target->behavior);
    if (behavior == NULL || behavior->retired ||
        eidolon_epr_program_find(&runtime->programs, target->behavior) == NULL) {
        return false;
    }
    for (size_t index = 0U; index < runtime->plan.claim_count; ++index) {
        const EidolonEprResourceClaim *claim = &runtime->plan.claims[index];
        if (claim->behavior == target->behavior &&
            claim->plan_generation == target->plan_generation &&
            claim->resource == EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN &&
            (claim->mode == EIDOLON_EPR_CLAIM_BASE || claim->mode == EIDOLON_EPR_CLAIM_OVERRIDE) &&
            target->sample_tick >= claim->start_tick &&
            target->valid_until_tick <= claim->end_tick) {
            return true;
        }
    }
    return false;
}

static bool right_arm_task_target_revisions_valid(const EidolonPerformanceRuntime *runtime,
                                                  const EidolonEprRightArmTaskTarget *target) {
    if (runtime->right_arm_task_target_revision == 0U) {
        return target->predecessor_revision == 0U;
    }
    return target->producer == runtime->right_arm_task_target_producer &&
           target->revision > runtime->right_arm_task_target_revision &&
           target->predecessor_revision == runtime->right_arm_task_target_revision;
}

static EidolonEprTick task_target_rejection_tick(const EidolonPerformanceRuntime *runtime,
                                                 const EidolonEprRightArmTaskTarget *target,
                                                 bool target_valid) {
    if (target_valid) {
        return target->sample_tick;
    }
    if (runtime->has_tick) {
        return runtime->last_tick;
    }
    return runtime->has_intent ? runtime->intent.observed_tick : 0;
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
                              const EidolonEprBodyProfile *body,
                              const EidolonEprRealizationProfile *realization) {
    if (runtime == NULL || !body_profile_valid(body) ||
        !eidolon_epr_realization_profile_validate(realization, body)) {
        return false;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->seed = seed;
    runtime->body = *body;
    runtime->realization = *realization;
    runtime->control.version = EIDOLON_EPR_CONTROL_VERSION;
    runtime->control.valid = true;
    runtime->control.gaze_target[2] = 1.0F;
    runtime->control.revision = 1U;
    runtime->control.hash = eidolon_epr_control_hash(&runtime->control);
    runtime->motion_result.status = EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED;
    runtime->motion_result.catalog_status = EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED;
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

bool eidolon_epr_runtime_publish_right_arm_task_target(EidolonPerformanceRuntime *runtime,
                                                       const EidolonEprRightArmTaskTarget *target) {
    bool target_valid;
    EidolonEprTraceReason reason;
    EidolonEprTick tick;
    if (runtime == NULL) {
        return false;
    }
    target_valid = eidolon_epr_right_arm_task_target_validate(target);
    tick = task_target_rejection_tick(runtime, target, target_valid);
    if (!target_valid) {
        reason = EIDOLON_EPR_REASON_INVALID_CANDIDATE;
    } else if (!right_arm_task_target_revisions_valid(runtime, target) ||
               (runtime->has_tick && target->sample_tick < runtime->last_tick)) {
        reason = EIDOLON_EPR_REASON_STALE_REVISION;
    } else if (!plan_claims_right_arm_task_target(runtime, target)) {
        reason = EIDOLON_EPR_REASON_REVISED;
    } else {
        if (runtime->has_right_arm_task_target) {
            release_right_arm_task_target(runtime, target->sample_tick, EIDOLON_EPR_REASON_REVISED);
        }
        runtime->right_arm_task_target = *target;
        runtime->right_arm_task_target_revision = target->revision;
        runtime->right_arm_task_target_producer = target->producer;
        runtime->has_right_arm_task_target = true;
        trace_right_arm_task_target(runtime, target, target->sample_tick,
                                    EIDOLON_EPR_TRACE_TASK_TARGET_ACCEPTED,
                                    EIDOLON_EPR_REASON_NONE);
        return true;
    }
    trace_right_arm_task_target(runtime, target, tick, EIDOLON_EPR_TRACE_TASK_TARGET_REJECTED,
                                reason);
    return false;
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
            const EidolonRealizationProgram *program =
                eidolon_epr_program_find(&runtime->programs, behavior->id);
            if (program != NULL && program->missing_anchor_mask != 0U) {
                EidolonEprTraceRecord fallback = trace_record(
                    runtime, runtime->intent.observed_tick, EIDOLON_EPR_TRACE_REALIZER_FALLBACK,
                    EIDOLON_EPR_REASON_CALIBRATION_MISSING);
                fallback.behavior = behavior->id;
                fallback.cause = behavior->cause;
                fallback.resource = program->missing_resource_mask != 0U
                                        ? program->missing_resource_mask
                                        : program->resource_mask;
                fallback.value = program->missing_anchor_mask;
                emit(runtime, fallback);
            }
        }
    }
}

static const EidolonRealizationProgram *
posture_program(const EidolonRealizationProgramSet *programs) {
    if (programs == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < programs->count; ++index) {
        if (programs->programs[index].modality == EIDOLON_EPR_MODALITY_POSTURE) {
            return &programs->programs[index];
        }
    }
    return NULL;
}
static const EidolonRealizationProgram *
settle_program(const EidolonRealizationProgramSet *programs) {
    if (programs == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < programs->count; ++index) {
        if (programs->programs[index].modality == EIDOLON_EPR_MODALITY_SETTLE) {
            return &programs->programs[index];
        }
    }
    return NULL;
}

bool eidolon_epr_runtime_accept(EidolonPerformanceRuntime *runtime,
                                const EidolonPerformanceIntent *intent) {
    EidolonBehaviorPlan candidate;
    EidolonRealizationProgramSet candidate_programs;
    EidolonBehaviorPlan previous;
    const EidolonRealizationProgram *previous_posture;
    const EidolonRealizationProgram *next_posture;
    const EidolonRealizationProgram *previous_settle;
    const EidolonRealizationProgram *next_settle;
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
    if (!eidolon_epr_program_set_compile(&candidate, &runtime->realization, &candidate_programs)) {
        EidolonEprTraceRecord record =
            trace_record(runtime, intent->observed_tick, EIDOLON_EPR_TRACE_REALIZER_FAILED,
                         EIDOLON_EPR_REASON_INVALID_CANDIDATE);
        record.provenance = intent->provenance;
        record.value = candidate.generation;
        emit(runtime, record);
        return false;
    }
    previous_posture = had_previous ? posture_program(&runtime->programs) : NULL;
    next_posture = posture_program(&candidate_programs);
    previous_settle = had_previous ? settle_program(&runtime->programs) : NULL;
    next_settle = settle_program(&candidate_programs);
    if (next_posture != NULL &&
        (previous_posture == NULL || previous_posture->behavior != next_posture->behavior)) {
        runtime->posture_start =
            runtime->has_posture_base ? runtime->posture_base : runtime->control;
    }
    if (next_settle != NULL &&
        (previous_settle == NULL || previous_settle->behavior != next_settle->behavior)) {
        runtime->settle_start = runtime->control;
    }
    runtime->intent = *intent;
    runtime->has_intent = true;
    runtime->has_motion_frame = false;
    runtime->motion_result.status = EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED;
    runtime->motion_result.catalog_status = EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED;
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
    if (runtime->has_right_arm_task_target) {
        release_right_arm_task_target(runtime, intent->observed_tick, EIDOLON_EPR_REASON_REVISED);
    }
    trace_new_realizers(runtime, had_previous ? &previous : NULL);
    return true;
}

bool eidolon_epr_runtime_set_motion_catalog(EidolonPerformanceRuntime *runtime,
                                            const EidolonEprMotionCatalog *catalog) {
    if (runtime == NULL || catalog == NULL || !eidolon_epr_motion_catalog_validate(catalog)) {
        return false;
    }
    runtime->motion_catalog = *catalog;
    runtime->has_motion_catalog = true;
    runtime->has_motion_frame = false;
    runtime->motion_result.status = EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED;
    runtime->motion_result.catalog_status = EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED;
    return true;
}

void eidolon_epr_runtime_clear_motion_catalog(EidolonPerformanceRuntime *runtime) {
    if (runtime == NULL) {
        return;
    }
    memset(&runtime->motion_catalog, 0, sizeof(runtime->motion_catalog));
    memset(&runtime->motion_frame, 0, sizeof(runtime->motion_frame));
    runtime->has_motion_catalog = false;
    runtime->has_motion_frame = false;
    runtime->motion_result.status = EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED;
    runtime->motion_result.catalog_status = EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED;
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
        EidolonEprBehaviorUnit *behavior = &runtime->plan.behaviors[index];
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
        if (desired == EIDOLON_EPR_BEHAVIOR_RETIRED && !behavior->retired) {
            behavior->retired = true;
            behavior->terminal_reason = terminal;
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

static bool compact_runtime_history(EidolonPerformanceRuntime *runtime) {
    size_t write = 0U;
    if (!eidolon_epr_plan_compact(&runtime->plan, runtime->has_intent ? &runtime->intent : NULL)) {
        return false;
    }
    for (size_t index = 0U; index < runtime->programs.count; ++index) {
        const EidolonRealizationProgram *program = &runtime->programs.programs[index];
        const EidolonEprBehaviorUnit *behavior =
            eidolon_epr_plan_find(&runtime->plan, program->behavior);
        if (behavior != NULL && !behavior->retired) {
            runtime->programs.programs[write] = *program;
            write += 1U;
        }
    }
    memset(&runtime->programs.programs[write], 0,
           (EIDOLON_EPR_PROGRAM_CAPACITY - write) * sizeof(runtime->programs.programs[0]));
    runtime->programs.count = write;

    write = 0U;
    for (size_t index = 0U; index < runtime->behavior_state_count; ++index) {
        const EidolonEprBehaviorRuntimeState *state = &runtime->behavior_states[index];
        if (state->state != EIDOLON_EPR_BEHAVIOR_RETIRED ||
            eidolon_epr_plan_find(&runtime->plan, state->behavior) != NULL) {
            runtime->behavior_states[write] = *state;
            write += 1U;
        }
    }
    memset(&runtime->behavior_states[write], 0,
           (EIDOLON_EPR_BEHAVIOR_CAPACITY - write) * sizeof(runtime->behavior_states[0]));
    runtime->behavior_state_count = write;
    return true;
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

static bool anchor_weights_valid(const EidolonCanonicalControl *candidate) {
    for (size_t resource = 0U; resource < EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
        float total = 0.0F;
        for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
            const float weight = candidate->pose_anchor_resource_weights[anchor][resource];
            if (!isfinite(weight) || weight < 0.0F || weight > 1.0F) {
                return false;
            }
            total += weight;
        }
        if (total > 1.001F) {
            return false;
        }
    }
    return true;
}

static uint32_t procedural_resource_mask(void) {
    return (UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD) | (UINT32_C(1) << EIDOLON_EPR_RESOURCE_EYES) |
           (UINT32_C(1) << EIDOLON_EPR_RESOURCE_FACE_EXPRESSION) |
           (UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
}

static void include_procedural_grants(const EidolonEprResourceResolution *resolution,
                                      const EidolonRealizationProgram *program,
                                      EidolonCanonicalControl *candidate) {
    if (resolution == NULL || program == NULL || candidate == NULL ||
        program->motion.generator != EIDOLON_EPR_MOTION_GENERATOR_NONE) {
        return;
    }
    for (size_t resource = 0U; resource < EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
        const uint32_t bit = UINT32_C(1) << (uint32_t)resource;
        if ((program->resource_mask & bit) != 0U &&
            eidolon_epr_resource_is_granted(resolution, program->behavior,
                                            (EidolonEprBodyResource)resource)) {
            candidate->procedural_resource_mask |= bit;
        }
    }
}

static uint64_t apply_right_arm_task_target(EidolonPerformanceRuntime *runtime,
                                            const EidolonEprResourceResolution *resolution,
                                            EidolonEprTick tick,
                                            EidolonCanonicalControl *candidate) {
    const EidolonEprRightArmTaskTarget *target;
    const float arm_length =
        runtime->body.right_upper_arm_length + runtime->body.right_lower_arm_length;
    if (!runtime->has_right_arm_task_target) {
        return 0U;
    }
    target = &runtime->right_arm_task_target;
    if (tick > target->valid_until_tick) {
        release_right_arm_task_target(runtime, tick, EIDOLON_EPR_REASON_COMPLETED);
        return 0U;
    }
    if (target->plan_generation != runtime->plan.generation) {
        release_right_arm_task_target(runtime, tick, EIDOLON_EPR_REASON_REVISED);
        return 0U;
    }
    if (tick < target->sample_tick ||
        !eidolon_epr_resource_is_granted(resolution, target->behavior,
                                         EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN)) {
        return 0U;
    }
    for (size_t axis = 0U; axis < 3U; ++axis) {
        candidate->right_hand_target[axis] =
            runtime->body.shoulder[axis] +
            arm_length * (target->hand_target[0] * runtime->body.right[axis] +
                          target->hand_target[1] * runtime->body.up[axis] +
                          target->hand_target[2] * runtime->body.forward[axis]);
        candidate->right_elbow_pole[axis] =
            runtime->body.shoulder[axis] +
            arm_length * (target->elbow_pole[0] * runtime->body.right[axis] +
                          target->elbow_pole[1] * runtime->body.up[axis] +
                          target->elbow_pole[2] * runtime->body.forward[axis]);
        candidate->right_wrist_euler[axis] = target->wrist_euler[axis];
    }
    candidate->right_arm_ik_weight = target->weight;
    candidate->right_arm_continuity_weight = 0.0F;
    candidate->right_arm_continuity_id = 0U;
    candidate->procedural_resource_mask |= UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    return target->revision;
}

static bool canonical_valid(const EidolonPerformanceRuntime *runtime,
                            const EidolonCanonicalControl *candidate) {
    const uint32_t right_arm = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    const float angles[] = {
        candidate->torso_pitch,   candidate->torso_yaw, candidate->torso_roll,
        candidate->head_pitch,    candidate->head_yaw,  candidate->head_roll,
        candidate->eye_yaw,       candidate->eye_pitch, candidate->head_gaze_pitch,
        candidate->head_gaze_yaw,
    };
    const float procedural_weights[] = {
        candidate->right_arm_ik_weight,
        candidate->right_arm_continuity_weight,
    };
    return finite_vector(angles, sizeof(angles) / sizeof(angles[0])) &&
           finite_vector(procedural_weights,
                         sizeof(procedural_weights) / sizeof(procedural_weights[0])) &&
           candidate->right_arm_ik_weight >= 0.0F && candidate->right_arm_ik_weight <= 1.0F &&
           candidate->right_arm_continuity_weight >= 0.0F &&
           candidate->right_arm_continuity_weight <= 1.0F &&
           (candidate->procedural_resource_mask & ~procedural_resource_mask()) == 0U &&
           ((candidate->procedural_resource_mask & right_arm) != 0U ||
            (candidate->right_arm_ik_weight == 0.0F &&
             candidate->right_arm_continuity_weight == 0.0F &&
             candidate->right_arm_continuity_id == 0U)) &&
           ((candidate->right_arm_continuity_weight > 0.0F) ==
            (candidate->right_arm_continuity_id != 0U)) &&
           finite_vector(candidate->gaze_target, 3U) &&
           finite_vector(candidate->right_hand_target, 3U) &&
           finite_vector(candidate->right_elbow_position, 3U) &&
           finite_vector(candidate->right_hand_position, 3U) &&
           finite_vector(candidate->right_wrist_euler, 3U) && anchor_weights_valid(candidate) &&
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
        &control->pose_anchor_resource_weights[0][0],
        &control->right_arm_ik_weight,
    };
    const size_t counts[] = {
        6U, 3U, 6U, 3U, 3U,
        3U, 3U, 3U, 3U, EIDOLON_EPR_POSE_ANCHOR_COUNT * EIDOLON_EPR_RESOURCE_COUNT,
        3U,
    };
    hash = hash_u64(hash, control->version);
    hash = hash_u64(hash, control->revision);
    hash = hash_u64(hash, control->plan_generation);
    hash = hash_u64(hash, (uint64_t)control->tick);
    for (size_t vector = 0; vector < sizeof(vectors) / sizeof(vectors[0]); ++vector) {
        for (size_t index = 0; index < counts[vector]; ++index) {
            hash = hash_u64(hash, (uint64_t)quantize(vectors[vector][index]));
        }
    }
    hash = hash_u64(hash, control->procedural_resource_mask);
    hash = hash_u64(hash, control->right_arm_continuity_id);
    hash = hash_u64(hash, control->valid ? 1U : 0U);
    hash = hash_u64(hash, control->eyes_degraded ? 1U : 0U);
    hash = hash_u64(hash, control->expression_degraded ? 1U : 0U);
    control->hash = hash;
    return hash;
}

bool eidolon_epr_runtime_step(EidolonPerformanceRuntime *runtime, EidolonEprTick tick) {
    EidolonEprResourceResolution resolution;
    EidolonCanonicalControl candidate;
    EidolonCanonicalControl posture_base;
    const EidolonRealizationProgram *posture;
    const EidolonRealizationProgram *gaze;
    const EidolonRealizationProgram *right_arm;
    const EidolonRealizationProgram *idle;
    const EidolonRealizationProgram *expression;
    EidolonEprOpaqueId posture_owner;
    EidolonEprOpaqueId gaze_owner;
    EidolonEprOpaqueId right_arm_owner;
    EidolonEprOpaqueId expression_owner;
    uint64_t decision_sequence;
    uint64_t applied_task_target_revision;
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
    EidolonHumanoidPose motion_base;
    EidolonEprMotionFrame motion_candidate;
    EidolonEprMotionExecutionResult motion_result = {
        .status = EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED,
        .catalog_status = EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED,
    };
    bool has_motion_candidate = false;
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
    eidolon_epr_realize_posture(&runtime->body, &runtime->realization, posture,
                                posture != NULL ? &runtime->posture_start : NULL, tick, &candidate);
    posture_base = candidate;
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
                                  &candidate);
    applied_task_target_revision =
        apply_right_arm_task_target(runtime, &resolution, tick, &candidate);
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
    include_procedural_grants(&resolution, gaze, &candidate);
    include_procedural_grants(&resolution, right_arm, &candidate);
    include_procedural_grants(&resolution, expression, &candidate);
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
    if (runtime->has_motion_catalog) {
        eidolon_humanoid_pose_init(&motion_base);
        motion_result =
            eidolon_epr_motion_programs_compose(&runtime->motion_catalog, &runtime->programs,
                                                &resolution, tick, &motion_base, &motion_candidate);
        has_motion_candidate = motion_result.status == EIDOLON_EPR_MOTION_EXECUTION_OK &&
                               motion_candidate.execution_count > 0U &&
                               (motion_candidate.pose.rotation_mask != 0U ||
                                motion_candidate.pose.has_hips_translation);
    }
    if (applied_task_target_revision != 0U &&
        applied_task_target_revision != runtime->right_arm_task_target_applied_revision) {
        runtime->right_arm_task_target_applied_revision = applied_task_target_revision;
        trace_right_arm_task_target(runtime, &runtime->right_arm_task_target, tick,
                                    EIDOLON_EPR_TRACE_TASK_TARGET_APPLIED,
                                    EIDOLON_EPR_REASON_SELECTED);
    }
    runtime->inject_next_solve_failure = false;
    (void)eidolon_epr_control_hash(&candidate);
    publish_checkpoint = candidate.plan_generation != runtime->control.plan_generation ||
                         runtime->trace.next_sequence != decision_sequence;
    runtime->control = candidate;
    runtime->posture_base = posture_base;
    runtime->has_posture_base = true;
    runtime->motion_result = motion_result;
    runtime->has_motion_frame = has_motion_candidate;
    if (has_motion_candidate) {
        runtime->motion_frame = motion_candidate;
    } else {
        memset(&runtime->motion_frame, 0, sizeof(runtime->motion_frame));
    }
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
    return compact_runtime_history(runtime);
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
const EidolonEprMotionFrame *
eidolon_epr_runtime_motion_frame(const EidolonPerformanceRuntime *runtime) {
    return runtime != NULL && runtime->has_motion_frame ? &runtime->motion_frame : NULL;
}

EidolonEprMotionExecutionResult
eidolon_epr_runtime_motion_result(const EidolonPerformanceRuntime *runtime) {
    const EidolonEprMotionExecutionResult unavailable = {
        .status = EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT,
        .catalog_status = EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT,
    };
    return runtime != NULL ? runtime->motion_result : unavailable;
}
