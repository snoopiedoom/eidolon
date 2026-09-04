#include "epr/motion_execution.h"

#include <float.h>
#include <math.h>
#include <string.h>

typedef enum TimingBuildStatus {
    TIMING_BUILD_OK = 0,
    TIMING_BUILD_NOT_ACTIVE,
    TIMING_BUILD_INVALID
} TimingBuildStatus;

static EidolonEprMotionExecutionResult result(EidolonEprMotionExecutionStatus status,
                                              EidolonEprMotionCatalogStatus catalog_status) {
    const EidolonEprMotionExecutionResult value = {
        .status = status,
        .catalog_status = catalog_status,
    };
    return value;
}

static EidolonEprMotionExecutionResult execution_result(EidolonEprMotionExecutionStatus status) {
    return result(status, EIDOLON_EPR_MOTION_CATALOG_OK);
}

const char *eidolon_epr_motion_execution_status_name(EidolonEprMotionExecutionStatus status) {
    switch (status) {
    case EIDOLON_EPR_MOTION_EXECUTION_OK:
        return "ok";
    case EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED:
        return "not_required";
    case EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE:
        return "not_active";
    case EIDOLON_EPR_MOTION_EXECUTION_NO_GRANTS:
        return "no_grants";
    case EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT:
        return "invalid_argument";
    case EIDOLON_EPR_MOTION_EXECUTION_INVALID_PROGRAM:
        return "invalid_program";
    case EIDOLON_EPR_MOTION_EXECUTION_INVALID_PHASES:
        return "invalid_phases";
    case EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION:
        return "invalid_resolution";
    case EIDOLON_EPR_MOTION_EXECUTION_CATALOG_FAILED:
        return "catalog_failed";
    case EIDOLON_EPR_MOTION_EXECUTION_COMPOSE_FAILED:
        return "compose_failed";
    }
    return "unknown";
}

static uint32_t valid_resource_mask(void) {
    return (UINT32_C(1) << (uint32_t)EIDOLON_EPR_RESOURCE_COUNT) - UINT32_C(1);
}

static uint32_t phase_mask(const EidolonRealizationProgram *program) {
    uint32_t mask = 0U;
    for (size_t phase = 0U; phase < EIDOLON_EPR_BEHAVIOR_PHASE_COUNT; ++phase) {
        if (program->has_phase[phase]) {
            mask |= UINT32_C(1) << (uint32_t)phase;
        }
    }
    return mask;
}

