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
    if (!isfinite(length) || length <= VRM_PROJECTION_EPSILON) {
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

static bool quaternion_normalize(float quaternion[4]) {
    const float length = SDL_sqrtf(quaternion[0] * quaternion[0] +
                                   quaternion[1] * quaternion[1] +
                                   quaternion[2] * quaternion[2] +
                                   quaternion[3] * quaternion[3]);
    if (!isfinite(length) || length <= VRM_PROJECTION_EPSILON) {
        return false;
    }
    for (size_t component = 0; component < 4U; ++component) {
        quaternion[component] /= length;
    }
    return true;
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
    return quaternion_normalize(quaternion);
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
        return quaternion_normalize(rotation);
    }
    {
        float parent[4];
        if (!node_world_rotation(rig, motion_node->parent, parent, remaining - 1U)) {
            return false;
        }
        quaternion_multiply(parent, motion_node->rotation, rotation);
        return quaternion_normalize(rotation);
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
    if (!quaternion_normalize(target_local)) {
        return false;
    }
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

static void quaternion_axis(const float axis[3], float radians, float result[4]) {
    const float half = radians * 0.5F;
    const float sine = SDL_sinf(half);
    result[0] = axis[0] * sine;
    result[1] = axis[1] * sine;
    result[2] = axis[2] * sine;
    result[3] = SDL_cosf(half);
}

static bool apply_corrected_euler(const EidolonVrmProjection *projection, EidolonMotionNode *node,
                                  EidolonVrmHumanBone bone, float pitch, float yaw, float roll) {
    float pitch_rotation[4];
    float yaw_rotation[4];
    float roll_rotation[4];
    float yaw_pitch[4];
    float world_delta[4];
    float correction_delta[4];
    float local_delta[4];
    float result[4];
    quaternion_axis(projection->right, pitch, pitch_rotation);
    quaternion_axis(projection->up, yaw, yaw_rotation);
    quaternion_axis(projection->forward, roll, roll_rotation);
    quaternion_multiply(yaw_rotation, pitch_rotation, yaw_pitch);
    quaternion_multiply(yaw_pitch, roll_rotation, world_delta);
    quaternion_multiply(projection->inverse_bind_world_rotations[(size_t)bone], world_delta,
                        correction_delta);
    quaternion_multiply(correction_delta, projection->bind_world_rotations[(size_t)bone],
                        local_delta);
    quaternion_multiply(node->rotation, local_delta, result);
    if (!quaternion_normalize(result)) {
        return false;
    }
    memcpy(node->rotation, result, sizeof(result));
    return true;
}

static EidolonVrmHumanBone chest_bone(const EidolonVrmProjection *projection) {
    if (projection->nodes[EIDOLON_VRM_BONE_UPPER_CHEST] >= 0) {
        return EIDOLON_VRM_BONE_UPPER_CHEST;
    }
    if (projection->nodes[EIDOLON_VRM_BONE_CHEST] >= 0) {
        return EIDOLON_VRM_BONE_CHEST;
    }
    return EIDOLON_VRM_BONE_SPINE;
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
    if (!rotate_node_toward(rig, lower, current_direction, desired_direction) ||
        !apply_corrected_euler(projection, &rig->nodes[(size_t)hand],
                               EIDOLON_VRM_BONE_RIGHT_HAND, control->right_wrist_euler[0],
                               control->right_wrist_euler[1], control->right_wrist_euler[2])) {
        return false;
    }
    return eidolon_motion_rebuild_world(rig);
}

static bool control_finite(const EidolonCanonicalControl *control) {
    const float scalars[] = {
        control->torso_pitch,
        control->torso_yaw,
        control->torso_roll,
        control->head_pitch,
        control->head_yaw,
        control->head_roll,
        control->eye_yaw,
        control->eye_pitch,
        control->eye_weight,
        control->head_gaze_weight,
        control->focused_expression_weight,
    };
    const float *vectors[] = {
        control->gaze_target,        control->right_hand_target,
        control->right_elbow_pole,   control->right_elbow_position,
        control->right_hand_position, control->right_wrist_euler,
        control->right_arm_velocity,
    };
    for (size_t index = 0U; index < SDL_arraysize(scalars); ++index) {
        if (!isfinite(scalars[index])) {
            return false;
        }
    }
    for (size_t vector = 0U; vector < SDL_arraysize(vectors); ++vector) {
        for (size_t axis = 0U; axis < 3U; ++axis) {
            if (!isfinite(vectors[vector][axis])) {
                return false;
            }
        }
    }
    for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
        for (size_t resource = 0U; resource < EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
            const float weight = control->pose_anchor_resource_weights[anchor][resource];
            if (!isfinite(weight) || weight < 0.0F || weight > 1.0F) {
                return false;
            }
        }
    }
    return true;
}

static bool pose_finite(const EidolonMotionRig *rig) {
    for (size_t index = 0U; index < rig->node_count; ++index) {
        const EidolonMotionNode *node = &rig->nodes[index];
        float length_squared = 0.0F;
        for (size_t axis = 0U; axis < 3U; ++axis) {
            if (!isfinite(node->translation[axis]) || !isfinite(node->scale[axis])) {
                return false;
            }
        }
        for (size_t component = 0U; component < 4U; ++component) {
            if (!isfinite(node->rotation[component])) {
                return false;
            }
            length_squared += node->rotation[component] * node->rotation[component];
        }
        if (!isfinite(length_squared) || length_squared <= VRM_PROJECTION_EPSILON) {
            return false;
        }
        for (size_t component = 0U; component < 16U; ++component) {
            if (!isfinite(node->world[component])) {
                return false;
            }
        }
    }
    return true;
}

static void copy_local_pose(EidolonMotionRig *destination, const EidolonMotionRig *source) {
    for (size_t index = 0U; index < source->node_count; ++index) {
        memcpy(destination->nodes[index].translation, source->nodes[index].translation,
               sizeof(destination->nodes[index].translation));
        memcpy(destination->nodes[index].rotation, source->nodes[index].rotation,
               sizeof(destination->nodes[index].rotation));
        memcpy(destination->nodes[index].scale, source->nodes[index].scale,
               sizeof(destination->nodes[index].scale));
        destination->nodes[index].world_state = 0U;
    }
}

static void stage_base_pose(EidolonVrmProjection *projection, const EidolonMotionRig *rig) {
    copy_local_pose(&projection->scratch, rig);
    for (size_t index = 0U; index < projection->node_count; ++index) {
        if (!projection->owned_nodes[index]) {
            continue;
        }
        memcpy(projection->scratch.nodes[index].translation, projection->base_translations[index],
               sizeof(projection->scratch.nodes[index].translation));
        memcpy(projection->scratch.nodes[index].rotation, projection->base_rotations[index],
               sizeof(projection->scratch.nodes[index].rotation));
        memcpy(projection->scratch.nodes[index].scale, projection->base_scales[index],
               sizeof(projection->scratch.nodes[index].scale));
    }
}

static int residual_resource(EidolonVrmHumanBone bone) {
    switch (bone) {
    case EIDOLON_VRM_BONE_HIPS:
    case EIDOLON_VRM_BONE_SPINE:
    case EIDOLON_VRM_BONE_CHEST:
    case EIDOLON_VRM_BONE_UPPER_CHEST:
        return EIDOLON_EPR_RESOURCE_TORSO;
    case EIDOLON_VRM_BONE_NECK:
    case EIDOLON_VRM_BONE_HEAD:
        return EIDOLON_EPR_RESOURCE_HEAD;
    case EIDOLON_VRM_BONE_LEFT_EYE:
    case EIDOLON_VRM_BONE_RIGHT_EYE:
        return EIDOLON_EPR_RESOURCE_EYES;
    case EIDOLON_VRM_BONE_LEFT_SHOULDER:
    case EIDOLON_VRM_BONE_LEFT_UPPER_ARM:
    case EIDOLON_VRM_BONE_LEFT_LOWER_ARM:
    case EIDOLON_VRM_BONE_LEFT_HAND:
        return EIDOLON_EPR_RESOURCE_LEFT_ARM_CHAIN;
    case EIDOLON_VRM_BONE_RIGHT_SHOULDER:
    case EIDOLON_VRM_BONE_RIGHT_UPPER_ARM:
    case EIDOLON_VRM_BONE_RIGHT_LOWER_ARM:
    case EIDOLON_VRM_BONE_RIGHT_HAND:
        return EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    case EIDOLON_VRM_BONE_LEFT_UPPER_LEG:
    case EIDOLON_VRM_BONE_LEFT_LOWER_LEG:
    case EIDOLON_VRM_BONE_LEFT_FOOT:
    case EIDOLON_VRM_BONE_RIGHT_UPPER_LEG:
    case EIDOLON_VRM_BONE_RIGHT_LOWER_LEG:
    case EIDOLON_VRM_BONE_RIGHT_FOOT:
    case EIDOLON_VRM_BONE_COUNT:
        return -1;
    }
    return -1;
}

static bool apply_calibration_residuals(EidolonMotionRig *rig,
                                        const EidolonVrmProjection *projection,
                                        const EidolonCanonicalControl *control,
                                        const EidolonVrmCalibration *calibration) {
    if (calibration == NULL) {
        return true;
    }
    if (calibration->version != EIDOLON_VRM_CALIBRATION_VERSION) {
        return false;
    }
    for (size_t anchor = 0U; anchor < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++anchor) {
        if ((calibration->anchor_mask & (UINT32_C(1) << (uint32_t)anchor)) == 0U) {
            continue;
        }
        const EidolonVrmCalibrationAnchor *source = &calibration->anchors[anchor];
        for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
            const int resource = residual_resource((EidolonVrmHumanBone)bone);
            const int node = projection->nodes[bone];
            if (resource < 0 || node < 0 ||
                (source->resource_mask & (UINT32_C(1) << (uint32_t)resource)) == 0U ||
                (source->residual_bone_mask & (UINT32_C(1) << (uint32_t)bone)) == 0U) {
                continue;
            }
            const float weight = control->pose_anchor_resource_weights[anchor][(size_t)resource];
            if (weight <= VRM_PROJECTION_EPSILON) {
                continue;
            }
            const float *authored = source->residual_rotation[bone];
            const float sign = authored[3] < 0.0F ? -1.0F : 1.0F;
            float weighted[4] = {
                authored[0] * sign * weight,
                authored[1] * sign * weight,
                authored[2] * sign * weight,
                1.0F + (authored[3] * sign - 1.0F) * weight,
            };
            float composed[4];
            if (!quaternion_normalize(weighted)) {
                return false;
            }
            quaternion_multiply(rig->nodes[(size_t)node].rotation, weighted, composed);
            if (!quaternion_normalize(composed)) {
                return false;
            }
            memcpy(rig->nodes[(size_t)node].rotation, composed, sizeof(composed));
        }
    }
    return eidolon_motion_rebuild_world(rig);
}

