#include "epr/behavior_plan.h"

#include <limits.h>
#include <string.h>

#define EIDOLON_EPR_FOREVER (INT64_MAX / 8)

static EidolonEprBehaviorUnit *find_mutable(EidolonBehaviorPlan *plan,
                                            EidolonEprOpaqueId behavior) {
    for (size_t index = 0; index < plan->behavior_count; ++index) {
        if (plan->behaviors[index].id == behavior) {
            return &plan->behaviors[index];
        }
    }
    return NULL;
}

static EidolonEprBehaviorUnit *find_active(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind,
                                           EidolonEprOpaqueId cause) {
    for (size_t index = 0; index < plan->behavior_count; ++index) {
        EidolonEprBehaviorUnit *behavior = &plan->behaviors[index];
        if (behavior->kind == kind && behavior->cause == cause && !behavior->retired) {
            return behavior;
        }
    }
    return NULL;
}

static bool has_retired(const EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind,
                        EidolonEprOpaqueId cause) {
    for (size_t index = 0; index < plan->behavior_count; ++index) {
        const EidolonEprBehaviorUnit *behavior = &plan->behaviors[index];
        if (behavior->kind == kind && behavior->cause == cause && behavior->retired) {
            return true;
        }
    }
    return false;
}

EidolonEprOpaqueId eidolon_epr_behavior_id(EidolonEprBehaviorKind kind, EidolonEprOpaqueId cause) {
    const uint64_t kind_part = ((uint64_t)kind + 1U) << 56U;
    return kind_part | (cause & UINT64_C(0x00ffffffffffffff));
}

EidolonEprAnchorId eidolon_epr_behavior_anchor_id(EidolonEprOpaqueId behavior,
                                                  EidolonEprBehaviorPhase phase) {
    uint64_t value = behavior ^ (UINT64_C(0x9e3779b97f4a7c15) +
                                 ((uint64_t)phase + 1U) * UINT64_C(0x100000001b3));
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31U;
    return value == 0U ? 1U : value;
}

void eidolon_epr_plan_init(EidolonBehaviorPlan *plan) {
    if (plan != NULL) {
        memset(plan, 0, sizeof(*plan));
        eidolon_epr_temporal_init(&plan->temporal);
    }
}

static EidolonEprBehaviorUnit *add_behavior(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind,
                                            EidolonEprOpaqueId cause, uint16_t priority,
                                            uint16_t urgency) {
    EidolonEprOpaqueId id = eidolon_epr_behavior_id(kind, cause);
    EidolonEprBehaviorUnit *behavior = find_active(plan, kind, cause);
    if (behavior != NULL) {
        behavior->priority = priority;
        behavior->urgency = urgency;
        return behavior;
    }
    for (uint64_t attempt = 0U; find_mutable(plan, id) != NULL; ++attempt) {
        const uint64_t occurrence =
            cause + (plan->generation + attempt) * UINT64_C(0x9e3779b97f4a7c15);
        id = eidolon_epr_behavior_id(kind, occurrence);
    }
    if (plan->behavior_count >= EIDOLON_EPR_BEHAVIOR_CAPACITY) {
        return NULL;
    }
    behavior = &plan->behaviors[plan->behavior_count];
    memset(behavior, 0, sizeof(*behavior));
    behavior->id = id;
    behavior->kind = kind;
    behavior->cause = cause;
    behavior->created_generation = plan->generation;
    behavior->priority = priority;
    behavior->urgency = urgency;
    plan->behavior_count += 1U;
    return behavior;
}

static void retire_kind(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind,
                        EidolonEprTerminalReason reason) {
    for (size_t index = 0; index < plan->behavior_count; ++index) {
        EidolonEprBehaviorUnit *behavior = &plan->behaviors[index];
        if (behavior->kind == kind && !behavior->retired) {
            behavior->retired = true;
            behavior->terminal_reason = reason;
        }
    }
}

