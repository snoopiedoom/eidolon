#include "semantic_motion_pack.h"

#include "vrma_motion_source.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t capacity, const char *format, ...) {
    va_list arguments;
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static bool binding_valid(const EidolonSemanticMotionBinding *binding) {
    return binding != NULL && binding->version == EIDOLON_SEMANTIC_MOTION_BINDING_VERSION &&
           binding->generator > EIDOLON_EPR_MOTION_GENERATOR_NONE &&
           binding->generator < EIDOLON_EPR_MOTION_GENERATOR_COUNT &&
           binding->source_identity != 0U;
}

static const EidolonEprMotionCatalogEntry *find_entry(const EidolonEprMotionCatalog *catalog,
                                                      EidolonEprMotionGeneratorId generator) {
    if (catalog == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < catalog->count; ++index) {
        if (catalog->entries[index].generator == generator) {
            return &catalog->entries[index];
        }
    }
    return NULL;
}

static size_t clip_allocation_count(const EidolonVrmaClip *clip) {
    return 1U + clip->track_count * 2U;
}

static const void *clip_allocation_at(const EidolonVrmaClip *clip, size_t index) {
    if (index == 0U) {
        return clip->tracks;
    }
    const size_t track = (index - 1U) / 2U;
    return ((index - 1U) & 1U) == 0U ? (const void *)clip->tracks[track].times
                                     : (const void *)clip->tracks[track].values;
}

static bool clip_storage_unique(const EidolonVrmaClip *clip) {
    const size_t count = clip_allocation_count(clip);
    for (size_t left = 0U; left < count; ++left) {
        const void *allocation = clip_allocation_at(clip, left);
        if (allocation == NULL) {
            return false;
        }
        for (size_t right = left + 1U; right < count; ++right) {
            if (allocation == clip_allocation_at(clip, right)) {
                return false;
            }
        }
    }
    return true;
}

static bool clip_storage_overlaps(const EidolonVrmaClip *left, const EidolonVrmaClip *right) {
    const size_t left_count = clip_allocation_count(left);
    const size_t right_count = clip_allocation_count(right);
    for (size_t left_index = 0U; left_index < left_count; ++left_index) {
        const void *allocation = clip_allocation_at(left, left_index);
        for (size_t right_index = 0U; right_index < right_count; ++right_index) {
            if (allocation == clip_allocation_at(right, right_index)) {
                return true;
            }
        }
    }
    return false;
}

void eidolon_semantic_motion_pack_init(EidolonSemanticMotionPack *pack) {
    if (pack == NULL) {
        return;
    }
    memset(pack, 0, sizeof(*pack));
    pack->version = EIDOLON_SEMANTIC_MOTION_PACK_VERSION;
    eidolon_epr_motion_catalog_init(&pack->catalog);
}

bool eidolon_semantic_motion_pack_validate(const EidolonSemanticMotionPack *pack) {
    size_t ready_count = 0U;
    if (pack == NULL || pack->version != EIDOLON_SEMANTIC_MOTION_PACK_VERSION ||
        !eidolon_epr_motion_catalog_validate(&pack->catalog)) {
        return false;
    }
    for (size_t slot = 0U; slot < EIDOLON_EPR_MOTION_CATALOG_CAPACITY; ++slot) {
        const EidolonEprMotionGeneratorId generator = (EidolonEprMotionGeneratorId)(slot + 1U);
        const EidolonEprMotionCatalogEntry *entry = find_entry(&pack->catalog, generator);
        EidolonEprMotionSource expected;
        if (!pack->clip_ready[slot]) {
            if (entry != NULL) {
                return false;
            }
            continue;
        }
        ready_count += 1U;
        if (entry == NULL || pack->clips[slot].version != EIDOLON_VRMA_CLIP_VERSION ||
            pack->clips[slot].tracks == NULL || pack->clips[slot].track_count == 0U ||
            pack->clips[slot].track_count > EIDOLON_HUMANOID_ROLE_COUNT + 1U ||
            !clip_storage_unique(&pack->clips[slot]) ||
            !eidolon_vrma_motion_source_build(&pack->clips[slot], entry->source.identity,
                                              entry->source.loop, &expected, NULL, 0U) ||
            entry->source.version != expected.version ||
            entry->source.identity != expected.identity ||
            entry->source.context != expected.context || entry->source.sample != expected.sample ||
            entry->source.rotation_mask != expected.rotation_mask ||
            entry->source.duration_seconds != expected.duration_seconds ||
            entry->source.owns_hips_translation != expected.owns_hips_translation ||
            entry->source.loop != expected.loop) {
            return false;
        }
    }
    return ready_count == pack->catalog.count;
}

