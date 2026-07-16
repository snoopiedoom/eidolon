#include "humanoid.h"

#include <SDL3/SDL.h>

#define HUMANOID_EPSILON 0.00001F

typedef struct BoneAliases {
    const char *names[5];
} BoneAliases;

static const char *BONE_NAMES[EIDOLON_HUMANOID_BONE_COUNT] = {
    "hips",          "spine",         "chest",         "neck",          "head",
    "leftEye",       "rightEye",      "leftShoulder",  "leftUpperArm",  "leftLowerArm",
    "leftHand",      "rightShoulder", "rightUpperArm", "rightLowerArm", "rightHand",
    "leftUpperLeg",  "leftLowerLeg",  "leftFoot",      "leftToes",      "rightUpperLeg",
    "rightLowerLeg", "rightFoot",     "rightToes",
};

static const BoneAliases BONE_ALIASES[EIDOLON_HUMANOID_BONE_COUNT] = {
    {{"Bip001 Pelvis", "hips", "Hips", NULL}},
    {{"Bip001 Spine", "spine", "Spine", NULL}},
    {{"Bip001 Spine1", "chest", "Chest", "upperChest", NULL}},
    {{"Bip001 Neck", "neck", "Neck", NULL}},
    {{"Bip001 Head", "head", "Head", NULL}},
    {{"Bip001 Xtra_eyeL", "leftEye", "LeftEye", NULL}},
    {{"Bip001 Xtra_eyeR", "rightEye", "RightEye", NULL}},
    {{"Bip001 L Clavicle", "leftShoulder", "LeftShoulder", NULL}},
    {{"Bip001 L UpperArm", "leftUpperArm", "LeftUpperArm", NULL}},
    {{"Bip001 L Forearm", "leftLowerArm", "LeftLowerArm", NULL}},
    {{"Bip001 L Hand", "leftHand", "LeftHand", NULL}},
    {{"Bip001 R Clavicle", "rightShoulder", "RightShoulder", NULL}},
    {{"Bip001 R UpperArm", "rightUpperArm", "RightUpperArm", NULL}},
    {{"Bip001 R Forearm", "rightLowerArm", "RightLowerArm", NULL}},
    {{"Bip001 R Hand", "rightHand", "RightHand", NULL}},
    {{"Bip001 L Thigh", "leftUpperLeg", "LeftUpperLeg", NULL}},
    {{"Bip001 L Calf", "leftLowerLeg", "LeftLowerLeg", NULL}},
    {{"Bip001 L Foot", "leftFoot", "LeftFoot", NULL}},
    {{"Bip001 L Toe0", "leftToes", "LeftToes", NULL}},
    {{"Bip001 R Thigh", "rightUpperLeg", "RightUpperLeg", NULL}},
    {{"Bip001 R Calf", "rightLowerLeg", "RightLowerLeg", NULL}},
    {{"Bip001 R Foot", "rightFoot", "RightFoot", NULL}},
    {{"Bip001 R Toe0", "rightToes", "RightToes", NULL}},
};

static const EidolonHumanoidBone REQUIRED_BONES[] = {
    EIDOLON_HUMANOID_HIPS,
    EIDOLON_HUMANOID_SPINE,
    EIDOLON_HUMANOID_CHEST,
    EIDOLON_HUMANOID_NECK,
    EIDOLON_HUMANOID_HEAD,
    EIDOLON_HUMANOID_LEFT_UPPER_ARM,
    EIDOLON_HUMANOID_LEFT_LOWER_ARM,
    EIDOLON_HUMANOID_LEFT_HAND,
    EIDOLON_HUMANOID_RIGHT_UPPER_ARM,
    EIDOLON_HUMANOID_RIGHT_LOWER_ARM,
    EIDOLON_HUMANOID_RIGHT_HAND,
    EIDOLON_HUMANOID_LEFT_UPPER_LEG,
    EIDOLON_HUMANOID_LEFT_LOWER_LEG,
    EIDOLON_HUMANOID_LEFT_FOOT,
    EIDOLON_HUMANOID_RIGHT_UPPER_LEG,
    EIDOLON_HUMANOID_RIGHT_LOWER_LEG,
    EIDOLON_HUMANOID_RIGHT_FOOT,
};

