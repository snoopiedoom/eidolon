#ifndef EIDOLON_EPR_MOTION_CATALOG_H
#define EIDOLON_EPR_MOTION_CATALOG_H

#include "epr/realization_program.h"
#include "humanoid_pose.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_EPR_MOTION_SOURCE_VERSION 1U
#define EIDOLON_EPR_MOTION_CATALOG_VERSION 1U
#define EIDOLON_EPR_MOTION_BINDING_VERSION 1U
#define EIDOLON_EPR_MOTION_SAMPLE_VERSION 1U
#define EIDOLON_EPR_MOTION_CATALOG_CAPACITY ((size_t)EIDOLON_EPR_MOTION_GENERATOR_COUNT - 1U)

/*
 * A source callback emits a canonical normalized humanoid pose at source-local
 * time. The context is borrowed and must outlive the immutable catalog.
 */
typedef bool (*EidolonEprMotionSourceSampler)(const void *context, float seconds, bool loop,
                                              EidolonHumanoidPose *pose);

typedef struct EidolonEprMotionSource {
    uint32_t version;
    uint64_t identity;
    const void *context;
    EidolonEprMotionSourceSampler sample;
    uint64_t rotation_mask;
    float duration_seconds;
    bool owns_hips_translation;
    bool loop;
} EidolonEprMotionSource;

typedef struct EidolonEprMotionCatalogEntry {
    EidolonEprMotionGeneratorId generator;
    EidolonEprMotionSource source;
} EidolonEprMotionCatalogEntry;

typedef struct EidolonEprMotionCatalog {
    uint32_t version;
    EidolonEprMotionCatalogEntry entries[EIDOLON_EPR_MOTION_CATALOG_CAPACITY];
    size_t count;
} EidolonEprMotionCatalog;

typedef struct EidolonEprMotionBinding {
    uint32_t version;
    EidolonEprMotionGeneratorId generator;
    EidolonEprMotionTakeoverPolicy takeover;
    EidolonEprMotionSource source;
    uint32_t resource_mask;
    uint64_t rotation_mask;
    float blend_weight;
    float intensity;
    float playback_rate;
    bool owns_hips_translation;
} EidolonEprMotionBinding;

typedef struct EidolonEprMotionSample {
    uint32_t version;
    EidolonEprMotionGeneratorId generator;
    EidolonEprMotionTakeoverPolicy takeover;
    uint64_t source_identity;
    uint32_t resource_mask;
    EidolonHumanoidPose pose;
    float blend_weight;
    float intensity;
    float playback_rate;
    float source_seconds;
    float source_duration_seconds;
    bool source_loop;
} EidolonEprMotionSample;

typedef enum EidolonEprMotionCatalogStatus {
    EIDOLON_EPR_MOTION_CATALOG_OK = 0,
    EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED,
    EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT,
    EIDOLON_EPR_MOTION_CATALOG_INVALID_CATALOG,
    EIDOLON_EPR_MOTION_CATALOG_INVALID_REFERENCE,
    EIDOLON_EPR_MOTION_CATALOG_INVALID_SOURCE,
    EIDOLON_EPR_MOTION_CATALOG_INVALID_BINDING,
    EIDOLON_EPR_MOTION_CATALOG_DUPLICATE_GENERATOR,
    EIDOLON_EPR_MOTION_CATALOG_FULL,
    EIDOLON_EPR_MOTION_CATALOG_MISSING_GENERATOR,
    EIDOLON_EPR_MOTION_CATALOG_NO_CHANNELS,
    EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED
} EidolonEprMotionCatalogStatus;

const char *eidolon_epr_motion_catalog_status_name(EidolonEprMotionCatalogStatus status);
void eidolon_epr_motion_catalog_init(EidolonEprMotionCatalog *catalog);
bool eidolon_epr_motion_catalog_validate(const EidolonEprMotionCatalog *catalog);
EidolonEprMotionCatalogStatus
eidolon_epr_motion_catalog_add(EidolonEprMotionCatalog *catalog,
                               const EidolonEprMotionCatalogEntry *entry);
/* Leaves binding untouched unless resolution succeeds. */
EidolonEprMotionCatalogStatus
eidolon_epr_motion_catalog_resolve(const EidolonEprMotionCatalog *catalog,
                                   const EidolonEprMotionGeneratorReference *reference,
                                   EidolonEprMotionBinding *binding);
/* Leaves sample untouched unless a valid, non-empty owned pose is produced. */
EidolonEprMotionCatalogStatus
eidolon_epr_motion_binding_sample(const EidolonEprMotionBinding *binding, float source_seconds,
                                  EidolonEprMotionSample *sample);

#endif