static bool set_phase(EidolonBehaviorPlan *plan, EidolonEprBehaviorUnit *behavior,
                      EidolonEprBehaviorPhase phase, EidolonEprTick tick, bool observed) {
    const EidolonEprAnchorId anchor = eidolon_epr_behavior_anchor_id(behavior->id, phase);
    behavior->phase_ticks[(size_t)phase] = tick;
    behavior->has_phase[(size_t)phase] = true;
    return eidolon_epr_temporal_set_tick(&plan->temporal, anchor, tick, observed);
}

static bool constrain(EidolonBehaviorPlan *plan, const EidolonEprBehaviorUnit *behavior,
                      EidolonEprBehaviorPhase from, EidolonEprBehaviorPhase to,
                      EidolonEprTick minimum, EidolonEprTick maximum) {
    const EidolonEprTemporalConstraint constraint = {
        .from = eidolon_epr_behavior_anchor_id(behavior->id, from),
        .to = eidolon_epr_behavior_anchor_id(behavior->id, to),
        .minimum = minimum,
        .maximum = maximum,
    };
    return eidolon_epr_temporal_add_constraint(&plan->temporal, constraint);
}

static bool add_claim(EidolonBehaviorPlan *plan, const EidolonEprBehaviorUnit *behavior,
                      EidolonEprBodyResource resource, EidolonEprClaimMode mode,
                      EidolonEprTick start, EidolonEprTick end, uint16_t rule,
                      uint16_t committed_phase) {
    EidolonEprResourceClaim *claim;
    if (plan->claim_count >= EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY || start > end) {
        return false;
    }
    claim = &plan->claims[plan->claim_count];
    memset(claim, 0, sizeof(*claim));
    claim->behavior = behavior->id;
    claim->plan_generation = plan->generation;
    claim->resource = resource;
    claim->mode = mode;
    claim->start_tick = start;
    claim->end_tick = end;
    claim->rank.urgency = behavior->urgency;
    claim->rank.priority = behavior->priority;
    claim->rank.committed_phase = committed_phase;
    claim->rank.anchor_tick = start;
    claim->rank.provenance_class = 1U;
    claim->rank.behavior = behavior->id;
    claim->composition_rule = rule;
    claim->preemptible = true;
    plan->claim_count += 1U;
    return true;
}

static bool add_idle(EidolonBehaviorPlan *plan) {
    EidolonEprBehaviorUnit *idle = add_behavior(plan, EIDOLON_EPR_BEHAVIOR_IDLE, 1U, 10U, 0U);
    if (idle == NULL) {
        return false;
    }
    return add_claim(plan, idle, EIDOLON_EPR_RESOURCE_TORSO, EIDOLON_EPR_CLAIM_ADDITIVE, 0,
                     EIDOLON_EPR_FOREVER, 1U, 1U) &&
           add_claim(plan, idle, EIDOLON_EPR_RESOURCE_HEAD, EIDOLON_EPR_CLAIM_ADDITIVE, 0,
                     EIDOLON_EPR_FOREVER, 1U, 1U);
}

static bool add_posture(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind,
                        EidolonEprTick start, uint16_t priority, uint16_t urgency) {
    EidolonEprBehaviorUnit *posture = add_behavior(plan, kind, 1U, priority, urgency);
    if (posture == NULL) {
        return false;
    }
    if (!posture->has_phase[EIDOLON_EPR_PHASE_ONSET] &&
        !set_phase(plan, posture, EIDOLON_EPR_PHASE_ONSET, start, true)) {
        return false;
    }
    start = posture->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
    return add_claim(plan, posture, EIDOLON_EPR_RESOURCE_TORSO, EIDOLON_EPR_CLAIM_BASE, start,
                     EIDOLON_EPR_FOREVER, 0U, 2U) &&
           add_claim(plan, posture, EIDOLON_EPR_RESOURCE_HEAD, EIDOLON_EPR_CLAIM_BASE, start,
                     EIDOLON_EPR_FOREVER, 0U, 2U) &&
           add_claim(plan, posture, EIDOLON_EPR_RESOURCE_LEFT_ARM_CHAIN, EIDOLON_EPR_CLAIM_BASE,
                     start, EIDOLON_EPR_FOREVER, 0U, 2U) &&
           add_claim(plan, posture, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN, EIDOLON_EPR_CLAIM_BASE,
                     start, EIDOLON_EPR_FOREVER, 0U, 2U);
}