static bool request_shape_valid(const EidolonSemanticMotionPack *pack,
                                const EidolonSemanticMotionBinding *bindings,
                                const EidolonVrmaClip *clips, size_t count, char *error,
                                size_t error_capacity) {
    if (!eidolon_semantic_motion_pack_validate(pack) || bindings == NULL || clips == NULL ||
        count == 0U || count > EIDOLON_EPR_MOTION_CATALOG_CAPACITY ||
        count > EIDOLON_EPR_MOTION_CATALOG_CAPACITY - pack->catalog.count) {
        set_error(error, error_capacity, "invalid semantic motion-pack binding request");
        return false;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!binding_valid(&bindings[index])) {
            set_error(error, error_capacity, "semantic motion binding %zu is invalid", index);
            return false;
        }
        const size_t slot = (size_t)bindings[index].generator - 1U;
        if (pack->clip_ready[slot]) {
            set_error(error, error_capacity, "semantic generator '%s' is already bound",
                      eidolon_epr_motion_generator_name(bindings[index].generator));
            return false;
        }
        for (size_t existing = 0U; existing < EIDOLON_EPR_MOTION_CATALOG_CAPACITY; ++existing) {
            if (pack->clip_ready[existing] && clips[index].tracks != NULL &&
                clips[index].tracks == pack->clips[existing].tracks) {
                set_error(error, error_capacity, "semantic motion clip %zu is already owned",
                          index);
                return false;
            }
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (bindings[index].generator == bindings[previous].generator) {
                set_error(error, error_capacity, "semantic generator '%s' is duplicated",
                          eidolon_epr_motion_generator_name(bindings[index].generator));
                return false;
            }
            if (clips[index].tracks != NULL && clips[index].tracks == clips[previous].tracks) {
                set_error(error, error_capacity, "semantic motion clips %zu and %zu alias",
                          previous, index);
                return false;
            }
        }
    }
    return true;
}

bool eidolon_semantic_motion_pack_bind(EidolonSemanticMotionPack *pack,
                                       const EidolonSemanticMotionBinding *bindings,
                                       EidolonVrmaClip *clips, size_t count, char *error,
                                       size_t error_capacity) {
    EidolonEprMotionCatalog original;
    EidolonEprMotionCatalog preflight;
    EidolonEprMotionCatalog committed;
    EidolonEprMotionSource sources[EIDOLON_EPR_MOTION_CATALOG_CAPACITY];
    size_t moved = 0U;
    char source_error[EIDOLON_VRMA_MOTION_SOURCE_ERROR_CAPACITY];
    if (!request_shape_valid(pack, bindings, clips, count, error, error_capacity)) {
        return false;
    }
    original = pack->catalog;
    preflight = original;
    for (size_t index = 0U; index < count; ++index) {
        EidolonEprMotionCatalogEntry entry;
        if (!eidolon_vrma_motion_source_build(&clips[index], bindings[index].source_identity,
                                              bindings[index].loop, &sources[index], source_error,
                                              sizeof(source_error))) {
            set_error(error, error_capacity, "semantic motion clip %zu is invalid: %s", index,
                      source_error);
            return false;
        }
        if (!clip_storage_unique(&clips[index])) {
            set_error(error, error_capacity, "semantic motion clip %zu reuses owned storage",
                      index);
            return false;
        }
        for (size_t existing = 0U; existing < EIDOLON_EPR_MOTION_CATALOG_CAPACITY; ++existing) {
            if (pack->clip_ready[existing] &&
                clip_storage_overlaps(&clips[index], &pack->clips[existing])) {
                set_error(error, error_capacity,
                          "semantic motion clip %zu aliases storage owned by the pack", index);
                return false;
            }
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (clip_storage_overlaps(&clips[index], &clips[previous])) {
                set_error(error, error_capacity,
                          "semantic motion clips %zu and %zu alias owned storage", previous, index);
                return false;
            }
        }
        entry.generator = bindings[index].generator;
        entry.source = sources[index];
        {
            const EidolonEprMotionCatalogStatus status =
                eidolon_epr_motion_catalog_add(&preflight, &entry);
            if (status != EIDOLON_EPR_MOTION_CATALOG_OK) {
                set_error(error, error_capacity, "semantic motion binding %zu was rejected: %s",
                          index, eidolon_epr_motion_catalog_status_name(status));
                return false;
            }
        }
    }

    committed = original;
    for (size_t index = 0U; index < count; ++index) {
        const size_t slot = (size_t)bindings[index].generator - 1U;
        EidolonEprMotionSource source;
        EidolonEprMotionCatalogEntry entry;
        pack->clips[slot] = clips[index];
        memset(&clips[index], 0, sizeof(clips[index]));
        pack->clip_ready[slot] = true;
        moved += 1U;
        if (!eidolon_vrma_motion_source_build(&pack->clips[slot], bindings[index].source_identity,
                                              bindings[index].loop, &source, source_error,
                                              sizeof(source_error))) {
            set_error(error, error_capacity, "could not activate semantic motion clip %zu: %s",
                      index, source_error);
            goto rollback;
        }
        entry.generator = bindings[index].generator;
        entry.source = source;
        if (eidolon_epr_motion_catalog_add(&committed, &entry) != EIDOLON_EPR_MOTION_CATALOG_OK) {
            set_error(error, error_capacity, "could not publish semantic motion binding %zu",
                      index);
            goto rollback;
        }
    }
    pack->catalog = committed;
    if (!eidolon_semantic_motion_pack_validate(pack)) {
        pack->catalog = original;
        set_error(error, error_capacity, "published semantic motion pack is inconsistent");
        goto rollback;
    }
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;

rollback:
    for (size_t index = 0U; index < moved; ++index) {
        const size_t slot = (size_t)bindings[index].generator - 1U;
        clips[index] = pack->clips[slot];
        memset(&pack->clips[slot], 0, sizeof(pack->clips[slot]));
        pack->clip_ready[slot] = false;
    }
    pack->catalog = original;
    return false;
}

