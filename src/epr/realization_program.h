#ifndef EIDOLON_EPR_REALIZATION_PROGRAM_H
#define EIDOLON_EPR_REALIZATION_PROGRAM_H

#include "epr/behavior_plan.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_PROGRAM_VERSION 1U
#define EIDOLON_EPR_PROGRAM_CAPACITY EIDOLON_EPR_BEHAVIOR_CAPACITY

typedef enum EidolonEprModality {
    EIDOLON_EPR_MODALITY_IDLE = 0,
    EIDOLON_EPR_MODALITY_POSTURE,
    EIDOLON_EPR_MODALITY_GAZE,
    EIDOLON_EPR_MODALITY_GESTURE,
    EIDOLON_EPR_MODALITY_SETTLE,
    EIDOLON_EPR_MODALITY_EXPRESSION
} EidolonEprModality;

typedef enum EidolonEprCapabilityRequirement {
    EIDOLON_EPR_CAPABILITY_NONE = 0,
    EIDOLON_EPR_CAPABILITY_EYES = 1U << 0U,
    EIDOLON_EPR_CAPABILITY_EXPRESSION = 1U << 1U
} EidolonEprCapabilityRequirement;

typedef struct EidolonRealizationProgram {
    uint32_t version;
    EidolonEprOpaqueId id;
    EidolonEprOpaqueId behavior;
    EidolonEprOpaqueId cause;
    uint64_t plan_generation;
    EidolonEprBehaviorKind behavior_kind;
    EidolonEprModality modality;
    uint32_t resource_mask;
    uint32_t capability_mask;
    EidolonEprTick phase_ticks[EIDOLON_EPR_BEHAVIOR_PHASE_COUNT];
    bool has_phase[EIDOLON_EPR_BEHAVIOR_PHASE_COUNT];
    float targets[3][3];
    float values[8];
} EidolonRealizationProgram;

typedef struct EidolonRealizationProgramSet {
    uint64_t plan_generation;
    EidolonRealizationProgram programs[EIDOLON_EPR_PROGRAM_CAPACITY];
    size_t count;
} EidolonRealizationProgramSet;

bool eidolon_epr_program_set_compile(const EidolonBehaviorPlan *plan,
                                     EidolonRealizationProgramSet *programs);
const EidolonRealizationProgram *
eidolon_epr_program_find(const EidolonRealizationProgramSet *programs, EidolonEprOpaqueId behavior);

#endif
