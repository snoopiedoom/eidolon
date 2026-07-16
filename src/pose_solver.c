#include "pose_solver.h"

#include "ik.h"

#include <SDL3/SDL.h>

#include <math.h>

#define POSE_EPSILON 0.00001F

static float vector_dot(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static void vector_subtract(const float left[3], const float right[3], float result[3]) {
    result[0] = left[0] - right[0];
    result[1] = left[1] - right[1];
    result[2] = left[2] - right[2];
}

static float vector_length(const float vector[3]) { return SDL_sqrtf(vector_dot(vector, vector)); }

static bool vector_normalize(float vector[3]) {
    const float length = vector_length(vector);
    if (length <= POSE_EPSILON) {
        return false;
    }
    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

static void vector_cross(const float left[3], const float right[3], float result[3]) {
    result[0] = left[1] * right[2] - left[2] * right[1];
    result[1] = left[2] * right[0] - left[0] * right[2];
    result[2] = left[0] * right[1] - left[1] * right[0];
}

static void point_from_world(const EidolonMotionRig *rig, int node, float point[3]) {
    point[0] = rig->nodes[(size_t)node].world[12];
    point[1] = rig->nodes[(size_t)node].world[13];
    point[2] = rig->nodes[(size_t)node].world[14];
}

static void quaternion_normalize(float quaternion[4]) {
    const float length = SDL_sqrtf(quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                                   quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3]);
    if (length <= POSE_EPSILON) {
        quaternion[0] = 0.0F;
        quaternion[1] = 0.0F;
        quaternion[2] = 0.0F;
        quaternion[3] = 1.0F;
        return;
    }
    for (size_t component = 0; component < 4; ++component) {
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
    SDL_memcpy(result, product, sizeof(product));
}

static void quaternion_inverse(const float quaternion[4], float inverse[4]) {
    inverse[0] = -quaternion[0];
    inverse[1] = -quaternion[1];
    inverse[2] = -quaternion[2];
    inverse[3] = quaternion[3];
}

static bool quaternion_from_to(const float from_vector[3], const float to_vector[3],
                               float quaternion[4]) {
    float from[3];
    float to[3];
    SDL_memcpy(from, from_vector, sizeof(from));
    SDL_memcpy(to, to_vector, sizeof(to));
    if (!vector_normalize(from) || !vector_normalize(to)) {
        return false;
    }
    const float dot = SDL_clamp(vector_dot(from, to), -1.0F, 1.0F);
    if (dot < -0.9999F) {
        const float axis_hint[3] = {SDL_fabsf(from[1]) < 0.9F ? 0.0F : 1.0F,
                                    SDL_fabsf(from[1]) < 0.9F ? 1.0F : 0.0F, 0.0F};
        vector_cross(from, axis_hint, quaternion);
        if (!vector_normalize(quaternion)) {
            return false;
        }
        quaternion[3] = 0.0F;
        return true;
    }
    vector_cross(from, to, quaternion);
    quaternion[3] = 1.0F + dot;
    quaternion_normalize(quaternion);
    return true;
}

static void quaternion_nlerp(const float from[4], const float to[4], float weight,
                             float result[4]) {
    float target[4];
    SDL_memcpy(target, to, sizeof(target));
    const float dot = from[0] * to[0] + from[1] * to[1] + from[2] * to[2] + from[3] * to[3];
    if (dot < 0.0F) {
        for (size_t component = 0; component < 4; ++component) {
            target[component] = -target[component];
        }
    }
    const float clamped = SDL_clamp(weight, 0.0F, 1.0F);
    for (size_t component = 0; component < 4; ++component) {
        result[component] = from[component] + (target[component] - from[component]) * clamped;
    }
    quaternion_normalize(result);
}

static bool node_world_rotation(const EidolonMotionRig *rig, int node, float rotation[4]) {
    if (node < 0 || (size_t)node >= rig->node_count) {
        return false;
    }
    const int parent = rig->nodes[(size_t)node].parent;
    if (parent < 0) {
        SDL_memcpy(rotation, rig->nodes[(size_t)node].rotation, sizeof(float) * 4U);
        quaternion_normalize(rotation);
        return true;
    }
    float parent_rotation[4];
    if (!node_world_rotation(rig, parent, parent_rotation)) {
        return false;
    }
    quaternion_multiply(parent_rotation, rig->nodes[(size_t)node].rotation, rotation);
    quaternion_normalize(rotation);
    return true;
}

static bool rotate_node_toward(EidolonMotionRig *rig, int node, const float current_direction[3],
                               const float desired_direction[3], float weight) {
    float delta[4];
    float current_world[4];
    if (!quaternion_from_to(current_direction, desired_direction, delta) ||
        !node_world_rotation(rig, node, current_world)) {
        return false;
    }
    float target_world[4];
    quaternion_multiply(delta, current_world, target_world);

    float target_local[4];
    const int parent = rig->nodes[(size_t)node].parent;
    if (parent >= 0) {
        float parent_world[4];
        float parent_inverse[4];
        if (!node_world_rotation(rig, parent, parent_world)) {
            return false;
        }
        quaternion_inverse(parent_world, parent_inverse);
        quaternion_multiply(parent_inverse, target_world, target_local);
    } else {
        SDL_memcpy(target_local, target_world, sizeof(target_local));
    }
    quaternion_nlerp(rig->nodes[(size_t)node].rotation, target_local, weight,
                     rig->nodes[(size_t)node].rotation);
    return eidolon_motion_rebuild_world(rig);
}

static bool current_body_basis(EidolonMotionRig *rig, const EidolonHumanoidProfile *profile,
                               float right[3], float up[3], float forward[3]) {
    float hips[3];
    float head[3];
    float left_shoulder[3];
    float right_shoulder[3];
    point_from_world(rig, profile->nodes[EIDOLON_HUMANOID_HIPS], hips);
    point_from_world(rig, profile->nodes[EIDOLON_HUMANOID_HEAD], head);
    point_from_world(rig, profile->nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM], left_shoulder);
    point_from_world(rig, profile->nodes[EIDOLON_HUMANOID_RIGHT_UPPER_ARM], right_shoulder);
    vector_subtract(head, hips, up);
    vector_subtract(right_shoulder, left_shoulder, right);
    if (!vector_normalize(up) || !vector_normalize(right)) {
        return false;
    }
    const float vertical_component = vector_dot(right, up);
    for (size_t axis = 0; axis < 3; ++axis) {
        right[axis] -= up[axis] * vertical_component;
    }
    if (!vector_normalize(right)) {
        return false;
    }
    vector_cross(right, up, forward);
    if (!vector_normalize(forward)) {
        return false;
    }
    if (vector_dot(forward, profile->forward) < 0.0F) {
        for (size_t axis = 0; axis < 3; ++axis) {
            forward[axis] = -forward[axis];
        }
    }
    return true;
}

static bool vector_is_finite(const float vector[3]) {
    return isfinite(vector[0]) && isfinite(vector[1]) && isfinite(vector[2]);
}

static bool pose_is_valid(const EidolonMotionRig *rig, const EidolonHumanoidProfile *profile,
                          const EidolonSemanticPose *pose) {
    if (rig == NULL || profile == NULL || pose == NULL || !isfinite(pose->soften_ratio) ||
        pose->soften_ratio < 0.0F || pose->soften_ratio > 0.95F) {
        SDL_SetError("semantic pose has invalid solver parameters");
        return false;
    }
    for (size_t side = 0; side < EIDOLON_POSE_ARM_COUNT; ++side) {
        const EidolonArmPoseGoal *goal = &pose->arms[side];
        if (!vector_is_finite(goal->hand) || !vector_is_finite(goal->elbow_pole) ||
            !isfinite(goal->weight) || goal->weight < 0.0F || goal->weight > 1.0F) {
            SDL_SetError("semantic pose has invalid arm goal");
            return false;
        }
        int upper = -1;
        int lower = -1;
        int hand = -1;
        eidolon_humanoid_arm_nodes(profile, (EidolonHumanoidSide)side, &upper, &lower, &hand);
        if (upper < 0 || lower < 0 || hand < 0 || (size_t)upper >= rig->node_count ||
            (size_t)lower >= rig->node_count || (size_t)hand >= rig->node_count) {
            SDL_SetError("semantic pose references an invalid arm chain");
            return false;
        }
    }
    return true;
}

static void body_relative_point(const float root[3], const float outward[3], const float up[3],
                                const float forward[3], const float coordinates[3], float scale,
                                float point[3]) {
    for (size_t axis = 0; axis < 3; ++axis) {
        point[axis] =
            root[axis] + scale * (outward[axis] * coordinates[0] + up[axis] * coordinates[1] +
                                  forward[axis] * coordinates[2]);
    }
}

static bool solve_arm(EidolonMotionRig *rig, const EidolonHumanoidProfile *profile,
                      const EidolonSemanticPose *pose, EidolonHumanoidSide side,
                      const float right[3], const float up[3], const float forward[3]) {
    const EidolonArmPoseGoal *goal = &pose->arms[(size_t)side];
    if (goal->weight <= 0.0F) {
        return true;
    }
    int upper_arm = -1;
    int lower_arm = -1;
    int hand = -1;
    eidolon_humanoid_arm_nodes(profile, side, &upper_arm, &lower_arm, &hand);
    float root[3];
    float mid[3];
    float end[3];
    point_from_world(rig, upper_arm, root);
    point_from_world(rig, lower_arm, mid);
    point_from_world(rig, hand, end);

    float outward[3];
    const float side_sign = side == EIDOLON_HUMANOID_LEFT ? -1.0F : 1.0F;
    for (size_t axis = 0; axis < 3; ++axis) {
        outward[axis] = right[axis] * side_sign;
    }
    const float upper_length =
        vector_length((float[3]){mid[0] - root[0], mid[1] - root[1], mid[2] - root[2]});
    const float lower_length =
        vector_length((float[3]){end[0] - mid[0], end[1] - mid[1], end[2] - mid[2]});
    const float arm_length = upper_length + lower_length;
    float target[3];
    float pole[3];
    body_relative_point(root, outward, up, forward, goal->hand, arm_length, target);
    body_relative_point(root, outward, up, forward, goal->elbow_pole, arm_length, pole);

    EidolonIkTwoBoneInput input = {
        .root = {root[0], root[1], root[2]},
        .target = {target[0], target[1], target[2]},
        .pole = {pole[0], pole[1], pole[2]},
        .fallback_direction = {end[0] - root[0], end[1] - root[1], end[2] - root[2]},
        .upper_length = upper_length,
        .lower_length = lower_length,
        .soften_ratio = pose->soften_ratio,
    };
    EidolonIkTwoBoneSolution solution;
    if (!eidolon_ik_solve_two_bone(&input, &solution)) {
        return false;
    }

    float current_upper[3];
    float desired_upper[3];
    vector_subtract(mid, root, current_upper);
    vector_subtract(solution.mid, root, desired_upper);
    if (!rotate_node_toward(rig, upper_arm, current_upper, desired_upper, goal->weight)) {
        return false;
    }

    point_from_world(rig, lower_arm, mid);
    point_from_world(rig, hand, end);
    float current_lower[3];
    float desired_lower[3];
    vector_subtract(end, mid, current_lower);
    vector_subtract(solution.end, solution.mid, desired_lower);
    return rotate_node_toward(rig, lower_arm, current_lower, desired_lower, goal->weight);
}

bool eidolon_pose_solve(EidolonMotionRig *rig, const EidolonHumanoidProfile *profile,
                        const EidolonSemanticPose *pose) {
    if (!pose_is_valid(rig, profile, pose)) {
        return false;
    }
    float right[3];
    float up[3];
    float forward[3];
    if (!current_body_basis(rig, profile, right, up, forward)) {
        SDL_SetError("could not derive current humanoid body basis");
        return false;
    }
    const int arm_nodes[4] = {
        profile->nodes[EIDOLON_HUMANOID_LEFT_UPPER_ARM],
        profile->nodes[EIDOLON_HUMANOID_LEFT_LOWER_ARM],
        profile->nodes[EIDOLON_HUMANOID_RIGHT_UPPER_ARM],
        profile->nodes[EIDOLON_HUMANOID_RIGHT_LOWER_ARM],
    };
    float original_rotations[4][4];
    for (size_t node = 0; node < SDL_arraysize(arm_nodes); ++node) {
        SDL_memcpy(original_rotations[node], rig->nodes[(size_t)arm_nodes[node]].rotation,
                   sizeof(original_rotations[node]));
    }
    if (!solve_arm(rig, profile, pose, EIDOLON_HUMANOID_LEFT, right, up, forward) ||
        !solve_arm(rig, profile, pose, EIDOLON_HUMANOID_RIGHT, right, up, forward)) {
        char cause[256];
        SDL_strlcpy(cause, SDL_GetError(), sizeof(cause));
        for (size_t node = 0; node < SDL_arraysize(arm_nodes); ++node) {
            SDL_memcpy(rig->nodes[(size_t)arm_nodes[node]].rotation, original_rotations[node],
                       sizeof(original_rotations[node]));
        }
        (void)eidolon_motion_rebuild_world(rig);
        SDL_SetError("could not solve semantic arm targets%s%s", cause[0] != '\0' ? ": " : "",
                     cause);
        return false;
    }
    return true;
}
