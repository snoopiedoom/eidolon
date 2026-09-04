#include "vrm_projection.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float IDENTITY[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float ONE[3] = {1.0F, 1.0F, 1.0F};

static EidolonEprBodyProfile body_profile(void) {
    EidolonEprBodyProfile profile;
    memset(&profile, 0, sizeof(profile));
    profile.version = EIDOLON_EPR_BODY_PROFILE_VERSION;
    profile.right[0] = 1.0F;
    profile.up[1] = 1.0F;
    profile.forward[2] = 1.0F;
    profile.has_required_humanoid = true;
    profile.has_right_arm = true;
    return profile;
}

static void add_node(EidolonMotionRig *rig, size_t index, int parent, float x, float y, float z) {
    const float translation[3] = {x, y, z};
    char name[32];
    (void)snprintf(name, sizeof(name), "node_%zu", index);
    assert(eidolon_motion_set_node(rig, index, name, parent, translation, IDENTITY, ONE));
}

static void build_rig(EidolonMotionRig *rig, EidolonVrmBody *body) {
    assert(eidolon_motion_init(rig, 18U));
    add_node(rig, 0U, -1, 0.0F, 0.90F, 0.0F);
    add_node(rig, 1U, 0, 0.0F, 0.40F, 0.0F);
    add_node(rig, 2U, 1, 0.0F, 0.40F, 0.0F);
    add_node(rig, 3U, 2, -0.03F, 0.03F, 0.08F);
    add_node(rig, 4U, 2, 0.03F, 0.03F, 0.08F);
    add_node(rig, 5U, 0, 0.0F, 0.0F, 0.0F);
    add_node(rig, 6U, 5, 0.0F, 0.0F, 0.0F);
    add_node(rig, 7U, 6, 0.0F, 0.0F, 0.0F);
    add_node(rig, 8U, 0, 0.0F, 0.0F, 0.0F);
    add_node(rig, 9U, 8, 0.0F, 0.0F, 0.0F);
    add_node(rig, 10U, 9, 0.0F, 0.0F, 0.0F);
    add_node(rig, 11U, 1, -0.22F, 0.12F, 0.0F);
    add_node(rig, 12U, 11, -0.30F, 0.0F, 0.0F);
    add_node(rig, 13U, 12, -0.28F, 0.0F, 0.0F);
    add_node(rig, 14U, 1, 0.22F, 0.12F, 0.0F);
    add_node(rig, 15U, 14, 0.30F, 0.0F, 0.0F);
    add_node(rig, 16U, 15, 0.28F, 0.0F, 0.0F);
    add_node(rig, 17U, 2, 0.0F, 0.0F, 0.0F);
    assert(eidolon_motion_rebuild_world(rig));

    memset(body, 0, sizeof(*body));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        body->node_by_role[role] = -1;
    }
    body->node_by_role[EIDOLON_HUMANOID_ROLE_HIPS] = 0;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_SPINE] = 1;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_HEAD] = 2;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_EYE] = 3;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_EYE] = 4;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG] = 5;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG] = 6;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_FOOT] = 7;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG] = 8;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG] = 9;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_FOOT] = 10;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM] = 11;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM] = 12;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_HAND] = 13;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM] = 14;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM] = 15;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_HAND] = 16;
    for (size_t index = 0; index < EIDOLON_VRM_BONE_COUNT; ++index) {
        body->node_by_bone[index] = -1;
    }
    body->node_by_bone[EIDOLON_VRM_BONE_HIPS] = 0;
    body->node_by_bone[EIDOLON_VRM_BONE_SPINE] = 1;
    body->node_by_bone[EIDOLON_VRM_BONE_CHEST] = 1;
    body->node_by_bone[EIDOLON_VRM_BONE_HEAD] = 2;
    body->node_by_bone[EIDOLON_VRM_BONE_LEFT_EYE] = 3;
    body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_EYE] = 4;
    body->node_by_bone[EIDOLON_VRM_BONE_LEFT_UPPER_ARM] = 11;
    body->node_by_bone[EIDOLON_VRM_BONE_LEFT_LOWER_ARM] = 12;
    body->node_by_bone[EIDOLON_VRM_BONE_LEFT_HAND] = 13;
    body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM] = 14;
    body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_LOWER_ARM] = 15;
    body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_HAND] = 16;
    body->look_at.state = EIDOLON_VRM_CAPABILITY_PARSED;
    body->relaxed_expression.state = EIDOLON_VRM_CAPABILITY_EXECUTABLE;
}

