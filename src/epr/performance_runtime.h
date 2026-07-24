#ifndef EIDOLON_EPR_PERFORMANCE_RUNTIME_H
#define EIDOLON_EPR_PERFORMANCE_RUNTIME_H

#include "epr/behavior_plan.h"
#include "epr/body_resources.h"
#include "epr/canonical_control.h"
#include "epr/performance_intent.h"
#include "epr/performance_trace.h"
#include "epr/realization_program.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    EidolonRealizationProgramSet programs;
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
    uint64_t projection_pending_after_revision;
} EidolonPerformanceRuntime;

EidolonEprBodyProfile eidolon_epr_default_body_profile(void);
bool eidolon_epr_runtime_init(EidolonPerformanceRuntime *runtime, uint64_t seed,
                              const EidolonEprBodyProfile *body);
bool eidolon_epr_runtime_accept(EidolonPerformanceRuntime *runtime,
                                const EidolonPerformanceIntent *intent);
bool eidolon_epr_runtime_step(EidolonPerformanceRuntime *runtime, EidolonEprTick tick);
void eidolon_epr_runtime_inject_solve_failure(EidolonPerformanceRuntime *runtime);
void eidolon_epr_runtime_note_projection(EidolonPerformanceRuntime *runtime,
                                         uint64_t control_revision, bool committed,
                                         EidolonEprTraceReason reason);
const EidolonCanonicalControl *
eidolon_epr_runtime_control(const EidolonPerformanceRuntime *runtime);
const EidolonBehaviorPlan *eidolon_epr_runtime_plan(const EidolonPerformanceRuntime *runtime);
const EidolonRealizationProgramSet *
eidolon_epr_runtime_programs(const EidolonPerformanceRuntime *runtime);
const EidolonEprTrace *eidolon_epr_runtime_trace(const EidolonPerformanceRuntime *runtime);

#endif
