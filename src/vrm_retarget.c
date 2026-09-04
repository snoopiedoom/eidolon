#include "vrm_retarget.h"

#include "humanoid_rest.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETARGET_EPSILON 0.000001F

static void set_error(char *error, size_t capacity, const char *format, ...) {
    va_list arguments;
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
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

static bool quaternion_normalize(float quaternion[4]) {
    const float length_squared = quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                                 quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3];
    if (!isfinite(length_squared) || length_squared <= RETARGET_EPSILON) {
        return false;
    }
    const float scale = 1.0F / sqrtf(length_squared);
    for (size_t component = 0U; component < 4U; ++component) {
        quaternion[component] *= scale;
    }
    return true;
}

static bool node_world_rotation(const EidolonMotionRig *rig, int node_index, float result[4],
                                size_t remaining) {
    if (rig == NULL || node_index < 0 || (size_t)node_index >= rig->node_count || remaining == 0U) {
        return false;
    }
    const EidolonMotionNode *node = &rig->nodes[(size_t)node_index];
    float local[4];
    memcpy(local, node->rotation, sizeof(local));
    if (!quaternion_normalize(local)) {
        return false;
    }
    if (node->parent < 0) {
        memcpy(result, local, sizeof(local));
        return true;
    }
    float parent[4];
    if (!node_world_rotation(rig, node->parent, parent, remaining - 1U)) {
        return false;
    }
    quaternion_multiply(parent, local, result);
    return quaternion_normalize(result);
}

static int role_for_node(const EidolonVrmRetargeter *retargeter, int node) {
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if (retargeter->destination_node_by_role[role] == node) {
            return (int)role;
        }
    }
    return -1;
}

static bool validate_destination_hierarchy(const EidolonVrmRetargeter *retargeter,
                                           const EidolonMotionRig *rig, char *error,
                                           size_t error_capacity) {
    for (size_t role_index = 0U; role_index < EIDOLON_HUMANOID_ROLE_COUNT; ++role_index) {
        const int node_index = retargeter->destination_node_by_role[role_index];
        if (node_index < 0) {
            continue;
        }
        int expected_parent = eidolon_humanoid_role_parent((EidolonHumanoidRole)role_index);
        while (expected_parent >= 0 &&
               retargeter->destination_node_by_role[(size_t)expected_parent] < 0) {
            expected_parent = eidolon_humanoid_role_parent((EidolonHumanoidRole)expected_parent);
        }
        int parent_node = rig->nodes[(size_t)node_index].parent;
        int actual_parent = -1;
        size_t remaining = rig->node_count + 1U;
        while (parent_node >= 0 && remaining > 0U) {
            if ((size_t)parent_node >= rig->node_count) {
                set_error(error, error_capacity, "destination role '%s' has an invalid parent",
                          eidolon_humanoid_role_name((EidolonHumanoidRole)role_index));
                return false;
            }
            actual_parent = role_for_node(retargeter, parent_node);
            if (actual_parent >= 0) {
                break;
            }
            parent_node = rig->nodes[(size_t)parent_node].parent;
            remaining -= 1U;
        }
        if (remaining == 0U || actual_parent != expected_parent) {
            set_error(error, error_capacity,
                      "destination role '%s' has incompatible humanoid ancestry",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role_index));
            return false;
        }
    }
    return true;
}

