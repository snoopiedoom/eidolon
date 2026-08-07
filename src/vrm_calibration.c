#include "vrm_calibration.h"

#include <SDL3/SDL.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define CALIBRATION_EPSILON 0.0001F
#define CALIBRATION_TARGET_LIMIT 2.0F
#define CALIBRATION_ANGLE_LIMIT 3.141593F

static const char *ANCHOR_NAMES[EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT] = {
    "neutral",
    "attentive",
    "thinking",
    "responding",
    "contrast_preparation",
    "contrast_peak",
    "contrast_recovery",
    "interrupted_guarded",
};

static const char *BONE_NAMES[EIDOLON_VRM_BONE_COUNT] = {
    "hips",          "spine",         "chest",         "upperChest",    "neck",
    "head",          "leftEye",       "rightEye",      "leftUpperLeg",  "leftLowerLeg",
    "leftFoot",      "rightUpperLeg", "rightLowerLeg", "rightFoot",     "leftShoulder",
    "leftUpperArm",  "leftLowerArm",  "leftHand",      "rightShoulder", "rightUpperArm",
    "rightLowerArm", "rightHand",
};

static void set_error(char *error, size_t capacity, const char *format, ...) {
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    SDL_vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static bool finite3(const float value[3]) {
    return isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]);
}

