#ifndef EIDOLON_VRM_PROJECTION_H
#define EIDOLON_VRM_PROJECTION_H

#include "epr/performance_runtime.h"
#include "motion.h"
#include "vrm_body.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct EidolonVrmProjection {
    int nodes[EIDOLON_VRM_BONE_COUNT];
    float (*last_valid_rotations)[4];
    size_t node_count;
    uint64_t control_revision;
    float focused_expression_weight;
    bool ready;
} EidolonVrmProjection;

bool eidolon_vrm_projection_init(EidolonVrmProjection *projection, const EidolonVrmBody *body,
                                 const EidolonMotionRig *rig);
bool eidolon_vrm_projection_apply(EidolonVrmProjection *projection, EidolonMotionRig *rig,
                                  const EidolonCanonicalControl *control);
void eidolon_vrm_projection_destroy(EidolonVrmProjection *projection);

#endif
