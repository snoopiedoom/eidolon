#include "epr/body_resources.h"
#include "epr/motion_catalog.h"
#include "humanoid_rest.h"
#include "vrma_motion_source.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct MotionFixture {
    EidolonVrmaClip clip;
    EidolonVrmaTrack tracks[2];
    float times[2];
    float rotation_values[8];
    float hips_values[6];
} MotionFixture;

static uint64_t role_bit(EidolonHumanoidRole role) { return UINT64_C(1) << (uint32_t)role; }

static uint64_t required_role_mask(void) {
    uint64_t mask = 0U;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
            mask |= role_bit((EidolonHumanoidRole)role);
        }
    }
    return mask;
}

static void quaternion_axis(float x, float y, float z, float radians, float result[4]) {
    const float sine = sinf(radians * 0.5F);
    result[0] = x * sine;
    result[1] = y * sine;
    result[2] = z * sine;
    result[3] = cosf(radians * 0.5F);
}

static bool quaternion_near(const float left[4], const float right[4]) {
    float direct = 0.0F;
    float negated = 0.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        const float same = left[component] - right[component];
        const float opposite = left[component] + right[component];
        direct += same * same;
        negated += opposite * opposite;
    }
    return fminf(direct, negated) < 0.00001F;
}

static bool vector_near(const float left[3], const float right[3]) {
    for (size_t axis = 0U; axis < 3U; ++axis) {
        if (fabsf(left[axis] - right[axis]) >= 0.0001F) {
            return false;
        }
    }
    return true;
}

static void initialize_fixture(MotionFixture *fixture, float hips_height, const float rest_hips[3],
                               const float rest_local[4], const float rest_world[4],
                               const float normalized_end[4], const float normalized_hips_end[3]) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    memset(fixture, 0, sizeof(*fixture));
    fixture->clip.version = EIDOLON_VRMA_CLIP_VERSION;
    fixture->clip.mapped_roles = required_role_mask();
    fixture->clip.source_hips_height = hips_height;
    fixture->clip.duration_seconds = 1.0F;
    fixture->clip.tracks = fixture->tracks;
    fixture->clip.track_count = 2U;
    memcpy(fixture->clip.rest_hips_translation, rest_hips,
           sizeof(fixture->clip.rest_hips_translation));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        memcpy(fixture->clip.rest_local_rotations[role], identity, sizeof(identity));
        memcpy(fixture->clip.rest_world_rotations[role], identity, sizeof(identity));
        fixture->clip.source_node_by_role[role] = -1;
    }
    memcpy(fixture->clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], rest_local,
           sizeof(float) * 4U);
    memcpy(fixture->clip.rest_world_rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], rest_world,
           sizeof(float) * 4U);

    fixture->times[0] = 0.0F;
    fixture->times[1] = 1.0F;
    assert(eidolon_humanoid_rest_rotation_from_normalized(rest_local, rest_world, identity,
                                                          &fixture->rotation_values[0]));
    assert(eidolon_humanoid_rest_rotation_from_normalized(rest_local, rest_world, normalized_end,
                                                          &fixture->rotation_values[4]));
    assert(eidolon_humanoid_rest_translation_from_normalized(
        rest_hips, hips_height, (const float[3]){0.0F, 0.0F, 0.0F}, &fixture->hips_values[0]));
    assert(eidolon_humanoid_rest_translation_from_normalized(
        rest_hips, hips_height, normalized_hips_end, &fixture->hips_values[3]));

    fixture->tracks[0].role = EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM;
    fixture->tracks[0].path = EIDOLON_VRMA_TRACK_ROTATION;
    fixture->tracks[0].interpolation = EIDOLON_VRMA_INTERPOLATION_LINEAR;
    fixture->tracks[0].times = fixture->times;
    fixture->tracks[0].values = fixture->rotation_values;
    fixture->tracks[0].key_count = 2U;
    fixture->tracks[0].component_count = 4U;
    fixture->tracks[0].values_per_key = 1U;

    fixture->tracks[1].role = EIDOLON_HUMANOID_ROLE_HIPS;
    fixture->tracks[1].path = EIDOLON_VRMA_TRACK_HIPS_TRANSLATION;
    fixture->tracks[1].interpolation = EIDOLON_VRMA_INTERPOLATION_LINEAR;
    fixture->tracks[1].times = fixture->times;
    fixture->tracks[1].values = fixture->hips_values;
    fixture->tracks[1].key_count = 2U;
    fixture->tracks[1].component_count = 3U;
    fixture->tracks[1].values_per_key = 1U;
}

