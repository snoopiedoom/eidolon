#include "semantic_motion_pack.h"

#include "humanoid.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t role_bit(EidolonHumanoidRole role) { return UINT64_C(1) << (uint32_t)role; }

static uint64_t required_role_mask(void) {
    uint64_t result = 0U;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
            result |= role_bit((EidolonHumanoidRole)role);
        }
    }
    return result;
}

static void make_owned_clip(EidolonVrmaClip *clip, float radians) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    const float sine = sinf(radians * 0.5F);
    const float cosine = cosf(radians * 0.5F);
    EidolonVrmaTrack *tracks;
    memset(clip, 0, sizeof(*clip));
    tracks = calloc(2U, sizeof(*tracks));
    assert(tracks != NULL);
    for (size_t index = 0U; index < 2U; ++index) {
        tracks[index].times = calloc(2U, sizeof(*tracks[index].times));
        tracks[index].values = calloc(index == 0U ? 8U : 6U, sizeof(*tracks[index].values));
        assert(tracks[index].times != NULL && tracks[index].values != NULL);
        tracks[index].times[0] = 0.0F;
        tracks[index].times[1] = 1.0F;
        tracks[index].key_count = 2U;
        tracks[index].values_per_key = 1U;
        tracks[index].interpolation = EIDOLON_VRMA_INTERPOLATION_LINEAR;
    }

    tracks[0].role = EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM;
    tracks[0].path = EIDOLON_VRMA_TRACK_ROTATION;
    tracks[0].component_count = 4U;
    memcpy(&tracks[0].values[0], identity, sizeof(identity));
    tracks[0].values[4] = 0.0F;
    tracks[0].values[5] = sine;
    tracks[0].values[6] = 0.0F;
    tracks[0].values[7] = cosine;

    tracks[1].role = EIDOLON_HUMANOID_ROLE_HIPS;
    tracks[1].path = EIDOLON_VRMA_TRACK_HIPS_TRANSLATION;
    tracks[1].component_count = 3U;
    tracks[1].values[0] = 0.0F;
    tracks[1].values[1] = 1.0F;
    tracks[1].values[2] = 0.0F;
    tracks[1].values[3] = 0.1F;
    tracks[1].values[4] = 1.05F;
    tracks[1].values[5] = -0.1F;

    clip->version = EIDOLON_VRMA_CLIP_VERSION;
    clip->mapped_roles = required_role_mask() | role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM) |
                         role_bit(EIDOLON_HUMANOID_ROLE_HIPS);
    clip->source_hips_height = 1.0F;
    clip->duration_seconds = 1.0F;
    clip->tracks = tracks;
    clip->track_count = 2U;
    clip->rest_hips_translation[1] = 1.0F;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        memcpy(clip->rest_local_rotations[role], identity, sizeof(identity));
        memcpy(clip->rest_world_rotations[role], identity, sizeof(identity));
        clip->source_node_by_role[role] = -1;
    }
}

static EidolonSemanticMotionBinding binding(EidolonEprMotionGeneratorId generator,
                                            uint64_t identity, bool loop) {
    const EidolonSemanticMotionBinding result = {
        .version = EIDOLON_SEMANTIC_MOTION_BINDING_VERSION,
        .generator = generator,
        .source_identity = identity,
        .loop = loop,
    };
    return result;
}

static const EidolonEprMotionCatalogEntry *entry_for(const EidolonSemanticMotionPack *pack,
                                                     EidolonEprMotionGeneratorId generator) {
    for (size_t index = 0U; index < pack->catalog.count; ++index) {
        if (pack->catalog.entries[index].generator == generator) {
            return &pack->catalog.entries[index];
        }
    }
    return NULL;
}

