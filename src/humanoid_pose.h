#ifndef EIDOLON_HUMANOID_POSE_H
#define EIDOLON_HUMANOID_POSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_HUMANOID_POSE_VERSION 1U
#define EIDOLON_HUMANOID_POSE_LAYER_VERSION 2U
#define EIDOLON_HUMANOID_POSE_LAYER_CAPACITY 32U

typedef enum EidolonHumanoidRole {
    EIDOLON_HUMANOID_ROLE_HIPS = 0,
    EIDOLON_HUMANOID_ROLE_SPINE,
    EIDOLON_HUMANOID_ROLE_CHEST,
    EIDOLON_HUMANOID_ROLE_UPPER_CHEST,
    EIDOLON_HUMANOID_ROLE_NECK,
    EIDOLON_HUMANOID_ROLE_HEAD,
    EIDOLON_HUMANOID_ROLE_LEFT_EYE,
    EIDOLON_HUMANOID_ROLE_RIGHT_EYE,
    EIDOLON_HUMANOID_ROLE_JAW,
    EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG,
    EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG,
    EIDOLON_HUMANOID_ROLE_LEFT_FOOT,
    EIDOLON_HUMANOID_ROLE_LEFT_TOES,
    EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG,
    EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG,
    EIDOLON_HUMANOID_ROLE_RIGHT_FOOT,
    EIDOLON_HUMANOID_ROLE_RIGHT_TOES,
    EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER,
    EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM,
    EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM,
    EIDOLON_HUMANOID_ROLE_LEFT_HAND,
    EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER,
    EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM,
    EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM,
    EIDOLON_HUMANOID_ROLE_RIGHT_HAND,
    EIDOLON_HUMANOID_ROLE_LEFT_THUMB_METACARPAL,
    EIDOLON_HUMANOID_ROLE_LEFT_THUMB_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_LEFT_THUMB_DISTAL,
    EIDOLON_HUMANOID_ROLE_LEFT_INDEX_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_LEFT_INDEX_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_LEFT_INDEX_DISTAL,
    EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_LEFT_MIDDLE_DISTAL,
    EIDOLON_HUMANOID_ROLE_LEFT_RING_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_LEFT_RING_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_LEFT_RING_DISTAL,
    EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_DISTAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_METACARPAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_DISTAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_RIGHT_INDEX_DISTAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_RIGHT_MIDDLE_DISTAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_RING_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_RING_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_RIGHT_RING_DISTAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_PROXIMAL,
    EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_INTERMEDIATE,
    EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_DISTAL,
    EIDOLON_HUMANOID_ROLE_COUNT
} EidolonHumanoidRole;

typedef struct EidolonHumanoidPose {
    uint32_t version;
    uint64_t rotation_mask;
    float rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float hips_translation[3];
    bool has_hips_translation;
} EidolonHumanoidPose;

typedef enum EidolonHumanoidPoseLayerMode {
    EIDOLON_HUMANOID_POSE_LAYER_ABSOLUTE = 0,
    EIDOLON_HUMANOID_POSE_LAYER_ADDITIVE,
    EIDOLON_HUMANOID_POSE_LAYER_MODE_COUNT
} EidolonHumanoidPoseLayerMode;

/* An ordered normalized-pose contribution; the mask must be a subset of pose ownership. */
typedef struct EidolonHumanoidPoseLayer {
    uint32_t version;
    const EidolonHumanoidPose *pose;
    uint64_t rotation_mask;
    EidolonHumanoidPoseLayerMode mode;
    float weight;
    float intensity;
    bool owns_hips_translation;
} EidolonHumanoidPoseLayer;

const char *eidolon_humanoid_role_name(EidolonHumanoidRole bone);
int eidolon_humanoid_role_from_name(const char *name, size_t length);
bool eidolon_humanoid_role_required(EidolonHumanoidRole bone);
bool eidolon_humanoid_role_allowed_in_vrma(EidolonHumanoidRole bone);
int eidolon_humanoid_role_parent(EidolonHumanoidRole role);
void eidolon_humanoid_pose_init(EidolonHumanoidPose *pose);
void eidolon_humanoid_pose_layer_init(EidolonHumanoidPoseLayer *layer,
                                      const EidolonHumanoidPose *pose);
bool eidolon_humanoid_pose_validate(const EidolonHumanoidPose *pose);
/* Applies layers in array order and leaves result untouched on any rejection. */
bool eidolon_humanoid_pose_compose(const EidolonHumanoidPose *base,
                                   const EidolonHumanoidPoseLayer *layers, size_t layer_count,
                                   EidolonHumanoidPose *result);

#endif