static float dot3(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static float distance3(const float left[3], const float right[3]) {
    const float difference[3] = {
        left[0] - right[0],
        left[1] - right[1],
        left[2] - right[2],
    };
    return sqrtf(dot3(difference, difference));
}

static bool normalize3(float value[3]) {
    const float length = sqrtf(dot3(value, value));
    if (!isfinite(length) || length <= CALIBRATION_EPSILON) {
        return false;
    }
    for (size_t axis = 0U; axis < 3U; ++axis) {
        value[axis] /= length;
    }
    return true;
}

static bool direction3(const float from[3], const float to[3], float result[3]) {
    for (size_t axis = 0U; axis < 3U; ++axis) {
        result[axis] = to[axis] - from[axis];
    }
    return normalize3(result);
}

static bool cross3(const float left[3], const float right[3], float result[3]) {
    const float value[3] = {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
    SDL_memcpy(result, value, sizeof(value));
    return normalize3(result);
}

static bool node_position(const cgltf_data *data, int node_index, float position[3]) {
    cgltf_float world[16];
    if (data == NULL || node_index < 0 || (size_t)node_index >= (size_t)data->nodes_count) {
        return false;
    }
    cgltf_node_transform_world(&data->nodes[(size_t)node_index], world);
    position[0] = world[12];
    position[1] = world[13];
    position[2] = world[14];
    return finite3(position);
}

static int first_present(const EidolonVrmBody *body, const EidolonVrmHumanBone *candidates,
                         size_t candidate_count) {
    for (size_t index = 0U; index < candidate_count; ++index) {
        const int node = body->node_by_bone[candidates[index]];
        if (node >= 0) {
            return (int)candidates[index];
        }
    }
    return -1;
}

static int semantic_parent(const EidolonVrmBody *body, EidolonVrmHumanBone bone) {
    static const EidolonVrmHumanBone torso[] = {
        EIDOLON_VRM_BONE_UPPER_CHEST,
        EIDOLON_VRM_BONE_CHEST,
        EIDOLON_VRM_BONE_SPINE,
        EIDOLON_VRM_BONE_HIPS,
    };
    switch (bone) {
    case EIDOLON_VRM_BONE_HIPS:
        return -1;
    case EIDOLON_VRM_BONE_SPINE:
        return EIDOLON_VRM_BONE_HIPS;
    case EIDOLON_VRM_BONE_CHEST:
        return EIDOLON_VRM_BONE_SPINE;
    case EIDOLON_VRM_BONE_UPPER_CHEST: {
        const EidolonVrmHumanBone candidates[] = {EIDOLON_VRM_BONE_CHEST, EIDOLON_VRM_BONE_SPINE};
        return first_present(body, candidates, SDL_arraysize(candidates));
    }
    case EIDOLON_VRM_BONE_NECK:
        return first_present(body, torso, SDL_arraysize(torso));
    case EIDOLON_VRM_BONE_HEAD: {
        const EidolonVrmHumanBone candidates[] = {
            EIDOLON_VRM_BONE_NECK,
            EIDOLON_VRM_BONE_UPPER_CHEST,
            EIDOLON_VRM_BONE_CHEST,
            EIDOLON_VRM_BONE_SPINE,
        };
        return first_present(body, candidates, SDL_arraysize(candidates));
    }
    case EIDOLON_VRM_BONE_LEFT_EYE:
    case EIDOLON_VRM_BONE_RIGHT_EYE:
        return EIDOLON_VRM_BONE_HEAD;
    case EIDOLON_VRM_BONE_LEFT_UPPER_LEG:
    case EIDOLON_VRM_BONE_RIGHT_UPPER_LEG:
        return EIDOLON_VRM_BONE_HIPS;
    case EIDOLON_VRM_BONE_LEFT_LOWER_LEG:
        return EIDOLON_VRM_BONE_LEFT_UPPER_LEG;
    case EIDOLON_VRM_BONE_LEFT_FOOT:
        return EIDOLON_VRM_BONE_LEFT_LOWER_LEG;
    case EIDOLON_VRM_BONE_RIGHT_LOWER_LEG:
        return EIDOLON_VRM_BONE_RIGHT_UPPER_LEG;
    case EIDOLON_VRM_BONE_RIGHT_FOOT:
        return EIDOLON_VRM_BONE_RIGHT_LOWER_LEG;
    case EIDOLON_VRM_BONE_LEFT_SHOULDER:
    case EIDOLON_VRM_BONE_RIGHT_SHOULDER:
        return first_present(body, torso, SDL_arraysize(torso));
    case EIDOLON_VRM_BONE_LEFT_UPPER_ARM: {
        const EidolonVrmHumanBone candidates[] = {
            EIDOLON_VRM_BONE_LEFT_SHOULDER,
            EIDOLON_VRM_BONE_UPPER_CHEST,
            EIDOLON_VRM_BONE_CHEST,
            EIDOLON_VRM_BONE_SPINE,
        };
        return first_present(body, candidates, SDL_arraysize(candidates));
    }
    case EIDOLON_VRM_BONE_LEFT_LOWER_ARM:
        return EIDOLON_VRM_BONE_LEFT_UPPER_ARM;
    case EIDOLON_VRM_BONE_LEFT_HAND:
        return EIDOLON_VRM_BONE_LEFT_LOWER_ARM;
    case EIDOLON_VRM_BONE_RIGHT_UPPER_ARM: {
        const EidolonVrmHumanBone candidates[] = {
            EIDOLON_VRM_BONE_RIGHT_SHOULDER,
            EIDOLON_VRM_BONE_UPPER_CHEST,
            EIDOLON_VRM_BONE_CHEST,
            EIDOLON_VRM_BONE_SPINE,
        };
        return first_present(body, candidates, SDL_arraysize(candidates));
    }
    case EIDOLON_VRM_BONE_RIGHT_LOWER_ARM:
        return EIDOLON_VRM_BONE_RIGHT_UPPER_ARM;
    case EIDOLON_VRM_BONE_RIGHT_HAND:
        return EIDOLON_VRM_BONE_RIGHT_LOWER_ARM;
    case EIDOLON_VRM_BONE_COUNT:
        break;
    }
    return -1;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    for (size_t byte = 0U; byte < sizeof(value); ++byte) {
        hash ^= (unsigned char)(value & UINT64_C(0xff));
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static uint64_t anatomy_fingerprint(const EidolonVrmMeasurements *measurements) {
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = hash_u64(hash, measurements->version);
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        const EidolonVrmBoneMeasurement *value = &measurements->bones[bone];
        hash = hash_u64(hash, value->present ? 1U : 0U);
        if (!value->present) {
            continue;
        }
        for (size_t axis = 0U; axis < 3U; ++axis) {
            const int64_t quantized = (int64_t)llroundf(value->bind_position[axis] * 1000000.0F);
            hash = hash_u64(hash, (uint64_t)quantized);
        }
        hash = hash_u64(hash, (uint64_t)llroundf(value->segment_length * 1000000.0F));
    }
    return hash;
}

static bool required_length(float value) { return isfinite(value) && value > CALIBRATION_EPSILON; }

bool eidolon_vrm_measure(const cgltf_data *data, const EidolonVrmBody *body,
                         EidolonVrmMeasurements *measurements, char *error, size_t error_capacity) {
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    if (data == NULL || body == NULL || measurements == NULL) {
        set_error(error, error_capacity, "invalid VRM measurement input");
        return false;
    }
    EidolonVrmMeasurements candidate;
    SDL_zero(candidate);
    candidate.version = EIDOLON_VRM_MEASUREMENTS_VERSION;
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        const int node = body->node_by_bone[bone];
        EidolonVrmBoneMeasurement *measurement = &candidate.bones[bone];
        if (node < 0) {
            continue;
        }
        if (!node_position(data, node, measurement->bind_position)) {
            set_error(error, error_capacity, "could not measure VRM bone '%s'", BONE_NAMES[bone]);
            return false;
        }
        measurement->present = true;
        const int parent = semantic_parent(body, (EidolonVrmHumanBone)bone);
        if (parent >= 0) {
            const EidolonVrmBoneMeasurement *parent_measurement = &candidate.bones[(size_t)parent];
            if (!parent_measurement->present) {
                set_error(error, error_capacity, "VRM bone '%s' has no measurable semantic parent",
                          BONE_NAMES[bone]);
                return false;
            }
            measurement->segment_length =
                distance3(parent_measurement->bind_position, measurement->bind_position);
        }
    }

    const float *hips = candidate.bones[EIDOLON_VRM_BONE_HIPS].bind_position;
    const float *head = candidate.bones[EIDOLON_VRM_BONE_HEAD].bind_position;
    const float *left_upper = candidate.bones[EIDOLON_VRM_BONE_LEFT_UPPER_ARM].bind_position;
    const float *right_upper = candidate.bones[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM].bind_position;
    if (!direction3(left_upper, right_upper, candidate.right) ||
        !direction3(hips, head, candidate.up)) {
        set_error(error, error_capacity, "VRM calibration axes are degenerate");
        return false;
    }
    const float vertical = dot3(candidate.right, candidate.up);
    for (size_t axis = 0U; axis < 3U; ++axis) {
        candidate.right[axis] -= candidate.up[axis] * vertical;
    }
    if (!normalize3(candidate.right) || !cross3(candidate.right, candidate.up, candidate.forward)) {
        set_error(error, error_capacity, "VRM calibration frame is degenerate");
        return false;
    }

    candidate.shoulder_width = distance3(left_upper, right_upper);
    candidate.torso_length = distance3(hips, head);
    const float foot_midpoint[3] = {
        (candidate.bones[EIDOLON_VRM_BONE_LEFT_FOOT].bind_position[0] +
         candidate.bones[EIDOLON_VRM_BONE_RIGHT_FOOT].bind_position[0]) *
            0.5F,
        (candidate.bones[EIDOLON_VRM_BONE_LEFT_FOOT].bind_position[1] +
         candidate.bones[EIDOLON_VRM_BONE_RIGHT_FOOT].bind_position[1]) *
            0.5F,
        (candidate.bones[EIDOLON_VRM_BONE_LEFT_FOOT].bind_position[2] +
         candidate.bones[EIDOLON_VRM_BONE_RIGHT_FOOT].bind_position[2]) *
            0.5F,
    };
    const float foot_to_head[3] = {
        head[0] - foot_midpoint[0],
        head[1] - foot_midpoint[1],
        head[2] - foot_midpoint[2],
    };
    candidate.skeleton_height = dot3(foot_to_head, candidate.up);
    candidate.upper_arm_length[EIDOLON_VRM_CALIBRATION_LEFT] =
        candidate.bones[EIDOLON_VRM_BONE_LEFT_LOWER_ARM].segment_length;
    candidate.lower_arm_length[EIDOLON_VRM_CALIBRATION_LEFT] =
        candidate.bones[EIDOLON_VRM_BONE_LEFT_HAND].segment_length;
    candidate.upper_arm_length[EIDOLON_VRM_CALIBRATION_RIGHT] =
        candidate.bones[EIDOLON_VRM_BONE_RIGHT_LOWER_ARM].segment_length;
    candidate.lower_arm_length[EIDOLON_VRM_CALIBRATION_RIGHT] =
        candidate.bones[EIDOLON_VRM_BONE_RIGHT_HAND].segment_length;
    candidate.upper_leg_length[EIDOLON_VRM_CALIBRATION_LEFT] =
        candidate.bones[EIDOLON_VRM_BONE_LEFT_LOWER_LEG].segment_length;
    candidate.lower_leg_length[EIDOLON_VRM_CALIBRATION_LEFT] =
        candidate.bones[EIDOLON_VRM_BONE_LEFT_FOOT].segment_length;
    candidate.upper_leg_length[EIDOLON_VRM_CALIBRATION_RIGHT] =
        candidate.bones[EIDOLON_VRM_BONE_RIGHT_LOWER_LEG].segment_length;
    candidate.lower_leg_length[EIDOLON_VRM_CALIBRATION_RIGHT] =
        candidate.bones[EIDOLON_VRM_BONE_RIGHT_FOOT].segment_length;

    if (!required_length(candidate.shoulder_width) || !required_length(candidate.torso_length) ||
        !required_length(candidate.skeleton_height)) {
        set_error(error, error_capacity, "VRM calibration has degenerate body measurements");
        return false;
    }
    for (size_t side = 0U; side < EIDOLON_VRM_CALIBRATION_SIDE_COUNT; ++side) {
        if (!required_length(candidate.upper_arm_length[side]) ||
            !required_length(candidate.lower_arm_length[side]) ||
            !required_length(candidate.upper_leg_length[side]) ||
            !required_length(candidate.lower_leg_length[side])) {
            set_error(error, error_capacity, "VRM calibration has a degenerate limb segment");
            return false;
        }
    }
    candidate.anatomy_fingerprint = anatomy_fingerprint(&candidate);
    *measurements = candidate;
    return true;
}

void eidolon_vrm_calibration_init(EidolonVrmCalibration *calibration,
                                  uint64_t anatomy_fingerprint_value) {
    if (calibration == NULL) {
        return;
    }
    SDL_zero(*calibration);
    calibration->version = EIDOLON_VRM_CALIBRATION_VERSION;
    calibration->anatomy_fingerprint = anatomy_fingerprint_value;
    for (size_t anchor = 0U; anchor < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++anchor) {
        for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
            calibration->anchors[anchor].residual_rotation[bone][3] = 1.0F;
        }
    }
}

static bool bounded3(const float value[3], float limit) {
    return finite3(value) && fabsf(value[0]) <= limit && fabsf(value[1]) <= limit &&
           fabsf(value[2]) <= limit;
}

static bool anchor_valid(const EidolonVrmCalibrationAnchor *anchor, char *error,
                         size_t error_capacity) {
    const uint32_t valid_resources = (UINT32_C(1) << EIDOLON_EPR_RESOURCE_COUNT) - 1U;
    if (anchor == NULL || anchor->resource_mask == 0U ||
        (anchor->resource_mask & ~valid_resources) != 0U ||
        !bounded3(anchor->torso_euler, CALIBRATION_ANGLE_LIMIT) ||
        !bounded3(anchor->head_euler, CALIBRATION_ANGLE_LIMIT)) {
        set_error(error, error_capacity, "calibration anchor has invalid resources or body angles");
        return false;
    }
    for (size_t side = 0U; side < EIDOLON_VRM_CALIBRATION_SIDE_COUNT; ++side) {
        const EidolonVrmCalibrationArm *arm = &anchor->arms[side];
        if (!bounded3(arm->hand_target, CALIBRATION_TARGET_LIMIT) ||
            !bounded3(arm->elbow_pole, CALIBRATION_TARGET_LIMIT) ||
            !bounded3(arm->wrist_euler, CALIBRATION_ANGLE_LIMIT) || !isfinite(arm->weight) ||
            arm->weight < 0.0F || arm->weight > 1.0F) {
            set_error(error, error_capacity, "calibration anchor has an invalid arm target");
            return false;
        }
    }
    const uint32_t valid_bones = (UINT32_C(1) << EIDOLON_VRM_BONE_COUNT) - 1U;
    if ((anchor->residual_bone_mask & ~valid_bones) != 0U) {
        set_error(error, error_capacity, "calibration anchor has an invalid residual bone mask");
        return false;
    }
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        if ((anchor->residual_bone_mask & (UINT32_C(1) << bone)) == 0U) {
            continue;
        }
        const float *rotation = anchor->residual_rotation[bone];
        const float norm = sqrtf(rotation[0] * rotation[0] + rotation[1] * rotation[1] +
                                 rotation[2] * rotation[2] + rotation[3] * rotation[3]);
        if (!isfinite(norm) || fabsf(norm - 1.0F) > 0.01F) {
            set_error(error, error_capacity, "calibration residual for '%s' is not normalized",
                      BONE_NAMES[bone]);
            return false;
        }
    }
    return true;
}