static EidolonEprMotionGeneratorReference make_reference(EidolonEprMotionGeneratorId generator,
                                                         uint32_t resource_mask) {
    EidolonEprMotionGeneratorReference reference;
    memset(&reference, 0, sizeof(reference));
    reference.version = EIDOLON_EPR_MOTION_GENERATOR_REFERENCE_VERSION;
    reference.generator = generator;
    reference.takeover = EIDOLON_EPR_MOTION_TAKEOVER_OVERRIDE;
    reference.resource_mask = resource_mask;
    reference.blend_weight = 1.0F;
    reference.intensity = 1.0F;
    reference.playback_rate = 1.0F;
    assert(eidolon_epr_resource_mask_humanoid_channels(
        resource_mask, &reference.humanoid_rotation_mask, &reference.owns_hips_translation));
    assert(eidolon_epr_motion_generator_reference_validate(&reference));
    return reference;
}

static void test_authored_rest_invariance(void) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    static const float rest_hips_a[3] = {0.0F, 1.0F, 0.0F};
    static const float rest_hips_b[3] = {0.1F, 2.0F, -0.2F};
    static const float normalized_hips[3] = {0.2F, 0.1F, -0.3F};
    float normalized_rotation[4];
    float rest_local_b[4];
    float rest_world_b[4];
    MotionFixture fixture_a;
    MotionFixture fixture_b;
    EidolonHumanoidPose pose_a;
    EidolonHumanoidPose pose_b;
    EidolonHumanoidPose start;
    EidolonHumanoidPose looped;
    EidolonHumanoidPose quarter;
    char error[EIDOLON_VRMA_MOTION_SOURCE_ERROR_CAPACITY];

    quaternion_axis(0.0F, 1.0F, 0.0F, 0.78539816339F, normalized_rotation);
    quaternion_axis(0.0F, 0.0F, 1.0F, 1.57079632679F, rest_local_b);
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.52359877559F, rest_world_b);
    initialize_fixture(&fixture_a, 1.0F, rest_hips_a, identity, identity, normalized_rotation,
                       normalized_hips);
    initialize_fixture(&fixture_b, 2.0F, rest_hips_b, rest_local_b, rest_world_b,
                       normalized_rotation, normalized_hips);

    assert(eidolon_vrma_motion_sample_normalized(&fixture_a.clip, 1.0F, false, &pose_a, error,
                                                 sizeof(error)));
    assert(eidolon_vrma_motion_sample_normalized(&fixture_b.clip, 1.0F, false, &pose_b, error,
                                                 sizeof(error)));
    assert(pose_a.rotation_mask == role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM));
    assert(pose_b.rotation_mask == pose_a.rotation_mask);
    assert(quaternion_near(pose_a.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM],
                           normalized_rotation));
    assert(quaternion_near(pose_b.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM],
                           normalized_rotation));
    assert(pose_a.has_hips_translation && pose_b.has_hips_translation);
    assert(vector_near(pose_a.hips_translation, normalized_hips));
    assert(vector_near(pose_b.hips_translation, normalized_hips));

    assert(eidolon_vrma_motion_sample_normalized(&fixture_b.clip, 0.0F, false, &start, error,
                                                 sizeof(error)));
    assert(quaternion_near(start.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], identity));
    assert(vector_near(start.hips_translation, (const float[3]){0.0F, 0.0F, 0.0F}));
    assert(eidolon_vrma_motion_sample_normalized(&fixture_b.clip, 1.25F, true, &looped, error,
                                                 sizeof(error)));
    assert(eidolon_vrma_motion_sample_normalized(&fixture_b.clip, 0.25F, false, &quarter, error,
                                                 sizeof(error)));
    assert(quaternion_near(looped.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM],
                           quarter.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM]));
    assert(vector_near(looped.hips_translation, quarter.hips_translation));
}