static EidolonCanonicalControl control(uint64_t revision) {
    EidolonCanonicalControl value;
    memset(&value, 0, sizeof(value));
    value.version = EIDOLON_EPR_CONTROL_VERSION;
    value.revision = revision;
    value.valid = true;
    value.torso_pitch = 0.05F;
    value.head_yaw = -0.20F;
    value.eye_yaw = -0.12F;
    value.eye_pitch = 0.03F;
    value.eye_weight = 1.0F;
    value.right_elbow_position[0] = 0.40F;
    value.right_elbow_position[1] = 1.34F;
    value.right_elbow_position[2] = 0.12F;
    value.right_hand_position[0] = 0.50F;
    value.right_hand_position[1] = 1.16F;
    value.right_hand_position[2] = 0.20F;
    value.right_wrist_euler[1] = 0.10F;
    value.focused_expression_weight = 0.45F;
    return value;
}

static void multiply_quaternion(const float left[4], const float right[4], float result[4]) {
    result[0] = left[3] * right[0] + left[0] * right[3] + left[1] * right[2] - left[2] * right[1];
    result[1] = left[3] * right[1] - left[0] * right[2] + left[1] * right[3] + left[2] * right[0];
    result[2] = left[3] * right[2] + left[0] * right[1] - left[1] * right[0] + left[2] * right[3];
    result[3] = left[3] * right[3] - left[0] * right[0] - left[1] * right[1] - left[2] * right[2];
}

static float quaternion_agreement(const float left[4], const float right[4]) {
    float agreement = 0.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        agreement += left[component] * right[component];
    }
    return fabsf(agreement);
}

static void quaternion_z(float radians, float result[4]) {
    result[0] = 0.0F;
    result[1] = 0.0F;
    result[2] = sinf(radians * 0.5F);
    result[3] = cosf(radians * 0.5F);
}