bool eidolon_vrm_calibration_set_anchor(EidolonVrmCalibration *calibration,
                                        EidolonVrmCalibrationAnchorId id,
                                        const EidolonVrmCalibrationAnchor *anchor, char *error,
                                        size_t error_capacity) {
    if (calibration == NULL || id < EIDOLON_VRM_CALIBRATION_NEUTRAL ||
        id >= EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT ||
        !anchor_valid(anchor, error, error_capacity)) {
        return false;
    }
    EidolonVrmCalibrationAnchor candidate = *anchor;
    candidate.calibrated = true;
    calibration->anchors[id] = candidate;
    calibration->anchor_mask |= UINT32_C(1) << (unsigned int)id;
    return true;
}

bool eidolon_vrm_calibration_validate(const EidolonVrmCalibration *calibration,
                                      const EidolonVrmMeasurements *measurements, char *error,
                                      size_t error_capacity) {
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    if (calibration == NULL || measurements == NULL ||
        calibration->version != EIDOLON_VRM_CALIBRATION_VERSION ||
        measurements->version != EIDOLON_VRM_MEASUREMENTS_VERSION) {
        set_error(error, error_capacity, "unsupported or invalid VRM calibration version");
        return false;
    }
    if (calibration->anatomy_fingerprint != measurements->anatomy_fingerprint) {
        set_error(error, error_capacity,
                  "calibration anatomy fingerprint does not match the loaded model");
        return false;
    }
    const uint32_t valid_anchors = (UINT32_C(1) << EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT) - 1U;
    if ((calibration->anchor_mask & ~valid_anchors) != 0U) {
        set_error(error, error_capacity, "calibration has an invalid anchor mask");
        return false;
    }
    for (size_t id = 0U; id < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++id) {
        const bool present = (calibration->anchor_mask & (UINT32_C(1) << id)) != 0U;
        if (present != calibration->anchors[id].calibrated) {
            set_error(error, error_capacity, "calibration anchor '%s' has inconsistent state",
                      ANCHOR_NAMES[id]);
            return false;
        }
        if (present && !anchor_valid(&calibration->anchors[id], error, error_capacity)) {
            return false;
        }
        if (present && (calibration->anchors[id].residual_bone_mask &
                        ~((uint32_t)((UINT32_C(1) << EIDOLON_VRM_BONE_COUNT) - 1U))) != 0U) {
            set_error(error, error_capacity, "calibration anchor '%s' references an unknown bone",
                      ANCHOR_NAMES[id]);
            return false;
        }
        for (size_t bone = 0U; present && bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
            if ((calibration->anchors[id].residual_bone_mask & (UINT32_C(1) << bone)) != 0U &&
                !measurements->bones[bone].present) {
                set_error(error, error_capacity,
                          "calibration anchor '%s' references absent bone '%s'", ANCHOR_NAMES[id],
                          BONE_NAMES[bone]);
                return false;
            }
        }
    }
    return true;
}

