#include "humanoid_pose.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

static uint64_t role_bit(EidolonHumanoidRole role) { return UINT64_C(1) << (uint32_t)role; }

static void set_rotation(EidolonHumanoidPose *pose, EidolonHumanoidRole role, float x, float y,
                         float z, float radians) {
    pose->rotation_mask |= role_bit(role);
    quaternion_axis(x, y, z, radians, pose->rotations[(size_t)role]);
}

static void test_masked_layers_preserve_unowned_channels(void) {
    EidolonHumanoidPose base;
    EidolonHumanoidPose upper;
    EidolonHumanoidPose arm;
    EidolonHumanoidPose result;
    EidolonHumanoidPose repeated;
    EidolonHumanoidPoseLayer layers[2];
    float expected[4];

    eidolon_humanoid_pose_init(&base);
    set_rotation(&base, EIDOLON_HUMANOID_ROLE_HEAD, 0.0F, 1.0F, 0.0F, 0.52359877559F);
    set_rotation(&base, EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM, 1.0F, 0.0F, 0.0F, 0.34906585040F);
    base.has_hips_translation = true;
    base.hips_translation[0] = 1.0F;
    base.hips_translation[1] = 2.0F;
    base.hips_translation[2] = 3.0F;

    eidolon_humanoid_pose_init(&upper);
    set_rotation(&upper, EIDOLON_HUMANOID_ROLE_HEAD, 0.0F, 1.0F, 0.0F, 1.57079632679F);
    set_rotation(&upper, EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM, 1.0F, 0.0F, 0.0F, 1.57079632679F);
    eidolon_humanoid_pose_layer_init(&layers[0], &upper);
    layers[0].rotation_mask = role_bit(EIDOLON_HUMANOID_ROLE_HEAD);
    layers[0].weight = 0.5F;

    eidolon_humanoid_pose_init(&arm);
    set_rotation(&arm, EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM, 0.0F, 0.0F, 1.0F, 1.57079632679F);
    arm.has_hips_translation = true;
    arm.hips_translation[0] = 3.0F;
    arm.hips_translation[1] = 4.0F;
    arm.hips_translation[2] = 5.0F;
    eidolon_humanoid_pose_layer_init(&layers[1], &arm);
    layers[1].weight = 0.5F;

    assert(eidolon_humanoid_pose_compose(&base, layers, 2U, &result));
    assert(eidolon_humanoid_pose_compose(&base, layers, 2U, &repeated));
    assert(memcmp(&result, &repeated, sizeof(result)) == 0);

    quaternion_axis(0.0F, 1.0F, 0.0F, 1.04719755120F, expected);
    assert(quaternion_near(result.rotations[EIDOLON_HUMANOID_ROLE_HEAD], expected));
    assert(memcmp(result.rotations[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM],
                  base.rotations[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM],
                  sizeof(base.rotations[0])) == 0);
    quaternion_axis(0.0F, 0.0F, 1.0F, 0.78539816339F, expected);
    assert(quaternion_near(result.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], expected));
    assert(result.rotation_mask ==
           (base.rotation_mask | role_bit(EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM)));
    assert(result.has_hips_translation);
    assert(fabsf(result.hips_translation[0] - 2.0F) < 0.0001F);
    assert(fabsf(result.hips_translation[1] - 3.0F) < 0.0001F);
    assert(fabsf(result.hips_translation[2] - 4.0F) < 0.0001F);
}

static void test_shortest_arc_and_layer_order(void) {
    EidolonHumanoidPose base;
    EidolonHumanoidPose first;
    EidolonHumanoidPose second;
    EidolonHumanoidPose result;
    EidolonHumanoidPoseLayer layers[2];
    float expected[4];

    eidolon_humanoid_pose_init(&base);
    eidolon_humanoid_pose_init(&first);
    set_rotation(&first, EIDOLON_HUMANOID_ROLE_HEAD, 0.0F, 1.0F, 0.0F, 1.57079632679F);
    for (size_t component = 0U; component < 4U; ++component) {
        first.rotations[EIDOLON_HUMANOID_ROLE_HEAD][component] *= -1.0F;
    }
    eidolon_humanoid_pose_init(&second);
    set_rotation(&second, EIDOLON_HUMANOID_ROLE_HEAD, 1.0F, 0.0F, 0.0F, 0.34906585040F);

    eidolon_humanoid_pose_layer_init(&layers[0], &first);
    layers[0].weight = 0.5F;
    eidolon_humanoid_pose_layer_init(&layers[1], &second);
    assert(eidolon_humanoid_pose_compose(&base, layers, 2U, &result));
    assert(quaternion_near(result.rotations[EIDOLON_HUMANOID_ROLE_HEAD],
                           second.rotations[EIDOLON_HUMANOID_ROLE_HEAD]));

    assert(eidolon_humanoid_pose_compose(&base, layers, 1U, &result));
    quaternion_axis(0.0F, 1.0F, 0.0F, 0.78539816339F, expected);
    assert(quaternion_near(result.rotations[EIDOLON_HUMANOID_ROLE_HEAD], expected));
}

