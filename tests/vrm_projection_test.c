#include "vrm_projection.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float IDENTITY[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float ONE[3] = {1.0F, 1.0F, 1.0F};

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
    EidolonCanonicalControl first;
    EidolonCanonicalControl invalid;
    float accepted_rotations[18][4];

    build_rig(&rig, &body);
    assert(eidolon_vrm_projection_init(&projection, &body, &rig));
    first = control(1U);
    assert(eidolon_vrm_projection_apply(&projection, &rig, &first));
    assert(projection.control_revision == 1U);
    assert(fabsf(projection.focused_expression_weight - 0.45F) < 0.0001F);
    assert(memcmp(rig.nodes[14].rotation, rig.nodes[14].bind_rotation,
                  sizeof(rig.nodes[14].rotation)) != 0);
    assert(memcmp(rig.nodes[11].rotation, rig.nodes[11].bind_rotation,
                  sizeof(rig.nodes[11].rotation)) != 0);
    for (size_t index = 0; index < rig.node_count; ++index) {
        memcpy(accepted_rotations[index], rig.nodes[index].rotation,
               sizeof(accepted_rotations[index]));
    }

    assert(!eidolon_vrm_projection_apply(&projection, &rig, &first));
    assert(projection.control_revision == 1U);
    invalid = control(2U);
    invalid.head_yaw = NAN;
    invalid.focused_expression_weight = 0.9F;
    assert(!eidolon_vrm_projection_apply(&projection, &rig, &invalid));
    assert(projection.control_revision == 1U);
    assert(fabsf(projection.focused_expression_weight - 0.45F) < 0.0001F);
    for (size_t index = 0; index < rig.node_count; ++index) {
        assert(memcmp(accepted_rotations[index], rig.nodes[index].rotation,
                      sizeof(accepted_rotations[index])) == 0);
    }
    eidolon_vrm_projection_destroy(&projection);
    eidolon_motion_destroy(&rig);
}

int main(void) {
    test_projection_is_monotonic_and_transactional();
    puts("vrm projection tests passed");
    return 0;
}