static bool add_gaze(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind, EidolonEprTick start,
                     uint16_t priority, uint16_t urgency) {
    EidolonEprBehaviorUnit *gaze = add_behavior(plan, kind, 1U, priority, urgency);
    if (gaze == NULL) {
        return false;
    }
    if (!gaze->has_phase[EIDOLON_EPR_PHASE_ONSET] &&
        !set_phase(plan, gaze, EIDOLON_EPR_PHASE_ONSET, start, true)) {
        return false;
    }
    start = gaze->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
    return add_claim(plan, gaze, EIDOLON_EPR_RESOURCE_EYES, EIDOLON_EPR_CLAIM_BASE, start,
                     EIDOLON_EPR_FOREVER, 0U, 2U) &&
           add_claim(plan, gaze, EIDOLON_EPR_RESOURCE_HEAD, EIDOLON_EPR_CLAIM_COOPERATIVE,
                     start + 80, EIDOLON_EPR_FOREVER, 2U, 2U);
}

static bool add_expression(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind kind,
                           EidolonEprTick start, uint16_t priority, uint16_t urgency) {
    EidolonEprBehaviorUnit *expression = add_behavior(plan, kind, 1U, priority, urgency);
    if (expression == NULL) {
        return false;
    }
    if (!expression->has_phase[EIDOLON_EPR_PHASE_ONSET] &&
        !set_phase(plan, expression, EIDOLON_EPR_PHASE_ONSET, start, true)) {
        return false;
    }
    start = expression->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
    return add_claim(plan, expression, EIDOLON_EPR_RESOURCE_FACE_EXPRESSION, EIDOLON_EPR_CLAIM_BASE,
                     start, EIDOLON_EPR_FOREVER, 0U, 2U);
}

static const EidolonEprSemanticBeat *stable_contrast(const EidolonPerformanceIntent *intent) {
    for (size_t index = 0; index < intent->beat_count; ++index) {
        const EidolonEprSemanticBeat *beat = &intent->beats[index];
        if (beat->kind == EIDOLON_EPR_BEAT_CONTRAST &&
            beat->stability >= EIDOLON_EPR_EVIDENCE_STABLE_PREFIX) {
            return beat;
        }
    }
    return NULL;
}

static bool add_gesture(EidolonBehaviorPlan *plan, const EidolonEprSemanticBeat *beat,
                        uint16_t urgency) {
    if (find_active(plan, EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT, beat->id) == NULL &&
        has_retired(plan, EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT, beat->id)) {
        return true;
    }
    EidolonEprBehaviorUnit *gesture =
        add_behavior(plan, EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT, beat->id, 700U, urgency);
    const EidolonEprTick preparation = beat->anchor_tick - 310;
    const EidolonEprTick onset = beat->anchor_tick - 100;
    const EidolonEprTick recovery = beat->anchor_tick + 60;
    const EidolonEprTick completion = beat->anchor_tick + 500;
    if (gesture == NULL ||
        !set_phase(plan, gesture, EIDOLON_EPR_PHASE_PREPARATION, preparation, false) ||
        !set_phase(plan, gesture, EIDOLON_EPR_PHASE_ONSET, onset, false) ||
        !set_phase(plan, gesture, EIDOLON_EPR_PHASE_PEAK, beat->anchor_tick, false) ||
        !set_phase(plan, gesture, EIDOLON_EPR_PHASE_RECOVERY, recovery, false) ||
        !set_phase(plan, gesture, EIDOLON_EPR_PHASE_COMPLETION, completion, false) ||
        !constrain(plan, gesture, EIDOLON_EPR_PHASE_PREPARATION, EIDOLON_EPR_PHASE_ONSET, 210,
                   210) ||
        !constrain(plan, gesture, EIDOLON_EPR_PHASE_ONSET, EIDOLON_EPR_PHASE_PEAK, 100, 100) ||
        !constrain(plan, gesture, EIDOLON_EPR_PHASE_PEAK, EIDOLON_EPR_PHASE_RECOVERY, 60, 60) ||
        !constrain(plan, gesture, EIDOLON_EPR_PHASE_RECOVERY, EIDOLON_EPR_PHASE_COMPLETION, 440,
                   440)) {
        return false;
    }
    return add_claim(plan, gesture, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN,
                     EIDOLON_EPR_CLAIM_OVERRIDE, preparation, completion, 0U, 3U);
}

