#include "epr/modality_realizers.h"

#include <math.h>
#include <string.h>

#define EIDOLON_EPR_PI 3.14159265358979323846F

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

static float smooth01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0F - 2.0F * t);
}

static float mixf(float from, float to, float weight) { return from + (to - from) * weight; }

static void mix3(const float from[3], const float to[3], float weight, float result[3]) {
    for (size_t index = 0; index < 3U; ++index) {
        result[index] = mixf(from[index], to[index], weight);
    }
}

static void body_point(const EidolonEprBodyProfile *body, const float semantic[3], float point[3]) {
    const float arm_length = body->right_upper_arm_length + body->right_lower_arm_length;
    for (size_t axis = 0; axis < 3U; ++axis) {
        point[axis] = body->shoulder[axis] + body->right[axis] * semantic[0] * arm_length +
                      body->up[axis] * semantic[1] * arm_length +
                      body->forward[axis] * semantic[2] * arm_length;
    }
}

void eidolon_epr_realize_posture(const EidolonEprBodyProfile *body,
                                 const EidolonRealizationProgram *program, EidolonEprTick tick,
                                 EidolonCanonicalControl *candidate, float right_arm_target[3]) {
    static const float neutral_semantic[3] = {0.45F, -0.75F, 0.08F};
    float neutral_arm[3];
    float target_arm[3];
    float weight = 1.0F;
    body_point(body, neutral_semantic, neutral_arm);
    memcpy(target_arm, neutral_arm, sizeof(target_arm));
    if (program != NULL && program->modality == EIDOLON_EPR_MODALITY_POSTURE) {
        const EidolonEprTick onset = program->has_phase[EIDOLON_EPR_PHASE_ONSET]
                                         ? program->phase_ticks[EIDOLON_EPR_PHASE_ONSET]
                                         : tick;
        weight = smooth01((float)(tick - onset) / program->values[2]);
        body_point(body, program->targets[0], target_arm);
        candidate->torso_pitch = program->values[0] * weight;
        candidate->head_pitch = program->values[1] * weight;
    }
    mix3(neutral_arm, target_arm, weight, right_arm_target);
}

void eidolon_epr_realize_idle(uint64_t seed, const EidolonRealizationProgram *program,
                              EidolonEprTick tick, EidolonCanonicalControl *candidate) {
    float seconds;
    float phase;
    if (program == NULL || program->modality != EIDOLON_EPR_MODALITY_IDLE) {
        return;
    }
    seconds = (float)tick / 1000.0F;
    phase = (float)(seed % UINT64_C(997)) * (2.0F * EIDOLON_EPR_PI / 997.0F);
    candidate->torso_pitch += program->values[0] * sinf(seconds * program->values[1] + phase);
    candidate->torso_roll +=
        program->values[2] * sinf(seconds * program->values[3] + phase * 0.73F);
    candidate->head_roll -= candidate->torso_roll * program->values[4];
}

bool eidolon_epr_realize_gaze(const EidolonEprBodyProfile *body,
                              const EidolonRealizationProgram *program, EidolonEprTick tick,
                              EidolonCanonicalControl *candidate) {
    static const float neutral_target[3] = {0.0F, 1.55F, 1.0F};
    float eye_weight;
    float head_weight;
    EidolonEprTick onset;
    if (program == NULL || program->modality != EIDOLON_EPR_MODALITY_GAZE) {
        memcpy(candidate->gaze_target, neutral_target, sizeof(neutral_target));
        return true;
    }
    onset = program->has_phase[EIDOLON_EPR_PHASE_ONSET]
                ? program->phase_ticks[EIDOLON_EPR_PHASE_ONSET]
                : tick;
    eye_weight = smooth01((float)(tick - onset) / program->values[2]);
    head_weight =
        smooth01((float)(tick - onset - (EidolonEprTick)program->values[3]) / program->values[4]);
    if (!body->has_eyes) {
        eye_weight = 0.0F;
        head_weight = smooth01((float)(tick - onset) / 200.0F);
        candidate->eyes_degraded = true;
    }
    memcpy(candidate->gaze_target, program->targets[0], sizeof(candidate->gaze_target));
    candidate->eye_yaw = program->values[0] * eye_weight;
    candidate->eye_pitch = program->values[1] * eye_weight;
    candidate->eye_weight = eye_weight;
    candidate->head_yaw += program->values[0] * head_weight * 0.72F;
    candidate->head_pitch += program->values[1] * head_weight * 0.50F;
    candidate->head_gaze_weight = head_weight;
    return body->has_eyes;
}

