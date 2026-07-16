#include "motion.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static void matrix_multiply(const float left[16], const float right[16], float result[16]) {
    float product[16];
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            product[column * 4 + row] = left[0 * 4 + row] * right[column * 4 + 0] +
                                        left[1 * 4 + row] * right[column * 4 + 1] +
                                        left[2 * 4 + row] * right[column * 4 + 2] +
                                        left[3 * 4 + row] * right[column * 4 + 3];
        }
    }
    SDL_memcpy(result, product, sizeof(product));
}

static void matrix_from_transform(const EidolonMotionNode *node, float matrix[16]) {
    const float qx = node->rotation[0];
    const float qy = node->rotation[1];
    const float qz = node->rotation[2];
    const float qw = node->rotation[3];
    const float sx = node->scale[0];
    const float sy = node->scale[1];
    const float sz = node->scale[2];

    matrix[0] = (1.0F - 2.0F * qy * qy - 2.0F * qz * qz) * sx;
    matrix[1] = (2.0F * qx * qy + 2.0F * qz * qw) * sx;
    matrix[2] = (2.0F * qx * qz - 2.0F * qy * qw) * sx;
    matrix[3] = 0.0F;
    matrix[4] = (2.0F * qx * qy - 2.0F * qz * qw) * sy;
    matrix[5] = (1.0F - 2.0F * qx * qx - 2.0F * qz * qz) * sy;
    matrix[6] = (2.0F * qy * qz + 2.0F * qx * qw) * sy;
    matrix[7] = 0.0F;
    matrix[8] = (2.0F * qx * qz + 2.0F * qy * qw) * sz;
    matrix[9] = (2.0F * qy * qz - 2.0F * qx * qw) * sz;
    matrix[10] = (1.0F - 2.0F * qx * qx - 2.0F * qy * qy) * sz;
    matrix[11] = 0.0F;
    matrix[12] = node->translation[0];
    matrix[13] = node->translation[1];
    matrix[14] = node->translation[2];
    matrix[15] = 1.0F;
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

static void apply_local_rotation(EidolonMotionNode *node, float axis_x, float axis_y, float axis_z,
                                 float radians) {
    const float half_angle = radians * 0.5F;
    const float sine = sinf(half_angle);
    const float delta[4] = {axis_x * sine, axis_y * sine, axis_z * sine, cosf(half_angle)};
    quaternion_multiply(node->bind_rotation, delta, node->rotation);
}

static int find_node(const EidolonMotionRig *rig, const char *name) {
    for (size_t node_index = 0; node_index < rig->node_count; ++node_index) {
        if (SDL_strcmp(rig->nodes[node_index].name, name) == 0) {
            return (int)node_index;
        }
    }
    return -1;
}

static bool resolve_world(EidolonMotionRig *rig, size_t node_index) {
    EidolonMotionNode *node = &rig->nodes[node_index];
    if (node->world_state == 2U) {
        return true;
    }
    if (node->world_state == 1U) {
        SDL_SetError("cycle in model node hierarchy at '%s'", node->name);
        return false;
    }
    node->world_state = 1U;

    float local[16];
    matrix_from_transform(node, local);
    if (node->parent >= 0) {
        const size_t parent = (size_t)node->parent;
        if (parent >= rig->node_count || !resolve_world(rig, parent)) {
            return false;
        }
        matrix_multiply(rig->nodes[parent].world, local, node->world);
    } else {
        SDL_memcpy(node->world, local, sizeof(local));
    }
    node->world_state = 2U;
    return true;
}

bool eidolon_motion_init(EidolonMotionRig *rig, size_t node_count) {
    SDL_zero(*rig);
    if (node_count == 0 || node_count > (size_t)INT_MAX) {
        SDL_SetError("invalid motion rig node count");
        return false;
    }
    rig->nodes = SDL_calloc(node_count, sizeof(*rig->nodes));
    if (rig->nodes == NULL) {
        SDL_SetError("out of memory while creating motion rig");
        return false;
    }
    rig->node_count = node_count;
    rig->idle_tuning = (EidolonIdleTuning){
        .breath_period_seconds = 3.696F,
        .breath_chest_radians = 0.012F,
        .breath_neck_counter_radians = 0.005F,
        .sway_period_seconds = 14.612F,
        .sway_spine_radians = 0.008F,
        .sway_chest_counter_radians = 0.006F,
        .sway_head_radians = 0.012F,
    };
    return true;
}

void eidolon_motion_destroy(EidolonMotionRig *rig) {
    SDL_free(rig->nodes);
    SDL_zero(*rig);
}

bool eidolon_motion_set_node(EidolonMotionRig *rig, size_t node_index, const char *name, int parent,
                             const float translation[3], const float rotation[4],
                             const float scale[3]) {
    if (node_index >= rig->node_count || parent < -1 ||
        (parent >= 0 && (size_t)parent >= rig->node_count)) {
        SDL_SetError("invalid motion rig node index");
        return false;
    }
    EidolonMotionNode *node = &rig->nodes[node_index];
    if (SDL_strlcpy(node->name, name != NULL ? name : "", sizeof(node->name)) >=
        sizeof(node->name)) {
        SDL_SetError("model node name is too long");
        return false;
    }
    node->parent = parent;
    SDL_memcpy(node->bind_translation, translation, sizeof(node->bind_translation));
    SDL_memcpy(node->bind_rotation, rotation, sizeof(node->bind_rotation));
    SDL_memcpy(node->bind_scale, scale, sizeof(node->bind_scale));
    SDL_memcpy(node->translation, translation, sizeof(node->translation));
    SDL_memcpy(node->rotation, rotation, sizeof(node->rotation));
    SDL_memcpy(node->scale, scale, sizeof(node->scale));
    return true;
}

bool eidolon_motion_finalize(EidolonMotionRig *rig) {
    rig->pelvis = find_node(rig, "Bip001 Pelvis");
    rig->spine = find_node(rig, "Bip001 Spine");
    rig->spine1 = find_node(rig, "Bip001 Spine1");
    rig->neck = find_node(rig, "Bip001 Neck");
    rig->head = find_node(rig, "Bip001 Head");
    rig->left_upper_arm = find_node(rig, "Bip001 L UpperArm");
    rig->left_forearm = find_node(rig, "Bip001 L Forearm");
    rig->right_upper_arm = find_node(rig, "Bip001 R UpperArm");
    rig->right_forearm = find_node(rig, "Bip001 R Forearm");
    if (rig->pelvis < 0 || rig->spine < 0 || rig->spine1 < 0 || rig->neck < 0 || rig->head < 0 ||
        rig->left_upper_arm < 0 || rig->left_forearm < 0 || rig->right_upper_arm < 0 ||
        rig->right_forearm < 0) {
        SDL_SetError("model is missing required humanoid motion nodes");
        return false;
    }
    SDL_ClearError();
    eidolon_motion_update_idle(rig, 0);
    return SDL_GetError()[0] == '\0';
}

void eidolon_motion_set_neutral_pose(EidolonMotionRig *rig, EidolonNeutralPose pose) {
    rig->neutral_pose = pose;
}

EidolonNeutralPose eidolon_motion_neutral_pose(const EidolonMotionRig *rig) {
    return rig->neutral_pose;
}

void eidolon_motion_set_idle_tuning(EidolonMotionRig *rig, EidolonIdleTuning tuning) {
    rig->idle_tuning = tuning;
}

EidolonIdleTuning eidolon_motion_idle_tuning(const EidolonMotionRig *rig) {
    return rig->idle_tuning;
}

void eidolon_motion_update_idle(EidolonMotionRig *rig, uint64_t now_ms) {
    for (size_t node_index = 0; node_index < rig->node_count; ++node_index) {
        EidolonMotionNode *node = &rig->nodes[node_index];
        SDL_memcpy(node->translation, node->bind_translation, sizeof(node->translation));
        SDL_memcpy(node->rotation, node->bind_rotation, sizeof(node->rotation));
        SDL_memcpy(node->scale, node->bind_scale, sizeof(node->scale));
        node->world_state = 0U;
    }

    const float time = (float)(now_ms % 600000U) * 0.001F;
    const float breath = sinf(time * (2.0F * SDL_PI_F / rig->idle_tuning.breath_period_seconds));
    const float sway = sinf(time * (2.0F * SDL_PI_F / rig->idle_tuning.sway_period_seconds) + 0.8F);

    /* Rio's bind is a symmetric A-pose. These are bind-relative anatomical adjustments. */
    apply_local_rotation(&rig->nodes[(size_t)rig->left_upper_arm], 0.0F, 1.0F, 0.0F,
                         rig->neutral_pose.shoulder_lower_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->right_upper_arm], 0.0F, 1.0F, 0.0F,
                         -rig->neutral_pose.shoulder_lower_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->left_forearm], 0.0F, 0.0F, 1.0F,
                         -rig->neutral_pose.elbow_bend_add_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->right_forearm], 0.0F, 0.0F, 1.0F,
                         -rig->neutral_pose.elbow_bend_add_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->spine], 0.0F, 0.0F, 1.0F,
                         sway * rig->idle_tuning.sway_spine_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->spine1], 0.0F, 0.0F, 1.0F,
                         breath * rig->idle_tuning.breath_chest_radians -
                             sway * rig->idle_tuning.sway_chest_counter_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->neck], 0.0F, 0.0F, 1.0F,
                         -breath * rig->idle_tuning.breath_neck_counter_radians);
    apply_local_rotation(&rig->nodes[(size_t)rig->head], 0.0F, 0.0F, 1.0F,
                         sway * rig->idle_tuning.sway_head_radians);

    (void)eidolon_motion_rebuild_world(rig);
}

bool eidolon_motion_rebuild_world(EidolonMotionRig *rig) {
    for (size_t node_index = 0; node_index < rig->node_count; ++node_index) {
        rig->nodes[node_index].world_state = 0U;
    }
    for (size_t node_index = 0; node_index < rig->node_count; ++node_index) {
        if (!resolve_world(rig, node_index)) {
            return false;
        }
    }
    return true;
}

const float *eidolon_motion_world(EidolonMotionRig *rig, size_t node_index) {
    if (node_index >= rig->node_count || !resolve_world(rig, node_index)) {
        return NULL;
    }
    return rig->nodes[node_index].world;
}
