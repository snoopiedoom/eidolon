#include "epr/body_resources.h"
#include "epr/motion_catalog.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct SampleFixture {
    uint64_t rotation_mask;
    bool has_hips_translation;
    bool fail;
    bool invalid;
} SampleFixture;

static uint64_t role_bit(EidolonHumanoidRole role) { return UINT64_C(1) << (uint32_t)role; }

static bool sample_fixture(const void *context, float seconds, bool loop,
                           EidolonHumanoidPose *pose) {
    const SampleFixture *fixture = context;
    if (fixture == NULL || pose == NULL || fixture->fail) {
        return false;
    }
    eidolon_humanoid_pose_init(pose);
    pose->rotation_mask = fixture->rotation_mask;
    pose->has_hips_translation = fixture->has_hips_translation;
    pose->hips_translation[0] = seconds;
    pose->hips_translation[1] = loop ? 1.0F : 0.0F;
    if (fixture->invalid && fixture->rotation_mask != 0U) {
        for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
            if ((fixture->rotation_mask & role_bit((EidolonHumanoidRole)role)) != 0U) {
                pose->rotations[role][3] = NAN;
                break;
            }
        }
    }
    return true;
}

static EidolonEprMotionGeneratorReference make_reference(EidolonEprMotionGeneratorId generator,
                                                         uint32_t resource_mask) {
    EidolonEprMotionGeneratorReference reference;
    memset(&reference, 0, sizeof(reference));
    reference.version = EIDOLON_EPR_MOTION_GENERATOR_REFERENCE_VERSION;
    reference.generator = generator;
    reference.takeover = EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE;
    reference.resource_mask = resource_mask;
    reference.blend_weight = 0.75F;
    reference.intensity = 0.5F;
    reference.playback_rate = 1.25F;
    assert(eidolon_epr_resource_mask_humanoid_channels(
        resource_mask, &reference.humanoid_rotation_mask, &reference.owns_hips_translation));
    assert(eidolon_epr_motion_generator_reference_validate(&reference));
    return reference;
}

static EidolonEprMotionCatalogEntry make_entry(EidolonEprMotionGeneratorId generator,
                                               uint64_t identity, uint64_t rotation_mask,
                                               bool owns_hips_translation, bool loop,
                                               const SampleFixture *fixture) {
    EidolonEprMotionCatalogEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.generator = generator;
    entry.source.version = EIDOLON_EPR_MOTION_SOURCE_VERSION;
    entry.source.identity = identity;
    entry.source.context = fixture;
    entry.source.sample = sample_fixture;
    entry.source.rotation_mask = rotation_mask;
    entry.source.duration_seconds = 2.0F;
    entry.source.owns_hips_translation = owns_hips_translation;
    entry.source.loop = loop;
    return entry;
}

