#include "humanoid_pose.h"

#include <math.h>
#include <string.h>

#define HUMANOID_QUATERNION_MIN_NORM 0.000001F
#define HUMANOID_QUATERNION_UNIT_TOLERANCE 0.002F

typedef struct EidolonHumanoidRoleDefinition {
    const char *name;
    bool required;
    bool allowed_in_vrma;
} EidolonHumanoidRoleDefinition;

static const EidolonHumanoidRoleDefinition BONE_DEFINITIONS[EIDOLON_HUMANOID_ROLE_COUNT] = {
    [EIDOLON_HUMANOID_ROLE_HIPS] = {"hips", true, true},
    [EIDOLON_HUMANOID_ROLE_SPINE] = {"spine", true, true},
    [EIDOLON_HUMANOID_ROLE_CHEST] = {"chest", false, true},
    [EIDOLON_HUMANOID_ROLE_UPPER_CHEST] = {"upperChest", false, true},
    [EIDOLON_HUMANOID_ROLE_NECK] = {"neck", false, true},
    [EIDOLON_HUMANOID_ROLE_HEAD] = {"head", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_EYE] = {"leftEye", false, false},
    [EIDOLON_HUMANOID_ROLE_RIGHT_EYE] = {"rightEye", false, false},
    [EIDOLON_HUMANOID_ROLE_JAW] = {"jaw", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG] = {"leftUpperLeg", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG] = {"leftLowerLeg", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_FOOT] = {"leftFoot", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_TOES] = {"leftToes", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG] = {"rightUpperLeg", true, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG] = {"rightLowerLeg", true, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_FOOT] = {"rightFoot", true, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_TOES] = {"rightToes", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER] = {"leftShoulder", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM] = {"leftUpperArm", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM] = {"leftLowerArm", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_HAND] = {"leftHand", true, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER] = {"rightShoulder", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM] = {"rightUpperArm", true, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM] = {"rightLowerArm", true, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_HAND] = {"rightHand", true, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_THUMB_METACARPAL] = {"leftThumbMetacarpal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_THUMB_PROXIMAL] = {"leftThumbProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_THUMB_DISTAL] = {"leftThumbDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_INDEX_PROXIMAL] = {"leftIndexProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_INDEX_INTERMEDIATE] = {"leftIndexIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_INDEX_DISTAL] = {"leftIndexDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_PROXIMAL] = {"leftMiddleProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_INTERMEDIATE] = {"leftMiddleIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_DISTAL] = {"leftMiddleDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_RING_PROXIMAL] = {"leftRingProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_RING_INTERMEDIATE] = {"leftRingIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_RING_DISTAL] = {"leftRingDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_PROXIMAL] = {"leftLittleProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_INTERMEDIATE] = {"leftLittleIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_DISTAL] = {"leftLittleDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_METACARPAL] = {"rightThumbMetacarpal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_PROXIMAL] = {"rightThumbProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_DISTAL] = {"rightThumbDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_PROXIMAL] = {"rightIndexProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_INTERMEDIATE] = {"rightIndexIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_DISTAL] = {"rightIndexDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_PROXIMAL] = {"rightMiddleProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_INTERMEDIATE] = {"rightMiddleIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_DISTAL] = {"rightMiddleDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_RING_PROXIMAL] = {"rightRingProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_RING_INTERMEDIATE] = {"rightRingIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_RING_DISTAL] = {"rightRingDistal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_PROXIMAL] = {"rightLittleProximal", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_INTERMEDIATE] = {"rightLittleIntermediate", false, true},
    [EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_DISTAL] = {"rightLittleDistal", false, true},
};

_Static_assert(EIDOLON_HUMANOID_ROLE_COUNT <= 64, "humanoid pose mask requires at most 64 bones");

const char *eidolon_humanoid_role_name(EidolonHumanoidRole bone) {
    if (bone < EIDOLON_HUMANOID_ROLE_HIPS || bone >= EIDOLON_HUMANOID_ROLE_COUNT) {
        return "unknown";
    }
    return BONE_DEFINITIONS[(size_t)bone].name;
}

int eidolon_humanoid_role_from_name(const char *name, size_t length) {
    if (name == NULL) {
        return -1;
    }
    for (size_t index = 0U; index < EIDOLON_HUMANOID_ROLE_COUNT; ++index) {
        const char *candidate = BONE_DEFINITIONS[index].name;
        if (strlen(candidate) == length && memcmp(candidate, name, length) == 0) {
            return (int)index;
        }
    }
    return -1;
}

bool eidolon_humanoid_role_required(EidolonHumanoidRole bone) {
    return bone >= EIDOLON_HUMANOID_ROLE_HIPS && bone < EIDOLON_HUMANOID_ROLE_COUNT &&
           BONE_DEFINITIONS[(size_t)bone].required;
}

bool eidolon_humanoid_role_allowed_in_vrma(EidolonHumanoidRole bone) {
    return bone >= EIDOLON_HUMANOID_ROLE_HIPS && bone < EIDOLON_HUMANOID_ROLE_COUNT &&
           BONE_DEFINITIONS[(size_t)bone].allowed_in_vrma;
}

int eidolon_humanoid_role_parent(EidolonHumanoidRole role) {
    switch (role) {
    case EIDOLON_HUMANOID_ROLE_HIPS:
        return -1;
    case EIDOLON_HUMANOID_ROLE_SPINE:
        return EIDOLON_HUMANOID_ROLE_HIPS;
    case EIDOLON_HUMANOID_ROLE_CHEST:
        return EIDOLON_HUMANOID_ROLE_SPINE;
    case EIDOLON_HUMANOID_ROLE_UPPER_CHEST:
        return EIDOLON_HUMANOID_ROLE_CHEST;
    case EIDOLON_HUMANOID_ROLE_NECK:
        return EIDOLON_HUMANOID_ROLE_UPPER_CHEST;
    case EIDOLON_HUMANOID_ROLE_HEAD:
        return EIDOLON_HUMANOID_ROLE_NECK;
    case EIDOLON_HUMANOID_ROLE_LEFT_EYE:
    case EIDOLON_HUMANOID_ROLE_RIGHT_EYE:
    case EIDOLON_HUMANOID_ROLE_JAW:
        return EIDOLON_HUMANOID_ROLE_HEAD;
    case EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG:
    case EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG:
        return EIDOLON_HUMANOID_ROLE_HIPS;
    case EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG:
        return EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG;
    case EIDOLON_HUMANOID_ROLE_LEFT_FOOT:
        return EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG;
    case EIDOLON_HUMANOID_ROLE_LEFT_TOES:
        return EIDOLON_HUMANOID_ROLE_LEFT_FOOT;
    case EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG:
        return EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG;
    case EIDOLON_HUMANOID_ROLE_RIGHT_FOOT:
        return EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG;
    case EIDOLON_HUMANOID_ROLE_RIGHT_TOES:
        return EIDOLON_HUMANOID_ROLE_RIGHT_FOOT;
    case EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER:
    case EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER:
        return EIDOLON_HUMANOID_ROLE_UPPER_CHEST;
    case EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM:
        return EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER;
    case EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM:
        return EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM;
    case EIDOLON_HUMANOID_ROLE_LEFT_HAND:
        return EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM;
    case EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM:
        return EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER;
    case EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM:
        return EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM;
    case EIDOLON_HUMANOID_ROLE_RIGHT_HAND:
        return EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM;
    case EIDOLON_HUMANOID_ROLE_LEFT_THUMB_METACARPAL:
    case EIDOLON_HUMANOID_ROLE_LEFT_INDEX_PROXIMAL:
    case EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_PROXIMAL:
    case EIDOLON_HUMANOID_ROLE_LEFT_RING_PROXIMAL:
    case EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_PROXIMAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_HAND;
    case EIDOLON_HUMANOID_ROLE_LEFT_THUMB_PROXIMAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_THUMB_METACARPAL;
    case EIDOLON_HUMANOID_ROLE_LEFT_THUMB_DISTAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_THUMB_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_LEFT_INDEX_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_LEFT_INDEX_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_LEFT_INDEX_DISTAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_INDEX_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_DISTAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_LEFT_RING_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_LEFT_RING_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_LEFT_RING_DISTAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_RING_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_DISTAL:
        return EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_METACARPAL:
    case EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_PROXIMAL:
    case EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_PROXIMAL:
    case EIDOLON_HUMANOID_ROLE_RIGHT_RING_PROXIMAL:
    case EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_PROXIMAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_HAND;
    case EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_PROXIMAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_METACARPAL;
    case EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_DISTAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_DISTAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_DISTAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_RIGHT_RING_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_RIGHT_RING_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_RIGHT_RING_DISTAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_RING_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_INTERMEDIATE:
        return EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_PROXIMAL;
    case EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_DISTAL:
        return EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_INTERMEDIATE;
    case EIDOLON_HUMANOID_ROLE_COUNT:
        return -1;
    }
    return -1;
}

void eidolon_humanoid_pose_init(EidolonHumanoidPose *pose) {
    if (pose == NULL) {
        return;
    }
    memset(pose, 0, sizeof(*pose));
    pose->version = EIDOLON_HUMANOID_POSE_VERSION;
    for (size_t index = 0U; index < EIDOLON_HUMANOID_ROLE_COUNT; ++index) {
        pose->rotations[index][3] = 1.0F;
    }
}

void eidolon_humanoid_pose_layer_init(EidolonHumanoidPoseLayer *layer,
                                      const EidolonHumanoidPose *pose) {
    if (layer == NULL) {
        return;
    }
    memset(layer, 0, sizeof(*layer));
    layer->version = EIDOLON_HUMANOID_POSE_LAYER_VERSION;
    layer->pose = pose;
    layer->mode = EIDOLON_HUMANOID_POSE_LAYER_ABSOLUTE;
    layer->weight = 1.0F;
    layer->intensity = 1.0F;
    if (pose != NULL) {
        layer->rotation_mask = pose->rotation_mask;
        layer->owns_hips_translation = pose->has_hips_translation;
    }
}

static uint64_t valid_rotation_mask(void) {
    uint64_t mask = 0U;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        mask |= UINT64_C(1) << (uint32_t)role;
    }
    return mask;
}

