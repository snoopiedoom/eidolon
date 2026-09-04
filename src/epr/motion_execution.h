#ifndef EIDOLON_EPR_MOTION_EXECUTION_H
#define EIDOLON_EPR_MOTION_EXECUTION_H

#include "epr/body_resources.h"
#include "epr/motion_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_MOTION_TIMING_VERSION 1U
#define EIDOLON_EPR_MOTION_EXECUTION_VERSION 1U
#define EIDOLON_EPR_MOTION_FRAME_VERSION 1U

typedef enum EidolonEprMotionTransitionStage {
    EIDOLON_EPR_MOTION_TRANSITION_STEADY = 0,
    EIDOLON_EPR_MOTION_TRANSITION_ENTER,
    EIDOLON_EPR_MOTION_TRANSITION_HOLD,
    EIDOLON_EPR_MOTION_TRANSITION_EXIT,
    EIDOLON_EPR_MOTION_TRANSITION_STAGE_COUNT
} EidolonEprMotionTransitionStage;

typedef struct EidolonEprMotionTiming {
    uint32_t version;
    EidolonEprTick tick;
    EidolonEprTick origin_tick;
    EidolonEprTick terminal_tick;
    float source_seconds;
    float normalized_source_time;
    float transition_weight;
    EidolonEprMotionTransitionStage stage;
    bool has_terminal_tick;
} EidolonEprMotionTiming;

typedef struct EidolonEprMotionExecution {
    uint32_t version;
    EidolonEprOpaqueId program;
    EidolonEprOpaqueId behavior;
    uint64_t plan_generation;
    uint32_t granted_resource_mask;
    EidolonHumanoidPoseLayerMode layer_mode;
    float layer_weight;
    EidolonEprMotionTiming timing;
    EidolonEprMotionSample sample;
} EidolonEprMotionExecution;

typedef struct EidolonEprMotionFrame {
    uint32_t version;
    uint64_t plan_generation;
    EidolonEprTick tick;
    EidolonHumanoidPose pose;
    EidolonEprMotionExecution executions[EIDOLON_EPR_PROGRAM_CAPACITY];
    size_t execution_count;
} EidolonEprMotionFrame;

typedef enum EidolonEprMotionExecutionStatus {
    EIDOLON_EPR_MOTION_EXECUTION_OK = 0,
    EIDOLON_EPR_MOTION_EXECUTION_NOT_REQUIRED,
    EIDOLON_EPR_MOTION_EXECUTION_NOT_ACTIVE,
    EIDOLON_EPR_MOTION_EXECUTION_NO_GRANTS,
    EIDOLON_EPR_MOTION_EXECUTION_INVALID_ARGUMENT,
    EIDOLON_EPR_MOTION_EXECUTION_INVALID_PROGRAM,
    EIDOLON_EPR_MOTION_EXECUTION_INVALID_PHASES,
    EIDOLON_EPR_MOTION_EXECUTION_INVALID_RESOLUTION,
    EIDOLON_EPR_MOTION_EXECUTION_CATALOG_FAILED,
    EIDOLON_EPR_MOTION_EXECUTION_COMPOSE_FAILED
} EidolonEprMotionExecutionStatus;

typedef struct EidolonEprMotionExecutionResult {
    EidolonEprMotionExecutionStatus status;
    EidolonEprMotionCatalogStatus catalog_status;
} EidolonEprMotionExecutionResult;

const char *eidolon_epr_motion_execution_status_name(EidolonEprMotionExecutionStatus status);

/* Leaves execution untouched unless a complete sample succeeds. */
EidolonEprMotionExecutionResult eidolon_epr_motion_program_execute(
    const EidolonEprMotionCatalog *catalog, const EidolonRealizationProgram *program,
    uint32_t granted_resource_mask, EidolonEprTick tick, EidolonEprMotionExecution *execution);

/*
 * Resolves live grants, orders base/cooperative/additive/override contributions deterministically,
 * and leaves frame untouched unless the complete composed frame succeeds.
 */
EidolonEprMotionExecutionResult eidolon_epr_motion_programs_compose(
    const EidolonEprMotionCatalog *catalog, const EidolonRealizationProgramSet *programs,
    const EidolonEprResourceResolution *resolution, EidolonEprTick tick,
    const EidolonHumanoidPose *base, EidolonEprMotionFrame *frame);

#endif