bool eidolon_vrm_calibration_performance_complete(const EidolonVrmCalibration *calibration,
                                                  char *error, size_t error_capacity) {
    const uint32_t posture_resources =
        (UINT32_C(1) << EIDOLON_EPR_RESOURCE_TORSO) |
        (UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD) |
        (UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    const uint32_t gesture_resources =
        UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    if (calibration == NULL || calibration->version != EIDOLON_VRM_CALIBRATION_VERSION) {
        set_error(error, error_capacity, "performance calibration is unavailable or invalid");
        return false;
    }
    for (size_t index = 0U; index < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++index) {
        const uint32_t bit = UINT32_C(1) << (uint32_t)index;
        if ((calibration->anchor_mask & bit) == 0U ||
            !calibration->anchors[index].calibrated) {
            set_error(error, error_capacity, "performance calibration is missing anchor '%s'",
                      ANCHOR_NAMES[index]);
            return false;
        }
        const bool gesture = index == EIDOLON_VRM_CALIBRATION_CONTRAST_PREPARATION ||
                             index == EIDOLON_VRM_CALIBRATION_CONTRAST_PEAK ||
                             index == EIDOLON_VRM_CALIBRATION_CONTRAST_RECOVERY;
        const uint32_t required = gesture ? gesture_resources : posture_resources;
        if ((calibration->anchors[index].resource_mask & required) != required) {
            set_error(error, error_capacity,
                      "performance calibration anchor '%s' is missing required resources",
                      ANCHOR_NAMES[index]);
            return false;
        }
    }
    if (calibration->anchor_mask != EIDOLON_VRM_CALIBRATION_COMPLETE_ANCHOR_MASK) {
        set_error(error, error_capacity, "performance calibration has unknown anchor state");
        return false;
    }
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

_Static_assert(EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT == EIDOLON_EPR_POSE_ANCHOR_COUNT,
               "VRM and EPR calibration anchor registries must remain aligned");

bool eidolon_vrm_calibration_compile_realization(
    const EidolonVrmCalibration *calibration, const EidolonVrmMeasurements *measurements,
    const EidolonEprBodyProfile *body, EidolonEprRealizationProfile *profile, char *error,
    size_t error_capacity) {
    EidolonEprRealizationProfile candidate;
    if (profile == NULL || body == NULL ||
        !eidolon_vrm_calibration_validate(calibration, measurements, error, error_capacity) ||
        body->fingerprint != measurements->anatomy_fingerprint) {
        if (body != NULL && measurements != NULL &&
            body->fingerprint != measurements->anatomy_fingerprint) {
            set_error(error, error_capacity,
                      "EPR body profile does not match measured calibration anatomy");
        }
        return false;
    }
    SDL_zero(candidate);
    candidate.version = EIDOLON_EPR_REALIZATION_PROFILE_VERSION;
    candidate.body_fingerprint = calibration->anatomy_fingerprint;
    for (size_t index = 0U; index < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++index) {
        if ((calibration->anchor_mask & (UINT32_C(1) << (uint32_t)index)) == 0U) {
            continue;
        }
        const EidolonVrmCalibrationAnchor *source = &calibration->anchors[index];
        EidolonEprPoseAnchor *target = &candidate.anchors[index];
        target->resource_mask = source->resource_mask;
        SDL_memcpy(target->torso_euler, source->torso_euler, sizeof(target->torso_euler));
        SDL_memcpy(target->head_euler, source->head_euler, sizeof(target->head_euler));
        SDL_memcpy(target->right_arm.hand_target,
                   source->arms[EIDOLON_VRM_CALIBRATION_RIGHT].hand_target,
                   sizeof(target->right_arm.hand_target));
        SDL_memcpy(target->right_arm.elbow_pole,
                   source->arms[EIDOLON_VRM_CALIBRATION_RIGHT].elbow_pole,
                   sizeof(target->right_arm.elbow_pole));
        SDL_memcpy(target->right_arm.wrist_euler,
                   source->arms[EIDOLON_VRM_CALIBRATION_RIGHT].wrist_euler,
                   sizeof(target->right_arm.wrist_euler));
        target->right_arm.weight = source->arms[EIDOLON_VRM_CALIBRATION_RIGHT].weight;
        candidate.anchor_mask |= UINT32_C(1) << (uint32_t)index;
    }
    if (!eidolon_epr_realization_profile_validate(&candidate, body)) {
        set_error(error, error_capacity,
                  "calibrated EPR playback requires a matching neutral right-arm anchor");
        return false;
    }
    *profile = candidate;
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

static char *trim(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    char *end = text + SDL_strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static bool parse_u64(const char *text, int base, uint64_t *value) {
    if (text == NULL || value == NULL || *text == '-' || *text == '+') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, base);
    if (end == text || errno == ERANGE) {
        return false;
    }
    end = trim(end);
    if (*end != '\0') {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_float(const char *text, float minimum, float maximum, float *value) {
    errno = 0;
    char *end = NULL;
    const float parsed = strtof(text, &end);
    if (end == text || errno == ERANGE || !isfinite(parsed)) {
        return false;
    }
    end = trim(end);
    if (*end != '\0' || parsed < minimum || parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_vector(char *text, size_t count, float minimum, float maximum, float *values) {
    char *cursor = text;
    for (size_t index = 0U; index < count; ++index) {
        char *comma = SDL_strchr(cursor, ',');
        if (index + 1U < count) {
            if (comma == NULL) {
                return false;
            }
            *comma = '\0';
        } else if (comma != NULL) {
            return false;
        }
        if (!parse_float(trim(cursor), minimum, maximum, &values[index])) {
            return false;
        }
        if (comma != NULL) {
            cursor = comma + 1;
        }
    }
    return true;
}

static int anchor_from_name(const char *name, size_t length) {
    for (size_t id = 0U; id < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++id) {
        if (SDL_strlen(ANCHOR_NAMES[id]) == length &&
            SDL_strncmp(name, ANCHOR_NAMES[id], length) == 0) {
            return (int)id;
        }
    }
    return -1;
}

static int bone_from_name(const char *name) {
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        if (SDL_strcmp(name, BONE_NAMES[bone]) == 0) {
            return (int)bone;
        }
    }
    return -1;
}

static int resource_from_name(const char *name) {
    static const char *names[EIDOLON_EPR_RESOURCE_COUNT] = {
        "torso", "head", "eyes", "face_expression", "left_arm", "right_arm",
    };
    for (size_t resource = 0U; resource < EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
        if (SDL_strcmp(name, names[resource]) == 0) {
            return (int)resource;
        }
    }
    return -1;
}

static bool parse_resources(char *text, uint32_t *mask) {
    uint32_t result = 0U;
    char *cursor = text;
    while (*cursor != '\0') {
        char *comma = SDL_strchr(cursor, ',');
        if (comma != NULL) {
            *comma = '\0';
        }
        const int resource = resource_from_name(trim(cursor));
        if (resource < 0 || (result & (UINT32_C(1) << (unsigned int)resource)) != 0U) {
            return false;
        }
        result |= UINT32_C(1) << (unsigned int)resource;
        if (comma == NULL) {
            break;
        }
        cursor = comma + 1;
    }
    *mask = result;
    return result != 0U;
}

typedef struct AnchorParseState {
    EidolonVrmCalibrationAnchor value;
    uint16_t fields;
} AnchorParseState;

enum {
    ANCHOR_FIELD_RESOURCES = 1U << 0U,
    ANCHOR_FIELD_TORSO = 1U << 1U,
    ANCHOR_FIELD_HEAD = 1U << 2U,
    ANCHOR_FIELD_LEFT_HAND = 1U << 3U,
    ANCHOR_FIELD_LEFT_POLE = 1U << 4U,
    ANCHOR_FIELD_LEFT_WRIST = 1U << 5U,
    ANCHOR_FIELD_LEFT_WEIGHT = 1U << 6U,
    ANCHOR_FIELD_RIGHT_HAND = 1U << 7U,
    ANCHOR_FIELD_RIGHT_POLE = 1U << 8U,
    ANCHOR_FIELD_RIGHT_WRIST = 1U << 9U,
    ANCHOR_FIELD_RIGHT_WEIGHT = 1U << 10U,
    ANCHOR_REQUIRED_FIELDS = (1U << 11U) - 1U,
};

static bool assign_anchor_field(AnchorParseState *state, const char *field, char *value) {
    uint16_t bit = 0U;
    bool valid = false;
    if (SDL_strcmp(field, "resources") == 0) {
        bit = ANCHOR_FIELD_RESOURCES;
        valid = parse_resources(value, &state->value.resource_mask);
    } else if (SDL_strcmp(field, "torso") == 0) {
        bit = ANCHOR_FIELD_TORSO;
        valid = parse_vector(value, 3U, -CALIBRATION_ANGLE_LIMIT, CALIBRATION_ANGLE_LIMIT,
                             state->value.torso_euler);
    } else if (SDL_strcmp(field, "head") == 0) {
        bit = ANCHOR_FIELD_HEAD;
        valid = parse_vector(value, 3U, -CALIBRATION_ANGLE_LIMIT, CALIBRATION_ANGLE_LIMIT,
                             state->value.head_euler);
    } else {
        EidolonVrmCalibrationSide side;
        if (SDL_strncmp(field, "left.", 5U) == 0) {
            side = EIDOLON_VRM_CALIBRATION_LEFT;
            field += 5U;
        } else if (SDL_strncmp(field, "right.", 6U) == 0) {
            side = EIDOLON_VRM_CALIBRATION_RIGHT;
            field += 6U;
        } else if (SDL_strncmp(field, "residual.", 9U) == 0) {
            const int bone = bone_from_name(field + 9U);
            if (bone < 0 ||
                (state->value.residual_bone_mask & (UINT32_C(1) << (unsigned int)bone)) != 0U ||
                !parse_vector(value, 4U, -1.0F, 1.0F,
                              state->value.residual_rotation[(size_t)bone])) {
                return false;
            }
            state->value.residual_bone_mask |= UINT32_C(1) << (unsigned int)bone;
            return true;
        } else {
            return false;
        }
        EidolonVrmCalibrationArm *arm = &state->value.arms[side];
        const bool left = side == EIDOLON_VRM_CALIBRATION_LEFT;
        if (SDL_strcmp(field, "hand") == 0) {
            bit = left ? ANCHOR_FIELD_LEFT_HAND : ANCHOR_FIELD_RIGHT_HAND;
            valid = parse_vector(value, 3U, -CALIBRATION_TARGET_LIMIT, CALIBRATION_TARGET_LIMIT,
                                 arm->hand_target);
        } else if (SDL_strcmp(field, "pole") == 0) {
            bit = left ? ANCHOR_FIELD_LEFT_POLE : ANCHOR_FIELD_RIGHT_POLE;
            valid = parse_vector(value, 3U, -CALIBRATION_TARGET_LIMIT, CALIBRATION_TARGET_LIMIT,
                                 arm->elbow_pole);
        } else if (SDL_strcmp(field, "wrist") == 0) {
            bit = left ? ANCHOR_FIELD_LEFT_WRIST : ANCHOR_FIELD_RIGHT_WRIST;
            valid = parse_vector(value, 3U, -CALIBRATION_ANGLE_LIMIT, CALIBRATION_ANGLE_LIMIT,
                                 arm->wrist_euler);
        } else if (SDL_strcmp(field, "weight") == 0) {
            bit = left ? ANCHOR_FIELD_LEFT_WEIGHT : ANCHOR_FIELD_RIGHT_WEIGHT;
            valid = parse_float(value, 0.0F, 1.0F, &arm->weight);
        } else {
            return false;
        }
    }
    if (!valid || (state->fields & bit) != 0U) {
        return false;
    }
    state->fields |= bit;
    return true;
}

bool eidolon_vrm_calibration_parse(const char *text, size_t size,
                                   const EidolonVrmMeasurements *measurements,
                                   EidolonVrmCalibration *calibration, char *error,
                                   size_t error_capacity) {
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    if (text == NULL || measurements == NULL || calibration == NULL || size == SIZE_MAX) {
        set_error(error, error_capacity, "invalid calibration input");
        return false;
    }
    char *copy = SDL_malloc(size + 1U);
    if (copy == NULL) {
        set_error(error, error_capacity, "out of memory parsing calibration");
        return false;
    }
    SDL_memcpy(copy, text, size);
    copy[size] = '\0';

    uint32_t version = 0U;
    uint64_t fingerprint = 0U;
    bool has_version = false;
    bool has_fingerprint = false;
    AnchorParseState states[EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT];
    SDL_zeroa(states);
    for (size_t id = 0U; id < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++id) {
        for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
            states[id].value.residual_rotation[bone][3] = 1.0F;
        }
    }

    bool valid = true;
    size_t line_number = 0U;
    char *cursor = copy;
    while (*cursor != '\0' && valid) {
        ++line_number;
        char *line = cursor;
        char *newline = SDL_strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += SDL_strlen(cursor);
        }
        char *comment = SDL_strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }
        line = trim(line);
        if (*line == '\0') {
            continue;
        }
        char *equals = SDL_strchr(line, '=');
        if (equals == NULL || SDL_strchr(equals + 1, '=') != NULL) {
            set_error(error, error_capacity, "line %zu: expected one '='", line_number);
            valid = false;
            break;
        }
        *equals = '\0';
        char *key = trim(line);
        char *value = trim(equals + 1);
        if (SDL_strcmp(key, "version") == 0) {
            uint64_t parsed = 0U;
            valid = !has_version && parse_u64(value, 10, &parsed) && parsed <= UINT32_MAX;
            if (valid) {
                version = (uint32_t)parsed;
                has_version = true;
            }
        } else if (SDL_strcmp(key, "anatomy_fingerprint") == 0) {
            valid = !has_fingerprint && parse_u64(value, 0, &fingerprint);
            has_fingerprint = valid;
        } else if (SDL_strncmp(key, "anchor.", 7U) == 0) {
            const char *name = key + 7U;
            const char *dot = SDL_strchr(name, '.');
            const int id = dot != NULL ? anchor_from_name(name, (size_t)(dot - name)) : -1;
            valid = id >= 0 && assign_anchor_field(&states[(size_t)id], dot + 1, value);
        } else {
            valid = false;
        }
        if (!valid) {
            set_error(error, error_capacity, "line %zu: invalid or duplicate key '%s'", line_number,
                      key);
        }
    }

    EidolonVrmCalibration candidate;
    eidolon_vrm_calibration_init(&candidate, fingerprint);
    if (valid && (!has_version || !has_fingerprint)) {
        set_error(error, error_capacity, "calibration is missing version or anatomy_fingerprint");
        valid = false;
    }
    if (valid && version != EIDOLON_VRM_CALIBRATION_VERSION) {
        set_error(error, error_capacity, "unsupported calibration version %u", version);
        valid = false;
    }
    for (size_t id = 0U; valid && id < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++id) {
        if (states[id].fields == 0U) {
            continue;
        }
        if (states[id].fields != ANCHOR_REQUIRED_FIELDS) {
            set_error(error, error_capacity, "calibration anchor '%s' is incomplete",
                      ANCHOR_NAMES[id]);
            valid = false;
            break;
        }
        if (!eidolon_vrm_calibration_set_anchor(&candidate, (EidolonVrmCalibrationAnchorId)id,
                                                &states[id].value, error, error_capacity)) {
            valid = false;
        }
    }
    if (valid) {
        valid = eidolon_vrm_calibration_validate(&candidate, measurements, error, error_capacity);
    }
    SDL_free(copy);
    if (!valid) {
        return false;
    }
    *calibration = candidate;
    return true;
}

static bool append_text(char *text, size_t capacity, size_t *used, const char *format, ...) {
    if (*used >= capacity) {
        return false;
    }
    va_list arguments;
    va_start(arguments, format);
    const int written = SDL_vsnprintf(text + *used, capacity - *used, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *used) {
        return false;
    }
    *used += (size_t)written;
    return true;
}

static bool append_resources(char *text, size_t capacity, size_t *used, uint32_t mask) {
    static const char *names[EIDOLON_EPR_RESOURCE_COUNT] = {
        "torso", "head", "eyes", "face_expression", "left_arm", "right_arm",
    };
    bool first = true;
    for (size_t resource = 0U; resource < EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
        if ((mask & (UINT32_C(1) << resource)) == 0U) {
            continue;
        }
        if (!append_text(text, capacity, used, "%s%s", first ? "" : ",", names[resource])) {
            return false;
        }
        first = false;
    }
    return !first;
}

static bool append_vector(char *text, size_t capacity, size_t *used, const float *values,
                          size_t count) {
    for (size_t component = 0U; component < count; ++component) {
        if (!append_text(text, capacity, used, "%s%.9g", component == 0U ? "" : ", ",
                         (double)values[component])) {
            return false;
        }
    }
    return true;
}

bool eidolon_vrm_calibration_serialize(const EidolonVrmCalibration *calibration,
                                       const EidolonVrmMeasurements *measurements, char *text,
                                       size_t capacity, size_t *size, char *error,
                                       size_t error_capacity) {
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    if (text == NULL || capacity == 0U || size == NULL ||
        !eidolon_vrm_calibration_validate(calibration, measurements, error, error_capacity)) {
        return false;
    }
    size_t used = 0U;
    if (!append_text(text, capacity, &used, "version = %u\nanatomy_fingerprint = 0x%016llx\n",
                     calibration->version, (unsigned long long)calibration->anatomy_fingerprint)) {
        set_error(error, error_capacity, "serialized calibration exceeds output capacity");
        return false;
    }
    for (size_t id = 0U; id < EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT; ++id) {
        if ((calibration->anchor_mask & (UINT32_C(1) << id)) == 0U) {
            continue;
        }
        const EidolonVrmCalibrationAnchor *anchor = &calibration->anchors[id];
        const char *name = ANCHOR_NAMES[id];
        if (!append_text(text, capacity, &used, "\nanchor.%s.resources = ", name) ||
            !append_resources(text, capacity, &used, anchor->resource_mask) ||
            !append_text(text, capacity, &used, "\nanchor.%s.torso = ", name) ||
            !append_vector(text, capacity, &used, anchor->torso_euler, 3U) ||
            !append_text(text, capacity, &used, "\nanchor.%s.head = ", name) ||
            !append_vector(text, capacity, &used, anchor->head_euler, 3U)) {
            set_error(error, error_capacity, "serialized calibration exceeds output capacity");
            return false;
        }
        static const char *side_names[EIDOLON_VRM_CALIBRATION_SIDE_COUNT] = {"left", "right"};
        for (size_t side = 0U; side < EIDOLON_VRM_CALIBRATION_SIDE_COUNT; ++side) {
            const EidolonVrmCalibrationArm *arm = &anchor->arms[side];
            if (!append_text(text, capacity, &used, "\nanchor.%s.%s.hand = ", name,
                             side_names[side]) ||
                !append_vector(text, capacity, &used, arm->hand_target, 3U) ||
                !append_text(text, capacity, &used, "\nanchor.%s.%s.pole = ", name,
                             side_names[side]) ||
                !append_vector(text, capacity, &used, arm->elbow_pole, 3U) ||
                !append_text(text, capacity, &used, "\nanchor.%s.%s.wrist = ", name,
                             side_names[side]) ||
                !append_vector(text, capacity, &used, arm->wrist_euler, 3U) ||
                !append_text(text, capacity, &used, "\nanchor.%s.%s.weight = %.9g", name,
                             side_names[side], (double)arm->weight)) {
                set_error(error, error_capacity, "serialized calibration exceeds output capacity");
                return false;
            }
        }
        for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
            if ((anchor->residual_bone_mask & (UINT32_C(1) << bone)) == 0U) {
                continue;
            }
            if (!append_text(text, capacity, &used, "\nanchor.%s.residual.%s = ", name,
                             BONE_NAMES[bone]) ||
                !append_vector(text, capacity, &used, anchor->residual_rotation[bone], 4U)) {
                set_error(error, error_capacity, "serialized calibration exceeds output capacity");
                return false;
            }
        }
        if (!append_text(text, capacity, &used, "\n")) {
            set_error(error, error_capacity, "serialized calibration exceeds output capacity");
            return false;
        }
    }
    *size = used;
    return true;
}

bool eidolon_vrm_calibration_save(const EidolonVrmCalibration *calibration,
                                  const EidolonVrmMeasurements *measurements, const char *path,
                                  char *error, size_t error_capacity) {
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    if (path == NULL || path[0] == '\0') {
        set_error(error, error_capacity, "calibration path is empty");
        return false;
    }
    char *text = SDL_malloc(EIDOLON_VRM_CALIBRATION_TEXT_CAPACITY);
    if (text == NULL) {
        set_error(error, error_capacity, "out of memory serializing calibration");
        return false;
    }
    size_t size = 0U;
    if (!eidolon_vrm_calibration_serialize(calibration, measurements, text,
                                           EIDOLON_VRM_CALIBRATION_TEXT_CAPACITY, &size, error,
                                           error_capacity)) {
        SDL_free(text);
        return false;
    }
    char temporary[1200];
    const int written = SDL_snprintf(temporary, sizeof(temporary), "%s.tmp-%llu", path,
                                     (unsigned long long)SDL_GetTicksNS());
    if (written <= 0 || (size_t)written >= sizeof(temporary)) {
        SDL_free(text);
        set_error(error, error_capacity, "calibration temporary path is too long");
        return false;
    }
    if (!SDL_SaveFile(temporary, text, size)) {
        set_error(error, error_capacity, "could not write calibration temporary file: %s",
                  SDL_GetError());
        SDL_free(text);
        return false;
    }
    SDL_free(text);
    if (!SDL_RenamePath(temporary, path)) {
        char cause[EIDOLON_VRM_CALIBRATION_ERROR_CAPACITY];
        SDL_strlcpy(cause, SDL_GetError(), sizeof(cause));
        (void)SDL_RemovePath(temporary);
        set_error(error, error_capacity, "could not replace calibration sidecar: %s", cause);
        return false;
    }
    return true;
}

const EidolonVrmCalibrationAnchor *
eidolon_vrm_calibration_anchor(const EidolonVrmCalibration *calibration,
                               EidolonVrmCalibrationAnchorId id) {
    if (calibration == NULL || id < EIDOLON_VRM_CALIBRATION_NEUTRAL ||
        id >= EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT ||
        (calibration->anchor_mask & (UINT32_C(1) << (unsigned int)id)) == 0U) {
        return NULL;
    }
    return &calibration->anchors[id];
}

const char *eidolon_vrm_calibration_anchor_name(EidolonVrmCalibrationAnchorId id) {
    if (id < EIDOLON_VRM_CALIBRATION_NEUTRAL || id >= EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT) {
        return "unknown";
    }
    return ANCHOR_NAMES[id];
}
