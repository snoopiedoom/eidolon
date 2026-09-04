#ifndef EIDOLON_VRMA_CLIP_H
#define EIDOLON_VRMA_CLIP_H

#include "humanoid_pose.h"

#include <cgltf.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRMA_CLIP_VERSION 1U
#define EIDOLON_VRMA_COVERAGE_VERSION 1U
#define EIDOLON_VRMA_ERROR_CAPACITY 256U

typedef enum EidolonVrmaChain {
    EIDOLON_VRMA_CHAIN_ROOT = UINT32_C(1) << 0U,
    EIDOLON_VRMA_CHAIN_TORSO = UINT32_C(1) << 1U,
    EIDOLON_VRMA_CHAIN_HEAD = UINT32_C(1) << 2U,
    EIDOLON_VRMA_CHAIN_LEFT_ARM = UINT32_C(1) << 3U,
    EIDOLON_VRMA_CHAIN_RIGHT_ARM = UINT32_C(1) << 4U,
    EIDOLON_VRMA_CHAIN_LEFT_LEG = UINT32_C(1) << 5U,
    EIDOLON_VRMA_CHAIN_RIGHT_LEG = UINT32_C(1) << 6U
} EidolonVrmaChain;

typedef enum EidolonVrmaTrackPath {
    EIDOLON_VRMA_TRACK_ROTATION = 0,
    EIDOLON_VRMA_TRACK_HIPS_TRANSLATION
} EidolonVrmaTrackPath;

typedef enum EidolonVrmaInterpolation {
    EIDOLON_VRMA_INTERPOLATION_STEP = 0,
    EIDOLON_VRMA_INTERPOLATION_LINEAR,
    EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE
} EidolonVrmaInterpolation;

typedef struct EidolonVrmaTrack {
    EidolonHumanoidRole role;
    EidolonVrmaTrackPath path;
    EidolonVrmaInterpolation interpolation;
    float *times;
    float *values;
    size_t key_count;
    size_t component_count;
    size_t values_per_key;
} EidolonVrmaTrack;

typedef struct EidolonVrmaClip {
    uint32_t version;
    uint64_t mapped_roles;
    int source_node_by_role[EIDOLON_HUMANOID_ROLE_COUNT];
    float rest_local_rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float rest_world_rotations[EIDOLON_HUMANOID_ROLE_COUNT][4];
    float rest_hips_translation[3];
    float source_hips_height;
    float duration_seconds;
    EidolonVrmaTrack *tracks;
    size_t track_count;
    size_t auxiliary_channel_count;
} EidolonVrmaClip;

typedef struct EidolonVrmaCoverage {
    uint32_t version;
    uint32_t tracked_chain_mask;
    uint32_t varying_chain_mask;
    uint64_t rotation_roles;
    uint64_t varying_rotation_roles;
    size_t rotation_track_count;
    size_t varying_rotation_track_count;
    size_t auxiliary_channel_count;
    bool has_hips_translation;
    bool hips_translation_varies;
} EidolonVrmaCoverage;

bool eidolon_vrma_clip_parse(const cgltf_data *data, EidolonVrmaClip *clip, char *error,
                             size_t error_capacity);
bool eidolon_vrma_clip_load(const char *path, EidolonVrmaClip *clip, char *error,
                            size_t error_capacity);
bool eidolon_vrma_clip_sample(const EidolonVrmaClip *clip, float seconds, bool loop,
                              EidolonHumanoidPose *pose, char *error, size_t error_capacity);
bool eidolon_vrma_clip_coverage(const EidolonVrmaClip *clip, EidolonVrmaCoverage *coverage);
const char *eidolon_vrma_track_path_name(EidolonVrmaTrackPath path);
const char *eidolon_vrma_interpolation_name(EidolonVrmaInterpolation interpolation);
void eidolon_vrma_clip_destroy(EidolonVrmaClip *clip);

#endif
