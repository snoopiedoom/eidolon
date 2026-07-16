#ifndef EIDOLON_POSE_H
#define EIDOLON_POSE_H

#include <stddef.h>

#define EIDOLON_SEMANTIC_POSE_CUSTOM (-1)
#define EIDOLON_POSE_ARM_COUNT 2
#define EIDOLON_POSE_TARGET_MIN (-1.25F)
#define EIDOLON_POSE_TARGET_MAX 1.25F

typedef struct EidolonArmPoseGoal {
    /* Shoulder-relative coordinates in arm-length units: outward, up, forward. */
    float hand[3];
    float elbow_pole[3];
    float weight;
} EidolonArmPoseGoal;

typedef struct EidolonSemanticPose {
    const char *name;
    const char *intent;
    EidolonArmPoseGoal arms[EIDOLON_POSE_ARM_COUNT];
    float soften_ratio;
} EidolonSemanticPose;

size_t eidolon_semantic_pose_count(void);
const EidolonSemanticPose *eidolon_semantic_pose(size_t index);

#endif
