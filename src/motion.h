#ifndef EIDOLON_MOTION_H
#define EIDOLON_MOTION_H

#include <SDL3/SDL.h>

#define EIDOLON_MOTION_NAME_CAPACITY 64

typedef struct EidolonNeutralPose {
    float shoulder_lower_radians;
    float elbow_bend_add_radians;
} EidolonNeutralPose;

typedef struct EidolonIdleTuning {
    float breath_period_seconds;
    float breath_chest_radians;
    float breath_neck_counter_radians;
    float sway_period_seconds;
    float sway_spine_radians;
    float sway_chest_counter_radians;
    float sway_head_radians;
} EidolonIdleTuning;

typedef struct EidolonMotionNode {
    char name[EIDOLON_MOTION_NAME_CAPACITY];
    int parent;
    float bind_translation[3];
    float bind_rotation[4];
    float bind_scale[3];
    float translation[3];
    float rotation[4];
    float scale[3];
    float world[16];
    Uint8 world_state;
} EidolonMotionNode;

typedef struct EidolonMotionRig {
    EidolonMotionNode *nodes;
    size_t node_count;
    int pelvis;
    int spine;
    int spine1;
    int neck;
    int head;
    int left_upper_arm;
    int left_forearm;
    int right_upper_arm;
    int right_forearm;
    EidolonNeutralPose neutral_pose;
    EidolonIdleTuning idle_tuning;
} EidolonMotionRig;

bool eidolon_motion_init(EidolonMotionRig *rig, size_t node_count);
void eidolon_motion_destroy(EidolonMotionRig *rig);
bool eidolon_motion_set_node(EidolonMotionRig *rig, size_t node_index, const char *name, int parent,
                             const float translation[3], const float rotation[4],
                             const float scale[3]);
bool eidolon_motion_finalize(EidolonMotionRig *rig);
void eidolon_motion_set_neutral_pose(EidolonMotionRig *rig, EidolonNeutralPose pose);
EidolonNeutralPose eidolon_motion_neutral_pose(const EidolonMotionRig *rig);
void eidolon_motion_set_idle_tuning(EidolonMotionRig *rig, EidolonIdleTuning tuning);
EidolonIdleTuning eidolon_motion_idle_tuning(const EidolonMotionRig *rig);
void eidolon_motion_update_idle(EidolonMotionRig *rig, uint64_t now_ms);
bool eidolon_motion_rebuild_world(EidolonMotionRig *rig);
const float *eidolon_motion_world(EidolonMotionRig *rig, size_t node_index);

#endif
