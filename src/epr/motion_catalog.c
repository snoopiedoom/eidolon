#include "epr/motion_catalog.h"

#include <math.h>
#include <string.h>

_Static_assert(EIDOLON_HUMANOID_ROLE_COUNT < 64,
               "motion catalog role mask requires fewer than 64 roles");

static uint64_t valid_rotation_mask(void) {
    return (UINT64_C(1) << (uint32_t)EIDOLON_HUMANOID_ROLE_COUNT) - UINT64_C(1);
}

static bool source_valid(const EidolonEprMotionSource *source) {
    return source != NULL && source->version == EIDOLON_EPR_MOTION_SOURCE_VERSION &&
           source->identity != 0U && source->sample != NULL &&
           (source->rotation_mask & ~valid_rotation_mask()) == 0U &&
           (source->rotation_mask != 0U || source->owns_hips_translation) &&
           isfinite(source->duration_seconds) && source->duration_seconds >= 0.0F;
}

static bool entry_valid(const EidolonEprMotionCatalogEntry *entry) {
    return entry != NULL && entry->generator > EIDOLON_EPR_MOTION_GENERATOR_NONE &&
           entry->generator < EIDOLON_EPR_MOTION_GENERATOR_COUNT && source_valid(&entry->source);
}

static bool binding_valid(const EidolonEprMotionBinding *binding) {
    uint64_t resource_rotations = 0U;
    bool resource_owns_hips = false;
    if (binding == NULL || binding->version != EIDOLON_EPR_MOTION_BINDING_VERSION ||
        binding->generator <= EIDOLON_EPR_MOTION_GENERATOR_NONE ||
        binding->generator >= EIDOLON_EPR_MOTION_GENERATOR_COUNT ||
        binding->takeover <= EIDOLON_EPR_MOTION_TAKEOVER_NONE ||
        binding->takeover >= EIDOLON_EPR_MOTION_TAKEOVER_COUNT || !source_valid(&binding->source) ||
        (binding->rotation_mask == 0U && !binding->owns_hips_translation) ||
        (binding->rotation_mask & ~binding->source.rotation_mask) != 0U ||
        (binding->owns_hips_translation && !binding->source.owns_hips_translation) ||
        !isfinite(binding->blend_weight) || binding->blend_weight < 0.0F ||
        binding->blend_weight > 1.0F || !isfinite(binding->intensity) ||
        binding->intensity < 0.0F || binding->intensity > 1.0F ||
        !isfinite(binding->playback_rate) || binding->playback_rate <= 0.0F ||
        binding->playback_rate > 4.0F ||
        !eidolon_epr_resource_mask_humanoid_channels(binding->resource_mask, &resource_rotations,
                                                     &resource_owns_hips)) {
        return false;
    }
    return (binding->rotation_mask & ~resource_rotations) == 0U &&
           (!binding->owns_hips_translation || resource_owns_hips);
}

const char *eidolon_epr_motion_catalog_status_name(EidolonEprMotionCatalogStatus status) {
    switch (status) {
    case EIDOLON_EPR_MOTION_CATALOG_OK:
        return "ok";
    case EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED:
        return "not_required";
    case EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT:
        return "invalid_argument";
    case EIDOLON_EPR_MOTION_CATALOG_INVALID_CATALOG:
        return "invalid_catalog";
    case EIDOLON_EPR_MOTION_CATALOG_INVALID_REFERENCE:
        return "invalid_reference";
    case EIDOLON_EPR_MOTION_CATALOG_INVALID_SOURCE:
        return "invalid_source";
    case EIDOLON_EPR_MOTION_CATALOG_INVALID_BINDING:
        return "invalid_binding";
    case EIDOLON_EPR_MOTION_CATALOG_DUPLICATE_GENERATOR:
        return "duplicate_generator";
    case EIDOLON_EPR_MOTION_CATALOG_FULL:
        return "full";
    case EIDOLON_EPR_MOTION_CATALOG_MISSING_GENERATOR:
        return "missing_generator";
    case EIDOLON_EPR_MOTION_CATALOG_NO_CHANNELS:
        return "no_channels";
    case EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED:
        return "sample_failed";
    }
    return "unknown";
}

void eidolon_epr_motion_catalog_init(EidolonEprMotionCatalog *catalog) {
    if (catalog == NULL) {
        return;
    }
    memset(catalog, 0, sizeof(*catalog));
    catalog->version = EIDOLON_EPR_MOTION_CATALOG_VERSION;
}

bool eidolon_epr_motion_catalog_validate(const EidolonEprMotionCatalog *catalog) {
    if (catalog == NULL || catalog->version != EIDOLON_EPR_MOTION_CATALOG_VERSION ||
        catalog->count > EIDOLON_EPR_MOTION_CATALOG_CAPACITY) {
        return false;
    }
    for (size_t index = 0U; index < catalog->count; ++index) {
        if (!entry_valid(&catalog->entries[index])) {
            return false;
        }
        for (size_t earlier = 0U; earlier < index; ++earlier) {
            if (catalog->entries[earlier].generator == catalog->entries[index].generator) {
                return false;
            }
        }
    }
    return true;
}

