#ifndef EIDOLON_EPR_BEHAVIOR_PLAN_H
#define EIDOLON_EPR_BEHAVIOR_PLAN_H

#include "epr/body_resources.h"
#include "epr/performance_intent.h"
#include "epr/temporal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_BEHAVIOR_CAPACITY 24U
#define EIDOLON_EPR_BEHAVIOR_PHASE_COUNT 7U

typedef enum EidolonEprBehaviorKind {
    EIDOLON_EPR_BEHAVIOR_IDLE = 0,
    EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE,
    EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING,
    EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING,
    EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED,
    EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION,
    EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE,
    EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED,
    EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT,
    EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM
} EidolonEprBehaviorKind;

typedef enum EidolonEprBehaviorPhase {
    EIDOLON_EPR_PHASE_PREPARATION = 0,
    EIDOLON_EPR_PHASE_ONSET,
    EIDOLON_EPR_PHASE_PEAK,
    EIDOLON_EPR_PHASE_RECOVERY,
    EIDOLON_EPR_PHASE_COMPLETION,
    EIDOLON_EPR_PHASE_INTERRUPT,
    EIDOLON_EPR_PHASE_SETTLE
} EidolonEprBehaviorPhase;

typedef enum EidolonEprBehaviorState {
    EIDOLON_EPR_BEHAVIOR_PROPOSED = 0,
    EIDOLON_EPR_BEHAVIOR_SCHEDULED,
    EIDOLON_EPR_BEHAVIOR_COMMITTED,
    EIDOLON_EPR_BEHAVIOR_EXECUTING,
    EIDOLON_EPR_BEHAVIOR_RETIRED
} EidolonEprBehaviorState;

typedef enum EidolonEprTerminalReason {
    EIDOLON_EPR_TERMINAL_NONE = 0,
    EIDOLON_EPR_TERMINAL_COMPLETED,
    EIDOLON_EPR_TERMINAL_INTERRUPTED,
    EIDOLON_EPR_TERMINAL_REVISED,
    EIDOLON_EPR_TERMINAL_DENIED,
    EIDOLON_EPR_TERMINAL_FAILED
} EidolonEprTerminalReason;

typedef struct EidolonEprBehaviorUnit {
    EidolonEprOpaqueId id;
    EidolonEprBehaviorKind kind;
    EidolonEprOpaqueId cause;
    uint64_t created_generation;
    uint16_t priority;
    uint16_t urgency;
    EidolonEprTick phase_ticks[EIDOLON_EPR_BEHAVIOR_PHASE_COUNT];
    bool has_phase[EIDOLON_EPR_BEHAVIOR_PHASE_COUNT];
    bool retired;
    EidolonEprTerminalReason terminal_reason;
} EidolonEprBehaviorUnit;

typedef struct EidolonBehaviorPlan {
    uint64_t generation;
    uint64_t predecessor_generation;
    uint64_t intent_revision;
    EidolonEprOperationalMode mode;
    EidolonEprBehaviorUnit behaviors[EIDOLON_EPR_BEHAVIOR_CAPACITY];
    size_t behavior_count;
    EidolonEprResourceClaim claims[EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY];
    size_t claim_count;
    EidolonEprTemporalNetwork temporal;
} EidolonBehaviorPlan;

void eidolon_epr_plan_init(EidolonBehaviorPlan *plan);
bool eidolon_epr_plan_apply(const EidolonBehaviorPlan *previous,
                            const EidolonPerformanceIntent *intent, EidolonBehaviorPlan *candidate);
const EidolonEprBehaviorUnit *eidolon_epr_plan_find(const EidolonBehaviorPlan *plan,
                                                    EidolonEprOpaqueId behavior);
EidolonEprOpaqueId eidolon_epr_behavior_id(EidolonEprBehaviorKind kind,
                                           EidolonEprOpaqueId cause);
EidolonEprAnchorId eidolon_epr_behavior_anchor_id(EidolonEprOpaqueId behavior,
                                                  EidolonEprBehaviorPhase phase);

#endif