static void realize_gesture(const EidolonEprBodyProfile *body,
                            const EidolonRealizationProgram *program, EidolonEprTick tick,
                            const float rest[3], float target[3], float wrist[3]) {
    float points[3][3];
    const EidolonEprTick preparation = program->phase_ticks[EIDOLON_EPR_PHASE_PREPARATION];
    const EidolonEprTick onset = program->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
    const EidolonEprTick peak = program->phase_ticks[EIDOLON_EPR_PHASE_PEAK];
    const EidolonEprTick recovery = program->phase_ticks[EIDOLON_EPR_PHASE_RECOVERY];
    const EidolonEprTick completion = program->phase_ticks[EIDOLON_EPR_PHASE_COMPLETION];
    for (size_t index = 0; index < 3U; ++index) {
        body_point(body, program->targets[index], points[index]);
    }
    if (tick < preparation) {
        memcpy(target, rest, sizeof(float) * 3U);
        return;
    }
    if (tick < onset) {
        mix3(rest, points[0], smooth01((float)(tick - preparation) / (float)(onset - preparation)),
             target);
    } else if (tick < peak) {
        mix3(points[0], points[1], smooth01((float)(tick - onset) / (float)(peak - onset)), target);
    } else if (tick < recovery) {
        mix3(points[1], points[2], smooth01((float)(tick - peak) / (float)(recovery - peak)),
             target);
    } else {
        mix3(points[2], rest, smooth01((float)(tick - recovery) / (float)(completion - recovery)),
             target);
    }
    wrist[0] = program->values[0] * smooth01((float)(tick - onset) / (float)(peak - onset));
    wrist[1] = program->values[1] * smooth01((float)(tick - onset) / (float)(peak - onset));
}

void eidolon_epr_realize_right_arm(const EidolonEprBodyProfile *body,
                                   const EidolonRealizationProgram *program, EidolonEprTick tick,
                                   const EidolonCanonicalControl *settle_start,
                                   const float posture_target[3],
                                   EidolonCanonicalControl *candidate) {
    static const float pole_semantic[3] = {0.50F, 0.05F, -0.45F};
    body_point(body, pole_semantic, candidate->right_elbow_pole);
    memcpy(candidate->right_hand_target, posture_target, sizeof(float) * 3U);
    if (program == NULL) {
        return;
    }
    if (program->modality == EIDOLON_EPR_MODALITY_GESTURE) {
        realize_gesture(body, program, tick, posture_target, candidate->right_hand_target,
                        candidate->right_wrist_euler);
    } else if (program->modality == EIDOLON_EPR_MODALITY_SETTLE && settle_start != NULL) {
        const EidolonEprTick start = program->phase_ticks[EIDOLON_EPR_PHASE_INTERRUPT];
        const EidolonEprTick end = program->phase_ticks[EIDOLON_EPR_PHASE_SETTLE];
        const float weight = smooth01((float)(tick - start) / (float)(end - start));
        mix3(settle_start->right_hand_position, posture_target, weight,
             candidate->right_hand_target);
        for (size_t index = 0; index < 3U; ++index) {
            candidate->right_wrist_euler[index] =
                mixf(settle_start->right_wrist_euler[index], 0.0F, weight);
        }
    }
}

bool eidolon_epr_realize_expression(const EidolonEprBodyProfile *body,
                                    const EidolonRealizationProgram *program,
                                    EidolonCanonicalControl *candidate) {
    if (program == NULL || program->modality != EIDOLON_EPR_MODALITY_EXPRESSION) {
        return true;
    }
    if (!body->has_expression) {
        candidate->expression_degraded = true;
        return false;
    }
    candidate->focused_expression_weight = program->values[0];
    return true;
}