static bool apply_control(const EidolonVrmProjection *projection, EidolonMotionRig *rig,
                          const EidolonCanonicalControl *control, float *expression_weight) {
    const EidolonVrmHumanBone torso_bone = chest_bone(projection);
    const int torso = projection->nodes[(size_t)torso_bone];
    const int head = projection->nodes[EIDOLON_VRM_BONE_HEAD];
    const int left_eye = projection->nodes[EIDOLON_VRM_BONE_LEFT_EYE];
    const int right_eye = projection->nodes[EIDOLON_VRM_BONE_RIGHT_EYE];
    if (!apply_corrected_euler(projection, &rig->nodes[(size_t)torso], torso_bone,
                               control->torso_pitch, control->torso_yaw,
                               control->torso_roll) ||
        !apply_corrected_euler(projection, &rig->nodes[(size_t)head], EIDOLON_VRM_BONE_HEAD,
                               control->head_pitch, control->head_yaw, control->head_roll)) {
        return false;
    }
    if (projection->look_at_executable && left_eye >= 0 && right_eye >= 0 &&
        control->eye_weight > 0.0F) {
        if (!apply_corrected_euler(projection, &rig->nodes[(size_t)left_eye],
                                   EIDOLON_VRM_BONE_LEFT_EYE, control->eye_pitch,
                                   control->eye_yaw, 0.0F) ||
            !apply_corrected_euler(projection, &rig->nodes[(size_t)right_eye],
                                   EIDOLON_VRM_BONE_RIGHT_EYE, control->eye_pitch,
                                   control->eye_yaw, 0.0F)) {
            return false;
        }
    }
    if (!eidolon_motion_rebuild_world(rig) || !apply_right_arm(rig, projection, control)) {
        return false;
    }
    *expression_weight = projection->expression_executable
                             ? SDL_clamp(control->focused_expression_weight, 0.0F, 1.0F)
                             : 0.0F;
    if (projection->expression_is_binary) {
        *expression_weight = *expression_weight > 0.5F ? 1.0F : 0.0F;
    }
    return true;
}