static bool program_semantics(EidolonEprBehaviorKind kind, EidolonEprModality *modality,
                              EidolonEprMotionGeneratorId *generator,
                              EidolonEprMotionTakeoverPolicy *takeover, uint32_t *resources) {
    const uint32_t torso = UINT32_C(1) << EIDOLON_EPR_RESOURCE_TORSO;
    const uint32_t head = UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD;
    const uint32_t eyes = UINT32_C(1) << EIDOLON_EPR_RESOURCE_EYES;
    const uint32_t expression = UINT32_C(1) << EIDOLON_EPR_RESOURCE_FACE_EXPRESSION;
    const uint32_t right_arm = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    *takeover = EIDOLON_EPR_MOTION_TAKEOVER_BASE;
    switch (kind) {
    case EIDOLON_EPR_BEHAVIOR_IDLE:
        *modality = EIDOLON_EPR_MODALITY_IDLE;
        *generator = EIDOLON_EPR_MOTION_IDLE_NEUTRAL;
        *takeover = EIDOLON_EPR_MOTION_TAKEOVER_ADDITIVE;
        *resources = torso | head;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE:
        *modality = EIDOLON_EPR_MODALITY_POSTURE;
        *generator = EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE;
        *resources = torso | head | right_arm;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING:
        *modality = EIDOLON_EPR_MODALITY_POSTURE;
        *generator = EIDOLON_EPR_MOTION_POSTURE_THINKING;
        *resources = torso | head | right_arm;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING:
        *modality = EIDOLON_EPR_MODALITY_POSTURE;
        *generator = EIDOLON_EPR_MOTION_POSTURE_RESPONDING;
        *resources = torso | head | right_arm;
        return true;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED:
        *modality = EIDOLON_EPR_MODALITY_POSTURE;
        *generator = EIDOLON_EPR_MOTION_POSTURE_INTERRUPTED_GUARDED;
        *resources = torso | head | right_arm;
        return true;
    case EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION:
    case EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE:
    case EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED:
        *modality = EIDOLON_EPR_MODALITY_GAZE;
        *generator = EIDOLON_EPR_MOTION_GENERATOR_NONE;
        *takeover = EIDOLON_EPR_MOTION_TAKEOVER_NONE;
        *resources = eyes | head;
        return true;
    case EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL:
    case EIDOLON_EPR_BEHAVIOR_EXPRESSION_FOCUSED:
        *modality = EIDOLON_EPR_MODALITY_EXPRESSION;
        *generator = EIDOLON_EPR_MOTION_GENERATOR_NONE;
        *takeover = EIDOLON_EPR_MOTION_TAKEOVER_NONE;
        *resources = expression;
        return true;
    case EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT:
        *modality = EIDOLON_EPR_MODALITY_GESTURE;
        *generator = EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT;
        *takeover = EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE;
        *resources = right_arm;
        return true;
    case EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM:
        *modality = EIDOLON_EPR_MODALITY_SETTLE;
        *generator = EIDOLON_EPR_MOTION_GENERATOR_NONE;
        *takeover = EIDOLON_EPR_MOTION_TAKEOVER_NONE;
        *resources = right_arm;
        return true;
    }
    return false;
}

static bool program_valid(const EidolonRealizationProgram *program) {
    EidolonEprModality expected_modality;
    EidolonEprMotionGeneratorId expected_generator;
    EidolonEprMotionTakeoverPolicy expected_takeover;
    uint32_t expected_resources;
    if (program == NULL || program->version != EIDOLON_EPR_PROGRAM_VERSION || program->id == 0U ||
        program->behavior == 0U || program->plan_generation == 0U ||
        (program->resource_mask & ~valid_resource_mask()) != 0U ||
        !eidolon_epr_motion_generator_reference_validate(&program->motion)) {
        return false;
    }
    if (!program_semantics(program->behavior_kind, &expected_modality, &expected_generator,
                           &expected_takeover, &expected_resources) ||
        program->modality != expected_modality || program->resource_mask != expected_resources ||
        program->motion.generator != expected_generator ||
        program->motion.takeover != expected_takeover) {
        return false;
    }
    if (program->motion.generator == EIDOLON_EPR_MOTION_GENERATOR_NONE) {
        return program->motion.resource_mask == 0U;
    }
    return program->resource_mask != 0U && program->motion.resource_mask == program->resource_mask;
}

static float minimum_jerk(float progress) {
    const float clamped = fminf(fmaxf(progress, 0.0F), 1.0F);
    const float square = clamped * clamped;
    const float cube = square * clamped;
    return cube * (10.0F + clamped * (-15.0F + 6.0F * clamped));
}

static float interval_progress(EidolonEprTick tick, EidolonEprTick from, EidolonEprTick to) {
    return (float)(((double)tick - (double)from) / ((double)to - (double)from));
}

