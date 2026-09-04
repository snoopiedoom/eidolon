#ifndef EIDOLON_VRM_RETARGET_H
#define EIDOLON_VRM_RETARGET_H

#include "motion.h"
#include "vrm_body.h"
#include "vrma_clip.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRM_RETARGETER_VERSION 1U
#define EIDOLON_VRM_RETARGET_ERROR_CAPACITY 256U

typedef enum EidolonVrmRootMotionPolicy {
    EIDOLON_VRM_ROOT_MOTION_IN_PLACE = 0,
    EIDOLON_VRM_ROOT_MOTION_FULL
} EidolonVrmRootMotionPolicy;

typedef struct EidolonVrmRetargeter {
    uint32_t version;
    uint64_t source_roles;
    uint64_t destination_roles;
    int destination_node_by_role[EIDOLON_HUMANOID_ROLE_COUNT];
    float source_rest_local_rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float source_rest_world_rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float destination_rest_local_rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float destination_rest_world_rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float source_rest_hips_translation[3];
    float destination_rest_hips_translation[3];
    float source_hips_height;
    float destination_hips_height;
    float hips_translation_scale;
    EidolonMotionRig scratch;
    size_t node_count;
    bool ready;
} EidolonVrmRetargeter;
/* Initializes only destination rest space for already-normalized humanoid frames. */
bool eidolon_vrm_retargeter_init_destination(EidolonVrmRetargeter *retargeter,
                                             const EidolonVrmBody *destination,
                                             const EidolonMotionRig *destination_rig, char *error,
                                             size_t error_capacity);
/*
 * Overwrites only normalized-owned destination channels on top of destination_rig and commits
 * nothing unless the complete hierarchy is valid.
 */
bool eidolon_vrm_retargeter_apply_normalized(EidolonVrmRetargeter *retargeter,
                                             const EidolonHumanoidPose *normalized_pose,
                                             EidolonVrmRootMotionPolicy root_motion_policy,
                                             EidolonMotionRig *destination_rig, char *error,
                                             size_t error_capacity);

bool eidolon_vrm_retargeter_init(EidolonVrmRetargeter *retargeter, const EidolonVrmaClip *source,
                                 const EidolonVrmBody *destination,
                                 const EidolonMotionRig *destination_rig, char *error,
                                 size_t error_capacity);
bool eidolon_vrm_retargeter_apply(EidolonVrmRetargeter *retargeter,
                                  const EidolonHumanoidPose *source_pose,
                                  EidolonVrmRootMotionPolicy root_motion_policy,
                                  EidolonMotionRig *destination_rig, char *error,
                                  size_t error_capacity);
void eidolon_vrm_retargeter_destroy(EidolonVrmRetargeter *retargeter);

#endif