static float vector_length(const float vector[3]) {
    return SDL_sqrtf(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

static float vector_dot(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static bool vector_direction(const float from[3], const float to[3], float direction[3]) {
    direction[0] = to[0] - from[0];
    direction[1] = to[1] - from[1];
    direction[2] = to[2] - from[2];
    const float length = vector_length(direction);
    if (length <= HUMANOID_EPSILON) {
        return false;
    }
    direction[0] /= length;
    direction[1] /= length;
    direction[2] /= length;
    return true;
}

static float point_distance(const float first[3], const float second[3]) {
    float difference[3];
    difference[0] = first[0] - second[0];
    difference[1] = first[1] - second[1];
    difference[2] = first[2] - second[2];
    return vector_length(difference);
}

static void vector_cross(const float left[3], const float right[3], float result[3]) {
    result[0] = left[1] * right[2] - left[2] * right[1];
    result[1] = left[2] * right[0] - left[0] * right[2];
    result[2] = left[0] * right[1] - left[1] * right[0];
    const float length = vector_length(result);
    if (length > HUMANOID_EPSILON) {
        result[0] /= length;
        result[1] /= length;
        result[2] /= length;
    }
}

static void orient_forward_from_feet(EidolonHumanoidProfile *profile) {
    float toe_direction[3] = {0.0F, 0.0F, 0.0F};
    size_t foot_count = 0U;
    const EidolonHumanoidBone feet[2] = {EIDOLON_HUMANOID_LEFT_FOOT, EIDOLON_HUMANOID_RIGHT_FOOT};
    const EidolonHumanoidBone toes[2] = {EIDOLON_HUMANOID_LEFT_TOES, EIDOLON_HUMANOID_RIGHT_TOES};
    for (size_t side = 0; side < SDL_arraysize(feet); ++side) {
        if (profile->nodes[feet[side]] < 0 || profile->nodes[toes[side]] < 0) {
            continue;
        }
        for (size_t axis = 0; axis < 3; ++axis) {
            toe_direction[axis] += profile->bind_positions[toes[side]][axis] -
                                   profile->bind_positions[feet[side]][axis];
        }
        foot_count += 1U;
    }
    if (foot_count == 0U) {
        return;
    }
    const float vertical = vector_dot(toe_direction, profile->up);
    for (size_t axis = 0; axis < 3; ++axis) {
        toe_direction[axis] -= profile->up[axis] * vertical;
    }
    const float length = vector_length(toe_direction);
    if (length <= HUMANOID_EPSILON) {
        return;
    }
    for (size_t axis = 0; axis < 3; ++axis) {
        toe_direction[axis] /= length;
    }
    if (vector_dot(profile->forward, toe_direction) < 0.0F) {
        for (size_t axis = 0; axis < 3; ++axis) {
            profile->forward[axis] = -profile->forward[axis];
        }
    }
}

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

static void bind_local_matrix(const EidolonMotionNode *node, float matrix[16]) {
    const float x = node->bind_rotation[0];
    const float y = node->bind_rotation[1];
    const float z = node->bind_rotation[2];
    const float w = node->bind_rotation[3];
    const float sx = node->bind_scale[0];
    const float sy = node->bind_scale[1];
    const float sz = node->bind_scale[2];
    matrix[0] = (1.0F - 2.0F * y * y - 2.0F * z * z) * sx;
    matrix[1] = (2.0F * x * y + 2.0F * z * w) * sx;
    matrix[2] = (2.0F * x * z - 2.0F * y * w) * sx;
    matrix[3] = 0.0F;
    matrix[4] = (2.0F * x * y - 2.0F * z * w) * sy;
    matrix[5] = (1.0F - 2.0F * x * x - 2.0F * z * z) * sy;
    matrix[6] = (2.0F * y * z + 2.0F * x * w) * sy;
    matrix[7] = 0.0F;
    matrix[8] = (2.0F * x * z + 2.0F * y * w) * sz;
    matrix[9] = (2.0F * y * z - 2.0F * x * w) * sz;
    matrix[10] = (1.0F - 2.0F * x * x - 2.0F * y * y) * sz;
    matrix[11] = 0.0F;
    matrix[12] = node->bind_translation[0];
    matrix[13] = node->bind_translation[1];
    matrix[14] = node->bind_translation[2];
    matrix[15] = 1.0F;
}

static bool bind_world_matrix_depth(const EidolonMotionRig *rig, size_t node_index,
                                    size_t remaining_depth, float matrix[16]) {
    if (node_index >= rig->node_count || remaining_depth == 0U) {
        return false;
    }
    float local[16];
    bind_local_matrix(&rig->nodes[node_index], local);
    const int parent = rig->nodes[node_index].parent;
    if (parent < 0) {
        SDL_memcpy(matrix, local, sizeof(local));
        return true;
    }
    if ((size_t)parent >= rig->node_count) {
        return false;
    }
    float parent_world[16];
    if (!bind_world_matrix_depth(rig, (size_t)parent, remaining_depth - 1U, parent_world)) {
        return false;
    }
    matrix_multiply(parent_world, local, matrix);
    return true;
}

static bool bind_world_matrix(const EidolonMotionRig *rig, size_t node_index, float matrix[16]) {
    return bind_world_matrix_depth(rig, node_index, rig->node_count + 1U, matrix);
}

static int find_alias(const EidolonMotionRig *rig, const BoneAliases *aliases) {
    for (size_t alias = 0; alias < SDL_arraysize(aliases->names) && aliases->names[alias] != NULL;
         ++alias) {
        for (size_t node = 0; node < rig->node_count; ++node) {
            if (SDL_strcasecmp(rig->nodes[node].name, aliases->names[alias]) == 0) {
                return (int)node;
            }
        }
    }
    return -1;
}

static bool is_descendant(const EidolonMotionRig *rig, int child, int ancestor) {
    size_t remaining = rig->node_count + 1U;
    for (int node = child; node >= 0 && remaining > 0U; --remaining) {
        if (node == ancestor) {
            return true;
        }
        if ((size_t)node >= rig->node_count) {
            return false;
        }
        node = rig->nodes[(size_t)node].parent;
    }
    return false;
}

static bool validate_chain(const EidolonMotionRig *rig, const EidolonHumanoidProfile *profile,
                           EidolonHumanoidBone ancestor, EidolonHumanoidBone descendant) {
    return is_descendant(rig, profile->nodes[descendant], profile->nodes[ancestor]);
}

bool eidolon_humanoid_profile_init(EidolonHumanoidProfile *profile, const EidolonMotionRig *rig) {
    SDL_memset(profile, 0, sizeof(*profile));
    for (size_t bone = 0; bone < EIDOLON_HUMANOID_BONE_COUNT; ++bone) {
        profile->nodes[bone] = find_alias(rig, &BONE_ALIASES[bone]);
        if (profile->nodes[bone] >= 0) {
            float world[16];
            if (!bind_world_matrix(rig, (size_t)profile->nodes[bone], world)) {
                SDL_SetError("could not resolve bind transform for humanoid bone '%s'",
                             BONE_NAMES[bone]);
                return false;
            }
            profile->bind_positions[bone][0] = world[12];
            profile->bind_positions[bone][1] = world[13];
            profile->bind_positions[bone][2] = world[14];
        }
    }
    for (size_t required = 0; required < SDL_arraysize(REQUIRED_BONES); ++required) {
        const EidolonHumanoidBone bone = REQUIRED_BONES[required];
        if (profile->nodes[bone] < 0) {
            SDL_SetError("model is missing required humanoid bone '%s'", BONE_NAMES[bone]);
            return false;
        }
    }

    const bool hierarchy_valid =
        validate_chain(rig, profile, EIDOLON_HUMANOID_HIPS, EIDOLON_HUMANOID_SPINE) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_SPINE, EIDOLON_HUMANOID_CHEST) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_CHEST, EIDOLON_HUMANOID_NECK) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_NECK, EIDOLON_HUMANOID_HEAD) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_LEFT_UPPER_ARM,
                       EIDOLON_HUMANOID_LEFT_LOWER_ARM) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_LEFT_LOWER_ARM, EIDOLON_HUMANOID_LEFT_HAND) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_RIGHT_UPPER_ARM,
                       EIDOLON_HUMANOID_RIGHT_LOWER_ARM) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_RIGHT_LOWER_ARM,
                       EIDOLON_HUMANOID_RIGHT_HAND) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_LEFT_UPPER_LEG,
                       EIDOLON_HUMANOID_LEFT_LOWER_LEG) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_LEFT_LOWER_LEG, EIDOLON_HUMANOID_LEFT_FOOT) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_RIGHT_UPPER_LEG,
                       EIDOLON_HUMANOID_RIGHT_LOWER_LEG) &&
        validate_chain(rig, profile, EIDOLON_HUMANOID_RIGHT_LOWER_LEG, EIDOLON_HUMANOID_RIGHT_FOOT);
    if (!hierarchy_valid) {
        SDL_SetError("humanoid bone hierarchy is invalid");
        return false;
    }

    if (!vector_direction(profile->bind_positions[EIDOLON_HUMANOID_HIPS],
                          profile->bind_positions[EIDOLON_HUMANOID_HEAD], profile->up) ||
        !vector_direction(profile->bind_positions[EIDOLON_HUMANOID_LEFT_UPPER_ARM],
                          profile->bind_positions[EIDOLON_HUMANOID_RIGHT_UPPER_ARM],
                          profile->right)) {
        SDL_SetError("humanoid bind pose has degenerate body axes");
        return false;
    }
    const float vertical_component = vector_dot(profile->right, profile->up);
    for (size_t axis = 0; axis < 3; ++axis) {
        profile->right[axis] -= profile->up[axis] * vertical_component;
    }
    const float right_length = vector_length(profile->right);
    if (right_length <= HUMANOID_EPSILON) {
        SDL_SetError("humanoid bind pose has degenerate shoulder axis");
        return false;
    }
    for (size_t axis = 0; axis < 3; ++axis) {
        profile->right[axis] /= right_length;
    }
    vector_cross(profile->right, profile->up, profile->forward);
    if (vector_length(profile->forward) <= HUMANOID_EPSILON) {
        SDL_SetError("humanoid bind pose has collinear body axes");
        return false;
    }
    orient_forward_from_feet(profile);

    profile->shoulder_width =
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_LEFT_UPPER_ARM],
                       profile->bind_positions[EIDOLON_HUMANOID_RIGHT_UPPER_ARM]);
    profile->torso_length = point_distance(profile->bind_positions[EIDOLON_HUMANOID_HIPS],
                                           profile->bind_positions[EIDOLON_HUMANOID_NECK]);
    profile->arm_length[EIDOLON_HUMANOID_LEFT] =
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_LEFT_UPPER_ARM],
                       profile->bind_positions[EIDOLON_HUMANOID_LEFT_LOWER_ARM]) +
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_LEFT_LOWER_ARM],
                       profile->bind_positions[EIDOLON_HUMANOID_LEFT_HAND]);
    profile->arm_length[EIDOLON_HUMANOID_RIGHT] =
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_RIGHT_UPPER_ARM],
                       profile->bind_positions[EIDOLON_HUMANOID_RIGHT_LOWER_ARM]) +
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_RIGHT_LOWER_ARM],
                       profile->bind_positions[EIDOLON_HUMANOID_RIGHT_HAND]);
    profile->leg_length[EIDOLON_HUMANOID_LEFT] =
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_LEFT_UPPER_LEG],
                       profile->bind_positions[EIDOLON_HUMANOID_LEFT_LOWER_LEG]) +
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_LEFT_LOWER_LEG],
                       profile->bind_positions[EIDOLON_HUMANOID_LEFT_FOOT]);
    profile->leg_length[EIDOLON_HUMANOID_RIGHT] =
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_RIGHT_UPPER_LEG],
                       profile->bind_positions[EIDOLON_HUMANOID_RIGHT_LOWER_LEG]) +
        point_distance(profile->bind_positions[EIDOLON_HUMANOID_RIGHT_LOWER_LEG],
                       profile->bind_positions[EIDOLON_HUMANOID_RIGHT_FOOT]);
    return true;
}

