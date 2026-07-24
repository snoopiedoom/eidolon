#include "vrm_projection.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <string.h>

#define VRM_PROJECTION_EPSILON 0.00001F

static float dot3(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static bool normalize3(float vector[3]) {
    const float length = SDL_sqrtf(dot3(vector, vector));
    if (length <= VRM_PROJECTION_EPSILON) {
        return false;
    }
    for (size_t axis = 0; axis < 3U; ++axis) {
        vector[axis] /= length;
    }
    return true;
}

static void cross3(const float left[3], const float right[3], float result[3]) {
    result[0] = left[1] * right[2] - left[2] * right[1];
    result[1] = left[2] * right[0] - left[0] * right[2];
    result[2] = left[0] * right[1] - left[1] * right[0];
}

static void quaternion_normalize(float quaternion[4]) {
    const float length = SDL_sqrtf(quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                                   quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3]);
    if (length <= VRM_PROJECTION_EPSILON) {
        quaternion[0] = 0.0F;
        quaternion[1] = 0.0F;
        quaternion[2] = 0.0F;
        quaternion[3] = 1.0F;
        return;
    }
    for (size_t component = 0; component < 4U; ++component) {
        quaternion[component] /= length;
    }
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

static void quaternion_inverse(const float quaternion[4], float inverse[4]) {
    inverse[0] = -quaternion[0];
    inverse[1] = -quaternion[1];
    inverse[2] = -quaternion[2];
    inverse[3] = quaternion[3];
}

static bool quaternion_from_to(const float from_value[3], const float to_value[3],
                               float quaternion[4]) {
    float from[3];
    float to[3];
    float cross[3];
    float dot;
    memcpy(from, from_value, sizeof(from));
    memcpy(to, to_value, sizeof(to));
    if (!normalize3(from) || !normalize3(to)) {
        return false;
    }
    dot = SDL_clamp(dot3(from, to), -1.0F, 1.0F);
    if (dot < -0.9999F) {
        const float hint[3] = {SDL_fabsf(from[1]) < 0.9F ? 0.0F : 1.0F,
                               SDL_fabsf(from[1]) < 0.9F ? 1.0F : 0.0F, 0.0F};
        cross3(from, hint, quaternion);
        if (!normalize3(quaternion)) {
            return false;
        }
        quaternion[3] = 0.0F;
        return true;
    }
    cross3(from, to, cross);
    quaternion[0] = cross[0];
    quaternion[1] = cross[1];
    quaternion[2] = cross[2];
    quaternion[3] = 1.0F + dot;
    quaternion_normalize(quaternion);
    return true;
}

static bool node_world_rotation(const EidolonMotionRig *rig, int node, float rotation[4],
                                size_t remaining) {
    const EidolonMotionNode *motion_node;
    if (node < 0 || (size_t)node >= rig->node_count || remaining == 0U) {
        return false;
    }
    motion_node = &rig->nodes[(size_t)node];
    if (motion_node->parent < 0) {
        memcpy(rotation, motion_node->rotation, sizeof(float) * 4U);
        quaternion_normalize(rotation);
        return true;
    }
    {
        float parent[4];
        if (!node_world_rotation(rig, motion_node->parent, parent, remaining - 1U)) {
            return false;
        }
        quaternion_multiply(parent, motion_node->rotation, rotation);
        quaternion_normalize(rotation);
        return true;
    }
}

static bool rotate_node_toward(EidolonMotionRig *rig, int node, const float current_direction[3],
                               const float desired_direction[3]) {
    float delta[4];
    float current_world[4];
    float target_world[4];
    float target_local[4];
    const int parent = rig->nodes[(size_t)node].parent;
    if (!quaternion_from_to(current_direction, desired_direction, delta) ||
        !node_world_rotation(rig, node, current_world, rig->node_count + 1U)) {
        return false;
    }
    quaternion_multiply(delta, current_world, target_world);
    if (parent >= 0) {
        float parent_world[4];
        float parent_inverse[4];
        if (!node_world_rotation(rig, parent, parent_world, rig->node_count + 1U)) {
            return false;
        }
        quaternion_inverse(parent_world, parent_inverse);
        quaternion_multiply(parent_inverse, target_world, target_local);
    } else {
        memcpy(target_local, target_world, sizeof(target_local));
    }
    quaternion_normalize(target_local);
    memcpy(rig->nodes[(size_t)node].rotation, target_local, sizeof(target_local));
    return eidolon_motion_rebuild_world(rig);
}

static void node_position(const EidolonMotionRig *rig, int node, float position[3]) {
    const float *world = rig->nodes[(size_t)node].world;
    position[0] = world[12];
    position[1] = world[13];
    position[2] = world[14];
}

static void subtract3(const float left[3], const float right[3], float result[3]) {
    result[0] = left[0] - right[0];
    result[1] = left[1] - right[1];
    result[2] = left[2] - right[2];
}

static void quaternion_axis(float x, float y, float z, float radians, float result[4]) {
    const float half = radians * 0.5F;
    const float sine = SDL_sinf(half);
    result[0] = x * sine;
    result[1] = y * sine;
    result[2] = z * sine;
    result[3] = SDL_cosf(half);
}

static void apply_euler(EidolonMotionNode *node, float pitch, float yaw, float roll) {
    float x_rotation[4];
    float y_rotation[4];
    float z_rotation[4];
    float xy[4];
    float delta[4];
    float result[4];
    quaternion_axis(1.0F, 0.0F, 0.0F, pitch, x_rotation);
    quaternion_axis(0.0F, 1.0F, 0.0F, yaw, y_rotation);
    quaternion_axis(0.0F, 0.0F, 1.0F, roll, z_rotation);
    quaternion_multiply(y_rotation, x_rotation, xy);
    quaternion_multiply(xy, z_rotation, delta);
    quaternion_multiply(node->bind_rotation, delta, result);
    quaternion_normalize(result);
    memcpy(node->rotation, result, sizeof(result));
}

static bool reset_to_bind(EidolonMotionRig *rig) {
    for (size_t index = 0; index < rig->node_count; ++index) {
        EidolonMotionNode *node = &rig->nodes[index];
        memcpy(node->translation, node->bind_translation, sizeof(node->translation));
        memcpy(node->rotation, node->bind_rotation, sizeof(node->rotation));
        memcpy(node->scale, node->bind_scale, sizeof(node->scale));
    }
    return eidolon_motion_rebuild_world(rig);
}

static bool apply_right_arm(EidolonMotionRig *rig, const EidolonVrmProjection *projection,
                            const EidolonCanonicalControl *control) {
    const int upper = projection->nodes[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM];
    const int lower = projection->nodes[EIDOLON_VRM_BONE_RIGHT_LOWER_ARM];
    const int hand = projection->nodes[EIDOLON_VRM_BONE_RIGHT_HAND];
    float upper_position[3];
    float lower_position[3];
    float hand_position[3];
    float current_direction[3];
    float desired_direction[3];
    node_position(rig, upper, upper_position);
    node_position(rig, lower, lower_position);
    subtract3(lower_position, upper_position, current_direction);
    subtract3(control->right_elbow_position, upper_position, desired_direction);
    if (!rotate_node_toward(rig, upper, current_direction, desired_direction)) {
        return false;
    }
    node_position(rig, lower, lower_position);
    node_position(rig, hand, hand_position);
    subtract3(hand_position, lower_position, current_direction);
    subtract3(control->right_hand_position, lower_position, desired_direction);
    if (!rotate_node_toward(rig, lower, current_direction, desired_direction)) {
        return false;
    }
    apply_euler(&rig->nodes[(size_t)hand], control->right_wrist_euler[0],
                control->right_wrist_euler[1], control->right_wrist_euler[2]);
    return eidolon_motion_rebuild_world(rig);
}

static bool apply_relaxed_left_arm(EidolonMotionRig *rig, const EidolonVrmProjection *projection) {
    const int upper = projection->nodes[EIDOLON_VRM_BONE_LEFT_UPPER_ARM];
    const int lower = projection->nodes[EIDOLON_VRM_BONE_LEFT_LOWER_ARM];
    const int hand = projection->nodes[EIDOLON_VRM_BONE_LEFT_HAND];
    float upper_position[3];
    float lower_position[3];
    float hand_position[3];
    float current_direction[3];
    float desired_direction[3];
    node_position(rig, upper, upper_position);
    node_position(rig, lower, lower_position);
    subtract3(lower_position, upper_position, current_direction);
    desired_direction[0] = current_direction[0] * 0.18F;
    desired_direction[1] = -1.0F;
    desired_direction[2] = 0.08F;
    if (!rotate_node_toward(rig, upper, current_direction, desired_direction)) {
        return false;
    }
    node_position(rig, lower, lower_position);
    node_position(rig, hand, hand_position);
    subtract3(hand_position, lower_position, current_direction);
    desired_direction[0] = current_direction[0] * 0.08F;
    desired_direction[1] = -1.0F;
    desired_direction[2] = 0.04F;
    if (!rotate_node_toward(rig, lower, current_direction, desired_direction)) {
        return false;
    }
    apply_euler(&rig->nodes[(size_t)hand], 0.0F, 0.0F, 0.0F);
    return eidolon_motion_rebuild_world(rig);
}

static bool apply_control(EidolonVrmProjection *projection, EidolonMotionRig *rig,
                          const EidolonCanonicalControl *control) {
    const int chest = projection->nodes[EIDOLON_VRM_BONE_UPPER_CHEST] >= 0
                          ? projection->nodes[EIDOLON_VRM_BONE_UPPER_CHEST]
                      : projection->nodes[EIDOLON_VRM_BONE_CHEST] >= 0
                          ? projection->nodes[EIDOLON_VRM_BONE_CHEST]
                          : projection->nodes[EIDOLON_VRM_BONE_SPINE];
    const int head = projection->nodes[EIDOLON_VRM_BONE_HEAD];
    const int left_eye = projection->nodes[EIDOLON_VRM_BONE_LEFT_EYE];
    const int right_eye = projection->nodes[EIDOLON_VRM_BONE_RIGHT_EYE];
    if (!reset_to_bind(rig)) {
        return false;
    }
    apply_euler(&rig->nodes[(size_t)chest], control->torso_pitch, control->torso_yaw,
                control->torso_roll);
    apply_euler(&rig->nodes[(size_t)head], control->head_pitch, control->head_yaw,
                control->head_roll);
    if (left_eye >= 0 && right_eye >= 0 && control->eye_weight > 0.0F) {
        apply_euler(&rig->nodes[(size_t)left_eye], control->eye_pitch, control->eye_yaw, 0.0F);
        apply_euler(&rig->nodes[(size_t)right_eye], control->eye_pitch, control->eye_yaw, 0.0F);
    }
    if (!eidolon_motion_rebuild_world(rig) || !apply_relaxed_left_arm(rig, projection) ||
        !apply_right_arm(rig, projection, control)) {
        return false;
    }
    projection->focused_expression_weight =
        SDL_clamp(control->focused_expression_weight, 0.0F, 1.0F);
    return true;
}

static bool rotations_finite(const EidolonMotionRig *rig) {
    for (size_t index = 0; index < rig->node_count; ++index) {
        for (size_t component = 0; component < 4U; ++component) {
            if (!isfinite(rig->nodes[index].rotation[component])) {
                return false;
            }
        }
    }
    return true;
}

static bool restore_last_valid(EidolonVrmProjection *projection, EidolonMotionRig *rig) {
    for (size_t index = 0; index < rig->node_count; ++index) {
        memcpy(rig->nodes[index].rotation, projection->last_valid_rotations[index],
               sizeof(rig->nodes[index].rotation));
    }
    return eidolon_motion_rebuild_world(rig);
}

bool eidolon_vrm_projection_init(EidolonVrmProjection *projection, const EidolonVrmBody *body,
                                 const EidolonMotionRig *rig) {
    if (projection == NULL || body == NULL || rig == NULL || rig->node_count == 0U) {
        return false;
    }
    memset(projection, 0, sizeof(*projection));
    memcpy(projection->nodes, body->node_by_bone, sizeof(projection->nodes));
    if (projection->nodes[EIDOLON_VRM_BONE_HEAD] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_SPINE] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_LEFT_UPPER_ARM] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_LEFT_LOWER_ARM] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_LEFT_HAND] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_RIGHT_LOWER_ARM] < 0 ||
        projection->nodes[EIDOLON_VRM_BONE_RIGHT_HAND] < 0) {
        return false;
    }
    projection->last_valid_rotations =
        SDL_calloc(rig->node_count, sizeof(*projection->last_valid_rotations));
    if (projection->last_valid_rotations == NULL) {
        return false;
    }
    projection->node_count = rig->node_count;
    for (size_t index = 0; index < rig->node_count; ++index) {
        memcpy(projection->last_valid_rotations[index], rig->nodes[index].rotation,
               sizeof(rig->nodes[index].rotation));
    }
    projection->ready = true;
    return true;
}

bool eidolon_vrm_projection_apply(EidolonVrmProjection *projection, EidolonMotionRig *rig,
                                  const EidolonCanonicalControl *control) {
    float previous_expression;
    if (projection == NULL || rig == NULL || control == NULL || !projection->ready ||
        !control->valid || rig->node_count != projection->node_count ||
        control->revision <= projection->control_revision) {
        return false;
    }
    previous_expression = projection->focused_expression_weight;
    if (!apply_control(projection, rig, control) || !rotations_finite(rig)) {
        projection->focused_expression_weight = previous_expression;
        (void)restore_last_valid(projection, rig);
        return false;
    }
    for (size_t index = 0; index < rig->node_count; ++index) {
        memcpy(projection->last_valid_rotations[index], rig->nodes[index].rotation,
               sizeof(rig->nodes[index].rotation));
    }
    projection->control_revision = control->revision;
    return true;
}

void eidolon_vrm_projection_destroy(EidolonVrmProjection *projection) {
    if (projection == NULL) {
        return;
    }
    SDL_free(projection->last_valid_rotations);
    memset(projection, 0, sizeof(*projection));
}