static TimingBuildStatus build_timing(const EidolonRealizationProgram *program, EidolonEprTick tick,
                                      EidolonEprMotionTiming *timing) {
    const uint32_t preparation = UINT32_C(1) << EIDOLON_EPR_PHASE_PREPARATION;
    const uint32_t onset = UINT32_C(1) << EIDOLON_EPR_PHASE_ONSET;
    const uint32_t peak = UINT32_C(1) << EIDOLON_EPR_PHASE_PEAK;
    const uint32_t recovery = UINT32_C(1) << EIDOLON_EPR_PHASE_RECOVERY;
    const uint32_t completion = UINT32_C(1) << EIDOLON_EPR_PHASE_COMPLETION;
    const uint32_t interrupt = UINT32_C(1) << EIDOLON_EPR_PHASE_INTERRUPT;
    const uint32_t settle = UINT32_C(1) << EIDOLON_EPR_PHASE_SETTLE;
    const uint32_t actual_phases = phase_mask(program);
    EidolonEprMotionTiming candidate;

    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_MOTION_TIMING_VERSION;
    candidate.tick = tick;
    candidate.transition_weight = 1.0F;
    candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_STEADY;

    switch (program->behavior_kind) {
    case EIDOLON_EPR_BEHAVIOR_IDLE:
        if (actual_phases != 0U) {
            return TIMING_BUILD_INVALID;
        }
        candidate.origin_tick = 0;
        if (tick < candidate.origin_tick) {
            return TIMING_BUILD_NOT_ACTIVE;
        }
        break;
    case EIDOLON_EPR_BEHAVIOR_POSTURE_ATTENTIVE:
    case EIDOLON_EPR_BEHAVIOR_POSTURE_THINKING:
    case EIDOLON_EPR_BEHAVIOR_POSTURE_RESPONDING:
    case EIDOLON_EPR_BEHAVIOR_POSTURE_GUARDED: {
        float progress;
        if (actual_phases != onset || !isfinite(program->values[0]) || program->values[0] <= 0.0F) {
            return TIMING_BUILD_INVALID;
        }
        candidate.origin_tick = program->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
        if (tick < candidate.origin_tick) {
            return TIMING_BUILD_NOT_ACTIVE;
        }
        progress =
            (float)(((double)tick - (double)candidate.origin_tick) / (double)program->values[0]);
        if (progress < 1.0F) {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_ENTER;
            candidate.transition_weight = minimum_jerk(progress);
        } else {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_HOLD;
        }
        break;
    }
    case EIDOLON_EPR_BEHAVIOR_GESTURE_CONTRAST_RIGHT: {
        const EidolonEprTick preparation_tick = program->phase_ticks[EIDOLON_EPR_PHASE_PREPARATION];
        const EidolonEprTick onset_tick = program->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
        const EidolonEprTick peak_tick = program->phase_ticks[EIDOLON_EPR_PHASE_PEAK];
        const EidolonEprTick recovery_tick = program->phase_ticks[EIDOLON_EPR_PHASE_RECOVERY];
        const EidolonEprTick completion_tick = program->phase_ticks[EIDOLON_EPR_PHASE_COMPLETION];
        if (actual_phases != (preparation | onset | peak | recovery | completion) ||
            preparation_tick >= onset_tick || onset_tick >= peak_tick ||
            peak_tick >= recovery_tick || recovery_tick >= completion_tick) {
            return TIMING_BUILD_INVALID;
        }
        candidate.origin_tick = preparation_tick;
        candidate.terminal_tick = completion_tick;
        candidate.has_terminal_tick = true;
        if (tick < preparation_tick || tick > completion_tick) {
            return TIMING_BUILD_NOT_ACTIVE;
        }
        if (tick < onset_tick) {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_ENTER;
            candidate.transition_weight =
                minimum_jerk(interval_progress(tick, preparation_tick, onset_tick));
        } else if (tick <= recovery_tick) {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_HOLD;
        } else {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_EXIT;
            candidate.transition_weight =
                1.0F - minimum_jerk(interval_progress(tick, recovery_tick, completion_tick));
        }
        break;
    }
    case EIDOLON_EPR_BEHAVIOR_SETTLE_RIGHT_ARM: {
        const EidolonEprTick interrupt_tick = program->phase_ticks[EIDOLON_EPR_PHASE_INTERRUPT];
        const EidolonEprTick settle_tick = program->phase_ticks[EIDOLON_EPR_PHASE_SETTLE];
        if (actual_phases != (interrupt | settle) || interrupt_tick >= settle_tick) {
            return TIMING_BUILD_INVALID;
        }
        candidate.origin_tick = interrupt_tick;
        candidate.terminal_tick = settle_tick;
        candidate.has_terminal_tick = true;
        if (tick < interrupt_tick || tick > settle_tick) {
            return TIMING_BUILD_NOT_ACTIVE;
        }
        if (tick < settle_tick) {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_ENTER;
            candidate.transition_weight =
                minimum_jerk(interval_progress(tick, interrupt_tick, settle_tick));
        } else {
            candidate.stage = EIDOLON_EPR_MOTION_TRANSITION_HOLD;
        }
        break;
    }
    case EIDOLON_EPR_BEHAVIOR_GAZE_ATTENTION:
    case EIDOLON_EPR_BEHAVIOR_GAZE_RESPONSE:
    case EIDOLON_EPR_BEHAVIOR_GAZE_INTERRUPTED:
    case EIDOLON_EPR_BEHAVIOR_EXPRESSION_NEUTRAL:
    case EIDOLON_EPR_BEHAVIOR_EXPRESSION_FOCUSED:
        return TIMING_BUILD_INVALID;
    default:
        return TIMING_BUILD_INVALID;
    }
    *timing = candidate;
    return TIMING_BUILD_OK;
}