static void mix_quaternion(const float from[4], const float to[4], float weight, float result[4]) {
    float dot = 0.0F;
    float length = 0.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        dot += from[component] * to[component];
    }
    const float sign = dot < 0.0F ? -1.0F : 1.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        result[component] = from[component] + (to[component] * sign - from[component]) * weight;
        length += result[component] * result[component];
    }
    length = sqrtf(length);
    assert(length > 0.0F);
    for (size_t component = 0U; component < 4U; ++component) {
        result[component] /= length;
    }
}
static void test_projection_is_monotonic_and_transactional(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl first;
    EidolonCanonicalControl invalid;
    EidolonMotionNode accepted_nodes[18];
    float preserved_left_rotation[4];
    float preserved_unowned_translation[3];
    float preserved_unowned_scale[3];

    build_rig(&rig, &body);
    rig.nodes[11].rotation[2] = 0.10F;
    rig.nodes[11].rotation[3] = 0.994987F;
    rig.nodes[17].translation[0] = 0.25F;
    rig.nodes[17].scale[0] = 1.25F;
    assert(eidolon_motion_rebuild_world(&rig));
    memcpy(preserved_left_rotation, rig.nodes[11].rotation, sizeof(preserved_left_rotation));
    memcpy(preserved_unowned_translation, rig.nodes[17].translation,
           sizeof(preserved_unowned_translation));
    memcpy(preserved_unowned_scale, rig.nodes[17].scale, sizeof(preserved_unowned_scale));
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    invalid = control(0U);
    assert(!eidolon_vrm_projection_apply(&projection, &rig, &invalid));
    first = control(1U);
    assert(eidolon_vrm_projection_apply(&projection, &rig, &first));
    assert(projection.control_revision == 1U);
    assert(fabsf(projection.focused_expression_weight - 0.45F) < 0.0001F);
    assert(memcmp(rig.nodes[14].rotation, rig.nodes[14].bind_rotation,
                  sizeof(rig.nodes[14].rotation)) != 0);
    assert(memcmp(rig.nodes[11].rotation, preserved_left_rotation,
                  sizeof(preserved_left_rotation)) == 0);
    assert(memcmp(rig.nodes[17].translation, preserved_unowned_translation,
                  sizeof(preserved_unowned_translation)) == 0);
    assert(memcmp(rig.nodes[17].scale, preserved_unowned_scale, sizeof(preserved_unowned_scale)) ==
           0);
    assert(memcmp(rig.nodes[3].rotation, rig.nodes[3].bind_rotation,
                  sizeof(rig.nodes[3].rotation)) == 0);
    assert(memcmp(rig.nodes[4].rotation, rig.nodes[4].bind_rotation,
                  sizeof(rig.nodes[4].rotation)) == 0);
    memcpy(accepted_nodes, rig.nodes, sizeof(accepted_nodes));

    assert(!eidolon_vrm_projection_apply(&projection, &rig, &first));
    assert(projection.control_revision == 1U);
    invalid = control(2U);
    invalid.torso_pitch = 0.0F;
    invalid.torso_yaw = 0.0F;
    invalid.torso_roll = 0.0F;
    invalid.right_elbow_position[0] = 0.22F;
    invalid.right_elbow_position[1] = 1.42F;
    invalid.right_elbow_position[2] = 0.0F;
    invalid.focused_expression_weight = 0.9F;
    assert(!eidolon_vrm_projection_apply(&projection, &rig, &invalid));
    assert(projection.control_revision == 1U);
    assert(fabsf(projection.focused_expression_weight - 0.45F) < 0.0001F);
    assert(memcmp(accepted_nodes, rig.nodes, sizeof(accepted_nodes)) == 0);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

static void test_expression_mapping_is_explicit_and_binary_aware(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value;

    build_rig(&rig, &body);
    body.relaxed_expression.is_binary = true;
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    value = control(1U);
    value.focused_expression_weight = 0.5F;
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    assert(projection.focused_expression_weight == 0.0F);
    value = control(2U);
    value.focused_expression_weight = 0.51F;
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    assert(projection.focused_expression_weight == 1.0F);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);

    build_rig(&rig, &body);
    body.relaxed_expression.state = EIDOLON_VRM_CAPABILITY_PARSED;
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    value = control(1U);
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    assert(projection.focused_expression_weight == 0.0F);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

static void test_bind_space_correction_uses_authored_frame(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value;
    const float half_sqrt = 0.70710678F;
    const float rotated_bind[4] = {0.0F, 0.0F, half_sqrt, half_sqrt};
    const float expected[4] = {0.5F, -0.5F, 0.5F, 0.5F};
    float agreement = 0.0F;

    build_rig(&rig, &body);
    memcpy(rig.nodes[2].bind_rotation, rotated_bind, sizeof(rotated_bind));
    memcpy(rig.nodes[2].rotation, rotated_bind, sizeof(rotated_bind));
    assert(eidolon_motion_rebuild_world(&rig));
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    value = control(1U);
    value.torso_pitch = 0.0F;
    value.torso_yaw = 0.0F;
    value.torso_roll = 0.0F;
    value.head_pitch = SDL_PI_F * 0.5F;
    value.head_yaw = 0.0F;
    value.head_roll = 0.0F;
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    for (size_t component = 0U; component < 4U; ++component) {
        agreement += rig.nodes[2].rotation[component] * expected[component];
    }
    assert(fabsf(agreement) > 0.999F);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

static void test_wrist_bind_space_correction_uses_authored_frame(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value;
    const float half_sqrt = 0.70710678F;
    const float rotated_bind[4] = {0.0F, 0.0F, half_sqrt, half_sqrt};
    const float expected[4] = {0.5F, -0.5F, 0.5F, 0.5F};
    float agreement = 0.0F;

    build_rig(&rig, &body);
    memcpy(rig.nodes[16].bind_rotation, rotated_bind, sizeof(rotated_bind));
    memcpy(rig.nodes[16].rotation, rotated_bind, sizeof(rotated_bind));
    assert(eidolon_motion_rebuild_world(&rig));
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    value = control(1U);
    value.torso_pitch = 0.0F;
    value.head_yaw = 0.0F;
    value.right_elbow_position[0] = 0.52F;
    value.right_elbow_position[1] = 1.42F;
    value.right_elbow_position[2] = 0.0F;
    value.right_hand_position[0] = 0.80F;
    value.right_hand_position[1] = 1.42F;
    value.right_hand_position[2] = 0.0F;
    value.right_wrist_euler[0] = SDL_PI_F * 0.5F;
    value.right_wrist_euler[1] = 0.0F;
    value.right_wrist_euler[2] = 0.0F;
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    for (size_t component = 0U; component < 4U; ++component) {
        agreement += rig.nodes[16].rotation[component] * expected[component];
    }
    assert(fabsf(agreement) > 0.999F);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

static void test_captured_base_pose_composes_without_accumulation(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value;
    const float animated_chest[4] = {0.0F, 0.0F, 0.14943813F, 0.98877108F};

    build_rig(&rig, &body);
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    value = control(1U);
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));

    for (size_t index = 0U; index < rig.node_count; ++index) {
        memcpy(rig.nodes[index].translation, rig.nodes[index].bind_translation,
               sizeof(rig.nodes[index].translation));
        memcpy(rig.nodes[index].rotation, rig.nodes[index].bind_rotation,
               sizeof(rig.nodes[index].rotation));
        memcpy(rig.nodes[index].scale, rig.nodes[index].bind_scale, sizeof(rig.nodes[index].scale));
    }
    memcpy(rig.nodes[1].rotation, animated_chest, sizeof(animated_chest));
    rig.nodes[1].translation[0] = 0.125F;
    rig.nodes[1].scale[1] = 1.1F;
    assert(eidolon_motion_rebuild_world(&rig));
    assert(eidolon_vrm_projection_capture_base(&projection, &rig));
    value = control(2U);
    value.torso_pitch = 0.0F;
    value.torso_yaw = 0.0F;
    value.torso_roll = 0.0F;
    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    for (size_t component = 0U; component < 4U; ++component) {
        assert(fabsf(rig.nodes[1].rotation[component] - animated_chest[component]) < 0.00001F);
    }
    assert(rig.nodes[1].translation[0] == 0.125F);
    assert(rig.nodes[1].scale[1] == 1.1F);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

static void test_calibrated_residual_composes_after_canonical_control(void) {
    EidolonMotionRig uncalibrated_rig;
    EidolonMotionRig calibrated_rig;
    EidolonVrmBody uncalibrated_body;
    EidolonVrmBody calibrated_body;
    EidolonVrmProjection uncalibrated_projection;
    EidolonVrmProjection calibrated_projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl uncalibrated = control(1U);
    EidolonCanonicalControl calibrated = control(1U);
    EidolonVrmCalibration calibration;
    const float residual[4] = {0.0F, 0.0F, 0.25881904F, 0.96592583F};
    float expected[4];
    float agreement = 0.0F;
    memset(&calibration, 0, sizeof(calibration));
    calibration.version = EIDOLON_VRM_CALIBRATION_VERSION;
    calibration.anchor_mask = UINT32_C(1) << EIDOLON_VRM_CALIBRATION_NEUTRAL;
    calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL].resource_mask =
        UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL].residual_bone_mask =
        UINT32_C(1) << EIDOLON_VRM_BONE_RIGHT_HAND;
    memcpy(calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL]
               .residual_rotation[EIDOLON_VRM_BONE_RIGHT_HAND],
           residual, sizeof(residual));
    calibrated.pose_anchor_resource_weights[EIDOLON_EPR_POSE_NEUTRAL]
                                           [EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN] = 1.0F;

    build_rig(&uncalibrated_rig, &uncalibrated_body);
    build_rig(&calibrated_rig, &calibrated_body);
    assert(eidolon_vrm_projection_init(&uncalibrated_projection, &uncalibrated_body, &profile,
                                       &uncalibrated_rig));
    assert(eidolon_vrm_projection_init(&calibrated_projection, &calibrated_body, &profile,
                                       &calibrated_rig));
    assert(eidolon_vrm_projection_apply_calibrated(&uncalibrated_projection, &uncalibrated_rig,
                                                   &uncalibrated, &calibration));
    assert(eidolon_vrm_projection_apply_calibrated(&calibrated_projection, &calibrated_rig,
                                                   &calibrated, &calibration));
    multiply_quaternion(uncalibrated_rig.nodes[16].rotation, residual, expected);
    for (size_t component = 0U; component < 4U; ++component) {
        agreement += calibrated_rig.nodes[16].rotation[component] * expected[component];
    }
    assert(fabsf(agreement) > 0.999F);
    assert(memcmp(uncalibrated_rig.nodes[16].rotation, calibrated_rig.nodes[16].rotation,
                  sizeof(uncalibrated_rig.nodes[16].rotation)) != 0);
    eidolon_vrm_projection_destroy(&uncalibrated_projection);
    eidolon_vrm_projection_destroy(&calibrated_projection);
    eidolon_motion_destroy(&uncalibrated_rig);
    eidolon_motion_destroy(&calibrated_rig);
}