static bool add_settle(EidolonBehaviorPlan *plan, EidolonEprOpaqueId cause, EidolonEprTick start) {
    EidolonEprBehaviorUnit *settle =
        add_behavior(plan, EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM, cause, 900U, 1000U);
    if (settle == NULL || !set_phase(plan, settle, EIDOLON_EPR_PHASE_INTERRUPT, start, true) ||
        !set_phase(plan, settle, EIDOLON_EPR_PHASE_SETTLE, start + 320, false) ||
        !constrain(plan, settle, EIDOLON_EPR_PHASE_INTERRUPT, EIDOLON_EPR_PHASE_SETTLE, 320, 320)) {
        return false;
    }
    return add_claim(plan, settle, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN, EIDOLON_EPR_CLAIM_OVERRIDE,
                     start, start + 320, 0U, 4U);
}

static void retire_postures(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind except) {
    const EidolonEprBehaviorKind kinds[] = {
        EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE,
        EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING,
        EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING,
        EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED,
    };
    for (size_t index = 0; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        if (kinds[index] != except) {
            retire_kind(plan, kinds[index], EIDOLON_EPR_TERMINAL_REVISED);
        }
    }
}

static void retire_gazes(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind except) {
    const EidolonEprBehaviorKind kinds[] = {
        EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION,
        EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE,
        EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED,
    };
    for (size_t index = 0; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        if (kinds[index] != except) {
            retire_kind(plan, kinds[index], EIDOLON_EPR_TERMINAL_REVISED);
        }
    }
}

static void retire_expressions(EidolonBehaviorPlan *plan, EidolonEprBehaviorKind except) {
    const EidolonEprBehaviorKind kinds[] = {
        EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL,
        EIDOLON_EPR_BEHAVIOR_EXPRESSION_FOCUSED,
    };
    for (size_t index = 0; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        if (kinds[index] != except) {
            retire_kind(plan, kinds[index], EIDOLON_EPR_TERMINAL_REVISED);
        }
    }
}