static bool finalize_source_time(EidolonEprMotionTiming *timing,
                                 const EidolonEprMotionBinding *binding) {
    const double elapsed_milliseconds = (double)timing->tick - (double)timing->origin_tick;
    const double source_seconds = elapsed_milliseconds * (double)binding->playback_rate / 1000.0;
    float sampled_seconds;
    if (elapsed_milliseconds < 0.0 || !isfinite(source_seconds) || source_seconds > FLT_MAX) {
        return false;
    }
    timing->source_seconds = (float)source_seconds;
    timing->normalized_source_time = 0.0F;
    if (binding->source.duration_seconds <= 0.0F) {
        return true;
    }
    sampled_seconds = timing->source_seconds;
    if (binding->source.loop) {
        sampled_seconds = fmodf(sampled_seconds, binding->source.duration_seconds);
        if (sampled_seconds < 0.0F) {
            sampled_seconds += binding->source.duration_seconds;
        }
    } else {
        sampled_seconds = fminf(sampled_seconds, binding->source.duration_seconds);
    }
    timing->normalized_source_time = sampled_seconds / binding->source.duration_seconds;
    return isfinite(timing->normalized_source_time) && timing->normalized_source_time >= 0.0F &&
           timing->normalized_source_time <= 1.0F;
}

static EidolonHumanoidPoseLayerMode layer_mode(EidolonEprMotionTakeoverPolicy takeover) {
    return takeover == EIDOLON_EPR_MOTION_TAKEOVER_ADDITIVE ? EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE
                                                            : EIDOLON_HUMANOID_POSE_LAYER_ABSOLUTE;
}