static void test_dynamic_residual_ownership_is_transactional(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value;
    EidolonVrmCalibration calibration;
    EidolonVrmCalibration invalid_calibration;
    EidolonMotionNode accepted_nodes[18];
    float first_rotation[4];
    const float residual[4] = {0.0F, 0.0F, 0.25881904F, 0.96592583F};

    build_rig(&rig, &body);
    body.node_by_bone[EIDOLON_VRM_BONE_NECK] = 17;
    memset(&calibration, 0, sizeof(calibration));
    calibration.version = EIDOLON_VRM_CALIBRATION_VERSION;
    calibration.anchor_mask = UINT32_C(1) << EIDOLON_VRM_CALIBRATION_NEUTRAL;
    calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL].resource_mask =
        UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD;
    calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL].residual_bone_mask =
        UINT32_C(1) << EIDOLON_VRM_BONE_NECK;
    memcpy(calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL]
               .residual_rotation[EIDOLON_VRM_BONE_NECK],
           residual, sizeof(residual));

    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    value = control(1U);
    value.pose_anchor_resource_weights[EIDOLON_EPR_POSE_NEUTRAL][EIDOLON_EPR_RESOURCE_HEAD] = 1.0F;
    assert(eidolon_vrm_projection_apply_calibrated(&projection, &rig, &value, &calibration));
    memcpy(first_rotation, rig.nodes[17].rotation, sizeof(first_rotation));
    assert(quaternion_agreement(first_rotation, residual) > 0.999F);

    value.revision = 2U;
    assert(eidolon_vrm_projection_apply_calibrated(&projection, &rig, &value, &calibration));
    assert(quaternion_agreement(rig.nodes[17].rotation, first_rotation) > 0.99999F);

    memcpy(accepted_nodes, rig.nodes, sizeof(accepted_nodes));
    invalid_calibration = calibration;
    memset(invalid_calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL]
               .residual_rotation[EIDOLON_VRM_BONE_NECK],
           0,
           sizeof(invalid_calibration.anchors[EIDOLON_VRM_CALIBRATION_NEUTRAL]
                      .residual_rotation[EIDOLON_VRM_BONE_NECK]));
    value.revision = 3U;
    assert(
        !eidolon_vrm_projection_apply_calibrated(&projection, &rig, &value, &invalid_calibration));
    assert(projection.control_revision == 2U);
    assert(memcmp(accepted_nodes, rig.nodes, sizeof(accepted_nodes)) == 0);

    value.revision = 3U;
    value.pose_anchor_resource_weights[EIDOLON_EPR_POSE_NEUTRAL][EIDOLON_EPR_RESOURCE_HEAD] = 0.0F;
    assert(eidolon_vrm_projection_apply_calibrated(&projection, &rig, &value, &calibration));
    assert(quaternion_agreement(rig.nodes[17].rotation, IDENTITY) > 0.99999F);

    value.revision = 4U;
    value.pose_anchor_resource_weights[EIDOLON_EPR_POSE_NEUTRAL][EIDOLON_EPR_RESOURCE_HEAD] = 1.0F;
    assert(eidolon_vrm_projection_apply_calibrated(&projection, &rig, &value, &calibration));
    assert(quaternion_agreement(rig.nodes[17].rotation, residual) > 0.999F);

    value.revision = 5U;
    assert(eidolon_vrm_projection_apply_calibrated(&projection, &rig, &value, NULL));
    assert(quaternion_agreement(rig.nodes[17].rotation, IDENTITY) > 0.99999F);

    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