static void mark_owned_node(EidolonVrmProjection *projection, int node) {
    if (node >= 0 && (size_t)node < projection->node_count) {
        projection->owned_nodes[(size_t)node] = true;
    }
}

static bool initialize_bind_frames(EidolonVrmProjection *projection,
                                   const EidolonMotionRig *rig) {
    copy_local_pose(&projection->scratch, rig);
    for (size_t index = 0U; index < projection->node_count; ++index) {
        EidolonMotionNode *node = &projection->scratch.nodes[index];
        memcpy(node->translation, node->bind_translation, sizeof(node->translation));
        memcpy(node->rotation, node->bind_rotation, sizeof(node->rotation));
        memcpy(node->scale, node->bind_scale, sizeof(node->scale));
        node->world_state = 0U;
    }
    if (!eidolon_motion_rebuild_world(&projection->scratch)) {
        return false;
    }
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        const int node = projection->nodes[bone];
        if (node < 0) {
            projection->bind_world_rotations[bone][3] = 1.0F;
            projection->inverse_bind_world_rotations[bone][3] = 1.0F;
            continue;
        }
        if (!node_world_rotation(&projection->scratch, node,
                                 projection->bind_world_rotations[bone],
                                 projection->node_count + 1U)) {
            return false;
        }
        quaternion_inverse(projection->bind_world_rotations[bone],
                           projection->inverse_bind_world_rotations[bone]);
    }
    return true;
}

