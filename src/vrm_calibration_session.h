#ifndef EIDOLON_VRM_CALIBRATION_SESSION_H
#define EIDOLON_VRM_CALIBRATION_SESSION_H

#include "epr/canonical_control.h"
#include "vrm_calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRM_CALIBRATION_PATH_CAPACITY 1024U
#define EIDOLON_VRM_CALIBRATION_RESIDUAL_LIMIT_RADIANS 0.78539816339F

typedef struct EidolonVrmCalibrationSession {
    EidolonVrmMeasurements measurements;
    EidolonEprBodyProfile body;
    EidolonVrmCalibration baseline;
    EidolonVrmCalibration working;
    EidolonVrmCalibrationAnchor draft;
    EidolonVrmCalibrationAnchor source_draft;
    EidolonCanonicalControl source_control;
    EidolonVrmCalibrationAnchorId selected_anchor;
    EidolonVrmHumanBone selected_residual_bone;
    uint64_t next_projection_revision;
    char path[EIDOLON_VRM_CALIBRATION_PATH_CAPACITY];
    char error[EIDOLON_VRM_CALIBRATION_ERROR_CAPACITY];
    bool active;
    bool source_was_calibrated;
    bool dirty;
    bool saved;
} EidolonVrmCalibrationSession;

EidolonEprTick eidolon_vrm_calibration_anchor_tick(EidolonVrmCalibrationAnchorId anchor);
bool eidolon_vrm_calibration_session_init(EidolonVrmCalibrationSession *session,
                                          const EidolonVrmMeasurements *measurements,
                                          const EidolonEprBodyProfile *body,
                                          const EidolonVrmCalibration *calibration,
                                          const char *path, uint64_t projection_revision);
bool eidolon_vrm_calibration_session_select(EidolonVrmCalibrationSession *session,
                                            EidolonVrmCalibrationAnchorId anchor,
                                            const EidolonCanonicalControl *source_control);
void eidolon_vrm_calibration_session_revert(EidolonVrmCalibrationSession *session);
bool eidolon_vrm_calibration_session_commit(EidolonVrmCalibrationSession *session);
bool eidolon_vrm_calibration_session_save(EidolonVrmCalibrationSession *session);
bool eidolon_vrm_calibration_session_make_control(EidolonVrmCalibrationSession *session,
                                                  EidolonCanonicalControl *control);
bool eidolon_vrm_calibration_session_residual_bone_editable(
    const EidolonVrmCalibrationSession *session, EidolonVrmHumanBone bone);
bool eidolon_vrm_calibration_session_select_residual_bone(EidolonVrmCalibrationSession *session,
                                                          EidolonVrmHumanBone bone);
bool eidolon_vrm_calibration_session_residual_vector(
    const EidolonVrmCalibrationSession *session, EidolonVrmHumanBone bone, float vector[3]);
bool eidolon_vrm_calibration_session_set_residual_vector(EidolonVrmCalibrationSession *session,
                                                         EidolonVrmHumanBone bone,
                                                         const float vector[3]);
bool eidolon_vrm_calibration_session_clear_residual(EidolonVrmCalibrationSession *session,
                                                    EidolonVrmHumanBone bone);

#endif
