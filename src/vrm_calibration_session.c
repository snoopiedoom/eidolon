#include "vrm_calibration_session.h"

#include "ik.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <string.h>

#define SESSION_EPSILON 0.0001F

static uint32_t resource_bit(EidolonEprBodyResource resource) {
    return UINT32_C(1) << (unsigned int)resource;
}

static uint32_t anchor_resources(EidolonVrmCalibrationAnchorId anchor) {
    if (anchor == EIDOLON_VRM_CALIBRATION_CONTRAST_PREPARATION ||
        anchor == EIDOLON_VRM_CALIBRATION_CONTRAST_PEAK ||
        anchor == EIDOLON_VRM_CALIBRATION_CONTRAST_RECOVERY) {
        return resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    }
    return resource_bit(EIDOLON_EPR_RESOURCE_TORSO) | resource_bit(EIDOLON_EPR_RESOURCE_HEAD) |
           resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
}

EidolonEprTick eidolon_vrm_calibration_anchor_tick(EidolonVrmCalibrationAnchorId anchor) {
    static const EidolonEprTick ticks[EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT] = {
        0, 800, 1400, 2500, 3210, 3520, 3580, 4200,
    };
    if (anchor < EIDOLON_VRM_CALIBRATION_NEUTRAL ||
        anchor >= EIDOLON_VRM_CALIBRATION_ANCHOR_COUNT) {
        return -1;
    }
    return ticks[(size_t)anchor];
}

static float dot3(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static bool finite3(const float value[3]) {
    return isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]);
}

static void copy3(float destination[3], const float source[3]) {
    SDL_memcpy(destination, source, sizeof(float) * 3U);
}

static void point_to_anatomical(const EidolonVrmCalibrationSession *session, const float point[3],
                                float result[3]) {
    const float offset[3] = {
        point[0] - session->body.shoulder[0],
        point[1] - session->body.shoulder[1],
        point[2] - session->body.shoulder[2],
    };
    const float reach = session->body.right_upper_arm_length + session->body.right_lower_arm_length;
    result[0] = dot3(offset, session->measurements.right) / reach;
    result[1] = dot3(offset, session->measurements.up) / reach;
    result[2] = dot3(offset, session->measurements.forward) / reach;
}

static void anatomical_to_point(const EidolonVrmCalibrationSession *session, const float value[3],
                                float result[3]) {
    const float reach = session->body.right_upper_arm_length + session->body.right_lower_arm_length;
    for (size_t axis = 0U; axis < 3U; ++axis) {
        result[axis] =
            session->body.shoulder[axis] + reach * (session->measurements.right[axis] * value[0] +
                                                    session->measurements.up[axis] * value[1] +
                                                    session->measurements.forward[axis] * value[2]);
    }
}

static EidolonVrmCalibrationAnchor capture_anchor(const EidolonVrmCalibrationSession *session,
                                                  EidolonVrmCalibrationAnchorId anchor,
                                                  const EidolonCanonicalControl *source) {
    EidolonVrmCalibrationAnchor result;
    SDL_zero(result);
    result.resource_mask = anchor_resources(anchor);
    result.torso_euler[0] = source->torso_pitch;
    result.torso_euler[1] = source->torso_yaw;
    result.torso_euler[2] = source->torso_roll;
    result.head_euler[0] = source->head_pitch;
    result.head_euler[1] = source->head_yaw;
    result.head_euler[2] = source->head_roll;
    point_to_anatomical(session, source->right_hand_target,
                        result.arms[EIDOLON_VRM_CALIBRATION_RIGHT].hand_target);
    point_to_anatomical(session, source->right_elbow_pole,
                        result.arms[EIDOLON_VRM_CALIBRATION_RIGHT].elbow_pole);
    copy3(result.arms[EIDOLON_VRM_CALIBRATION_RIGHT].wrist_euler, source->right_wrist_euler);
    result.arms[EIDOLON_VRM_CALIBRATION_RIGHT].weight = 1.0F;
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        result.residual_rotation[bone][3] = 1.0F;
    }
    result.calibrated = true;
    return result;
}

bool eidolon_vrm_calibration_session_init(EidolonVrmCalibrationSession *session,
                                          const EidolonVrmMeasurements *measurements,
                                          const EidolonEprBodyProfile *body,
                                          const EidolonVrmCalibration *calibration,
                                          const char *path, uint64_t projection_revision) {
    char error[EIDOLON_VRM_CALIBRATION_ERROR_CAPACITY];
    if (session == NULL || measurements == NULL || body == NULL || calibration == NULL) {
        return SDL_SetError("invalid VRM calibration session input");
    }
    if (path == NULL || path[0] == '\0' ||
        SDL_strlen(path) >= EIDOLON_VRM_CALIBRATION_PATH_CAPACITY) {
        return SDL_SetError("invalid VRM calibration sidecar path");
    }
    if (body->version != EIDOLON_EPR_BODY_PROFILE_VERSION || !body->has_required_humanoid ||
        !body->has_right_arm) {
        return SDL_SetError("VRM calibration requires a complete right-arm body profile");
    }
    if (body->fingerprint != measurements->anatomy_fingerprint) {
        return SDL_SetError("VRM calibration profile fingerprint is inconsistent");
    }
    if (!eidolon_vrm_calibration_validate(calibration, measurements, error, sizeof(error))) {
        return SDL_SetError("VRM calibration input is invalid: %s", error);
    }
    SDL_zero(*session);
    session->measurements = *measurements;
    session->body = *body;
    session->baseline = *calibration;
    session->working = *calibration;
    session->selected_anchor = EIDOLON_VRM_CALIBRATION_NEUTRAL;
    session->next_projection_revision = projection_revision;
    SDL_strlcpy(session->path, path, sizeof(session->path));
    session->active = true;
    session->saved = calibration->anchor_mask != 0U;
    return true;
}

