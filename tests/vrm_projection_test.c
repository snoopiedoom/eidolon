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
    for (size_t index = 5U; index < 11U; ++index) {
        add_node(rig, index, 0, 0.0F, 0.0F, 0.0F);
    }
    add_node(rig, 11U, 1, -0.22F, 0.12F, 0.0F);
    add_node(rig, 12U, 11, -0.30F, 0.0F, 0.0F);
    add_node(rig, 13U, 12, -0.28F, 0.0F, 0.0F);
    add_node(rig, 14U, 1, 0.22F, 0.12F, 0.0F);
    add_node(rig, 15U, 14, 0.30F, 0.0F, 0.0F);
    add_node(rig, 16U, 15, 0.28F, 0.0F, 0.0F);
    add_node(rig, 17U, 2, 0.0F, 0.0F, 0.0F);
    assert(eidolon_motion_rebuild_world(rig));

    memset(body, 0, sizeof(*body));
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
    assert(memcmp(rig.nodes[17].scale, preserved_unowned_scale,
                  sizeof(preserved_unowned_scale)) == 0);
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
        memcpy(rig.nodes[index].scale, rig.nodes[index].bind_scale,
               sizeof(rig.nodes[index].scale));
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

int main(void) {
    test_projection_is_monotonic_and_transactional();
    test_expression_mapping_is_explicit_and_binary_aware();
    test_bind_space_correction_uses_authored_frame();
    test_wrist_bind_space_correction_uses_authored_frame();
    test_captured_base_pose_composes_without_accumulation();
    puts("vrm projection tests passed");
    return 0;
}