static bool quaternion_valid(const float quaternion[4]) {
    float norm = 0.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        if (!isfinite(quaternion[component])) {
            return false;
        }
        norm += quaternion[component] * quaternion[component];
    }
    return norm > HUMANOID_QUATERNION_MIN_NORM &&
           fabsf(norm - 1.0F) <= HUMANOID_QUATERNION_UNIT_TOLERANCE;
}

bool eidolon_humanoid_pose_validate(const EidolonHumanoidPose *pose) {
    if (pose == NULL || pose->version != EIDOLON_HUMANOID_POSE_VERSION ||
        (pose->rotation_mask & ~valid_rotation_mask()) != 0U) {
        return false;
    }
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        const uint64_t role_bit = UINT64_C(1) << (uint32_t)role;
        if ((pose->rotation_mask & role_bit) != 0U && !quaternion_valid(pose->rotations[role])) {
            return false;
        }
    }
    for (size_t axis = 0U; pose->has_hips_translation && axis < 3U; ++axis) {
        if (!isfinite(pose->hips_translation[axis])) {
            return false;
        }
    }
    return true;
}

static bool layer_valid(const EidolonHumanoidPoseLayer *layer) {
    return layer != NULL && layer->version == EIDOLON_HUMANOID_POSE_LAYER_VERSION &&
           layer->pose != NULL && eidolon_humanoid_pose_validate(layer->pose) &&
           layer->mode >= EIDOLON_HUMANOID_POSE_LAYER_ABSOLUTE &&
           layer->mode < EIDOLON_HUMANOID_POSE_LAYER_MODE_COUNT &&
           (layer->rotation_mask & ~layer->pose->rotation_mask) == 0U &&
           (!layer->owns_hips_translation || layer->pose->has_hips_translation) &&
           isfinite(layer->weight) && layer->weight >= 0.0F && layer->weight <= 1.0F &&
           isfinite(layer->intensity) && layer->intensity >= 0.0F && layer->intensity <= 1.0F;
}

