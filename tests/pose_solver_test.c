#include "humanoid.h"
#include "pose_solver.h"

#include <assert.h>
#include <math.h>

static const float IDENTITY[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float ONE[3] = {1.0F, 1.0F, 1.0F};

static void node(EidolonMotionRig *rig, size_t index, const char *name, int parent, float x,
                 float y, float z) {
    const float translation[3] = {x, y, z};
    assert(eidolon_motion_set_node(rig, index, name, parent, translation, IDENTITY, ONE));
}

static void build_rig(EidolonMotionRig *rig) {
    assert(eidolon_motion_init(rig, 21));
    node(rig, 0, "Bip001 Pelvis", -1, 0.0F, 1.0F, 0.0F);
    node(rig, 1, "Bip001 Spine", 0, 0.0F, 0.2F, 0.0F);
    node(rig, 2, "Bip001 Spine1", 1, 0.0F, 0.2F, 0.0F);
    node(rig, 3, "Bip001 Neck", 2, 0.0F, 0.2F, 0.0F);
    node(rig, 4, "Bip001 Head", 3, 0.0F, 0.2F, 0.0F);
    node(rig, 5, "Bip001 L UpperArm", 2, 0.3F, 0.1F, 0.0F);
    node(rig, 6, "Bip001 L Forearm", 5, 0.4F, 0.0F, 0.0F);
    node(rig, 7, "Bip001 L Hand", 6, 0.3F, 0.0F, 0.0F);
    node(rig, 8, "Bip001 R UpperArm", 2, -0.3F, 0.1F, 0.0F);
    node(rig, 9, "Bip001 R Forearm", 8, -0.4F, 0.0F, 0.0F);
    node(rig, 10, "Bip001 R Hand", 9, -0.3F, 0.0F, 0.0F);
    node(rig, 11, "Bip001 L Thigh", 0, 0.15F, -0.1F, 0.0F);
    node(rig, 12, "Bip001 L Calf", 11, 0.0F, -0.5F, 0.0F);
    node(rig, 13, "Bip001 L Foot", 12, 0.0F, -0.5F, 0.0F);
    node(rig, 14, "Bip001 R Thigh", 0, -0.15F, -0.1F, 0.0F);
    node(rig, 15, "Bip001 R Calf", 14, 0.0F, -0.5F, 0.0F);
    node(rig, 16, "Bip001 R Foot", 15, 0.0F, -0.5F, 0.0F);
    node(rig, 17, "Bip001 L Toe0", 13, 0.0F, 0.0F, 0.2F);
    node(rig, 18, "Bip001 R Toe0", 16, 0.0F, 0.0F, 0.2F);
    node(rig, 19, "Bip001 Xtra_eyeL", 4, 0.05F, 0.05F, -0.1F);
    node(rig, 20, "Bip001 Xtra_eyeR", 4, -0.05F, 0.05F, -0.1F);
    assert(eidolon_motion_rebuild_world(rig));
}

static void position(const EidolonMotionRig *rig, int node_index, float result[3]) {
    const float *world = rig->nodes[(size_t)node_index].world;
    result[0] = world[12];
    result[1] = world[13];
    result[2] = world[14];
}

static void subtract(const float left[3], const float right[3], float result[3]) {
    result[0] = left[0] - right[0];
    result[1] = left[1] - right[1];
    result[2] = left[2] - right[2];
}

static float dot(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static float length(const float vector[3]) { return sqrtf(dot(vector, vector)); }

static void normalize(float vector[3]) {
    const float magnitude = length(vector);
    assert(magnitude > 0.00001F);
    vector[0] /= magnitude;
    vector[1] /= magnitude;
    vector[2] /= magnitude;
}

static void reset_pose(EidolonMotionRig *rig) {
    for (size_t node_index = 0; node_index < rig->node_count; ++node_index) {
        EidolonMotionNode *motion_node = &rig->nodes[node_index];
        SDL_memcpy(motion_node->translation, motion_node->bind_translation,
                   sizeof(motion_node->translation));
        SDL_memcpy(motion_node->rotation, motion_node->bind_rotation,
                   sizeof(motion_node->rotation));
        SDL_memcpy(motion_node->scale, motion_node->bind_scale, sizeof(motion_node->scale));
    }
    assert(eidolon_motion_rebuild_world(rig));
}

static void elbow_radial(const EidolonMotionRig *rig, int shoulder_node, int elbow_node,
                         int hand_node, float result[3]) {
    float shoulder[3];
    float elbow[3];
    float hand[3];
    float direction[3];
    position(rig, shoulder_node, shoulder);
    position(rig, elbow_node, elbow);
    position(rig, hand_node, hand);
    subtract(hand, shoulder, direction);
    normalize(direction);
    subtract(elbow, shoulder, result);
    const float along = dot(result, direction);
    for (size_t axis = 0; axis < 3; ++axis) {
        result[axis] -= direction[axis] * along;
    }
    normalize(result);
}

int main(void) {
    EidolonMotionRig rig;
    build_rig(&rig);
    EidolonHumanoidProfile profile;
    assert(eidolon_humanoid_profile_init(&profile, &rig));

    float right_hand_before[3];
    position(&rig, profile.nodes[EIDOLON_HUMANOID_RIGHT_HAND], right_hand_before);
    EidolonSemanticPose pose = {
        .name = "test",
        .intent = "test",
        .arms = {{{0.45F, -0.55F, 0.25F}, {0.0F, 0.0F, 1.0F}, 1.0F},
                 {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 0.0F}},
        .soften_ratio = 0.0F,
    };
    assert(eidolon_pose_solve(&rig, &profile, &pose));

    float shoulder[3];
    float elbow[3];
    float hand[3];
    position(&rig, profile.nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM], shoulder);
    position(&rig, profile.nodes[EIDOLON_HUMANOID_LEFT_LOWER_ARM], elbow);
    position(&rig, profile.nodes[EIDOLON_HUMANOID_LEFT_HAND], hand);
    float shoulder_to_hand[3];
    subtract(hand, shoulder, shoulder_to_hand);
    const float arm_length = profile.arm_length[EIDOLON_HUMANOID_LEFT];
    const float outward[3] = {-profile.right[0], -profile.right[1], -profile.right[2]};
    assert(fabsf(dot(shoulder_to_hand, outward) / arm_length - 0.45F) < 0.0002F);
    assert(fabsf(dot(shoulder_to_hand, profile.up) / arm_length + 0.55F) < 0.0002F);
    assert(fabsf(dot(shoulder_to_hand, profile.forward) / arm_length - 0.25F) < 0.0002F);
    float segment[3];
    subtract(elbow, shoulder, segment);
    assert(fabsf(length(segment) - 0.4F) < 0.0002F);
    subtract(hand, elbow, segment);
    assert(fabsf(length(segment) - 0.3F) < 0.0002F);
    float right_hand_after[3];
    position(&rig, profile.nodes[EIDOLON_HUMANOID_RIGHT_HAND], right_hand_after);
    subtract(right_hand_after, right_hand_before, segment);
    assert(length(segment) < 0.00001F);

    float first_radial[3];
    elbow_radial(&rig, profile.nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM],
                 profile.nodes[EIDOLON_HUMANOID_LEFT_LOWER_ARM],
                 profile.nodes[EIDOLON_HUMANOID_LEFT_HAND], first_radial);
    reset_pose(&rig);
    pose.arms[EIDOLON_HUMANOID_LEFT].elbow_pole[2] = -1.0F;
    assert(eidolon_pose_solve(&rig, &profile, &pose));
    float second_radial[3];
    elbow_radial(&rig, profile.nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM],
                 profile.nodes[EIDOLON_HUMANOID_LEFT_LOWER_ARM],
                 profile.nodes[EIDOLON_HUMANOID_LEFT_HAND], second_radial);
    assert(dot(first_radial, second_radial) < -0.99F);

    float left_upper_before[4];
    SDL_memcpy(left_upper_before,
               rig.nodes[(size_t)profile.nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM]].rotation,
               sizeof(left_upper_before));
    pose.arms[EIDOLON_HUMANOID_RIGHT].hand[0] = NAN;
    assert(!eidolon_pose_solve(&rig, &profile, &pose));
    assert(SDL_memcmp(left_upper_before,
                      rig.nodes[(size_t)profile.nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM]].rotation,
                      sizeof(left_upper_before)) == 0);
    SDL_ClearError();
    eidolon_motion_destroy(&rig);
    return 0;
}