EidolonEprMotionCatalogStatus
eidolon_epr_motion_catalog_add(EidolonEprMotionCatalog *catalog,
                               const EidolonEprMotionCatalogEntry *entry) {
    EidolonEprMotionCatalog candidate;
    if (catalog == NULL || entry == NULL) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT;
    }
    if (!eidolon_epr_motion_catalog_validate(catalog)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_CATALOG;
    }
    if (entry->generator <= EIDOLON_EPR_MOTION_GENERATOR_NONE ||
        entry->generator >= EIDOLON_EPR_MOTION_GENERATOR_COUNT) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT;
    }
    if (!source_valid(&entry->source)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_SOURCE;
    }
    for (size_t index = 0U; index < catalog->count; ++index) {
        if (catalog->entries[index].generator == entry->generator) {
            return EIDOLON_EPR_MOTION_CATALOG_DUPLICATE_GENERATOR;
        }
    }
    if (catalog->count >= EIDOLON_EPR_MOTION_CATALOG_CAPACITY) {
        return EIDOLON_EPR_MOTION_CATALOG_FULL;
    }
    candidate = *catalog;
    candidate.entries[candidate.count++] = *entry;
    if (!eidolon_epr_motion_catalog_validate(&candidate)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_SOURCE;
    }
    *catalog = candidate;
    return EIDOLON_EPR_MOTION_CATALOG_OK;
}

EidolonEprMotionCatalogStatus
eidolon_epr_motion_catalog_resolve(const EidolonEprMotionCatalog *catalog,
                                   const EidolonEprMotionGeneratorReference *reference,
                                   EidolonEprMotionBinding *binding) {
    EidolonEprMotionBinding candidate;
    const EidolonEprMotionCatalogEntry *entry = NULL;
    if (catalog == NULL || reference == NULL || binding == NULL) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT;
    }
    if (!eidolon_epr_motion_catalog_validate(catalog)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_CATALOG;
    }
    if (!eidolon_epr_motion_generator_reference_validate(reference)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_REFERENCE;
    }
    if (reference->generator == EIDOLON_EPR_MOTION_GENERATOR_NONE) {
        return EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED;
    }
    for (size_t index = 0U; index < catalog->count; ++index) {
        if (catalog->entries[index].generator == reference->generator) {
            entry = &catalog->entries[index];
            break;
        }
    }
    if (entry == NULL) {
        return EIDOLON_EPR_MOTION_CATALOG_MISSING_GENERATOR;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_MOTION_BINDING_VERSION;
    candidate.generator = reference->generator;
    candidate.takeover = reference->takeover;
    candidate.source = entry->source;
    candidate.resource_mask = reference->resource_mask;
    candidate.rotation_mask = reference->humanoid_rotation_mask & entry->source.rotation_mask;
    candidate.blend_weight = reference->blend_weight;
    candidate.intensity = reference->intensity;
    candidate.playback_rate = reference->playback_rate;
    candidate.owns_hips_translation =
        reference->owns_hips_translation && entry->source.owns_hips_translation;
    if (candidate.rotation_mask == 0U && !candidate.owns_hips_translation) {
        return EIDOLON_EPR_MOTION_CATALOG_NO_CHANNELS;
    }
    if (!binding_valid(&candidate)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_SOURCE;
    }
    *binding = candidate;
    return EIDOLON_EPR_MOTION_CATALOG_OK;
}

EidolonEprMotionCatalogStatus
eidolon_epr_motion_binding_sample(const EidolonEprMotionBinding *binding, float source_seconds,
                                  EidolonEprMotionSample *sample) {
    EidolonHumanoidPose sampled_pose;
    EidolonEprMotionSample candidate;
    uint64_t actual_rotations;
    bool actual_hips;
    if (binding == NULL || sample == NULL || !isfinite(source_seconds) || source_seconds < 0.0F) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_ARGUMENT;
    }
    if (!binding_valid(binding)) {
        return EIDOLON_EPR_MOTION_CATALOG_INVALID_BINDING;
    }
    eidolon_humanoid_pose_init(&sampled_pose);
    if (!binding->source.sample(binding->source.context, source_seconds, binding->source.loop,
                                &sampled_pose) ||
        !eidolon_humanoid_pose_validate(&sampled_pose)) {
        return EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED;
    }
    actual_rotations =
        binding->rotation_mask & binding->source.rotation_mask & sampled_pose.rotation_mask;
    actual_hips = binding->owns_hips_translation && binding->source.owns_hips_translation &&
                  sampled_pose.has_hips_translation;
    if (actual_rotations == 0U && !actual_hips) {
        return EIDOLON_EPR_MOTION_CATALOG_NO_CHANNELS;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_MOTION_SAMPLE_VERSION;
    candidate.generator = binding->generator;
    candidate.takeover = binding->takeover;
    candidate.source_identity = binding->source.identity;
    candidate.resource_mask = binding->resource_mask;
    candidate.blend_weight = binding->blend_weight;
    candidate.intensity = binding->intensity;
    candidate.playback_rate = binding->playback_rate;
    candidate.source_seconds = source_seconds;
    candidate.source_duration_seconds = binding->source.duration_seconds;
    candidate.source_loop = binding->source.loop;
    eidolon_humanoid_pose_init(&candidate.pose);
    candidate.pose.rotation_mask = actual_rotations;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if ((actual_rotations & (UINT64_C(1) << (uint32_t)role)) != 0U) {
            memcpy(candidate.pose.rotations[role], sampled_pose.rotations[role],
                   sizeof(candidate.pose.rotations[role]));
        }
    }
    candidate.pose.has_hips_translation = actual_hips;
    if (actual_hips) {
        memcpy(candidate.pose.hips_translation, sampled_pose.hips_translation,
               sizeof(candidate.pose.hips_translation));
    }
    if (!eidolon_humanoid_pose_validate(&candidate.pose)) {
        return EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED;
    }
    *sample = candidate;
    return EIDOLON_EPR_MOTION_CATALOG_OK;
}