static void test_atomic_normalized_frame_composes_before_procedural_control(void) {
    EidolonMotionRig rig;
    EidolonMotionRig candidate;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    EidolonVrmRetargeter projector;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value = control(1U);
    EidolonEprMotionFrame frame;
    EidolonEprMotionFrame invalid;
    EidolonMotionNode accepted[18];
    char error[EIDOLON_VRM_RETARGET_ERROR_CAPACITY];
    float overlay[4];
    float pitch_rotation[4];
    float yaw_rotation[4];
    float procedural[4];
    float expected[4];
    uint64_t accepted_base_revision;
    const uint32_t procedural_mask = (UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD) |
                                     (UINT32_C(1) << EIDOLON_EPR_RESOURCE_EYES) |
                                     (UINT32_C(1) << EIDOLON_EPR_RESOURCE_FACE_EXPRESSION);

    build_rig(&rig, &body);
    body.look_at.state = EIDOLON_VRM_CAPABILITY_EXECUTABLE;
    assert(eidolon_motion_init(&candidate, rig.node_count));
    memcpy(candidate.nodes, rig.nodes, rig.node_count * sizeof(*rig.nodes));
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    assert(eidolon_vrm_retargeter_init_destination(&projector, &body, &rig, error, sizeof(error)));
    value.plan_generation = 1U;
    value.tick = 100;
    value.procedural_resource_mask = procedural_mask;
    value.head_gaze_pitch = -0.04F;
    value.head_gaze_yaw = 0.10F;
    memset(&frame, 0, sizeof(frame));
    frame.version = EIDOLON_EPR_MOTION_FRAME_VERSION;
    frame.plan_generation = 1U;
    frame.tick = 100;
    frame.execution_count = 1U;
    eidolon_humanoid_pose_init(&frame.pose);
    frame.pose.rotation_mask = UINT64_C(1) << EIDOLON_HUMANOID_ROLE_HEAD;
    overlay[0] = 0.0F;
    overlay[1] = 0.0F;
    overlay[2] = sinf(0.15F);
    overlay[3] = cosf(0.15F);
    pitch_rotation[0] = sinf(value.head_gaze_pitch * 0.5F);
    pitch_rotation[1] = 0.0F;
    pitch_rotation[2] = 0.0F;
    pitch_rotation[3] = cosf(value.head_gaze_pitch * 0.5F);
    yaw_rotation[0] = 0.0F;
    yaw_rotation[1] = sinf(value.head_gaze_yaw * 0.5F);
    yaw_rotation[2] = 0.0F;
    yaw_rotation[3] = cosf(value.head_gaze_yaw * 0.5F);
    multiply_quaternion(yaw_rotation, pitch_rotation, procedural);
    multiply_quaternion(overlay, procedural, expected);
    memcpy(frame.pose.rotations[EIDOLON_HUMANOID_ROLE_HEAD], overlay, sizeof(overlay));

    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    assert(projection.has_motion_frame);
    assert(projection.motion_plan_generation == 1U);
    assert(projection.motion_tick == 100);
    assert(quaternion_agreement(rig.nodes[2].rotation, expected) > 0.99999F);
    assert(fabsf(projection.focused_expression_weight - 0.45F) < 0.0001F);
    assert(memcmp(rig.nodes[3].rotation, rig.nodes[3].bind_rotation,
                  sizeof(rig.nodes[3].rotation)) != 0);
    assert(memcmp(rig.nodes[4].rotation, rig.nodes[4].bind_rotation,
                  sizeof(rig.nodes[4].rotation)) != 0);
    assert(memcmp(rig.nodes[14].rotation, rig.nodes[14].bind_rotation,
                  sizeof(rig.nodes[14].rotation)) == 0);
    memcpy(accepted, rig.nodes, sizeof(accepted));
    assert(!eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                 &frame, &value, NULL));
    assert(memcmp(accepted, rig.nodes, sizeof(accepted)) == 0);

    candidate.nodes[17].translation[0] = 0.50F;
    assert(eidolon_motion_rebuild_world(&candidate));
    accepted_base_revision = projection.base_revision;
    assert(eidolon_vrm_projection_publish_base_motion_frame_calibrated(
        &projection, &projector, &rig, &candidate, &frame, &value, NULL));
    assert(projection.base_revision == accepted_base_revision + 1U);
    assert(rig.nodes[17].translation[0] == 0.50F);
    assert(quaternion_agreement(rig.nodes[2].rotation, expected) > 0.99999F);

    memcpy(accepted, rig.nodes, sizeof(accepted));
    accepted_base_revision = projection.base_revision;
    invalid = frame;
    invalid.tick = 120;
    value.revision = 2U;
    value.tick = 120;
    value.procedural_resource_mask = UINT32_C(1) << EIDOLON_EPR_RESOURCE_TORSO;
    assert(!eidolon_vrm_projection_publish_base_motion_frame_calibrated(
        &projection, &projector, &rig, &candidate, &invalid, &value, NULL));
    assert(projection.base_revision == accepted_base_revision);
    assert(projection.control_revision == 1U);
    assert(memcmp(accepted, rig.nodes, sizeof(accepted)) == 0);

    value.procedural_resource_mask = procedural_mask;
    memset(invalid.pose.rotations[EIDOLON_HUMANOID_ROLE_HEAD], 0,
           sizeof(invalid.pose.rotations[EIDOLON_HUMANOID_ROLE_HEAD]));
    assert(!eidolon_vrm_projection_publish_base_motion_frame_calibrated(
        &projection, &projector, &rig, &candidate, &invalid, &value, NULL));
    assert(projection.base_revision == accepted_base_revision);
    assert(projection.control_revision == 1U);
    assert(memcmp(accepted, rig.nodes, sizeof(accepted)) == 0);

    assert(eidolon_vrm_projection_apply(&projection, &rig, &value));
    assert(!projection.has_motion_frame);
    assert(quaternion_agreement(rig.nodes[2].rotation, overlay) < 0.999F);

    eidolon_vrm_retargeter_destroy(&projector);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&candidate);
    eidolon_motion_destroy(&rig);
}