bool eidolon_vrm_projection_init(EidolonVrmProjection *projection, const EidolonVrmBody *body,
                                 const EidolonEprBodyProfile *profile,
                                 const EidolonMotionRig *rig) {
    EidolonMotionNode *scratch_nodes;
    if (projection == NULL || body == NULL || profile == NULL || rig == NULL ||
        rig->node_count == 0U || profile->version != EIDOLON_EPR_BODY_PROFILE_VERSION) {
        return false;
    }
    memset(projection, 0, sizeof(*projection));
    memcpy(projection->nodes, body->node_by_bone, sizeof(projection->nodes));
    projection->look_at_executable =
        body->look_at.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE;
    projection->expression_executable =
        body->relaxed_expression.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE;
    projection->expression_is_binary = body->relaxed_expression.is_binary;
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
    projection->node_count = rig->node_count;
    projection->base_translations =
        SDL_calloc(rig->node_count, sizeof(*projection->base_translations));
    projection->base_rotations = SDL_calloc(rig->node_count, sizeof(*projection->base_rotations));
    projection->base_scales = SDL_calloc(rig->node_count, sizeof(*projection->base_scales));
    projection->owned_nodes = SDL_calloc(rig->node_count, sizeof(*projection->owned_nodes));
    scratch_nodes = SDL_calloc(rig->node_count, sizeof(*scratch_nodes));
    if (projection->base_translations == NULL || projection->base_rotations == NULL ||
        projection->base_scales == NULL || projection->owned_nodes == NULL ||
        scratch_nodes == NULL) {
        SDL_free(scratch_nodes);
        eidolon_vrm_projection_destroy(projection);
        return false;
    }
    projection->scratch = *rig;
    projection->scratch.nodes = scratch_nodes;
    memcpy(projection->scratch.nodes, rig->nodes, rig->node_count * sizeof(*rig->nodes));
    memcpy(projection->right, profile->right, sizeof(projection->right));
    memcpy(projection->up, profile->up, sizeof(projection->up));
    memcpy(projection->forward, profile->forward, sizeof(projection->forward));
    if (!normalize3(projection->right) || !normalize3(projection->up) ||
        !normalize3(projection->forward)) {
        eidolon_vrm_projection_destroy(projection);
        return false;
    }
    mark_owned_node(projection, projection->nodes[(size_t)chest_bone(projection)]);
    mark_owned_node(projection, projection->nodes[EIDOLON_VRM_BONE_HEAD]);
    if (projection->look_at_executable) {
        mark_owned_node(projection, projection->nodes[EIDOLON_VRM_BONE_LEFT_EYE]);
        mark_owned_node(projection, projection->nodes[EIDOLON_VRM_BONE_RIGHT_EYE]);
    }
    mark_owned_node(projection, projection->nodes[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM]);
    mark_owned_node(projection, projection->nodes[EIDOLON_VRM_BONE_RIGHT_LOWER_ARM]);
    mark_owned_node(projection, projection->nodes[EIDOLON_VRM_BONE_RIGHT_HAND]);
    if (!initialize_bind_frames(projection, rig) ||
        !eidolon_vrm_projection_capture_base(projection, rig)) {
        eidolon_vrm_projection_destroy(projection);
        return false;
    }
    projection->ready = true;
    return true;
}

