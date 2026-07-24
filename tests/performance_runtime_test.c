#include "epr/body_resources.h"
#include "epr/performance_runtime.h"
#include "epr/temporal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static EidolonPerformanceIntent intent(uint64_t revision, uint64_t predecessor,
                                       EidolonEprTick tick,
                                       EidolonEprOperationalMode mode) {
    EidolonPerformanceIntent value;
    memset(&value, 0, sizeof(value));
    value.version = EIDOLON_EPR_INTENT_VERSION;
    value.revision = revision;
    value.predecessor_revision = predecessor;
    value.observed_tick = tick;
    value.mode = mode;
    value.urgency = mode == EIDOLON_EPR_MODE_INTERRUPTED ? 1000U : 400U;
    value.continuity = 800U;
    value.provenance.source = UINT64_C(0x101);
    value.provenance.session = UINT64_C(0x202);
    value.provenance.turn = UINT64_C(0x303);
    value.provenance.response = UINT64_C(0x404);
    value.provenance.message = UINT64_C(0x505);
    value.provenance.truth_revision = revision;
    if (mode != EIDOLON_EPR_MODE_ABSENT) {
        value.has_performance_lease = true;
        value.lease_source = value.provenance.source;
        value.lease_session = value.provenance.session;
    }
    return value;
}

static void add_contrast(EidolonPerformanceIntent *value) {
    value->beats[0].id = UINT64_C(0xc017a57);
    value->beats[0].kind = EIDOLON_EPR_BEAT_CONTRAST;
    value->beats[0].stability = EIDOLON_EPR_EVIDENCE_STABLE_PREFIX;
    value->beats[0].source_start = 18U;
    value->beats[0].source_end = 36U;
    value->beats[0].anchor_tick = 3510;
    value->beat_count = 1U;
}

static void test_intent_validation(void) {
    EidolonPerformanceIntent listening = intent(1U, 0U, 400, EIDOLON_EPR_MODE_LISTENING);
    assert(eidolon_epr_intent_validate(&listening));
    listening.has_performance_lease = false;
    assert(!eidolon_epr_intent_validate(&listening));

    listening = intent(1U, 0U, 400, EIDOLON_EPR_MODE_LISTENING);
    add_contrast(&listening);
    listening.beats[0].source_end = 1U;
    assert(!eidolon_epr_intent_validate(&listening));
}

static void test_temporal_transaction(void) {
    EidolonEprTemporalNetwork network;
    EidolonEprTemporalNetwork previous;
    const EidolonEprTemporalConstraint valid = {
        .from = 1U,
        .to = 2U,
        .minimum = 5,
        .maximum = 15,
    };
    const EidolonEprTemporalConstraint invalid = {
        .from = 1U,
        .to = 2U,
        .minimum = 20,
        .maximum = 30,
    };
    eidolon_epr_temporal_init(&network);
    assert(eidolon_epr_temporal_set_tick(&network, 1U, 0, true));
    assert(eidolon_epr_temporal_set_tick(&network, 2U, 10, false));
    assert(eidolon_epr_temporal_add_constraint(&network, valid));
    previous = network;
    assert(!eidolon_epr_temporal_add_constraint(&network, invalid));
    assert(memcmp(&network, &previous, sizeof(network)) == 0);
}

static EidolonEprResourceClaim claim(EidolonEprOpaqueId behavior,
                                     EidolonEprClaimMode mode, uint16_t priority) {
    EidolonEprResourceClaim value;
    memset(&value, 0, sizeof(value));
    value.behavior = behavior;
    value.plan_generation = 1U;
    value.resource = EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    value.mode = mode;
    value.start_tick = 0;
    value.end_tick = 100;
    value.rank.priority = priority;
    value.rank.anchor_tick = 0;
    value.rank.behavior = behavior;
    value.preemptible = true;
    return value;
}