static void test_resolve_and_sample_intersect_ownership(void) {
    const uint64_t right_upper = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM);
    const uint64_t right_lower = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM);
    const uint64_t right_hand = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_HAND);
    const uint64_t left_upper = role_bit(EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM);
    const uint32_t right_arm_resource = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    SampleFixture fixture = {
        .rotation_mask = right_upper | left_upper,
        .has_hips_translation = true,
    };
    EidolonEprMotionCatalog catalog;
    EidolonEprMotionCatalogEntry entry =
        make_entry(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, UINT64_C(0x1234),
                   right_upper | right_lower | right_hand | left_upper, true, true, &fixture);
    EidolonEprMotionGeneratorReference reference =
        make_reference(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, right_arm_resource);
    EidolonEprMotionBinding binding;
    EidolonEprMotionSample sample;
    EidolonEprMotionSample repeated;

    eidolon_epr_motion_catalog_init(&catalog);
    assert(eidolon_epr_motion_catalog_validate(&catalog));
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) == EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(binding.version == EIDOLON_EPR_MOTION_BINDING_VERSION);
    assert(binding.rotation_mask == (right_upper | right_lower | right_hand));
    assert(!binding.owns_hips_translation);
    assert(binding.source.identity == UINT64_C(0x1234));
    assert(binding.resource_mask == right_arm_resource);
    assert(fabsf(binding.blend_weight - 0.75F) < 0.0001F);
    assert(fabsf(binding.intensity - 0.5F) < 0.0001F);
    assert(fabsf(binding.playback_rate - 1.25F) < 0.0001F);

    assert(eidolon_epr_motion_binding_sample(&binding, 0.625F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(eidolon_epr_motion_binding_sample(&binding, 0.625F, &repeated) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(memcmp(&sample, &repeated, sizeof(sample)) == 0);
    assert(sample.version == EIDOLON_EPR_MOTION_SAMPLE_VERSION);
    assert(sample.source_identity == UINT64_C(0x1234));
    assert(sample.pose.rotation_mask == right_upper);
    assert(!sample.pose.has_hips_translation);
    assert(sample.pose.rotations[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM][3] == 1.0F);
    assert(fabsf(sample.blend_weight - reference.blend_weight) < 0.0001F);
    assert(fabsf(sample.intensity - reference.intensity) < 0.0001F);
    assert(sample.resource_mask == right_arm_resource);
    assert(fabsf(sample.source_seconds - 0.625F) < 0.0001F);
    assert(fabsf(sample.source_duration_seconds - 2.0F) < 0.0001F);
    assert(sample.source_loop);
}

static void test_hips_ownership_and_sample_rollback(void) {
    const uint64_t spine = role_bit(EIDOLON_HUMANOID_ROLE_SPINE);
    const uint32_t torso_resource = UINT32_C(1) << EIDOLON_EPR_RESOURCE_TORSO;
    SampleFixture fixture = {
        .rotation_mask = spine,
        .has_hips_translation = true,
    };
    EidolonEprMotionCatalog catalog;
    EidolonEprMotionCatalogEntry entry = make_entry(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE,
                                                    UINT64_C(0x2345), spine, true, false, &fixture);
    EidolonEprMotionGeneratorReference reference =
        make_reference(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, torso_resource);
    EidolonEprMotionBinding binding;
    EidolonEprMotionSample sample;
    EidolonEprMotionSample before;

    eidolon_epr_motion_catalog_init(&catalog);
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) == EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(binding.rotation_mask == spine);
    assert(binding.owns_hips_translation);
    assert(eidolon_epr_motion_binding_sample(&binding, 0.25F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(sample.pose.rotation_mask == spine);
    assert(sample.pose.has_hips_translation);
    assert(fabsf(sample.pose.hips_translation[0] - 0.25F) < 0.0001F);
    assert(sample.pose.hips_translation[1] == 0.0F);

    before = sample;
    fixture.fail = true;
    assert(eidolon_epr_motion_binding_sample(&binding, 0.5F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED);
    assert(memcmp(&sample, &before, sizeof(sample)) == 0);
    fixture.fail = false;
    fixture.invalid = true;
    assert(eidolon_epr_motion_binding_sample(&binding, 0.5F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_SAMPLE_FAILED);
    assert(memcmp(&sample, &before, sizeof(sample)) == 0);
    fixture.invalid = false;
    binding.playback_rate = NAN;
    assert(eidolon_epr_motion_binding_sample(&binding, 0.5F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_INVALID_BINDING);
    assert(memcmp(&sample, &before, sizeof(sample)) == 0);
}

static void test_typed_resolution_failures_are_transactional(void) {
    const uint64_t head = role_bit(EIDOLON_HUMANOID_ROLE_HEAD);
    const uint32_t head_resource = UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD;
    SampleFixture fixture = {.rotation_mask = head};
    EidolonEprMotionCatalog catalog;
    EidolonEprMotionCatalogEntry entry = make_entry(EIDOLON_EPR_MOTION_POSTURE_THINKING,
                                                    UINT64_C(0x3456), head, false, false, &fixture);
    EidolonEprMotionGeneratorReference reference =
        make_reference(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, head_resource);
    EidolonEprMotionBinding binding;
    EidolonEprMotionBinding before;

    eidolon_epr_motion_catalog_init(&catalog);
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) == EIDOLON_EPR_MOTION_CATALOG_OK);
    memset(&binding, 0x5a, sizeof(binding));
    before = binding;
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_MISSING_GENERATOR);
    assert(memcmp(&binding, &before, sizeof(binding)) == 0);

    reference.generator = EIDOLON_EPR_MOTION_GENERATOR_NONE;
    reference.takeover = EIDOLON_EPR_MOTION_TAKEOVER_NONE;
    reference.resource_mask = 0U;
    reference.humanoid_rotation_mask = 0U;
    reference.blend_weight = 0.0F;
    reference.intensity = 0.0F;
    reference.playback_rate = 0.0F;
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_NOT_REQUIRED);
    assert(memcmp(&binding, &before, sizeof(binding)) == 0);

    reference = make_reference(EIDOLON_EPR_MOTION_POSTURE_THINKING, head_resource);
    entry.source.rotation_mask = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_HAND);
    eidolon_epr_motion_catalog_init(&catalog);
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) == EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_NO_CHANNELS);
    assert(memcmp(&binding, &before, sizeof(binding)) == 0);
}

static void test_catalog_rejects_invalid_and_duplicate_sources(void) {
    const uint64_t head = role_bit(EIDOLON_HUMANOID_ROLE_HEAD);
    SampleFixture fixture = {.rotation_mask = head};
    EidolonEprMotionCatalog catalog;
    EidolonEprMotionCatalog before;
    EidolonEprMotionCatalogEntry entry = make_entry(EIDOLON_EPR_MOTION_POSTURE_THINKING,
                                                    UINT64_C(0x4567), head, false, false, &fixture);

    eidolon_epr_motion_catalog_init(&catalog);
    before = catalog;
    entry.source.duration_seconds = NAN;
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) ==
           EIDOLON_EPR_MOTION_CATALOG_INVALID_SOURCE);
    assert(memcmp(&catalog, &before, sizeof(catalog)) == 0);
    entry.source.duration_seconds = 1.0F;
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) == EIDOLON_EPR_MOTION_CATALOG_OK);
    before = catalog;
    assert(eidolon_epr_motion_catalog_add(&catalog, &entry) ==
           EIDOLON_EPR_MOTION_CATALOG_DUPLICATE_GENERATOR);
    assert(memcmp(&catalog, &before, sizeof(catalog)) == 0);
    catalog.entries[0].source.rotation_mask = UINT64_MAX;
    assert(!eidolon_epr_motion_catalog_validate(&catalog));
}

int main(void) {
    test_resolve_and_sample_intersect_ownership();
    test_hips_ownership_and_sample_rollback();
    test_typed_resolution_failures_are_transactional();
    test_catalog_rejects_invalid_and_duplicate_sources();
    assert(strcmp(eidolon_epr_motion_catalog_status_name(EIDOLON_EPR_MOTION_CATALOG_NO_CHANNELS),
                  "no_channels") == 0);
    puts("EPR motion catalog tests passed");
    return 0;
}