static void test_additive_mode_and_intensity_are_distinct(void) {
    EidolonHumanoidPose base;
    EidolonHumanoidPose contribution;
    EidolonHumanoidPose result;
    EidolonHumanoidPoseLayer layer;
    float expected[4];

    eidolon_humanoid_pose_init(&base);
    set_rotation(&base, EIDOLON_HUMANOID_ROLE_HEAD, 1.0F, 0.0F, 0.0F, 0.52359877559F);
    base.has_hips_translation = true;
    base.hips_translation[0] = 1.0F;

    eidolon_humanoid_pose_init(&contribution);
    set_rotation(&contribution, EIDOLON_HUMANOID_ROLE_HEAD, 1.0F, 0.0F, 0.0F, 0.69813170080F);
    contribution.has_hips_translation = true;
    contribution.hips_translation[0] = 2.0F;

    eidolon_humanoid_pose_layer_init(&layer, &contribution);
    layer.mode = EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE;
    layer.weight = 0.5F;
    layer.intensity = 0.5F;
    assert(eidolon_humanoid_pose_compose(&base, &layer, 1U, &result));
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.69813170080F, expected);
    assert(quaternion_near(result.rotations[EIDOLON_HUMANOID_ROLE_HEAD], expected));
    assert(fabsf(result.hips_translation[0] - 1.5F) < 0.0001F);

    layer.mode = EIDOLON_HUMANOID_POSE_LAYER_ABSOLUTE;
    assert(eidolon_humanoid_pose_compose(&base, &layer, 1U, &result));
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.43633231299F, expected);
    assert(quaternion_near(result.rotations[EIDOLON_HUMANOID_ROLE_HEAD], expected));
    assert(fabsf(result.hips_translation[0] - 1.0F) < 0.0001F);
}

static void test_invalid_layer_rolls_back(void) {
    EidolonHumanoidPose base;
    EidolonHumanoidPose layer_pose;
    EidolonHumanoidPose result;
    EidolonHumanoidPose before;
    EidolonHumanoidPoseLayer layer;

    eidolon_humanoid_pose_init(&base);
    set_rotation(&base, EIDOLON_HUMANOID_ROLE_HEAD, 0.0F, 1.0F, 0.0F, 0.2F);
    eidolon_humanoid_pose_init(&layer_pose);
    set_rotation(&layer_pose, EIDOLON_HUMANOID_ROLE_RIGHT_HAND, 1.0F, 0.0F, 0.0F, 0.5F);
    eidolon_humanoid_pose_init(&result);
    result.hips_translation[0] = 17.0F;
    before = result;

    eidolon_humanoid_pose_layer_init(&layer, &layer_pose);
    layer.rotation_mask |= role_bit(EIDOLON_HUMANOID_ROLE_LEFT_HAND);
    assert(!eidolon_humanoid_pose_compose(&base, &layer, 1U, &result));
    assert(memcmp(&result, &before, sizeof(result)) == 0);

    layer.rotation_mask = layer_pose.rotation_mask;
    layer.weight = NAN;
    assert(!eidolon_humanoid_pose_compose(&base, &layer, 1U, &result));
    assert(memcmp(&result, &before, sizeof(result)) == 0);

    layer.weight = 1.0F;
    layer.intensity = 1.1F;
    assert(!eidolon_humanoid_pose_compose(&base, &layer, 1U, &result));
    assert(memcmp(&result, &before, sizeof(result)) == 0);

    layer.intensity = 1.0F;
    layer.mode = EIDOLON_HUMANOID_POSE_LAYER_MODE_COUNT;
    assert(!eidolon_humanoid_pose_compose(&base, &layer, 1U, &result));
    assert(memcmp(&result, &before, sizeof(result)) == 0);
}

int main(void) {
    test_masked_layers_preserve_unowned_channels();
    test_shortest_arc_and_layer_order();
    test_additive_mode_and_intensity_are_distinct();
    test_invalid_layer_rolls_back();
    puts("humanoid pose tests passed");
    return 0;
}
