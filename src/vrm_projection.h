#ifndef EIDOLON_VRM_PROJECTION_H
#define EIDOLON_VRM_PROJECTION_H

#include "epr/performance_runtime.h"
#include "motion.h"
#include "vrm_body.h"
#include "vrm_calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct EidolonVrmProjection {
    int nodes[EIDOLON_VRM_BONE_COUNT];
    EidolonMotionRig scratch;
    float (*base_translations)[3];
    float (*base_rotations)[4];
    float (*base_scales)[3];
    float bind_world_rotations[EIDOLON_VRM_BONE_COUNT][4];
    float inverse_bind_world_rotations[EIDOLON_VRM_BONE_COUNT][4];
    float right[3];
    float up[3];
    float forward[3];
    bool *owned_nodes;
    /* Dynamic ownership prevents calibrated residuals from accumulating or persisting on nodes
       outside the fixed canonical projection set. */
    bool *residual_owned_nodes;
    bool *candidate_residual_owned_nodes;
    size_t node_count;
    uint64_t control_revision;
    float focused_expression_weight;
    bool look_at_executable;
    bool expression_executable;
    bool expression_is_binary;
    bool ready;
} EidolonVrmProjection;

bool eidolon_vrm_projection_init(EidolonVrmProjection *projection, const EidolonVrmBody *body,
                                 const EidolonEprBodyProfile *profile,
                                 const EidolonMotionRig *rig);
bool eidolon_vrm_projection_capture_base(EidolonVrmProjection *projection,
                                         const EidolonMotionRig *rig);
bool eidolon_vrm_projection_apply(EidolonVrmProjection *projection, EidolonMotionRig *rig,
                                  const EidolonCanonicalControl *control);
bool eidolon_vrm_projection_apply_calibrated(EidolonVrmProjection *projection,
                                             EidolonMotionRig *rig,
                                             const EidolonCanonicalControl *control,
                                             const EidolonVrmCalibration *calibration);
void eidolon_vrm_projection_destroy(EidolonVrmProjection *projection);

#endif