static bool pose_finite(const EidolonMotionRig *rig) {
    if (rig == NULL || rig->nodes == NULL) {
        return false;
    }
    for (size_t node_index = 0U; node_index < rig->node_count; ++node_index) {
        const EidolonMotionNode *node = &rig->nodes[node_index];
        for (size_t axis = 0U; axis < 3U; ++axis) {
            if (!isfinite(node->translation[axis]) || !isfinite(node->scale[axis]) ||
                node->scale[axis] <= 0.0F) {
                return false;
            }
        }
        float rotation[4];
        memcpy(rotation, node->rotation, sizeof(rotation));
        if (!quaternion_normalize(rotation)) {
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

static void reset_scratch_to_destination_rest(EidolonVrmRetargeter *retargeter,
                                              const EidolonMotionRig *rig) {
    memcpy(retargeter->scratch.nodes, rig->nodes,
           rig->node_count * sizeof(*retargeter->scratch.nodes));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        const int node_index = retargeter->destination_node_by_role[role];
        if (node_index < 0) {
            continue;
        }
        EidolonMotionNode *node = &retargeter->scratch.nodes[(size_t)node_index];
        memcpy(node->translation, node->bind_translation, sizeof(node->translation));
        memcpy(node->rotation, retargeter->destination_rest_local_rotations[role],
               sizeof(node->rotation));
        node->world_state = 0U;
    }
}

static bool normalized_source_rotation(const EidolonVrmRetargeter *retargeter,
                                       EidolonHumanoidRole role, const float source_local[4],
                                       float result[4]) {
    return eidolon_humanoid_rest_rotation_to_normalized(
        retargeter->source_rest_local_rotations[(size_t)role],
        retargeter->source_rest_world_rotations[(size_t)role], source_local, result);
}

static bool destination_local_rotation(const EidolonVrmRetargeter *retargeter,
                                       EidolonHumanoidRole role, const float normalized[4],
                                       float result[4]) {
    return eidolon_humanoid_rest_rotation_from_normalized(
        retargeter->destination_rest_local_rotations[(size_t)role],
        retargeter->destination_rest_world_rotations[(size_t)role], normalized, result);
}

static bool compose_destination_role(const EidolonVrmRetargeter *retargeter,
                                     const EidolonHumanoidPose *source_pose,
                                     EidolonHumanoidRole destination_role, float result[4],
                                     bool *has_rotation) {
    EidolonHumanoidRole path[EIDOLON_HUMANOID_ROLE_COUNT];
    size_t path_count = 0U;
    int current = (int)destination_role;
    while (current >= 0 && path_count < EIDOLON_HUMANOID_ROLE_COUNT) {
        path[path_count++] = (EidolonHumanoidRole)current;
        const int parent = eidolon_humanoid_role_parent((EidolonHumanoidRole)current);
        if (parent < 0 || retargeter->destination_node_by_role[(size_t)parent] >= 0) {
            break;
        }
        current = parent;
    }
    if (path_count == EIDOLON_HUMANOID_ROLE_COUNT && current >= 0) {
        return false;
    }
    const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    memcpy(result, identity, sizeof(identity));
    *has_rotation = false;
    for (size_t path_index = path_count; path_index > 0U; --path_index) {
        const EidolonHumanoidRole source_role = path[path_index - 1U];
        if ((source_pose->rotation_mask & (UINT64_C(1) << (uint32_t)source_role)) == 0U) {
            continue;
        }
        float normalized[4];
        float composed[4];
        if (!normalized_source_rotation(retargeter, source_role,
                                        source_pose->rotations[(size_t)source_role], normalized)) {
            return false;
        }
        quaternion_multiply(result, normalized, composed);
        if (!quaternion_normalize(composed)) {
            return false;
        }
        memcpy(result, composed, sizeof(composed));
        *has_rotation = true;
    }
    return true;
}

static void commit_scratch(EidolonMotionRig *destination, const EidolonMotionRig *scratch) {
    for (size_t node_index = 0U; node_index < destination->node_count; ++node_index) {
        EidolonMotionNode *target = &destination->nodes[node_index];
        const EidolonMotionNode *source = &scratch->nodes[node_index];
        memcpy(target->translation, source->translation, sizeof(target->translation));
        memcpy(target->rotation, source->rotation, sizeof(target->rotation));
        memcpy(target->scale, source->scale, sizeof(target->scale));
        memcpy(target->world, source->world, sizeof(target->world));
        target->world_state = source->world_state;
    }
}

static bool compose_normalized_destination_role(const EidolonVrmRetargeter *retargeter,
                                                const EidolonHumanoidPose *normalized_pose,
                                                EidolonHumanoidRole destination_role,
                                                float result[4], bool *has_rotation) {
    EidolonHumanoidRole path[EIDOLON_HUMANOID_ROLE_COUNT];
    size_t path_count = 0U;
    int current = (int)destination_role;
    const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    while (current >= 0 && path_count < EIDOLON_HUMANOID_ROLE_COUNT) {
        path[path_count++] = (EidolonHumanoidRole)current;
        const int parent = eidolon_humanoid_role_parent((EidolonHumanoidRole)current);
        if (parent < 0 || retargeter->destination_node_by_role[(size_t)parent] >= 0) {
            break;
        }
        current = parent;
    }
    if (path_count == EIDOLON_HUMANOID_ROLE_COUNT && current >= 0) {
        return false;
    }
    memcpy(result, identity, sizeof(identity));
    *has_rotation = false;
    for (size_t path_index = path_count; path_index > 0U; --path_index) {
        const EidolonHumanoidRole role = path[path_index - 1U];
        float composed[4];
        if ((normalized_pose->rotation_mask & (UINT64_C(1) << (uint32_t)role)) == 0U) {
            continue;
        }
        quaternion_multiply(result, normalized_pose->rotations[(size_t)role], composed);
        if (!quaternion_normalize(composed)) {
            return false;
        }
        memcpy(result, composed, sizeof(composed));
        *has_rotation = true;
    }
    return true;
}

bool eidolon_vrm_retargeter_init_destination(EidolonVrmRetargeter *retargeter,
                                             const EidolonVrmBody *destination,
                                             const EidolonMotionRig *destination_rig, char *error,
                                             size_t error_capacity) {
    EidolonVrmaClip normalized_source;
    memset(&normalized_source, 0, sizeof(normalized_source));
    normalized_source.version = EIDOLON_VRMA_CLIP_VERSION;
    normalized_source.mapped_roles = (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_COUNT) - UINT64_C(1);
    normalized_source.source_hips_height = 1.0F;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        normalized_source.rest_local_rotations[role][3] = 1.0F;
        normalized_source.rest_world_rotations[role][3] = 1.0F;
    }
    return eidolon_vrm_retargeter_init(retargeter, &normalized_source, destination, destination_rig,
                                       error, error_capacity);
}

bool eidolon_vrm_retargeter_init(EidolonVrmRetargeter *retargeter, const EidolonVrmaClip *source,
                                 const EidolonVrmBody *destination,
                                 const EidolonMotionRig *destination_rig, char *error,
                                 size_t error_capacity) {
    if (retargeter == NULL || source == NULL || destination == NULL || destination_rig == NULL ||
        destination_rig->nodes == NULL || destination_rig->node_count == 0U ||
        source->version != EIDOLON_VRMA_CLIP_VERSION || !isfinite(source->source_hips_height) ||
        source->source_hips_height <= RETARGET_EPSILON) {
        set_error(error, error_capacity, "invalid VRMA source or destination retarget request");
        return false;
    }
    memset(retargeter, 0, sizeof(*retargeter));
    retargeter->version = EIDOLON_VRM_RETARGETER_VERSION;
    retargeter->source_roles = source->mapped_roles;
    retargeter->source_hips_height = source->source_hips_height;
    memcpy(retargeter->source_rest_local_rotations, source->rest_local_rotations,
           sizeof(retargeter->source_rest_local_rotations));
    memcpy(retargeter->source_rest_world_rotations, source->rest_world_rotations,
           sizeof(retargeter->source_rest_world_rotations));
    memcpy(retargeter->source_rest_hips_translation, source->rest_hips_translation,
           sizeof(retargeter->source_rest_hips_translation));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        const uint64_t role_bit = UINT64_C(1) << (uint32_t)role;
        if ((retargeter->source_roles & role_bit) == 0U) {
            if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
                set_error(error, error_capacity, "source required role '%s' is missing",
                          eidolon_humanoid_role_name((EidolonHumanoidRole)role));
                eidolon_vrm_retargeter_destroy(retargeter);
                return false;
            }
            continue;
        }
        if (!quaternion_normalize(retargeter->source_rest_local_rotations[role]) ||
            !quaternion_normalize(retargeter->source_rest_world_rotations[role])) {
            set_error(error, error_capacity, "source role '%s' has an invalid rest frame",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role));
            eidolon_vrm_retargeter_destroy(retargeter);
            return false;
        }
    }
    for (size_t axis = 0U; axis < 3U; ++axis) {
        if (!isfinite(retargeter->source_rest_hips_translation[axis])) {
            set_error(error, error_capacity, "source hips rest translation is not finite");
            eidolon_vrm_retargeter_destroy(retargeter);
            return false;
        }
    }
    retargeter->node_count = destination_rig->node_count;
    retargeter->scratch = *destination_rig;
    retargeter->scratch.nodes =
        calloc(destination_rig->node_count, sizeof(*retargeter->scratch.nodes));
    if (retargeter->scratch.nodes == NULL) {
        set_error(error, error_capacity, "out of memory allocating retarget scratch rig");
        eidolon_vrm_retargeter_destroy(retargeter);
        return false;
    }
    memcpy(retargeter->scratch.nodes, destination_rig->nodes,
           destination_rig->node_count * sizeof(*destination_rig->nodes));
    for (size_t node_index = 0U; node_index < destination_rig->node_count; ++node_index) {
        EidolonMotionNode *node = &retargeter->scratch.nodes[node_index];
        memcpy(node->translation, node->bind_translation, sizeof(node->translation));
        memcpy(node->rotation, node->bind_rotation, sizeof(node->rotation));
        memcpy(node->scale, node->bind_scale, sizeof(node->scale));
        node->world_state = 0U;
    }
    memcpy(retargeter->destination_node_by_role, destination->node_by_role,
           sizeof(retargeter->destination_node_by_role));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        const int node_index = retargeter->destination_node_by_role[role];
        if (node_index < 0) {
            if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
                set_error(error, error_capacity, "destination required role '%s' is missing",
                          eidolon_humanoid_role_name((EidolonHumanoidRole)role));
                eidolon_vrm_retargeter_destroy(retargeter);
                return false;
            }
            retargeter->destination_rest_local_rotations[role][3] = 1.0F;
            retargeter->destination_rest_world_rotations[role][3] = 1.0F;
            continue;
        }
        if ((size_t)node_index >= destination_rig->node_count ||
            role_for_node(retargeter, node_index) != (int)role) {
            set_error(error, error_capacity,
                      "destination role '%s' has an invalid or duplicate node",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role));
            eidolon_vrm_retargeter_destroy(retargeter);
            return false;
        }
        retargeter->destination_roles |= UINT64_C(1) << (uint32_t)role;
    }
    if (!validate_destination_hierarchy(retargeter, &retargeter->scratch, error, error_capacity) ||
        !eidolon_motion_rebuild_world(&retargeter->scratch)) {
        if (error != NULL && error_capacity > 0U && error[0] == '\0') {
            set_error(error, error_capacity, "could not rebuild destination T-pose");
        }
        eidolon_vrm_retargeter_destroy(retargeter);
        return false;
    }
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        const int node_index = retargeter->destination_node_by_role[role];
        if (node_index < 0) {
            continue;
        }
        const EidolonMotionNode *node = &retargeter->scratch.nodes[(size_t)node_index];
        memcpy(retargeter->destination_rest_local_rotations[role], node->rotation,
               sizeof(node->rotation));
        if (!quaternion_normalize(retargeter->destination_rest_local_rotations[role]) ||
            !node_world_rotation(&retargeter->scratch, node_index,
                                 retargeter->destination_rest_world_rotations[role],
                                 retargeter->node_count + 1U)) {
            set_error(error, error_capacity, "destination role '%s' has an invalid rest rotation",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role));
            eidolon_vrm_retargeter_destroy(retargeter);
            return false;
        }
    }
    const int hips_node = retargeter->destination_node_by_role[EIDOLON_HUMANOID_ROLE_HIPS];
    memcpy(retargeter->destination_rest_hips_translation,
           retargeter->scratch.nodes[(size_t)hips_node].translation,
           sizeof(retargeter->destination_rest_hips_translation));
    retargeter->destination_hips_height = retargeter->scratch.nodes[(size_t)hips_node].world[13];
    if (!isfinite(retargeter->destination_hips_height) ||
        retargeter->destination_hips_height <= RETARGET_EPSILON) {
        set_error(error, error_capacity, "destination T-pose hips height must be positive");
        eidolon_vrm_retargeter_destroy(retargeter);
        return false;
    }
    retargeter->hips_translation_scale =
        retargeter->destination_hips_height / retargeter->source_hips_height;
    if (!isfinite(retargeter->hips_translation_scale) ||
        retargeter->hips_translation_scale <= RETARGET_EPSILON) {
        set_error(error, error_capacity, "invalid source/destination hips scale");
        eidolon_vrm_retargeter_destroy(retargeter);
        return false;
    }
    retargeter->ready = true;
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