static void quaternion_mix(const float from[4], const float to[4], float weight, float result[4]) {
    float dot = 0.0F;
    float norm = 0.0F;
    float sign = 1.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        dot += from[component] * to[component];
    }
    if (dot < 0.0F) {
        sign = -1.0F;
    }
    for (size_t component = 0U; component < 4U; ++component) {
        result[component] = from[component] * (1.0F - weight) + to[component] * sign * weight;
        norm += result[component] * result[component];
    }
    norm = sqrtf(norm);
    for (size_t component = 0U; component < 4U; ++component) {
        result[component] /= norm;
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

static void compose_rotation(const float from[4], const float authored[4],
                             const EidolonHumanoidPoseLayer *layer, float result[4]) {
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    float target[4];
    quaternion_mix(identity, authored, layer->intensity, target);
    if (layer->mode == EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE) {
        quaternion_multiply(from, target, target);
    }
    quaternion_mix(from, target, layer->weight, result);
}

bool eidolon_humanoid_pose_compose(const EidolonHumanoidPose *base,
                                   const EidolonHumanoidPoseLayer *layers, size_t layer_count,
                                   EidolonHumanoidPose *result) {
    EidolonHumanoidPose candidate;
    static const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    if (!eidolon_humanoid_pose_validate(base) || result == NULL ||
        (layers == NULL && layer_count > 0U) ||
        layer_count > EIDOLON_HUMANOID_POSE_LAYER_CAPACITY) {
        return false;
    }
    candidate = *base;
    for (size_t layer_index = 0U; layer_index < layer_count; ++layer_index) {
        const EidolonHumanoidPoseLayer *layer = &layers[layer_index];
        if (!layer_valid(layer)) {
            return false;
        }
        if (layer->weight <= 0.0F) {
            continue;
        }
        for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
            const uint64_t role_bit = UINT64_C(1) << (uint32_t)role;
            const float *from;
            if ((layer->rotation_mask & role_bit) == 0U) {
                continue;
            }
            from =
                (candidate.rotation_mask & role_bit) != 0U ? candidate.rotations[role] : identity;
            compose_rotation(from, layer->pose->rotations[role], layer, candidate.rotations[role]);
            candidate.rotation_mask |= role_bit;
        }
        if (layer->owns_hips_translation) {
            for (size_t axis = 0U; axis < 3U; ++axis) {
                const float from =
                    candidate.has_hips_translation ? candidate.hips_translation[axis] : 0.0F;
                const float contribution = layer->pose->hips_translation[axis] * layer->intensity;
                if (layer->mode == EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE) {
                    candidate.hips_translation[axis] = from + contribution * layer->weight;
                } else {
                    candidate.hips_translation[axis] =
                        from * (1.0F - layer->weight) + contribution * layer->weight;
                }
            }
            candidate.has_hips_translation = true;
        }
    }
    *result = candidate;
    return true;
}