static void test_resource_order_independence(void) {
    const EidolonEprResourceClaim first[] = {
        claim(20U, EIDOLON_EPR_CLAIM_OVERRIDE, 500U),
        claim(10U, EIDOLON_EPR_CLAIM_OVERRIDE, 500U),
        claim(30U, EIDOLON_EPR_CLAIM_BASE, 900U),
    };
    const EidolonEprResourceClaim second[] = {
        first[2],
        first[0],
        first[1],
    };
    EidolonEprResourceResolution a;
    EidolonEprResourceResolution b;
    assert(eidolon_epr_resource_resolve(first, 3U, 50, &a));
    assert(eidolon_epr_resource_resolve(second, 3U, 50, &b));
    assert(a.grant_count == 1U);
    assert(b.grant_count == 1U);
    assert(a.grants[0].behavior == 10U);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
}

static void accept_and_step(EidolonPerformanceRuntime *runtime,
                            EidolonPerformanceIntent *value,
                            const EidolonEprTick *ticks, size_t tick_count) {
    assert(eidolon_epr_runtime_accept(runtime, value));
    for (size_t index = 0; index < tick_count; ++index) {
        assert(eidolon_epr_runtime_step(runtime, ticks[index]));
    }
}

static void run_scenario(EidolonPerformanceRuntime *runtime,
                         const EidolonEprBodyProfile *profile) {
    EidolonPerformanceIntent value;
    const EidolonEprTick idle_ticks[] = {0};
    const EidolonEprTick listening_ticks[] = {400, 480, 800};
    const EidolonEprTick thinking_ticks[] = {1100, 1400};
    const EidolonEprTick responding_ticks[] = {2000, 2500};
    const EidolonEprTick gesture_ticks[] = {3000, 3200, 3410, 3510, 3570};
    const EidolonEprTick interrupted_ticks[] = {3600, 3760, 3920, 4200};

    assert(eidolon_epr_runtime_init(runtime, UINT64_C(0x51eed), profile));
    value = intent(1U, 0U, 0, EIDOLON_EPR_MODE_ABSENT);
    accept_and_step(runtime, &value, idle_ticks, 1U);
    value = intent(2U, 1U, 400, EIDOLON_EPR_MODE_LISTENING);
    accept_and_step(runtime, &value, listening_ticks, 3U);
    value = intent(3U, 2U, 1100, EIDOLON_EPR_MODE_THINKING);
    accept_and_step(runtime, &value, thinking_ticks, 2U);
    value = intent(4U, 3U, 2000, EIDOLON_EPR_MODE_RESPONDING);
    accept_and_step(runtime, &value, responding_ticks, 2U);
    value = intent(5U, 4U, 3000, EIDOLON_EPR_MODE_RESPONDING);
    add_contrast(&value);
    accept_and_step(runtime, &value, gesture_ticks, 5U);
    value = intent(6U, 5U, 3600, EIDOLON_EPR_MODE_INTERRUPTED);
    add_contrast(&value);
    accept_and_step(runtime, &value, interrupted_ticks, 4U);
}

static bool trace_has_transfer(const EidolonPerformanceRuntime *runtime,
                               EidolonEprOpaqueId from, EidolonEprOpaqueId to) {
    const EidolonEprTrace *trace = eidolon_epr_runtime_trace(runtime);
    for (size_t index = 0; index < trace->count; ++index) {
        const EidolonEprTraceRecord *record = &trace->records[index];
        if (record->event == EIDOLON_EPR_TRACE_RESOURCE_TRANSFERRED &&
            record->cause == from && record->behavior == to &&
            record->resource == (uint32_t)EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN) {
            return true;
        }
    }
    return false;
}

static const EidolonEprBehaviorRuntimeState *
find_runtime_state(const EidolonPerformanceRuntime *runtime, EidolonEprOpaqueId behavior) {
    for (size_t index = 0; index < runtime->behavior_state_count; ++index) {
        if (runtime->behavior_states[index].behavior == behavior) {
            return &runtime->behavior_states[index];
        }
    }
    return NULL;
}