static bool rebuild_claims(EidolonBehaviorPlan *plan, const EidolonPerformanceIntent *intent) {
    const EidolonEprSemanticBeat *contrast = stable_contrast(intent);
    plan->claim_count = 0U;
    if (!add_idle(plan)) {
        return false;
    }
    switch (intent->mode) {
    case EIDOLON_EPR_MODE_ABSENT:
        return add_expression(plan, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL, intent->observed_tick,
                              100U, intent->urgency);
    case EIDOLON_EPR_MODE_LISTENING:
        return add_posture(plan, EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE, intent->observed_tick,
                           300U, intent->urgency) &&
               add_gaze(plan, EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION, intent->observed_tick, 400U,
                        intent->urgency) &&
               add_expression(plan, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL, intent->observed_tick,
                              100U, intent->urgency);
    case EIDOLON_EPR_MODE_THINKING:
        return add_posture(plan, EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING, intent->observed_tick, 350U,
                           intent->urgency) &&
               add_gaze(plan, EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION, intent->observed_tick, 400U,
                        intent->urgency) &&
               add_expression(plan, EIDOLON_EPR_BEHAVIOR_EXPRESSION_FOCUSED, intent->observed_tick,
                              500U, intent->urgency);
    case EIDOLON_EPR_MODE_RESPONDING:
        if (!add_posture(plan, EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING, intent->observed_tick, 450U,
                         intent->urgency) ||
            !add_gaze(plan, EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE, intent->observed_tick, 500U,
                      intent->urgency) ||
            !add_expression(plan, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL, intent->observed_tick,
                            100U, intent->urgency)) {
            return false;
        }
        return contrast == NULL || add_gesture(plan, contrast, intent->urgency);
    case EIDOLON_EPR_MODE_INTERRUPTED:
        if (!add_posture(plan, EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED, intent->observed_tick, 800U,
                         1000U) ||
            !add_gaze(plan, EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED, intent->observed_tick, 900U,
                      1000U) ||
            !add_expression(plan, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL, intent->observed_tick,
                            100U, 1000U)) {
            return false;
        }
        return contrast == NULL || add_settle(plan, contrast->id, intent->observed_tick);
    case EIDOLON_EPR_MODE_COMPLETED:
    case EIDOLON_EPR_MODE_ERRORED:
        return add_posture(plan, EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE, intent->observed_tick,
                           300U, intent->urgency) &&
               add_gaze(plan, EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION, intent->observed_tick, 400U,
                        intent->urgency) &&
               add_expression(plan, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL, intent->observed_tick,
                              100U, intent->urgency);
    }
    return false;
}

bool eidolon_epr_plan_apply(const EidolonBehaviorPlan *previous,
                            const EidolonPerformanceIntent *intent,
                            EidolonBehaviorPlan *candidate) {
    if (intent == NULL || candidate == NULL || !eidolon_epr_intent_validate(intent)) {
        return false;
    }
    if (previous == NULL) {
        eidolon_epr_plan_init(candidate);
    } else {
        *candidate = *previous;
    }
    candidate->predecessor_generation = previous == NULL ? 0U : previous->generation;
    candidate->generation = candidate->predecessor_generation + 1U;
    candidate->intent_revision = intent->revision;
    candidate->mode = intent->mode;

    switch (intent->mode) {
    case EIDOLON_EPR_MODE_LISTENING:
        retire_postures(candidate, EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE);
        retire_gazes(candidate, EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION);
        retire_expressions(candidate, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL);
        break;
    case EIDOLON_EPR_MODE_THINKING:
        retire_postures(candidate, EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING);
        retire_gazes(candidate, EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION);
        retire_expressions(candidate, EIDOLON_EPR_BEHAVIOR_EXPRESSION_FOCUSED);
        break;
    case EIDOLON_EPR_MODE_RESPONDING:
        retire_postures(candidate, EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING);
        retire_gazes(candidate, EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE);
        retire_expressions(candidate, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL);
        break;
    case EIDOLON_EPR_MODE_INTERRUPTED:
        retire_postures(candidate, EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED);
        retire_gazes(candidate, EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED);
        retire_expressions(candidate, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL);
        retire_kind(candidate, EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT,
                    EIDOLON_EPR_TERMINAL_INTERRUPTED);
        break;
    case EIDOLON_EPR_MODE_ABSENT:
    case EIDOLON_EPR_MODE_COMPLETED:
    case EIDOLON_EPR_MODE_ERRORED:
        retire_postures(candidate, EIDOLON_EPR_BEHAVIOR_IDLE);
        retire_gazes(candidate, EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION);
        retire_expressions(candidate, EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL);
        break;
    }
    if (!rebuild_claims(candidate, intent) ||
        !eidolon_epr_temporal_validate(&candidate->temporal)) {
        return false;
    }
    return true;
}

const EidolonEprBehaviorUnit *eidolon_epr_plan_find(const EidolonBehaviorPlan *plan,
                                                    EidolonEprOpaqueId behavior) {
    if (plan == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < plan->behavior_count; ++index) {
        if (plan->behaviors[index].id == behavior) {
            return &plan->behaviors[index];
        }
    }
    return NULL;
}