bool eidolon_vrm_retargeter_apply(EidolonVrmRetargeter *retargeter,
                                  const EidolonHumanoidPose *source_pose,
                                  EidolonVrmRootMotionPolicy root_motion_policy,
                                  EidolonMotionRig *destination_rig, char *error,
                                  size_t error_capacity) {
    if (retargeter == NULL || source_pose == NULL || destination_rig == NULL ||
        !retargeter->ready || retargeter->version != EIDOLON_VRM_RETARGETER_VERSION ||
        source_pose->version != EIDOLON_HUMANOID_POSE_VERSION || destination_rig->nodes == NULL ||
        destination_rig->node_count != retargeter->node_count ||
        (source_pose->rotation_mask & ~retargeter->source_roles) != 0U ||
        (root_motion_policy != EIDOLON_VRM_ROOT_MOTION_IN_PLACE &&
         root_motion_policy != EIDOLON_VRM_ROOT_MOTION_FULL)) {
        set_error(error, error_capacity, "invalid humanoid retarget request");
        return false;
    }
    for (size_t axis = 0U; source_pose->has_hips_translation && axis < 3U; ++axis) {
        if (!isfinite(source_pose->hips_translation[axis])) {
            set_error(error, error_capacity, "source hips translation is not finite");
            return false;
        }
    }
    reset_scratch_to_destination_rest(retargeter, destination_rig);
    for (size_t role_index = 0U; role_index < EIDOLON_HUMANOID_ROLE_COUNT; ++role_index) {
        const int node_index = retargeter->destination_node_by_role[role_index];
        if (node_index < 0) {
            continue;
        }
        float normalized[4];
        bool has_rotation = false;
        if (!compose_destination_role(retargeter, source_pose, (EidolonHumanoidRole)role_index,
                                      normalized, &has_rotation)) {
            set_error(error, error_capacity, "could not compose destination role '%s'",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role_index));
            return false;
        }
        if (!has_rotation) {
            continue;
        }
        float local[4];
        if (!destination_local_rotation(retargeter, (EidolonHumanoidRole)role_index, normalized,
                                        local)) {
            set_error(error, error_capacity, "could not reconstruct destination role '%s'",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role_index));
            return false;
        }
        memcpy(retargeter->scratch.nodes[(size_t)node_index].rotation, local, sizeof(local));
    }
    const int hips_node = retargeter->destination_node_by_role[EIDOLON_HUMANOID_ROLE_HIPS];
    if (source_pose->has_hips_translation) {
        float *destination = retargeter->scratch.nodes[(size_t)hips_node].translation;
        for (size_t axis = 0U; axis < 3U; ++axis) {
            const float delta = source_pose->hips_translation[axis] -
                                retargeter->source_rest_hips_translation[axis];
            if (root_motion_policy == EIDOLON_VRM_ROOT_MOTION_FULL || axis == 1U) {
                destination[axis] = retargeter->destination_rest_hips_translation[axis] +
                                    delta * retargeter->hips_translation_scale;
            }
        }
    }
    if (!eidolon_motion_rebuild_world(&retargeter->scratch) || !pose_finite(&retargeter->scratch)) {
        set_error(error, error_capacity, "retargeted destination hierarchy is invalid");
        return false;
    }
    commit_scratch(destination_rig, &retargeter->scratch);
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