int eidolon_humanoid_node(const EidolonHumanoidProfile *profile, EidolonHumanoidBone bone) {
    return bone >= 0 && bone < EIDOLON_HUMANOID_BONE_COUNT ? profile->nodes[bone] : -1;
}

const char *eidolon_humanoid_bone_name(EidolonHumanoidBone bone) {
    return bone >= 0 && bone < EIDOLON_HUMANOID_BONE_COUNT ? BONE_NAMES[bone] : "unknown";
}

void eidolon_humanoid_arm_nodes(const EidolonHumanoidProfile *profile, EidolonHumanoidSide side,
                                int *upper_arm, int *lower_arm, int *hand) {
    const bool left = side == EIDOLON_HUMANOID_LEFT;
    *upper_arm =
        profile->nodes[left ? EIDOLON_HUMANOID_LEFT_UPPER_ARM : EIDOLON_HUMANOID_RIGHT_UPPER_ARM];
    *lower_arm =
        profile->nodes[left ? EIDOLON_HUMANOID_LEFT_LOWER_ARM : EIDOLON_HUMANOID_RIGHT_LOWER_ARM];
    *hand = profile->nodes[left ? EIDOLON_HUMANOID_LEFT_HAND : EIDOLON_HUMANOID_RIGHT_HAND];
}