static void test_catalog_binding_uses_track_ownership(void) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    static const float rest_hips[3] = {0.0F, 1.0F, 0.0F};
    static const float normalized_hips[3] = {0.2F, 0.1F, -0.3F};
    const uint64_t right_upper = role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM);
    const uint32_t right_arm_resource = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    const uint32_t torso_resource = UINT32_C(1) << EIDOLON_EPR_RESOURCE_TORSO;
    float normalized_rotation[4];
    MotionFixture fixture;
    EidolonEprMotionSource source;
    EidolonEprMotionCatalog catalog;
    EidolonEprMotionCatalogEntry gesture_entry;
    EidolonEprMotionCatalogEntry posture_entry;
    EidolonEprMotionBinding binding;
    EidolonEprMotionSample sample;
    EidolonEprMotionGeneratorReference reference;
    char error[EIDOLON_VRMA_MOTION_SOURCE_ERROR_CAPACITY];

    quaternion_axis(0.0F, 1.0F, 0.0F, 0.78539816339F, normalized_rotation);
    initialize_fixture(&fixture, 1.0F, rest_hips, identity, identity, normalized_rotation,
                       normalized_hips);
    assert(eidolon_vrma_motion_source_build(&fixture.clip, UINT64_C(0xabc123), false, &source,
                                            error, sizeof(error)));
    assert(source.rotation_mask == right_upper);
    assert(source.owns_hips_translation);
    assert(source.duration_seconds == 1.0F);
    assert(!source.loop);

    memset(&gesture_entry, 0, sizeof(gesture_entry));
    gesture_entry.generator = EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT;
    gesture_entry.source = source;
    posture_entry = gesture_entry;
    posture_entry.generator = EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE;
    eidolon_epr_motion_catalog_init(&catalog);
    assert(eidolon_epr_motion_catalog_add(&catalog, &gesture_entry) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(eidolon_epr_motion_catalog_add(&catalog, &posture_entry) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);

    reference = make_reference(EIDOLON_EPR_MOTION_GESTURE_CONTRAST_RIGHT, right_arm_resource);
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(binding.rotation_mask == right_upper);
    assert(!binding.owns_hips_translation);
    assert(eidolon_epr_motion_binding_sample(&binding, 1.0F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(sample.pose.rotation_mask == right_upper);
    assert(!sample.pose.has_hips_translation);
    assert(quaternion_near(sample.pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM],
                           normalized_rotation));

    reference = make_reference(EIDOLON_EPR_MOTION_POSTURE_ATTENTIVE, torso_resource);
    assert(eidolon_epr_motion_catalog_resolve(&catalog, &reference, &binding) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(binding.rotation_mask == 0U);
    assert(binding.owns_hips_translation);
    assert(eidolon_epr_motion_binding_sample(&binding, 1.0F, &sample) ==
           EIDOLON_EPR_MOTION_CATALOG_OK);
    assert(sample.pose.rotation_mask == 0U);
    assert(sample.pose.has_hips_translation);
    assert(vector_near(sample.pose.hips_translation, normalized_hips));
}

static void test_invalid_source_and_sample_rollback(void) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    static const float rest_hips[3] = {0.0F, 1.0F, 0.0F};
    static const float normalized_hips[3] = {0.2F, 0.1F, -0.3F};
    float normalized_rotation[4];
    MotionFixture fixture;
    EidolonEprMotionSource source;
    EidolonEprMotionSource source_before;
    EidolonHumanoidPose pose;
    EidolonHumanoidPose pose_before;
    char error[EIDOLON_VRMA_MOTION_SOURCE_ERROR_CAPACITY];

    quaternion_axis(0.0F, 1.0F, 0.0F, 0.78539816339F, normalized_rotation);
    initialize_fixture(&fixture, 1.0F, rest_hips, identity, identity, normalized_rotation,
                       normalized_hips);
    memset(&source, 0x5a, sizeof(source));
    source_before = source;
    assert(
        !eidolon_vrma_motion_source_build(&fixture.clip, 0U, false, &source, error, sizeof(error)));
    assert(strstr(error, "identity") != NULL);
    assert(memcmp(&source, &source_before, sizeof(source)) == 0);

    fixture.clip.duration_seconds = 2.0F;
    assert(
        !eidolon_vrma_motion_source_build(&fixture.clip, 7U, false, &source, error, sizeof(error)));
    assert(strstr(error, "duration") != NULL);
    assert(memcmp(&source, &source_before, sizeof(source)) == 0);
    fixture.clip.duration_seconds = 1.0F;

    fixture.clip.mapped_roles &= ~role_bit(EIDOLON_HUMANOID_ROLE_LEFT_HAND);
    assert(
        !eidolon_vrma_motion_source_build(&fixture.clip, 7U, false, &source, error, sizeof(error)));
    assert(strstr(error, "required") != NULL);
    assert(memcmp(&source, &source_before, sizeof(source)) == 0);
    fixture.clip.mapped_roles |= role_bit(EIDOLON_HUMANOID_ROLE_LEFT_HAND);

    fixture.tracks[1].role = EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM;
    fixture.tracks[1].path = EIDOLON_VRMA_TRACK_ROTATION;
    fixture.tracks[1].values = fixture.rotation_values;
    fixture.tracks[1].component_count = 4U;
    assert(
        !eidolon_vrma_motion_source_build(&fixture.clip, 7U, false, &source, error, sizeof(error)));
    assert(strstr(error, "duplicate") != NULL || strstr(error, "ownership") != NULL);
    assert(memcmp(&source, &source_before, sizeof(source)) == 0);
    fixture.tracks[1].role = EIDOLON_HUMANOID_ROLE_HIPS;
    fixture.tracks[1].path = EIDOLON_VRMA_TRACK_HIPS_TRANSLATION;
    fixture.tracks[1].values = fixture.hips_values;
    fixture.tracks[1].component_count = 3U;

    memset(fixture.clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], 0,
           sizeof(fixture.clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM]));
    assert(
        !eidolon_vrma_motion_source_build(&fixture.clip, 7U, false, &source, error, sizeof(error)));
    assert(strstr(error, "rest frame") != NULL);
    assert(memcmp(&source, &source_before, sizeof(source)) == 0);
    memcpy(fixture.clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], identity,
           sizeof(identity));

    eidolon_humanoid_pose_init(&pose);
    pose.hips_translation[0] = 17.0F;
    pose_before = pose;
    fixture.rotation_values[7] = NAN;
    assert(!eidolon_vrma_motion_sample_normalized(&fixture.clip, 1.0F, false, &pose, error,
                                                  sizeof(error)));
    assert(memcmp(&pose, &pose_before, sizeof(pose)) == 0);
    fixture.rotation_values[7] = normalized_rotation[3];
    fixture.tracks[0].times = NULL;
    assert(!eidolon_vrma_motion_sample_normalized(&fixture.clip, 1.0F, false, &pose, error,
                                                  sizeof(error)));
    assert(strstr(error, "sample shape") != NULL);
    assert(memcmp(&pose, &pose_before, sizeof(pose)) == 0);
}

static void test_rest_conversion_is_transactional(void) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    static const float zero[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    float result[4] = {1.0F, 2.0F, 3.0F, 4.0F};
    float before[4];
    float translation[3] = {1.0F, 2.0F, 3.0F};
    float translation_before[3];
    memcpy(before, result, sizeof(before));
    memcpy(translation_before, translation, sizeof(translation_before));
    assert(!eidolon_humanoid_rest_rotation_to_normalized(zero, identity, identity, result));
    assert(memcmp(result, before, sizeof(result)) == 0);
    assert(!eidolon_humanoid_rest_translation_to_normalized(
        (const float[3]){0.0F, 1.0F, 0.0F}, 0.0F, (const float[3]){0.0F, 1.0F, 0.0F}, translation));
    assert(memcmp(translation, translation_before, sizeof(translation)) == 0);
}

int main(void) {
    test_authored_rest_invariance();
    test_catalog_binding_uses_track_ownership();
    test_invalid_source_and_sample_rollback();
    test_rest_conversion_is_transactional();
    puts("VRMA motion source tests passed");
    return 0;
}