static void test_normalized_right_arm_continuity_and_weighted_ik(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    EidolonVrmRetargeter projector;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value = control(1U);
    EidolonEprMotionFrame frame;
    EidolonEprMotionFrame invalid;
    EidolonMotionNode accepted[18];
    float accepted_continuity[EIDOLON_VRM_PROJECTION_RIGHT_ARM_BONE_COUNT][4];
    float outgoing[4];
    float normalized[4];
    float midpoint[4];
    float full_ik[4];
    float half_ik[4];
    char error[EIDOLON_VRM_RETARGET_ERROR_CAPACITY];
    const uint32_t right_arm = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;

    build_rig(&rig, &body);
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    assert(eidolon_vrm_retargeter_init_destination(&projector, &body, &rig, error, sizeof(error)));

    value.plan_generation = 1U;
    value.tick = 100;
    memset(&frame, 0, sizeof(frame));
    frame.version = EIDOLON_EPR_MOTION_FRAME_VERSION;
    frame.plan_generation = 1U;
    frame.tick = 100;
    frame.execution_count = 1U;
    eidolon_humanoid_pose_init(&frame.pose);
    frame.pose.rotation_mask = UINT64_C(1) << EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM;
    quaternion_z(0.60F, outgoing);
    memcpy(frame.pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], outgoing, sizeof(outgoing));
    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    assert(quaternion_agreement(rig.nodes[14].rotation, outgoing) > 0.99999F);

    value.revision = 2U;
    value.tick = 120;
    value.procedural_resource_mask = right_arm;
    value.right_arm_continuity_id = 77U;
    value.right_arm_continuity_weight = 1.0F;
    frame.tick = 120;
    quaternion_z(-0.20F, normalized);
    memcpy(frame.pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], normalized,
           sizeof(normalized));
    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    assert(projection.has_right_arm_continuity);
    assert(projection.right_arm_continuity_id == 77U);
    assert(quaternion_agreement(rig.nodes[14].rotation, outgoing) > 0.99999F);

    value.revision = 3U;
    value.tick = 140;
    value.right_arm_continuity_weight = 0.5F;
    frame.tick = 140;
    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    mix_quaternion(normalized, outgoing, 0.5F, midpoint);
    assert(quaternion_agreement(rig.nodes[14].rotation, midpoint) > 0.99999F);

    memcpy(accepted, rig.nodes, sizeof(accepted));
    memcpy(accepted_continuity, projection.right_arm_continuity_rotations,
           sizeof(accepted_continuity));
    invalid = frame;
    invalid.tick = 160;
    memset(invalid.pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM], 0,
           sizeof(invalid.pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM]));
    value.revision = 4U;
    value.tick = 160;
    value.right_arm_continuity_id = 88U;
    value.right_arm_continuity_weight = 1.0F;
    assert(!eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                 &invalid, &value, NULL));
    assert(memcmp(accepted, rig.nodes, sizeof(accepted)) == 0);
    assert(projection.right_arm_continuity_id == 77U);
    assert(memcmp(accepted_continuity, projection.right_arm_continuity_rotations,
                  sizeof(accepted_continuity)) == 0);

    value.right_arm_continuity_id = 0U;
    value.right_arm_continuity_weight = 0.0F;
    frame.tick = 160;
    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    assert(!projection.has_right_arm_continuity);
    assert(projection.right_arm_continuity_id == 0U);
    assert(quaternion_agreement(rig.nodes[14].rotation, normalized) > 0.99999F);

    value.revision = 5U;
    value.tick = 180;
    value.right_arm_ik_weight = 1.0F;
    frame.tick = 180;
    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    memcpy(full_ik, rig.nodes[14].rotation, sizeof(full_ik));
    assert(quaternion_agreement(full_ik, normalized) < 0.999F);

    value.revision = 6U;
    value.tick = 200;
    value.right_arm_ik_weight = 0.5F;
    frame.tick = 200;
    assert(eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                &frame, &value, NULL));
    mix_quaternion(normalized, full_ik, 0.5F, half_ik);
    assert(quaternion_agreement(rig.nodes[14].rotation, half_ik) > 0.99999F);

    memcpy(accepted, rig.nodes, sizeof(accepted));
    value.revision = 7U;
    value.tick = 220;
    value.procedural_resource_mask = 0U;
    frame.tick = 220;
    assert(!eidolon_vrm_projection_apply_motion_frame_calibrated(&projection, &projector, &rig,
                                                                 &frame, &value, NULL));
    assert(memcmp(accepted, rig.nodes, sizeof(accepted)) == 0);

    eidolon_vrm_retargeter_destroy(&projector);
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}
static void test_atomic_imported_base_recomposes_same_control(void) {
    EidolonMotionRig rig;
    EidolonMotionRig candidate;
    EidolonVrmBody body;
    EidolonVrmProjection projection;
    const EidolonEprBodyProfile profile = body_profile();
    EidolonCanonicalControl value = control(1U);
    EidolonMotionNode before[18];
    const float first_base[4] = {0.0F, 0.0F, 0.14943813F, 0.98877108F};
    const float second_base[4] = {0.0F, 0.0F, 0.29552021F, 0.95533649F};

    build_rig(&rig, &body);
    assert(eidolon_motion_init(&candidate, rig.node_count));
    memcpy(candidate.nodes, rig.nodes, rig.node_count * sizeof(*rig.nodes));
    assert(eidolon_vrm_projection_init(&projection, &body, &profile, &rig));
    const uint64_t initial_base_revision = projection.base_revision;

    memcpy(candidate.nodes[1].rotation, first_base, sizeof(first_base));
    candidate.nodes[17].translation[0] = 0.25F;
    assert(eidolon_motion_rebuild_world(&candidate));
    assert(eidolon_vrm_projection_publish_base_calibrated(&projection, &rig, &candidate, &value,
                                                          NULL));
    assert(projection.base_revision == initial_base_revision + 1U);
    assert(projection.projected_base_revision == projection.base_revision);
    assert(projection.control_revision == value.revision);
    assert(rig.nodes[17].translation[0] == 0.25F);

    memcpy(candidate.nodes[1].rotation, second_base, sizeof(second_base));
    candidate.nodes[17].translation[0] = 0.50F;
    assert(eidolon_motion_rebuild_world(&candidate));
    assert(eidolon_vrm_projection_publish_base_calibrated(&projection, &rig, &candidate, &value,
                                                          NULL));
    assert(projection.base_revision == initial_base_revision + 2U);
    assert(projection.projected_base_revision == projection.base_revision);
    assert(rig.nodes[17].translation[0] == 0.50F);
    assert(!eidolon_vrm_projection_apply(&projection, &rig, &value));

    memcpy(before, rig.nodes, sizeof(before));
    const uint64_t accepted_base_revision = projection.base_revision;
    memset(candidate.nodes[2].rotation, 0, sizeof(candidate.nodes[2].rotation));
    assert(!eidolon_vrm_projection_publish_base_calibrated(&projection, &rig, &candidate, &value,
                                                           NULL));
    assert(projection.base_revision == accepted_base_revision);
    assert(memcmp(before, rig.nodes, sizeof(before)) == 0);

    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&candidate);
    eidolon_motion_destroy(&rig);
}

int main(void) {
    test_projection_is_monotonic_and_transactional();
    test_expression_mapping_is_explicit_and_binary_aware();
    test_bind_space_correction_uses_authored_frame();
    test_wrist_bind_space_correction_uses_authored_frame();
    test_captured_base_pose_composes_without_accumulation();
    test_atomic_imported_base_recomposes_same_control();
    test_atomic_normalized_frame_composes_before_procedural_control();
    test_normalized_right_arm_continuity_and_weighted_ik();
    test_calibrated_residual_composes_after_canonical_control();
    test_dynamic_residual_ownership_is_transactional();
    puts("vrm projection tests passed");
    return 0;
}