static void test_complete_scenario_and_determinism(void) {
    EidolonPerformanceRuntime first;
    EidolonPerformanceRuntime second;
    const EidolonEprBodyProfile profile = eidolon_epr_default_body_profile();
    const EidolonEprOpaqueId gesture =
        eidolon_epr_behavior_id(EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT,
                                UINT64_C(0xc017a57));
    const EidolonEprOpaqueId settle =
        eidolon_epr_behavior_id(EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM,
                                UINT64_C(0xc017a57));
    const EidolonEprBehaviorRuntimeState *gesture_state;

    run_scenario(&first, &profile);
    run_scenario(&second, &profile);
    assert(first.control.valid);
    assert(first.plan.generation == 6U);
    assert(first.control.plan_generation == 6U);
    assert(first.control.tick == 4200);
    assert(first.control.hash == second.control.hash);
    assert(eidolon_epr_trace_hash(&first.trace) == eidolon_epr_trace_hash(&second.trace));
    assert(first.trace.count == second.trace.count);
    assert(memcmp(first.trace.records, second.trace.records,
                  first.trace.count * sizeof(first.trace.records[0])) == 0);
    assert(trace_has_transfer(&first, gesture, settle));

    gesture_state = find_runtime_state(&first, gesture);
    assert(gesture_state != NULL);
    assert(gesture_state->state == EIDOLON_EPR_BEHAVIOR_RETIRED);
    assert(gesture_state->terminal_reason == EIDOLON_EPR_TERMINAL_INTERRUPTED);
    assert(gesture_state->highest_observed_phase >= (int)EIDOLON_EPR_PHASE_PEAK);
}

static void test_stale_and_solve_failure_are_transactional(void) {
    EidolonPerformanceRuntime runtime;
    const EidolonEprBodyProfile profile = eidolon_epr_default_body_profile();
    EidolonPerformanceIntent value;
    EidolonCanonicalControl previous_control;
    uint64_t generation;

    assert(eidolon_epr_runtime_init(&runtime, 7U, &profile));
    value = intent(1U, 0U, 0, EIDOLON_EPR_MODE_ABSENT);
    assert(eidolon_epr_runtime_accept(&runtime, &value));
    assert(eidolon_epr_runtime_step(&runtime, 0));
    generation = runtime.plan.generation;
    value = intent(1U, 0U, 10, EIDOLON_EPR_MODE_LISTENING);
    assert(!eidolon_epr_runtime_accept(&runtime, &value));
    assert(runtime.plan.generation == generation);

    previous_control = runtime.control;
    eidolon_epr_runtime_inject_solve_failure(&runtime);
    assert(!eidolon_epr_runtime_step(&runtime, 10));
    assert(memcmp(&runtime.control, &previous_control, sizeof(previous_control)) == 0);
}

static void test_optional_capabilities_degrade_locally(void) {
    EidolonPerformanceRuntime runtime;
    EidolonEprBodyProfile profile = eidolon_epr_default_body_profile();
    EidolonPerformanceIntent value;
    const EidolonEprTick ticks[] = {400, 700};
    profile.has_eyes = false;
    profile.has_expression = false;
    assert(eidolon_epr_runtime_init(&runtime, 9U, &profile));
    value = intent(1U, 0U, 400, EIDOLON_EPR_MODE_THINKING);
    accept_and_step(&runtime, &value, ticks, 2U);
    assert(runtime.control.valid);
    assert(runtime.control.eyes_degraded);
    assert(runtime.control.expression_degraded);
    assert(runtime.control.eye_weight == 0.0F);
    assert(runtime.control.head_gaze_weight > 0.0F);
}

int main(void) {
    test_intent_validation();
    test_temporal_transaction();
    test_resource_order_independence();
    test_complete_scenario_and_determinism();
    test_stale_and_solve_failure_are_transactional();
    test_optional_capabilities_degrade_locally();
    puts("performance runtime tests passed");
    return 0;
}