EidolonEprMotionExecutionResult eidolon_epr_motion_program_execute(
    const EidolonEprMotionCatalog *catalog, const EidolonRealizationProgram *program,
    uint32_t granted_resource_mask, EidolonEprTick tick, EidolonEprMotionExecution *execution) {
    EidolonEprMotionGeneratorReference narrowed;
    EidolonEprMotionBinding binding;
    EidolonEprMotionExecution candidate;
    EidolonEprMotionCatalogStatus catalog_status;
    TimingBuildStatus timing_status;
    if (catalog == NULL || program == NULL || execution == NULL) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT);
    }
    if (!program_valid(program)) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_PROGRAM);
    }
    if (program->motion.generator == EIDOLON_EPR_MOTION_GENERATOR_NONE) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED);
    }
    if ((granted_resource_mask & ~valid_resource_mask()) != 0U ||
        (granted_resource_mask & ~program->resource_mask) != 0U) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT);
    }
    if (granted_resource_mask == 0U) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_NO_GRANTS);
    }

    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_MOTION_EXECUTION_VERSION;
    candidate.program = program->id;
    candidate.behavior = program->behavior;
    candidate.plan_generation = program->plan_generation;
    candidate.granted_resource_mask = granted_resource_mask;
    timing_status = build_timing(program, tick, &candidate.timing);
    if (timing_status == TIMING_BUILD_NOT_ACTIVE) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE);
    }
    if (timing_status != TIMING_BUILD_OK) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_PHASES);
    }

    narrowed = program->motion;
    narrowed.resource_mask = granted_resource_mask;
    if (!eidolon_epr_resource_mask_humanoid_channels(granted_resource_mask,
                                                     &narrowed.humanoid_rotation_mask,
                                                     &narrowed.owns_hips_translation) ||
        !eidolon_epr_motion_generator_reference_validate(&narrowed)) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_PROGRAM);
    }
    catalog_status = eidolon_epr_motion_catalog_resolve(catalog, &narrowed, &binding);
    if (catalog_status != EIDOLON_EPR_MOTION_CATALOG_OK) {
        return result(EIDOLON_EPR_MOTION_EXECUTION_CATALOG_FAILED, catalog_status);
    }
    if (!finalize_source_time(&candidate.timing, &binding)) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_PHASES);
    }
    catalog_status = eidolon_epr_motion_binding_sample(&binding, candidate.timing.source_seconds,
                                                       &candidate.sample);
    if (catalog_status != EIDOLON_EPR_MOTION_CATALOG_OK) {
        return result(EIDOLON_EPR_MOTION_EXECUTION_CATALOG_FAILED, catalog_status);
    }
    candidate.layer_mode = layer_mode(candidate.sample.takeover);
    candidate.layer_weight = candidate.sample.blend_weight * candidate.timing.transition_weight;
    if (!isfinite(candidate.layer_weight) || candidate.layer_weight < 0.0F ||
        candidate.layer_weight > 1.0F) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_PROGRAM);
    }
    *execution = candidate;
    return execution_result(EIDOLON_EPR_MOTION_EXECUTION_OK);
}

static bool resolution_valid(const EidolonEprResourceResolution *resolution) {
    if (resolution == NULL || resolution->grant_count > EIDOLON_EPR_RESOURCE_GRANT_CAPACITY ||
        resolution->denied_count > EIDOLON_EPR_RESOURCE_CLAIM_CAPACITY) {
        return false;
    }
    for (size_t index = 0U; index < resolution->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &resolution->grants[index];
        if (grant->behavior == 0U || grant->resource < EIDOLON_EPR_RESOURCE_TORSO ||
            grant->resource >= EIDOLON_EPR_RESOURCE_COUNT || grant->mode < EIDOLON_EPR_CLAIM_BASE ||
            grant->mode > EIDOLON_EPR_CLAIM_OVERRIDE) {
            return false;
        }
        for (size_t prior = 0U; prior < index; ++prior) {
            const EidolonEprResourceGrant *other = &resolution->grants[prior];
            if (other->resource == grant->resource &&
                (other->mode == grant->mode || other->mode == EIDOLON_EPR_CLAIM_OVERRIDE ||
                 grant->mode == EIDOLON_EPR_CLAIM_OVERRIDE)) {
                return false;
            }
        }
    }
    for (size_t index = 0U; index < resolution->denied_count; ++index) {
        const EidolonEprResourceDenial *denial = &resolution->denied[index];
        if (denial->behavior == 0U || denial->resource < EIDOLON_EPR_RESOURCE_TORSO ||
            denial->resource >= EIDOLON_EPR_RESOURCE_COUNT ||
            denial->mode < EIDOLON_EPR_CLAIM_BASE || denial->mode > EIDOLON_EPR_CLAIM_OVERRIDE) {
            return false;
        }
    }
    return true;
}