static void test_atomic_commit_rebases_borrowed_sources(void) {
    EidolonSemanticMotionPack pack;
    EidolonSemanticMotionBinding bindings[2];
    EidolonVrmaClip clips[2];
    EidolonVrmaTrack *owned_tracks[2];
    const EidolonEprMotionCatalogEntry *entry;
    EidolonHumanoidPose pose;
    char error[EIDOLON_SEMANTIC_MOTION_PACK_ERROR_CAPACITY];

    eidolon_semantic_motion_pack_init(&pack);
    assert(eidolon_semantic_motion_pack_validate(&pack));
    make_owned_clip(&clips[0], 0.25F);
    make_owned_clip(&clips[1], 0.5F);
    owned_tracks[0] = clips[0].tracks;
    owned_tracks[1] = clips[1].tracks;
    bindings[0] = binding(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, UINT64_C(0x101), true);
    bindings[1] = binding(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, UINT64_C(0x202), false);

    assert(eidolon_semantic_motion_pack_bind(&pack, bindings, clips, 2U, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(eidolon_semantic_motion_pack_validate(&pack));
    assert(pack.catalog.count == 2U);
    assert(clips[0].version == 0U && clips[0].tracks == NULL);
    assert(clips[1].version == 0U && clips[1].tracks == NULL);
    assert(pack.clips[(size_t)bindings[0].generator - 1U].tracks == owned_tracks[0]);
    assert(pack.clips[(size_t)bindings[1].generator - 1U].tracks == owned_tracks[1]);

    entry = entry_for(&pack, EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT);
    assert(entry != NULL);
    assert(entry->source.context ==
           &pack.clips[(size_t)EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT - 1U]);
    assert(entry->source.identity == UINT64_C(0x202));
    assert(!entry->source.loop);
    assert(entry->source.sample(entry->source.context, 1.0F, entry->source.loop, &pose));
    assert((pose.rotation_mask & role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM)) != 0U);

    eidolon_semantic_motion_pack_destroy(&pack);
    assert(pack.version == 0U);
}

static void test_invalid_batch_rolls_back_every_clip(void) {
    EidolonSemanticMotionPack pack;
    EidolonSemanticMotionBinding initial_binding;
    EidolonSemanticMotionBinding bindings[2];
    EidolonVrmaClip initial_clip;
    EidolonVrmaClip clips[2];
    EidolonEprMotionCatalog before;
    EidolonVrmaTrack *before_tracks[2];
    char error[EIDOLON_SEMANTIC_MOTION_PACK_ERROR_CAPACITY];

    eidolon_semantic_motion_pack_init(&pack);
    make_owned_clip(&initial_clip, 0.1F);
    initial_binding = binding(EIDOLON_EPR_MOTION_IDLE_NEUTRAL, UINT64_C(0x303), true);
    assert(eidolon_semantic_motion_pack_bind(&pack, &initial_binding, &initial_clip, 1U, error,
                                             sizeof(error)));
    before = pack.catalog;

    make_owned_clip(&clips[0], 0.3F);
    make_owned_clip(&clips[1], 0.6F);
    before_tracks[0] = clips[0].tracks;
    before_tracks[1] = clips[1].tracks;
    clips[1].duration_seconds = 2.0F;
    bindings[0] = binding(EIDOLON_EPR_MOTION_POSTURE_THINKING, UINT64_C(0x404), true);
    bindings[1] = binding(EIDOLON_EPR_MOTION_POSTURE_RESPONDING, UINT64_C(0x505), true);

    assert(!eidolon_semantic_motion_pack_bind(&pack, bindings, clips, 2U, error, sizeof(error)));
    assert(strstr(error, "duration") != NULL);
    assert(eidolon_semantic_motion_pack_validate(&pack));
    assert(memcmp(&pack.catalog, &before, sizeof(before)) == 0);
    assert(clips[0].tracks == before_tracks[0]);
    assert(clips[1].tracks == before_tracks[1]);
    assert(!pack.clip_ready[(size_t)bindings[0].generator - 1U]);
    assert(!pack.clip_ready[(size_t)bindings[1].generator - 1U]);

    eidolon_vrma_clip_destroy(&clips[0]);
    eidolon_vrma_clip_destroy(&clips[1]);
    eidolon_semantic_motion_pack_destroy(&pack);
}

static void test_nested_allocation_alias_rolls_back(void) {
    EidolonSemanticMotionPack pack;
    EidolonSemanticMotionBinding bindings[2];
    EidolonVrmaClip clips[2];
    EidolonEprMotionCatalog before;
    float *second_times;
    char error[EIDOLON_SEMANTIC_MOTION_PACK_ERROR_CAPACITY];

    eidolon_semantic_motion_pack_init(&pack);
    make_owned_clip(&clips[0], 0.2F);
    make_owned_clip(&clips[1], 0.4F);
    second_times = clips[1].tracks[0].times;
    clips[1].tracks[0].times = clips[0].tracks[0].times;
    bindings[0] = binding(EIDOLON_EPR_MOTION_POSTURE_THINKING, UINT64_C(0x909), true);
    bindings[1] = binding(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, UINT64_C(0xa0a), false);
    before = pack.catalog;

    assert(!eidolon_semantic_motion_pack_bind(&pack, bindings, clips, 2U, error, sizeof(error)));
    assert(strstr(error, "alias") != NULL);
    assert(eidolon_semantic_motion_pack_validate(&pack));
    assert(memcmp(&pack.catalog, &before, sizeof(before)) == 0);
    assert(clips[0].tracks != NULL && clips[1].tracks != NULL);
    assert(clips[1].tracks[0].times == clips[0].tracks[0].times);

    clips[1].tracks[0].times = second_times;
    eidolon_vrma_clip_destroy(&clips[0]);
    eidolon_vrma_clip_destroy(&clips[1]);
    eidolon_semantic_motion_pack_destroy(&pack);
}

static void test_duplicate_and_file_failure_preserve_state(void) {
    EidolonSemanticMotionPack pack;
    EidolonSemanticMotionBinding bindings[2];
    EidolonVrmaClip clips[2];
    EidolonVrmaTrack *before_tracks[2];
    EidolonSemanticMotionAsset asset;
    EidolonEprMotionCatalog before;
    char error[EIDOLON_SEMANTIC_MOTION_PACK_ERROR_CAPACITY];

    eidolon_semantic_motion_pack_init(&pack);
    make_owned_clip(&clips[0], 0.2F);
    make_owned_clip(&clips[1], 0.4F);
    before_tracks[0] = clips[0].tracks;
    before_tracks[1] = clips[1].tracks;
    bindings[0] = binding(EIDOLON_EPR_MOTION_POSTURE_THINKING, UINT64_C(0x606), true);
    bindings[1] = binding(EIDOLON_EPR_MOTION_POSTURE_THINKING, UINT64_C(0x707), false);
    before = pack.catalog;

    assert(!eidolon_semantic_motion_pack_bind(&pack, bindings, clips, 2U, error, sizeof(error)));
    assert(strstr(error, "duplicated") != NULL);
    assert(eidolon_semantic_motion_pack_validate(&pack));
    assert(memcmp(&pack.catalog, &before, sizeof(before)) == 0);
    assert(clips[0].tracks == before_tracks[0]);
    assert(clips[1].tracks == before_tracks[1]);

    asset.binding = binding(EIDOLON_EPR_MOTION_POSTURE_RESPONDING, UINT64_C(0x808), true);
    asset.path = "build/fixtures/vrma/does-not-exist.vrma";
    assert(!eidolon_semantic_motion_pack_load(&pack, &asset, 1U, error, sizeof(error)));
    assert(strstr(error, "could not load") != NULL);
    assert(eidolon_semantic_motion_pack_validate(&pack));
    assert(memcmp(&pack.catalog, &before, sizeof(before)) == 0);

    eidolon_vrma_clip_destroy(&clips[0]);
    eidolon_vrma_clip_destroy(&clips[1]);
    eidolon_semantic_motion_pack_destroy(&pack);
}

int main(void) {
    test_atomic_commit_rebases_borrowed_sources();
    test_invalid_batch_rolls_back_every_clip();
    test_duplicate_and_file_failure_preserve_state();
    test_nested_allocation_alias_rolls_back();
    puts("semantic motion pack tests passed");
    return 0;
}
