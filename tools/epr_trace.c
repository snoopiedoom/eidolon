#include "epr/performance_runtime.h"
#include "performance_fixture.h"

#include <inttypes.h>
#include <stdio.h>

static const char *reason_name(EidolonEprTraceReason reason) {
    static const char *const names[] = {
        "none",     "stale_revision",   "invalid_intent",    "temporal_conflict", "capacity",
        "selected", "preempted",        "interrupted",       "revised",           "completed",
        "settled",  "optional_missing", "invalid_candidate", "injected_failure",
    };
    if (reason < EIDOLON_EPR_REASON_NONE || reason > EIDOLON_EPR_REASON_INJECTED_FAILURE) {
        return "unknown";
    }
    return names[(size_t)reason];
}

static const char *behavior_kind_name(EidolonEprBehaviorKind kind) {
    static const char *const names[] = {
        "idle",
        "posture.attentive",
        "posture.thinking",
        "posture.responding",
        "posture.guarded",
        "gaze.attention",
        "gaze.response",
        "gaze.interrupted",
        "expression.neutral",
        "expression.focused",
        "gesture.contrast.right",
        "settle.right_arm",
    };
    if (kind < EIDOLON_EPR_BEHAVIOR_IDLE || kind > EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM) {
        return "unknown";
    }
    return names[(size_t)kind];
}

static const char *behavior_name(const EidolonBehaviorPlan *plan, EidolonEprOpaqueId behavior) {
    const EidolonEprBehaviorUnit *unit = eidolon_epr_plan_find(plan, behavior);
    return unit != NULL ? behavior_kind_name(unit->kind) : "none";
}

static const char *resource_name(uint32_t resource) {
    static const char *const names[] = {
        "torso", "head", "eyes", "face_expression", "left_arm_chain", "right_arm_chain",
    };
    return resource < (uint32_t)EIDOLON_EPR_RESOURCE_COUNT ? names[resource] : "none";
}

static const char *record_resource_name(const EidolonEprTraceRecord *record) {
    switch (record->event) {
    case EIDOLON_EPR_TRACE_RESOURCE_GRANTED:
    case EIDOLON_EPR_TRACE_RESOURCE_DENIED:
    case EIDOLON_EPR_TRACE_RESOURCE_TRANSFERRED:
    case EIDOLON_EPR_TRACE_RESOURCE_RELEASED:
    case EIDOLON_EPR_TRACE_CAPABILITY_DEGRADED:
        return resource_name(record->resource);
    default:
        return "none";
    }
}

static const char *value_name(const EidolonEprTraceRecord *record) {
    static const char *const modes[] = {
        "absent", "listening", "thinking", "responding", "interrupted", "completed", "errored",
    };
    static const char *const states[] = {
        "proposed", "scheduled", "committed", "executing", "retired",
    };
    static const char *const phases[] = {
        "preparation", "onset", "peak", "recovery", "completion", "interrupt", "settle",
    };
    static const char *const claims[] = {
        "base",
        "additive",
        "cooperative",
        "override",
    };
    switch (record->event) {
    case EIDOLON_EPR_TRACE_INTENT_ACCEPTED:
        return record->value <= (uint64_t)EIDOLON_EPR_MODE_ERRORED ? modes[(size_t)record->value]
                                                                   : "unknown";
    case EIDOLON_EPR_TRACE_REALIZER_SELECTED:
        return record->value <= (uint64_t)EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM
                   ? behavior_kind_name((EidolonEprBehaviorKind)record->value)
                   : "unknown";
    case EIDOLON_EPR_TRACE_BEHAVIOR_TRANSITION:
        return record->value <= (uint64_t)EIDOLON_EPR_BEHAVIOR_RETIRED
                   ? states[(size_t)record->value]
                   : "unknown";
    case EIDOLON_EPR_TRACE_ANCHOR_OBSERVED:
        return record->value <= (uint64_t)EIDOLON_EPR_PHASE_SETTLE ? phases[(size_t)record->value]
                                                                   : "unknown";
    case EIDOLON_EPR_TRACE_RESOURCE_GRANTED:
    case EIDOLON_EPR_TRACE_RESOURCE_DENIED:
        return record->value <= (uint64_t)EIDOLON_EPR_CLAIM_OVERRIDE ? claims[(size_t)record->value]
                                                                     : "unknown";
    default:
        return "scalar";
    }
}

static void print_trace(const EidolonPerformanceRuntime *runtime) {
    const EidolonEprTrace *trace = eidolon_epr_runtime_trace(runtime);
    const EidolonBehaviorPlan *plan = eidolon_epr_runtime_plan(runtime);
    for (size_t index = 0; index < trace->count; ++index) {
        const EidolonEprTraceRecord *record = eidolon_epr_trace_record(trace, index);
        printf("{\"v\":%u,\"seq\":%" PRIu64 ",\"tick\":%" PRId64 ",\"intent\":%" PRIu64
               ",\"plan\":%" PRIu64 ",\"source\":\"%016" PRIx64 "\",\"session\":\"%016" PRIx64
               "\",\"turn\":\"%016" PRIx64 "\",\"response\":\"%016" PRIx64
               "\",\"message\":\"%016" PRIx64 "\",\"interaction\":\"%016" PRIx64
               "\",\"truth_revision\":%" PRIu64
               ",\"event\":\"%s\",\"reason\":\"%s\",\"behavior\":\"%s\""
               ",\"behavior_id\":\"%016" PRIx64 "\",\"cause\":\"%016" PRIx64
               "\",\"resource\":\"%s\",\"value\":%" PRIu64
               ",\"value_name\":\"%s\",\"control\":\"%016" PRIx64 "\"}\n",
               record->version, record->sequence, record->tick, record->intent_revision,
               record->plan_generation, record->provenance.source, record->provenance.session,
               record->provenance.turn, record->provenance.response, record->provenance.message,
               record->provenance.interaction, record->provenance.truth_revision,
               eidolon_epr_trace_event_name(record->event), reason_name(record->reason),
               behavior_name(plan, record->behavior), record->behavior, record->cause,
               record_resource_name(record), record->value, value_name(record),
               record->control_hash);
    }
    printf("{\"summary\":{\"records\":%zu,\"dropped\":%" PRIu64 ",\"trace_hash\":\"%016" PRIx64
           "\",\"control_hash\":\"%016" PRIx64 "\",\"control_revision\":%" PRIu64
           ",\"plan\":%" PRIu64 "}}\n",
           trace->count, trace->dropped, eidolon_epr_trace_hash(trace), runtime->control.hash,
           runtime->control.revision, runtime->plan.generation);
}

int main(void) {
    EidolonPerformanceRuntime runtime;
    EidolonPerformanceFixture fixture;
    const EidolonEprBodyProfile profile = eidolon_epr_default_body_profile();
    if (!eidolon_epr_runtime_init(&runtime, UINT64_C(0x51eed), &profile)) {
        fputs("could not initialize EPR trace fixture\n", stderr);
        return 1;
    }
    eidolon_performance_fixture_init(&fixture);
    for (uint64_t now_ms = 0U; now_ms <= 4200U; now_ms += 20U) {
        if (!eidolon_performance_fixture_update(&fixture, &runtime, now_ms)) {
            fputs("EPR trace fixture failed\n", stderr);
            return 1;
        }
    }
    print_trace(&runtime);
    return runtime.trace.dropped == 0U && runtime.plan.generation == 6U ? 0 : 1;
}