static int takeover_order(EidolonEprMotionTakeoverPolicy takeover) {
    switch (takeover) {
    case EIDOLON_EPR_MOTION_TAKEOVER_BASE:
        return 0;
    case EIDOLON_EPR_MOTION_TAKEOVER_COOPERATIVE:
        return 1;
    case EIDOLON_EPR_MOTION_TAKEOVER_ADDITIVE:
        return 2;
    case EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE:
        return 3;
    case EIDOLON_EPR_MOTION_TAKEOVER_NONE:
    case EIDOLON_EPR_MOTION_TAKEOVER_COUNT:
        return 4;
    }
    return 4;
}

static EidolonEprClaimMode claim_mode(EidolonEprMotionTakeoverPolicy takeover) {
    switch (takeover) {
    case EIDOLON_EPR_MOTION_TAKEOVER_BASE:
        return EIDOLON_EPR_CLAIM_BASE;
    case EIDOLON_EPR_MOTION_TAKEOVER_ADDITIVE:
        return EIDOLON_EPR_CLAIM_ADDITIVE;
    case EIDOLON_EPR_MOTION_TAKEOVER_COOPERATIVE:
        return EIDOLON_EPR_CLAIM_COOPERATIVE;
    case EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE:
        return EIDOLON_EPR_CLAIM_OVERRIDE;
    case EIDOLON_EPR_MOTION_TAKEOVER_NONE:
    case EIDOLON_EPR_MOTION_TAKEOVER_COUNT:
        return EIDOLON_EPR_CLAIM_BASE;
    }
    return EIDOLON_EPR_CLAIM_BASE;
}

static bool program_set_valid(const EidolonRealizationProgramSet *programs,
                              const EidolonEprResourceResolution *resolution) {
    if (programs == NULL || programs->plan_generation == 0U ||
        programs->count > EIDOLON_EPR_PROGRAM_CAPACITY || !resolution_valid(resolution)) {
        return false;
    }
    for (size_t index = 0U; index < programs->count; ++index) {
        const EidolonRealizationProgram *program = &programs->programs[index];
        if (!program_valid(program) || program->plan_generation != programs->plan_generation) {
            return false;
        }
        for (size_t prior = 0U; prior < index; ++prior) {
            if (programs->programs[prior].id == program->id ||
                programs->programs[prior].behavior == program->behavior) {
                return false;
            }
        }
    }
    for (size_t grant_index = 0U; grant_index < resolution->grant_count; ++grant_index) {
        const EidolonEprResourceGrant *grant = &resolution->grants[grant_index];
        const uint32_t bit = UINT32_C(1) << (uint32_t)grant->resource;
        const EidolonRealizationProgram *owner = NULL;
        for (size_t program_index = 0U; program_index < programs->count; ++program_index) {
            if (programs->programs[program_index].behavior == grant->behavior) {
                owner = &programs->programs[program_index];
                break;
            }
        }
        if (owner == NULL || (owner->resource_mask & bit) == 0U) {
            return false;
        }
    }
    return true;
}

static void sort_program_order(const EidolonRealizationProgramSet *programs, size_t order[],
                               size_t count) {
    for (size_t index = 1U; index < count; ++index) {
        const size_t value = order[index];
        size_t insert = index;
        while (insert > 0U) {
            const EidolonRealizationProgram *left = &programs->programs[order[insert - 1U]];
            const EidolonRealizationProgram *right = &programs->programs[value];
            const int left_order = takeover_order(left->motion.takeover);
            const int right_order = takeover_order(right->motion.takeover);
            if (left_order < right_order || (left_order == right_order && left->id < right->id)) {
                break;
            }
            order[insert] = order[insert - 1U];
            insert -= 1U;
        }
        order[insert] = value;
    }
}

