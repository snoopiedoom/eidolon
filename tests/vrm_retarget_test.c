#include "vrm_retarget.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DESTINATION_NODE_COUNT 17U

static void quaternion_axis(float x, float y, float z, float radians, float result[4]) {
    const float sine = sinf(radians * 0.5F);
    result[0] = x * sine;
    result[1] = y * sine;
    result[2] = z * sine;
    result[3] = cosf(radians * 0.5F);
}

static void quaternion_multiply(const float left[4], const float right[4], float result[4]) {
    const float product[4] = {
        left[3] * right[0] + left[0] * right[3] + left[1] * right[2] - left[2] * right[1],
        left[3] * right[1] - left[0] * right[2] + left[1] * right[3] + left[2] * right[0],
        left[3] * right[2] + left[0] * right[1] - left[1] * right[0] + left[2] * right[3],
        left[3] * right[3] - left[0] * right[0] - left[1] * right[1] - left[2] * right[2],
    };
    memcpy(result, product, sizeof(product));
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

static void initialize_body(EidolonVrmBody *body) {
    memset(body, 0, sizeof(*body));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        body->node_by_role[role] = -1;
    }
    body->node_by_role[EIDOLON_HUMANOID_ROLE_HIPS] = 0;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_SPINE] = 1;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_NECK] = 2;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_HEAD] = 3;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG] = 4;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG] = 5;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_FOOT] = 6;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG] = 7;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG] = 8;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_FOOT] = 9;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM] = 10;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM] = 11;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_HAND] = 12;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM] = 13;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM] = 14;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_HAND] = 15;
}

static void set_node(EidolonMotionRig *rig, size_t index, const char *name, int parent, float x,
                     float y, float z, const float rotation[4]) {
    const float translation[3] = {x, y, z};
    const float scale[3] = {1.0F, 1.0F, 1.0F};
    assert(eidolon_motion_set_node(rig, index, name, parent, translation, rotation, scale));
}

static void initialize_destination_rig(EidolonMotionRig *rig) {
    const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    float authored_arm[4];
    quaternion_axis(0.0F, 0.0F, 1.0F, 1.57079632679F, authored_arm);
    assert(eidolon_motion_init(rig, DESTINATION_NODE_COUNT));
    set_node(rig, 0U, "hips", -1, 0.0F, 2.0F, 0.0F, identity);
    set_node(rig, 1U, "spine", 0, 0.0F, 0.25F, 0.0F, identity);
    set_node(rig, 2U, "neck", 1, 0.0F, 0.50F, 0.0F, identity);
    set_node(rig, 3U, "head", 2, 0.0F, 0.25F, 0.0F, identity);
    set_node(rig, 4U, "leftUpperLeg", 0, -0.10F, -0.40F, 0.0F, identity);
    set_node(rig, 5U, "leftLowerLeg", 4, 0.0F, -0.45F, 0.0F, identity);
    set_node(rig, 6U, "leftFoot", 5, 0.0F, -0.40F, -0.05F, identity);
    set_node(rig, 7U, "rightUpperLeg", 0, 0.10F, -0.40F, 0.0F, identity);
    set_node(rig, 8U, "rightLowerLeg", 7, 0.0F, -0.45F, 0.0F, identity);
    set_node(rig, 9U, "rightFoot", 8, 0.0F, -0.40F, -0.05F, identity);
    set_node(rig, 10U, "leftUpperArm", 1, -0.25F, 0.35F, 0.0F, identity);
    set_node(rig, 11U, "leftLowerArm", 10, -0.35F, 0.0F, 0.0F, identity);
    set_node(rig, 12U, "leftHand", 11, -0.30F, 0.0F, 0.0F, identity);
    set_node(rig, 13U, "rightUpperArm", 1, 0.25F, 0.35F, 0.0F, authored_arm);
    set_node(rig, 14U, "rightLowerArm", 13, 0.35F, 0.0F, 0.0F, identity);
    set_node(rig, 15U, "rightHand", 14, 0.30F, 0.0F, 0.0F, identity);
    set_node(rig, 16U, "headAccessory", 3, 0.0F, 0.20F, 0.0F, identity);
    assert(eidolon_motion_rebuild_world(rig));
}

static uint64_t required_role_mask(void) {
    uint64_t mask = 0U;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
            mask |= UINT64_C(1) << (uint32_t)role;
        }
    }
    return mask;
}

static void initialize_source(EidolonVrmaClip *clip) {
    memset(clip, 0, sizeof(*clip));
    clip->version = EIDOLON_VRMA_CLIP_VERSION;
    clip->mapped_roles = required_role_mask() | (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_UPPER_CHEST) |
                         (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_NECK);
    clip->source_hips_height = 1.0F;
    clip->rest_hips_translation[1] = 1.0F;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        clip->rest_local_rotations[role][3] = 1.0F;
        clip->rest_world_rotations[role][3] = 1.0F;
    }
}

