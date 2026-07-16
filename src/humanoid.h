#ifndef EIDOLON_HUMANOID_H
#define EIDOLON_HUMANOID_H

#include "motion.h"

typedef enum EidolonHumanoidBone {
    EIDOLON_HUMANOID_HIPS,
    EIDOLON_HUMANOID_SPINE,
    EIDOLON_HUMANOID_CHEST,
    EIDOLON_HUMANOID_NECK,
    EIDOLON_HUMANOID_HEAD,
    EIDOLON_HUMANOID_LEFT_EYE,
    EIDOLON_HUMANOID_RIGHT_EYE,
    EIDOLON_HUMANOID_LEFT_SHOULDER,
    EIDOLON_HUMANOID_LEFT_UPPER_ARM,
    EIDOLON_HUMANOID_LEFT_LOWER_ARM,
    EIDOLON_HUMANOID_LEFT_HAND,
    EIDOLON_HUMANOID_RIGHT_SHOULDER,
    EIDOLON_HUMANOID_RIGHT_UPPER_ARM,
    EIDOLON_HUMANOID_RIGHT_LOWER_ARM,
    EIDOLON_HUMANOID_RIGHT_HAND,
    EIDOLON_HUMANOID_LEFT_UPPER_LEG,
    EIDOLON_HUMANOID_LEFT_LOWER_LEG,
    EIDOLON_HUMANOID_LEFT_FOOT,
    EIDOLON_HUMANOID_LEFT_TOES,
    EIDOLON_HUMANOID_RIGHT_UPPER_LEG,
    EIDOLON_HUMANOID_RIGHT_LOWER_LEG,
    EIDOLON_HUMANOID_RIGHT_FOOT,
    EIDOLON_HUMANOID_RIGHT_TOES,
    EIDOLON_HUMANOID_BONE_COUNT,
} EidolonHumanoidBone;

typedef enum EidolonHumanoidSide {
    EIDOLON_HUMANOID_LEFT,
    EIDOLON_HUMANOID_RIGHT,
} EidolonHumanoidSide;

typedef struct EidolonHumanoidProfile {
    int nodes[EIDOLON_HUMANOID_BONE_COUNT];
    float bind_positions[EIDOLON_HUMANOID_BONE_COUNT][3];
    float right[3];
    float up[3];
    float forward[3];
    float shoulder_width;
    float torso_length;
    float arm_length[2];
    float leg_length[2];
} EidolonHumanoidProfile;

bool eidolon_humanoid_profile_init(EidolonHumanoidProfile *profile, const EidolonMotionRig *rig);
int eidolon_humanoid_node(const EidolonHumanoidProfile *profile, EidolonHumanoidBone bone);
const char *eidolon_humanoid_bone_name(EidolonHumanoidBone bone);
void eidolon_humanoid_arm_nodes(const EidolonHumanoidProfile *profile, EidolonHumanoidSide side,
                                int *upper_arm, int *lower_arm, int *hand);

#endif
