#include "vrma_motion_source.h"

#include "humanoid_rest.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define VRMA_MOTION_SOURCE_EPSILON 0.000001F

static void set_error(char *error, size_t capacity, const char *format, ...) {
    va_list arguments;
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static uint64_t valid_rotation_mask(void) {
    return (UINT64_C(1) << (uint32_t)EIDOLON_HUMANOID_ROLE_COUNT) - UINT64_C(1);
}

static uint64_t required_rotation_mask(void) {
    uint64_t result = 0U;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
            result |= UINT64_C(1) << (uint32_t)role;
        }
    }
    return result;
}

static size_t bit_count(uint64_t value) {
    size_t result = 0U;
    while (value != 0U) {
        result += (size_t)(value & UINT64_C(1));
        value >>= 1U;
    }
    return result;
}

static bool clip_header_valid(const EidolonVrmaClip *clip, char *error, size_t error_capacity) {
    if (clip == NULL || clip->version != EIDOLON_VRMA_CLIP_VERSION || clip->tracks == NULL ||
        clip->track_count == 0U || clip->track_count > EIDOLON_HUMANOID_ROLE_COUNT + 1U ||
        (clip->mapped_roles & ~valid_rotation_mask()) != 0U || !isfinite(clip->duration_seconds) ||
        clip->duration_seconds < 0.0F || !isfinite(clip->source_hips_height) ||
        clip->source_hips_height <= VRMA_MOTION_SOURCE_EPSILON) {
        set_error(error, error_capacity, "invalid VRMA normalized-motion source");
        return false;
    }
    for (size_t axis = 0U; axis < 3U; ++axis) {
        if (!isfinite(clip->rest_hips_translation[axis])) {
            set_error(error, error_capacity, "VRMA hips rest translation is not finite");
            return false;
        }
    }
    return true;
}

static bool track_shape_valid(const EidolonVrmaTrack *track) {
    size_t expected_components;
    size_t expected_values_per_key;
    if (track == NULL || track->times == NULL || track->values == NULL || track->key_count == 0U ||
        track->interpolation < EIDOLON_VRMA_INTERPOLATION_STEP ||
        track->interpolation > EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE) {
        return false;
    }
    if (track->path == EIDOLON_VRMA_TRACK_ROTATION) {
        expected_components = 4U;
    } else if (track->path == EIDOLON_VRMA_TRACK_HIPS_TRANSLATION &&
               track->role == EIDOLON_HUMANOID_ROLE_HIPS) {
        expected_components = 3U;
    } else {
        return false;
    }
    expected_values_per_key =
        track->interpolation == EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE ? 3U : 1U;
    if (track->component_count != expected_components ||
        track->values_per_key != expected_values_per_key || !isfinite(track->times[0]) ||
        track->times[0] < 0.0F) {
        return false;
    }
    for (size_t key = 1U; key < track->key_count; ++key) {
        if (!isfinite(track->times[key]) || track->times[key] <= track->times[key - 1U]) {
            return false;
        }
    }
    return true;
}

static bool collect_track_ownership(const EidolonVrmaClip *clip, uint64_t *rotation_mask,
                                    bool *owns_hips_translation, char *error,
                                    size_t error_capacity) {
    uint64_t rotations = 0U;
    bool owns_hips = false;
    if (rotation_mask == NULL || owns_hips_translation == NULL) {
        set_error(error, error_capacity, "invalid VRMA ownership output");
        return false;
    }
    for (size_t index = 0U; index < clip->track_count; ++index) {
        const EidolonVrmaTrack *track = &clip->tracks[index];
        if (!track_shape_valid(track)) {
            set_error(error, error_capacity, "VRMA track %zu has an invalid sample shape", index);
            return false;
        }
        if (track->role < EIDOLON_HUMANOID_ROLE_HIPS ||
            track->role >= EIDOLON_HUMANOID_ROLE_COUNT) {
            set_error(error, error_capacity, "VRMA track has an invalid humanoid role");
            return false;
        }
        const uint64_t role = UINT64_C(1) << (uint32_t)track->role;
        if ((clip->mapped_roles & role) == 0U) {
            set_error(error, error_capacity, "VRMA track role '%s' is not mapped",
                      eidolon_humanoid_role_name(track->role));
            return false;
        }
        if (track->path == EIDOLON_VRMA_TRACK_ROTATION) {
            if ((rotations & role) != 0U) {
                set_error(error, error_capacity, "VRMA has duplicate '%s' rotation tracks",
                          eidolon_humanoid_role_name(track->role));
                return false;
            }
            rotations |= role;
        } else if (track->path == EIDOLON_VRMA_TRACK_HIPS_TRANSLATION &&
                   track->role == EIDOLON_HUMANOID_ROLE_HIPS) {
            if (owns_hips) {
                set_error(error, error_capacity, "VRMA has duplicate hips translation tracks");
                return false;
            }
            owns_hips = true;
        } else {
            set_error(error, error_capacity, "VRMA track for '%s' has an invalid path",
                      eidolon_humanoid_role_name(track->role));
            return false;
        }
    }
    *rotation_mask = rotations;
    *owns_hips_translation = owns_hips;
    return true;
}