static bool granted_mask(const EidolonRealizationProgram *program,
                         const EidolonEprResourceResolution *resolution, uint32_t *mask) {
    const EidolonEprClaimMode expected_mode = claim_mode(program->motion.takeover);
    uint32_t candidate = 0U;
    for (size_t index = 0U; index < resolution->grant_count; ++index) {
        const EidolonEprResourceGrant *grant = &resolution->grants[index];
        const uint32_t bit = UINT32_C(1) << (uint32_t)grant->resource;
        if (grant->behavior != program->behavior || (program->resource_mask & bit) == 0U) {
            continue;
        }
        if (program->motion.generator != EIDOLON_EPR_MOTION_GENERATOR_NONE &&
            grant->mode != expected_mode) {
            return false;
        }
        candidate |= bit;
    }
    *mask = candidate;
    return true;
}

EidolonEprMotionExecutionResult eidolon_epr_motion_programs_compose(
    const EidolonEprMotionCatalog *catalog, const EidolonRealizationProgramSet *programs,
    const EidolonEprResourceResolution *resolution, EidolonEprTick tick,
    const EidolonHumanoidPose *base, EidolonEprMotionFrame *frame) {
    EidolonEprMotionFrame candidate;
    EidolonHumanoidPoseLayer layers[EIDOLON_EPR_PROGRAM_CAPACITY];
    size_t order[EIDOLON_EPR_PROGRAM_CAPACITY];
    size_t order_count = 0U;
    if (catalog == NULL || programs == NULL || resolution == NULL || base == NULL ||
        frame == NULL) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT);
    }
    if (!program_set_valid(programs, resolution)) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION);
    }
    if (!eidolon_humanoid_pose_validate(base)) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT);
    }
    for (size_t index = 0U; index < programs->count; ++index) {
        if (programs->programs[index].motion.generator != EIDOLON_EPR_MOTION_GENERATOR_NONE) {
            order[order_count++] = index;
        }
    }
    sort_program_order(programs, order, order_count);

    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_MOTION_FRAME_VERSION;
    candidate.plan_generation = programs->plan_generation;
    candidate.tick = tick;
    for (size_t ordered = 0U; ordered < order_count; ++ordered) {
        const EidolonRealizationProgram *program = &programs->programs[order[ordered]];
        EidolonEprMotionExecutionResult execute_result;
        uint32_t resources = 0U;
        if (!granted_mask(program, resolution, &resources)) {
            return execution_result(EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION);
        }
        if (resources == 0U) {
            continue;
        }
        execute_result = eidolon_epr_motion_program_execute(
            catalog, program, resources, tick, &candidate.executions[candidate.execution_count]);
        if (execute_result.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE ||
            execute_result.status == EIDOLON_EPR_MOTION_EXECUTION_NO_GRANTS ||
            execute_result.status == EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED) {
            continue;
        }
        if (execute_result.status != EIDOLON_EPR_MOTION_EXECUTION_OK) {
            return execute_result;
        }
        eidolon_humanoid_pose_layer_init(
            &layers[candidate.execution_count],
            &candidate.executions[candidate.execution_count].sample.pose);
        layers[candidate.execution_count].mode =
            candidate.executions[candidate.execution_count].layer_mode;
        layers[candidate.execution_count].weight =
            candidate.executions[candidate.execution_count].layer_weight;
        layers[candidate.execution_count].intensity =
            candidate.executions[candidate.execution_count].sample.intensity;
        candidate.execution_count += 1U;
    }
    if (!eidolon_humanoid_pose_compose(base, layers, candidate.execution_count, &candidate.pose)) {
        return execution_result(EIDOLON_EPR_MOTION_EXECUTION_COMPOSE_FAILED);
    }
    *frame = candidate;
    return execution_result(EIDOLON_EPR_MOTION_EXECUTION_OK);
}
