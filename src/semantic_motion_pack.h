#ifndef EIDOLON_SEMANTIC_MOTION_PACK_H
#define EIDOLON_SEMANTIC_MOTION_PACK_H

#include "epr/motion_catalog.h"
#include "vrma_clip.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_SEMANTIC_MOTION_PACK_VERSION 1U
#define EIDOLON_SEMANTIC_MOTION_BINDING_VERSION 1U
#define EIDOLON_SEMANTIC_MOTION_PACK_ERROR_CAPACITY 256U

typedef struct EidolonSemanticMotionBinding {
    uint32_t version;
    EidolonEprMotionGeneratorId generator;
    uint64_t source_identity;
    bool loop;
} EidolonSemanticMotionBinding;

typedef struct EidolonSemanticMotionAsset {
    EidolonSemanticMotionBinding binding;
    const char *path;
} EidolonSemanticMotionAsset;

/*
 * Owns the VRMA clips behind an immutable EPR motion catalog. Catalog source
 * contexts always point into this object, so a live pack must not be copied.
 */
typedef struct EidolonSemanticMotionPack {
    uint32_t version;
    EidolonVrmaClip clips[EIDOLON_EPR_MOTION_CATALOG_CAPACITY];
    bool clip_ready[EIDOLON_EPR_MOTION_CATALOG_CAPACITY];
    EidolonEprMotionCatalog catalog;
} EidolonSemanticMotionPack;

void eidolon_semantic_motion_pack_init(EidolonSemanticMotionPack *pack);
bool eidolon_semantic_motion_pack_validate(const EidolonSemanticMotionPack *pack);

/*
 * Atomically extends pack. On success, ownership of every input clip moves to
 * pack and each input is zeroed. On failure, pack and every input clip remain
 * unchanged.
 */
bool eidolon_semantic_motion_pack_bind(EidolonSemanticMotionPack *pack,
                                       const EidolonSemanticMotionBinding *bindings,
                                       EidolonVrmaClip *clips, size_t count, char *error,
                                       size_t error_capacity);

/* Atomically loads and binds every asset; a partial pack is never published. */
bool eidolon_semantic_motion_pack_load(EidolonSemanticMotionPack *pack,
                                       const EidolonSemanticMotionAsset *assets, size_t count,
                                       char *error, size_t error_capacity);

void eidolon_semantic_motion_pack_destroy(EidolonSemanticMotionPack *pack);

#endif
