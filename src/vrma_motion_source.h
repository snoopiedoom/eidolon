#ifndef EIDOLON_VRMA_MOTION_SOURCE_H
#define EIDOLON_VRMA_MOTION_SOURCE_H

#include "epr/motion_catalog.h"
#include "vrma_clip.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_VRMA_MOTION_SOURCE_ERROR_CAPACITY 256U

/* Emits only channels backed by actual VRMA animation tracks. */
bool eidolon_vrma_motion_sample_normalized(const EidolonVrmaClip *clip, float seconds, bool loop,
                                           EidolonHumanoidPose *pose, char *error,
                                           size_t error_capacity);
/* The returned source borrows clip, which must remain immutable and outlive the catalog. */
bool eidolon_vrma_motion_source_build(const EidolonVrmaClip *clip, uint64_t source_identity,
                                      bool loop, EidolonEprMotionSource *source, char *error,
                                      size_t error_capacity);

#endif