static void initialize_pose(const EidolonVrmaClip *clip, EidolonHumanoidPose *pose) {
    eidolon_humanoid_pose_init(pose);
    pose->rotation_mask = clip->mapped_roles;
    pose->has_hips_translation = true;
    pose->hips_translation[0] = 1.0F;
    pose->hips_translation[1] = 1.5F;
    pose->hips_translation[2] = 2.0F;
    quaternion_axis(0.0F, 1.0F, 0.0F, 0.52359877559F,
                    pose->rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST]);
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.34906585040F, pose->rotations[EIDOLON_HUMANOID_ROLE_NECK]);
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.78539816339F,
                    pose->rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM]);
}

static void test_rest_conversion_optional_composition_and_root_policy(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmaClip clip;
    EidolonHumanoidPose pose;
    EidolonVrmRetargeter retargeter;
    char error[EIDOLON_VRM_RETARGET_ERROR_CAPACITY];
    float upper_chest[4];
    float neck[4];
    float expected_neck[4];
    float source_arm[4];
    float combined_arm[4];
    float destination_arm_rest[4];
    float expected_arm[4];

    initialize_body(&body);
    initialize_destination_rig(&rig);
    initialize_source(&clip);
    initialize_pose(&clip, &pose);
    assert(eidolon_vrm_retargeter_init(&retargeter, &clip, &body, &rig, error, sizeof(error)));
    assert(fabsf(retargeter.hips_translation_scale - 2.0F) < 0.0001F);

    rig.nodes[15].scale[0] = 1.25F;
    rig.nodes[15].scale[1] = 0.80F;
    rig.nodes[15].scale[2] = 1.10F;
    rig.nodes[16].translation[0] = 0.42F;
    quaternion_axis(0.0F, 0.0F, 1.0F, 0.2F, rig.nodes[16].rotation);
    assert(eidolon_motion_rebuild_world(&rig));

    assert(eidolon_vrm_retargeter_apply(&retargeter, &pose, EIDOLON_VRM_ROOT_MOTION_IN_PLACE, &rig,
                                        error, sizeof(error)));
    assert(fabsf(rig.nodes[0].translation[0]) < 0.0001F);
    assert(fabsf(rig.nodes[0].translation[1] - 3.0F) < 0.0001F);
    assert(fabsf(rig.nodes[0].translation[2]) < 0.0001F);

    memcpy(upper_chest, pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST], sizeof(upper_chest));
    memcpy(neck, pose.rotations[EIDOLON_HUMANOID_ROLE_NECK], sizeof(neck));
    quaternion_multiply(upper_chest, neck, expected_neck);
    assert(quaternion_near(rig.nodes[2].rotation, expected_neck));

    memcpy(source_arm, pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], sizeof(source_arm));
    memcpy(destination_arm_rest, rig.nodes[13].bind_rotation, sizeof(destination_arm_rest));
    quaternion_multiply(upper_chest, source_arm, combined_arm);
    quaternion_multiply(combined_arm, destination_arm_rest, expected_arm);
    assert(quaternion_near(rig.nodes[13].rotation, expected_arm));
    assert(fabsf(rig.nodes[15].scale[0] - 1.25F) < 0.0001F);
    assert(fabsf(rig.nodes[15].scale[1] - 0.80F) < 0.0001F);
    assert(fabsf(rig.nodes[15].scale[2] - 1.10F) < 0.0001F);
    assert(fabsf(rig.nodes[16].translation[0] - 0.42F) < 0.0001F);

    assert(eidolon_vrm_retargeter_apply(&retargeter, &pose, EIDOLON_VRM_ROOT_MOTION_FULL, &rig,
                                        error, sizeof(error)));
    assert(fabsf(rig.nodes[0].translation[0] - 2.0F) < 0.0001F);
    assert(fabsf(rig.nodes[0].translation[1] - 3.0F) < 0.0001F);
    assert(fabsf(rig.nodes[0].translation[2] - 4.0F) < 0.0001F);

    eidolon_vrm_retargeter_destroy(&retargeter);
    eidolon_motion_destroy(&rig);
}