bool eidolon_vrm_calibration_session_select(EidolonVrmCalibrationSession *session,
                                            EidolonVrmCalibrationAnchorId anchor,
                                            const EidolonCanonicalControl *source_control) {
    if (session == NULL || !session->active || source_control == NULL || !source_control->valid ||
        source_control->version != EIDOLON_EPR_CONTROL_VERSION ||
        eidolon_vrm_calibration_anchor_tick(anchor) < 0) {
        return false;
    }
    const EidolonVrmCalibrationAnchor *stored =
        eidolon_vrm_calibration_anchor(&session->working, anchor);
    session->source_control = *source_control;
    session->selected_anchor = anchor;
    session->source_was_calibrated = stored != NULL;
    session->source_draft =
        stored != NULL ? *stored : capture_anchor(session, anchor, source_control);
    session->draft = session->source_draft;
    session->dirty = false;
    session->error[0] = '\0';
    return true;
}

void eidolon_vrm_calibration_session_revert(EidolonVrmCalibrationSession *session) {
    if (session == NULL || !session->active) {
        return;
    }
    session->draft = session->source_draft;
    session->dirty = false;
    session->error[0] = '\0';
}

bool eidolon_vrm_calibration_session_commit(EidolonVrmCalibrationSession *session) {
    if (session == NULL || !session->active) {
        return false;
    }
    if (!eidolon_vrm_calibration_set_anchor(&session->working, session->selected_anchor,
                                            &session->draft, session->error,
                                            sizeof(session->error))) {
        return false;
    }
    session->source_draft = session->draft;
    session->source_was_calibrated = true;
    session->dirty = false;
    session->saved = false;
    return true;
}

bool eidolon_vrm_calibration_session_save(EidolonVrmCalibrationSession *session) {
    if (session == NULL || !session->active ||
        (session->dirty && !eidolon_vrm_calibration_session_commit(session))) {
        return false;
    }
    if (!eidolon_vrm_calibration_save(&session->working, &session->measurements, session->path,
                                      session->error, sizeof(session->error))) {
        return false;
    }
    session->baseline = session->working;
    session->saved = true;
    session->error[0] = '\0';
    return true;
}

bool eidolon_vrm_calibration_session_make_control(EidolonVrmCalibrationSession *session,
                                                  EidolonCanonicalControl *control) {
    if (session == NULL || !session->active || control == NULL) {
        return false;
    }
    EidolonCanonicalControl candidate = session->source_control;
    const EidolonVrmCalibrationAnchor *anchor = &session->draft;
    if ((anchor->resource_mask & resource_bit(EIDOLON_EPR_RESOURCE_TORSO)) != 0U) {
        candidate.torso_pitch = anchor->torso_euler[0];
        candidate.torso_yaw = anchor->torso_euler[1];
        candidate.torso_roll = anchor->torso_euler[2];
    }
    if ((anchor->resource_mask & resource_bit(EIDOLON_EPR_RESOURCE_HEAD)) != 0U) {
        candidate.head_pitch = anchor->head_euler[0];
        candidate.head_yaw = anchor->head_euler[1];
        candidate.head_roll = anchor->head_euler[2];
    }
    if ((anchor->resource_mask & resource_bit(EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN)) != 0U) {
        const EidolonVrmCalibrationArm *arm = &anchor->arms[EIDOLON_VRM_CALIBRATION_RIGHT];
        float target[3];
        float pole[3];
        anatomical_to_point(session, arm->hand_target, target);
        anatomical_to_point(session, arm->elbow_pole, pole);
        const float weight = SDL_clamp(arm->weight, 0.0F, 1.0F);
        for (size_t axis = 0U; axis < 3U; ++axis) {
            candidate.right_hand_target[axis] =
                candidate.right_hand_target[axis] * (1.0F - weight) + target[axis] * weight;
            candidate.right_elbow_pole[axis] =
                candidate.right_elbow_pole[axis] * (1.0F - weight) + pole[axis] * weight;
            candidate.right_wrist_euler[axis] =
                candidate.right_wrist_euler[axis] * (1.0F - weight) +
                arm->wrist_euler[axis] * weight;
        }
        EidolonIkTwoBoneInput input;
        SDL_zero(input);
        copy3(input.root, session->body.shoulder);
        copy3(input.target, candidate.right_hand_target);
        copy3(input.pole, candidate.right_elbow_pole);
        for (size_t axis = 0U; axis < 3U; ++axis) {
            input.fallback_direction[axis] = 0.4F * session->measurements.right[axis] -
                                             0.8F * session->measurements.up[axis] +
                                             0.1F * session->measurements.forward[axis];
        }
        input.upper_length = session->body.right_upper_arm_length;
        input.lower_length = session->body.right_lower_arm_length;
        input.soften_ratio = session->body.maximum_reach_ratio;
        EidolonIkTwoBoneSolution solution;
        if (!eidolon_ik_solve_two_bone(&input, &solution) || !finite3(solution.mid) ||
            !finite3(solution.end)) {
            SDL_strlcpy(session->error, "calibration arm IK failed", sizeof(session->error));
            return false;
        }
        copy3(candidate.right_elbow_position, solution.mid);
        copy3(candidate.right_hand_position, solution.end);
    }
    if (session->next_projection_revision == UINT64_MAX) {
        SDL_strlcpy(session->error, "calibration projection revision exhausted",
                    sizeof(session->error));
        return false;
    }
    candidate.revision = ++session->next_projection_revision;
    candidate.hash = 0U;
    candidate.valid = true;
    session->error[0] = '\0';
    *control = candidate;
    return true;
}
