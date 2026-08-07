#ifndef EIDOLON_VRM_CALIBRATION_H
#define EIDOLON_VRM_CALIBRATION_H

#include "epr/body_resources.h"
#include "epr/realization_program.h"
#include "vrm_body.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRM_MEASUREMENTS_VERSION 1U
#define EIDOLON_VRM_CALIBRATION_VERSION 1U
#define EIDOLON_VRM_CALIBRATION_ERROR_CAPACITY 256U
#define EIDOLON_VRM_CALIBRATION_TEXT_CAPACITY 65536U
#define EIDOLON_VRM_CALIBRATION_SIDE_COUNT 2U

typedef enum EidolonVrmCalibrationSide {
    EIDOLON_VRM_CALIBRATION_LEFT = 0,
    EIDOLON_VRM_CALIBRATION_RIGHT,
} EidolonVrmCalibrationSide;

typedef enum EidolonVrmCalibrationAnchorId {
    EIDOLON_VRM_CALIBRATION_NEUTRAL = 0,
    EIDOLON_VRM_CALIBRATION_ATTENTIVE,
    EIDOLON_VRM_CALIBRATION_THINKING,
    EIDOLON_VRM_CALIBRATION_RESPONDING,
    EIDOLON_VRM_CALIBRATION_CONTRAST_PREPARATION,
    EIDOLON_VRM_CALIBRATION_CONTRAST_PEAK,
    EIDOLON_VRM_CALIBRATION_CONTRAST_RECOVERY,
    EIDOLON_VRM_CALIBRATION_INTERRUPTED_GUARDED,
    EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT,
} EidolonVrmCalibrationAnchorId;

#define EIDOLON_VRM_CALIBRATION_COMPLETE_ANCHOR_MASK                                      \
    ((UINT32_C(1) << (uint32_t)EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT) - UINT32_C(1))

typedef struct EidolonVrmBoneMeasurement {
    float bind_position[3];
    /* Distance from the nearest present semantic humanoid parent. */
    float segment_length;
    bool present;
} EidolonVrmBoneMeasurement;

typedef struct EidolonVrmMeasurements {
    uint32_t version;
    uint64_t anatomy_fingerprint;
    float right[3];
    float up[3];
    float forward[3];
    float skeleton_height;
    float shoulder_width;
    float torso_length;
    float upper_arm_length[EIDOLON_VRM_CALIBRATION_SIDE_COUNT];
    float lower_arm_length[EIDOLON_VRM_CALIBRATION_SIDE_COUNT];
    float upper_leg_length[EIDOLON_VRM_CALIBRATION_SIDE_COUNT];
    float lower_leg_length[EIDOLON_VRM_CALIBRATION_SIDE_COUNT];
    EidolonVrmBoneMeasurement bones[EIDOLON_VRM_BONE_COUNT];
} EidolonVrmMeasurements;

typedef struct EidolonVrmCalibrationArm {
    /* Shoulder-relative anatomical coordinates in whole-arm units: outward, up, forward. */
    float hand_target[3];
    float elbow_pole[3];
    float wrist_euler[3];
    float weight;
} EidolonVrmCalibrationArm;

typedef struct EidolonVrmCalibrationAnchor {
    uint32_t resource_mask;
    float torso_euler[3];
    float head_euler[3];
    EidolonVrmCalibrationArm arms[EIDOLON_VRM_CALIBRATION_SIDE_COUNT];
    uint32_t residual_bone_mask;
    /* Model-local correction deltas applied after canonical task-space solving. */
    float residual_rotation[EIDOLON_VRM_BONE_COUNT][4];
    bool calibrated;
} EidolonVrmCalibrationAnchor;

typedef struct EidolonVrmCalibration {
    uint32_t version;
    uint64_t anatomy_fingerprint;
    uint32_t anchor_mask;
    EidolonVrmCalibrationAnchor anchors[EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT];
} EidolonVrmCalibration;

bool eidolon_vrm_measure(const cgltf_data *data, const EidolonVrmBody *body,
                         EidolonVrmMeasurements *measurements, char *error, size_t error_capacity);
void eidolon_vrm_calibration_init(EidolonVrmCalibration *calibration, uint64_t anatomy_fingerprint);
bool eidolon_vrm_calibration_set_anchor(EidolonVrmCalibration *calibration,
                                        EidolonVrmCalibrationAnchorId id,
                                        const EidolonVrmCalibrationAnchor *anchor, char *error,
                                        size_t error_capacity);
bool eidolon_vrm_calibration_validate(const EidolonVrmCalibration *calibration,
                                      const EidolonVrmMeasurements *measurements, char *error,
                                      size_t error_capacity);
bool eidolon_vrm_calibration_performance_complete(const EidolonVrmCalibration *calibration,
                                                  char *error, size_t error_capacity);
bool eidolon_vrm_calibration_parse(const char *text, size_t size,
                                   const EidolonVrmMeasurements *measurements,
                                   EidolonVrmCalibration *calibration, char *error,
                                   size_t error_capacity);
bool eidolon_vrm_calibration_serialize(const EidolonVrmCalibration *calibration,
                                       const EidolonVrmMeasurements *measurements, char *text,
                                       size_t capacity, size_t *size, char *error,
                                       size_t error_capacity);
bool eidolon_vrm_calibration_save(const EidolonVrmCalibration *calibration,
                                  const EidolonVrmMeasurements *measurements, const char *path,
                                  char *error, size_t error_capacity);
bool eidolon_vrm_calibration_compile_realization(
    const EidolonVrmCalibration *calibration, const EidolonVrmMeasurements *measurements,
    const EidolonEprBodyProfile *body, EidolonEprRealizationProfile *profile, char *error,
    size_t error_capacity);
const EidolonVrmCalibrationAnchor *
eidolon_vrm_calibration_anchor(const EidolonVrmCalibration *calibration,
                               EidolonVrmCalibrationAnchorId id);
const char *eidolon_vrm_calibration_anchor_name(EidolonVrmCalibrationAnchorId id);
const char *eidolon_vrm_calibration_bone_name(EidolonVrmHumanBone bone);

#endif