bool eidolon_vrm_retargeter_apply_normalized(EidolonVrmRetargeter *retargeter,
                                             const EidolonHumanoidPose *normalized_pose,
                                             EidolonVrmRootMotionPolicy root_motion_policy,
                                             EidolonMotionRig *destination_rig, char *error,
                                             size_t error_capacity) {
    if (retargeter == NULL || normalized_pose == NULL || destination_rig == NULL ||
        !retargeter->ready || retargeter->version != EIDOLON_VRM_RETARGETER_VERSION ||
        destination_rig->nodes == NULL || destination_rig->node_count != retargeter->node_count ||
        !eidolon_humanoid_pose_validate(normalized_pose) ||
        (root_motion_policy != EIDOLON_VRM_ROOT_MOTION_IN_PLACE &&
         root_motion_policy != EIDOLON_VRM_ROOT_MOTION_FULL)) {
        set_error(error, error_capacity, "invalid normalized humanoid projection request");
        return false;
    }
    memcpy(retargeter->scratch.nodes, destination_rig->nodes,
           destination_rig->node_count * sizeof(*destination_rig->nodes));
    for (size_t role_index = 0U; role_index < EIDOLON_HUMANOID_ROLE_COUNT; ++role_index) {
        const int node_index = retargeter->destination_node_by_role[role_index];
        float normalized[4];
        bool has_rotation = false;
        if (node_index < 0) {
            continue;
        }
        if (!compose_normalized_destination_role(retargeter, normalized_pose,
                                                 (EidolonHumanoidRole)role_index, normalized,
                                                 &has_rotation)) {
            set_error(error, error_capacity, "could not compose normalized destination role '%s'",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role_index));
            return false;
        }
        if (!has_rotation) {
            continue;
        }
        float local[4];
        if (!destination_local_rotation(retargeter, (EidolonHumanoidRole)role_index, normalized,
                                        local)) {
            set_error(error, error_capacity,
                      "could not reconstruct normalized destination role '%s'",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role_index));
            return false;
        }
        memcpy(retargeter->scratch.nodes[(size_t)node_index].rotation, local, sizeof(local));
    }
    if (normalized_pose->has_hips_translation) {
        const int hips_node = retargeter->destination_node_by_role[EIDOLON_HUMANOID_ROLE_HIPS];
        float translated[3];
        if (!eidolon_humanoid_rest_translation_from_normalized(
                retargeter->destination_rest_hips_translation, retargeter->destination_hips_height,
                normalized_pose->hips_translation, translated)) {
            set_error(error, error_capacity, "could not reconstruct normalized hips translation");
            return false;
        }
        for (size_t axis = 0U; axis < 3U; ++axis) {
            if (root_motion_policy == EIDOLON_VRM_ROOT_MOTION_FULL || axis == 1U) {
                retargeter->scratch.nodes[(size_t)hips_node].translation[axis] = translated[axis];
            }
        }
    }
    if (!eidolon_motion_rebuild_world(&retargeter->scratch) || !pose_finite(&retargeter->scratch)) {
        set_error(error, error_capacity, "normalized destination hierarchy is invalid");
        return false;
    }
    commit_scratch(destination_rig, &retargeter->scratch);
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

void eidolon_vrm_retargeter_destroy(EidolonVrmRetargeter *retargeter) {
    if (retargeter == NULL) {
        return;
    }
    free(retargeter->scratch.nodes);
    memset(retargeter, 0, sizeof(*retargeter));
}