bool eidolon_vrm_projection_capture_base(EidolonVrmProjection *projection,
                                         const EidolonMotionRig *rig) {
    if (projection == NULL || rig == NULL || rig->node_count != projection->node_count ||
        projection->base_translations == NULL || projection->base_rotations == NULL ||
        projection->base_scales == NULL || !pose_finite(rig)) {
        return false;
    }
    for (size_t index = 0U; index < rig->node_count; ++index) {
        memcpy(projection->base_translations[index], rig->nodes[index].translation,
               sizeof(projection->base_translations[index]));
        memcpy(projection->base_rotations[index], rig->nodes[index].rotation,
               sizeof(projection->base_rotations[index]));
        memcpy(projection->base_scales[index], rig->nodes[index].scale,
               sizeof(projection->base_scales[index]));
    }
    return true;
}

bool eidolon_vrm_projection_apply(EidolonVrmProjection *projection, EidolonMotionRig *rig,
                                  const EidolonCanonicalControl *control) {
    return eidolon_vrm_projection_apply_calibrated(projection, rig, control, NULL);
}

bool eidolon_vrm_projection_apply_calibrated(EidolonVrmProjection *projection,
                                             EidolonMotionRig *rig,
                                             const EidolonCanonicalControl *control,
                                             const EidolonVrmCalibration *calibration) {
    float expression_weight = 0.0F;
    if (projection == NULL || rig == NULL || control == NULL || !projection->ready ||
        !control->valid || control->version != EIDOLON_EPR_CONTROL_VERSION ||
        !control_finite(control) || rig->node_count != projection->node_count ||
        control->revision <= projection->control_revision) {
        return false;
    }
    stage_base_pose(projection, rig);
    if (!eidolon_motion_rebuild_world(&projection->scratch) ||
        !apply_control(projection, &projection->scratch, control, &expression_weight) ||
        !apply_calibration_residuals(&projection->scratch, projection, control, calibration) ||
        !pose_finite(&projection->scratch)) {
        return false;
    }
    copy_local_pose(rig, &projection->scratch);
    for (size_t index = 0U; index < rig->node_count; ++index) {
        memcpy(rig->nodes[index].world, projection->scratch.nodes[index].world,
               sizeof(rig->nodes[index].world));
        rig->nodes[index].world_state = projection->scratch.nodes[index].world_state;
    }
    projection->focused_expression_weight = expression_weight;
    projection->control_revision = control->revision;
    return true;
}

void eidolon_vrm_projection_destroy(EidolonVrmProjection *projection) {
    if (projection == NULL) {
        return;
    }
    SDL_free(projection->scratch.nodes);
    SDL_free(projection->base_translations);
    SDL_free(projection->base_rotations);
    SDL_free(projection->base_scales);
    SDL_free(projection->owned_nodes);
    memset(projection, 0, sizeof(*projection));
}