bool eidolon_vrma_motion_sample_normalized(const EidolonVrmaClip *clip, float seconds, bool loop,
                                           EidolonHumanoidPose *pose, char *error,
                                           size_t error_capacity) {
    EidolonHumanoidPose authored;
    EidolonHumanoidPose candidate;
    uint64_t rotation_mask = 0U;
    bool owns_hips_translation = false;
    char sample_error[EIDOLON_VRMA_ERROR_CAPACITY];
    if (pose == NULL) {
        set_error(error, error_capacity, "normalized VRMA pose output is missing");
        return false;
    }
    if (!isfinite(seconds) || seconds < 0.0F) {
        set_error(error, error_capacity, "normalized VRMA sample time is invalid");
        return false;
    }
    if (!clip_header_valid(clip, error, error_capacity) ||
        !collect_track_ownership(clip, &rotation_mask, &owns_hips_translation, error,
                                 error_capacity)) {
        return false;
    }
    sample_error[0] = '\0';
    if (!eidolon_vrma_clip_sample(clip, seconds, loop, &authored, sample_error,
                                  sizeof(sample_error))) {
        set_error(error, error_capacity, "%s",
                  sample_error[0] != '\0' ? sample_error : "could not sample VRMA source");
        return false;
    }
    eidolon_humanoid_pose_init(&candidate);
    candidate.rotation_mask = rotation_mask;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if ((rotation_mask & (UINT64_C(1) << (uint32_t)role)) != 0U &&
            !eidolon_humanoid_rest_rotation_to_normalized(
                clip->rest_local_rotations[role], clip->rest_world_rotations[role],
                authored.rotations[role], candidate.rotations[role])) {
            set_error(error, error_capacity, "could not normalize VRMA role '%s'",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role));
            return false;
        }
    }
    candidate.has_hips_translation = owns_hips_translation;
    if (owns_hips_translation && !eidolon_humanoid_rest_translation_to_normalized(
                                     clip->rest_hips_translation, clip->source_hips_height,
                                     authored.hips_translation, candidate.hips_translation)) {
        set_error(error, error_capacity, "could not normalize VRMA hips translation");
        return false;
    }
    if (!eidolon_humanoid_pose_validate(&candidate)) {
        set_error(error, error_capacity, "normalized VRMA pose is invalid");
        return false;
    }
    *pose = candidate;
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

static bool sample_source(const void *context, float seconds, bool loop,
                          EidolonHumanoidPose *pose) {
    return eidolon_vrma_motion_sample_normalized(context, seconds, loop, pose, NULL, 0U);
}

bool eidolon_vrma_motion_source_build(const EidolonVrmaClip *clip, uint64_t source_identity,
                                      bool loop, EidolonEprMotionSource *source, char *error,
                                      size_t error_capacity) {
    EidolonEprMotionSource candidate;
    EidolonVrmaCoverage coverage;
    EidolonHumanoidPose first_pose;
    uint64_t rotation_mask = 0U;
    bool owns_hips_translation = false;
    float track_end = 0.0F;
    if (source == NULL) {
        set_error(error, error_capacity, "VRMA motion source output is missing");
        return false;
    }
    if (source_identity == 0U) {
        set_error(error, error_capacity, "VRMA motion source identity must be nonzero");
        return false;
    }
    if (!clip_header_valid(clip, error, error_capacity)) {
        return false;
    }
    if ((clip->mapped_roles & required_rotation_mask()) != required_rotation_mask()) {
        set_error(error, error_capacity, "VRMA motion source is missing required humanoid roles");
        return false;
    }
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        float normalized[4];
        if ((clip->mapped_roles & (UINT64_C(1) << (uint32_t)role)) != 0U &&
            !eidolon_humanoid_rest_rotation_to_normalized(
                clip->rest_local_rotations[role], clip->rest_world_rotations[role],
                clip->rest_local_rotations[role], normalized)) {
            set_error(error, error_capacity, "VRMA role '%s' has an invalid authored rest frame",
                      eidolon_humanoid_role_name((EidolonHumanoidRole)role));
            return false;
        }
    }
    if (!eidolon_vrma_clip_coverage(clip, &coverage)) {
        set_error(error, error_capacity, "VRMA motion-source coverage is invalid");
        return false;
    }
    if (!collect_track_ownership(clip, &rotation_mask, &owns_hips_translation, error,
                                 error_capacity)) {
        return false;
    }
    if (coverage.rotation_roles != rotation_mask ||
        coverage.has_hips_translation != owns_hips_translation ||
        coverage.rotation_track_count != bit_count(rotation_mask) ||
        clip->track_count !=
            coverage.rotation_track_count + (coverage.has_hips_translation ? 1U : 0U)) {
        set_error(error, error_capacity, "VRMA motion-source ownership is inconsistent");
        return false;
    }
    for (size_t index = 0U; index < clip->track_count; ++index) {
        const EidolonVrmaTrack *track = &clip->tracks[index];
        const float end = track->times[track->key_count - 1U];
        if (end > track_end) {
            track_end = end;
        }
    }
    if (fabsf(track_end - clip->duration_seconds) > VRMA_MOTION_SOURCE_EPSILON) {
        set_error(error, error_capacity, "VRMA motion-source duration disagrees with its tracks");
        return false;
    }
    if (!eidolon_vrma_motion_sample_normalized(clip, 0.0F, loop, &first_pose, error,
                                               error_capacity)) {
        return false;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_EPR_MOTION_SOURCE_VERSION;
    candidate.identity = source_identity;
    candidate.context = clip;
    candidate.sample = sample_source;
    candidate.rotation_mask = rotation_mask;
    candidate.duration_seconds = clip->duration_seconds;
    candidate.owns_hips_translation = owns_hips_translation;
    candidate.loop = loop;
    *source = candidate;
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}