bool eidolon_semantic_motion_pack_load(EidolonSemanticMotionPack *pack,
                                       const EidolonSemanticMotionAsset *assets, size_t count,
                                       char *error, size_t error_capacity) {
    EidolonSemanticMotionBinding bindings[EIDOLON_EPR_MOTION_CATALOG_CAPACITY];
    EidolonVrmaClip clips[EIDOLON_EPR_MOTION_CATALOG_CAPACITY];
    char load_error[EIDOLON_VRMA_ERROR_CAPACITY];
    bool result = false;
    memset(clips, 0, sizeof(clips));
    if (assets == NULL || count == 0U || count > EIDOLON_EPR_MOTION_CATALOG_CAPACITY) {
        set_error(error, error_capacity, "invalid semantic motion asset request");
        return false;
    }
    for (size_t index = 0U; index < count; ++index) {
        bindings[index] = assets[index].binding;
        if (!binding_valid(&bindings[index]) || assets[index].path == NULL ||
            assets[index].path[0] == '\0') {
            set_error(error, error_capacity, "semantic motion asset %zu is invalid", index);
            goto cleanup;
        }
    }
    if (!request_shape_valid(pack, bindings, clips, count, error, error_capacity)) {
        goto cleanup;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!eidolon_vrma_clip_load(assets[index].path, &clips[index], load_error,
                                    sizeof(load_error))) {
            set_error(error, error_capacity, "could not load semantic motion asset '%s': %s",
                      assets[index].path, load_error);
            goto cleanup;
        }
    }
    result = eidolon_semantic_motion_pack_bind(pack, bindings, clips, count, error, error_capacity);

cleanup:
    for (size_t index = 0U; index < count; ++index) {
        eidolon_vrma_clip_destroy(&clips[index]);
    }
    return result;
}

void eidolon_semantic_motion_pack_destroy(EidolonSemanticMotionPack *pack) {
    if (pack == NULL) {
        return;
    }
    if (pack->version == EIDOLON_SEMANTIC_MOTION_PACK_VERSION) {
        for (size_t slot = 0U; slot < EIDOLON_EPR_MOTION_CATALOG_CAPACITY; ++slot) {
            if (pack->clip_ready[slot]) {
                eidolon_vrma_clip_destroy(&pack->clips[slot]);
            }
        }
    }
    memset(pack, 0, sizeof(*pack));
}