static void test_normalized_overlay_preserves_unowned_base_and_rolls_back(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmRetargeter projector;
    EidolonHumanoidPose pose;
    EidolonMotionNode accepted[DESTINATION_NODE_COUNT];
    char error[EIDOLON_VRM_RETARGET_ERROR_CAPACITY];
    float upper_chest[4];
    float upper_arm[4];
    float combined[4];
    float expected[4];
    float preserved_left[4];
    float preserved_accessory[3];

    initialize_body(&body);
    initialize_destination_rig(&rig);
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.15F, rig.nodes[4].rotation);
    rig.nodes[16].translation[0] = 0.33F;
    assert(eidolon_motion_rebuild_world(&rig));
    memcpy(preserved_left, rig.nodes[4].rotation, sizeof(preserved_left));
    memcpy(preserved_accessory, rig.nodes[16].translation, sizeof(preserved_accessory));
    assert(eidolon_vrm_retargeter_init_destination(&projector, &body, &rig, error, sizeof(error)));

    eidolon_humanoid_pose_init(&pose);
    pose.rotation_mask = (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_UPPER_CHEST) |
                         (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM);
    quaternion_axis(0.0F, 1.0F, 0.0F, 0.25F, pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST]);
    quaternion_axis(1.0F, 0.0F, 0.0F, 0.35F, pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM]);
    pose.has_hips_translation = true;
    pose.hips_translation[0] = 0.5F;
    pose.hips_translation[1] = 0.25F;
    pose.hips_translation[2] = -0.5F;
    assert(eidolon_vrm_retargeter_apply_normalized(
        &projector, &pose, EIDOLON_VRM_ROOT_MOTION_IN_PLACE, &rig, error, sizeof(error)));
    assert(fabsf(rig.nodes[0].translation[0]) < 0.0001F);
    assert(fabsf(rig.nodes[0].translation[1] - 2.5F) < 0.0001F);
    assert(fabsf(rig.nodes[0].translation[2]) < 0.0001F);
    assert(memcmp(rig.nodes[4].rotation, preserved_left, sizeof(preserved_left)) == 0);
    assert(memcmp(rig.nodes[16].translation, preserved_accessory, sizeof(preserved_accessory)) ==
           0);

    memcpy(upper_chest, pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST], sizeof(upper_chest));
    memcpy(upper_arm, pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], sizeof(upper_arm));
    quaternion_multiply(upper_chest, upper_arm, combined);
    quaternion_multiply(combined, rig.nodes[13].bind_rotation, expected);
    assert(quaternion_near(rig.nodes[13].rotation, expected));

    memcpy(accepted, rig.nodes, sizeof(accepted));
    memset(pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST], 0,
           sizeof(pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST]));
    assert(!eidolon_vrm_retargeter_apply_normalized(
        &projector, &pose, EIDOLON_VRM_ROOT_MOTION_IN_PLACE, &rig, error, sizeof(error)));
    assert(memcmp(accepted, rig.nodes, sizeof(accepted)) == 0);

    eidolon_vrm_retargeter_destroy(&projector);
    eidolon_motion_destroy(&rig);
}

static void test_failure_is_transactional(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmaClip clip;
    EidolonHumanoidPose pose;
    EidolonVrmRetargeter retargeter;
    EidolonMotionNode before[DESTINATION_NODE_COUNT];
    char error[EIDOLON_VRM_RETARGET_ERROR_CAPACITY];

    initialize_body(&body);
    initialize_destination_rig(&rig);
    initialize_source(&clip);
    initialize_pose(&clip, &pose);
    assert(eidolon_vrm_retargeter_init(&retargeter, &clip, &body, &rig, error, sizeof(error)));
    memcpy(before, rig.nodes, sizeof(before));
    memset(pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST], 0,
           sizeof(pose.rotations[EIDOLON_HUMANOID_ROLE_UPPER_CHEST]));
    assert(!eidolon_vrm_retargeter_apply(&retargeter, &pose, EIDOLON_VRM_ROOT_MOTION_IN_PLACE, &rig,
                                         error, sizeof(error)));
    assert(strstr(error, "neck") != NULL);
    assert(memcmp(before, rig.nodes, sizeof(before)) == 0);

    eidolon_vrm_retargeter_destroy(&retargeter);
    eidolon_motion_destroy(&rig);
}

static void test_invalid_source_binding_is_rejected(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmaClip clip;
    EidolonVrmRetargeter retargeter;
    char error[EIDOLON_VRM_RETARGET_ERROR_CAPACITY];

    initialize_body(&body);
    initialize_destination_rig(&rig);
    initialize_source(&clip);
    memset(clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_HIPS], 0,
           sizeof(clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_HIPS]));
    assert(!eidolon_vrm_retargeter_init(&retargeter, &clip, &body, &rig, error, sizeof(error)));
    assert(strstr(error, "hips") != NULL);
    assert(strstr(error, "rest frame") != NULL);

    eidolon_motion_destroy(&rig);
}

int main(void) {
    test_rest_conversion_optional_composition_and_root_policy();
    test_normalized_overlay_preserves_unowned_base_and_rolls_back();
    test_failure_is_transactional();
    test_invalid_source_binding_is_rejected();
    puts("vrm retarget tests passed");
    return 0;
}
