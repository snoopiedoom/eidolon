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
    return t * t * t * (10.0F + t * (-15.0F + 6.0F * t));
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

static bool anchor_owns(const EidolonEprPoseAnchor *anchor, EidolonEprBodyResource resource) {
    return anchor != NULL && (anchor->resource_mask & (UINT32_C(1) << (uint32_t)resource)) != 0U;
}

static void anchor_euler(const EidolonEprPoseAnchor *neutral, const EidolonEprPoseAnchor *target,
                         EidolonEprBodyResource resource, float result[3]) {
    const float *source = NULL;
    if (anchor_owns(target, resource)) {
        source = resource == EIDOLON_EPR_RESOURCE_TORSO ? target->torso_euler : target->head_euler;
    } else if (anchor_owns(neutral, resource)) {
        source =
            resource == EIDOLON_EPR_RESOURCE_TORSO ? neutral->torso_euler : neutral->head_euler;
    }
    if (source != NULL) {
        memcpy(result, source, sizeof(float) * 3U);
    } else {
        memset(result, 0, sizeof(float) * 3U);
    }
}

static void anchor_arm(const EidolonEprBodyProfile *body, const EidolonEprPoseAnchor *neutral,
                       const EidolonEprPoseAnchor *target, float hand[3], float pole[3],
                       float wrist[3]) {
    float neutral_hand[3];
    float neutral_pole[3];
    body_point(body, neutral->right_arm.hand_target, neutral_hand);
    body_point(body, neutral->right_arm.elbow_pole, neutral_pole);
    memcpy(hand, neutral_hand, sizeof(neutral_hand));
    memcpy(pole, neutral_pole, sizeof(neutral_pole));
    memcpy(wrist, neutral->right_arm.wrist_euler, sizeof(float) * 3U);
    if (target != NULL && target != neutral &&
        anchor_owns(target, EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN)) {
        float target_hand[3];
        float target_pole[3];
        const float weight = clamp01(target->right_arm.weight);
        body_point(body, target->right_arm.hand_target, target_hand);
        body_point(body, target->right_arm.elbow_pole, target_pole);
        mix3(neutral_hand, target_hand, weight, hand);
        mix3(neutral_pole, target_pole, weight, pole);
        mix3(neutral->right_arm.wrist_euler, target->right_arm.wrist_euler, weight, wrist);
    }
}

static void
posture_anchor_weights(const EidolonEprPoseAnchor *neutral, const EidolonEprPoseAnchor *target,
                       EidolonEprPoseAnchorId target_id,
                       float weights[EIDOLON_EPR_POSE_ANCHOR_COUNT][EIDOLON_EPR_RESOURCE_COUNT]) {
    memset(weights, 0, sizeof(float) * EIDOLON_EPR_POSE_ANCHOR_COUNT * EIDOLON_EPR_RESOURCE_COUNT);
    const EidolonEprBodyResource resources[] = {
        EIDOLON_EPR_RESOURCE_TORSO,
        EIDOLON_EPR_RESOURCE_HEAD,
        EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN,
    };
    for (size_t index = 0U; index < sizeof(resources) / sizeof(resources[0]); ++index) {
        const EidolonEprBodyResource resource = resources[index];
        if (target != neutral && anchor_owns(target, resource)) {
            float target_weight = 1.0F;
            if (resource == EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN) {
                target_weight = clamp01(target->right_arm.weight);
                weights[EIDOLON_EPR_POSE_NEUTRAL][resource] = 1.0F - target_weight;
            }
            weights[target_id][resource] += target_weight;
        } else if (anchor_owns(neutral, resource)) {
            weights[EIDOLON_EPR_POSE_NEUTRAL][resource] = 1.0F;
        }
    }
}

void eidolon_epr_realize_posture(const EidolonEprBodyProfile *body,
                                 const EidolonEprRealizationProfile *profile,
                                 const EidolonRealizationProgram *program,
                                 const EidolonCanonicalControl *transition_start,
                                 EidolonEprTick tick, EidolonCanonicalControl *candidate) {
    const EidolonEprPoseAnchor *neutral =
        eidolon_epr_realization_anchor(profile, EIDOLON_EPR_POSE_NEUTRAL);
    const EidolonEprPoseAnchor *target = neutral;
    float torso[3];
    float head[3];
    float hand[3];
    float pole[3];
    float wrist[3];
    float target_weights[EIDOLON_EPR_POSE_ANCHOR_COUNT][EIDOLON_EPR_RESOURCE_COUNT];
    EidolonEprPoseAnchorId target_id = EIDOLON_EPR_POSE_NEUTRAL;
    float weight = 1.0F;
    if (neutral == NULL) {
        return;
    }
    if (program != NULL && program->modality == EIDOLON_EPR_MODALITY_POSTURE &&
        program->pose_count == 2U) {
        const EidolonEprTick onset = program->has_phase[EIDOLON_EPR_PHASE_ONSET]
                                         ? program->phase_ticks[EIDOLON_EPR_PHASE_ONSET]
                                         : tick;
        target = &program->poses[1];
        target_id = program->pose_ids[1];
        weight = smooth01((float)(tick - onset) / program->values[0]);
    }
    anchor_euler(neutral, target, EIDOLON_EPR_RESOURCE_TORSO, torso);
    anchor_euler(neutral, target, EIDOLON_EPR_RESOURCE_HEAD, head);
    anchor_arm(body, neutral, target, hand, pole, wrist);
    posture_anchor_weights(neutral, target, target_id, target_weights);
    if (program != NULL && transition_start != NULL && transition_start->valid) {
        const float start_torso[3] = {
            transition_start->torso_pitch,
            transition_start->torso_yaw,
            transition_start->torso_roll,
        };
        const float start_head[3] = {
            transition_start->head_pitch,
            transition_start->head_yaw,
            transition_start->head_roll,
        };
        mix3(start_torso, torso, weight, torso);
        mix3(start_head, head, weight, head);
        mix3(transition_start->right_hand_target, hand, weight, hand);
        mix3(transition_start->right_elbow_pole, pole, weight, pole);
        mix3(transition_start->right_wrist_euler, wrist, weight, wrist);
        for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
            for (size_t resource = 0U; resource < EIDOLON_EPR_RESOURCE_COUNT; ++resource) {
                candidate->pose_anchor_resource_weights[anchor][resource] =
                    mixf(transition_start->pose_anchor_resource_weights[anchor][resource],
                         target_weights[anchor][resource], weight);
            }
        }
    } else {
        memcpy(candidate->pose_anchor_resource_weights, target_weights,
               sizeof(candidate->pose_anchor_resource_weights));
    }
    candidate->torso_pitch = torso[0];
    candidate->torso_yaw = torso[1];
    candidate->torso_roll = torso[2];
    candidate->head_pitch = head[0];
    candidate->head_yaw = head[1];
    candidate->head_roll = head[2];
    memcpy(candidate->right_hand_target, hand, sizeof(hand));
    memcpy(candidate->right_elbow_pole, pole, sizeof(pole));
    memcpy(candidate->right_wrist_euler, wrist, sizeof(wrist));
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
    candidate->head_gaze_weight = head_weight;
    candidate->head_gaze_yaw = program->values[0] * head_weight * 0.72F;
    candidate->head_gaze_pitch = program->values[1] * head_weight * 0.50F;
    candidate->head_yaw += candidate->head_gaze_yaw;
    candidate->head_pitch += candidate->head_gaze_pitch;
    return body->has_eyes;
}

static void realize_gesture(const EidolonEprBodyProfile *body,
                            const EidolonRealizationProgram *program, EidolonEprTick tick,
                            EidolonCanonicalControl *candidate) {
    float hands[3][3];
    float poles[3][3];
    float wrists[3][3];
    const float rest_hand[3] = {
        candidate->right_hand_target[0],
        candidate->right_hand_target[1],
        candidate->right_hand_target[2],
    };
    const float rest_pole[3] = {
        candidate->right_elbow_pole[0],
        candidate->right_elbow_pole[1],
        candidate->right_elbow_pole[2],
    };
    const float rest_wrist[3] = {
        candidate->right_wrist_euler[0],
        candidate->right_wrist_euler[1],
        candidate->right_wrist_euler[2],
    };
    float rest_weights[EIDOLON_EPR_POSE_ANCHOR_COUNT];
    const EidolonEprTick preparation = program->phase_ticks[EIDOLON_EPR_PHASE_PREPARATION];
    const EidolonEprTick onset = program->phase_ticks[EIDOLON_EPR_PHASE_ONSET];
    const EidolonEprTick peak = program->phase_ticks[EIDOLON_EPR_PHASE_PEAK];
    const EidolonEprTick recovery = program->phase_ticks[EIDOLON_EPR_PHASE_RECOVERY];
    const EidolonEprTick completion = program->phase_ticks[EIDOLON_EPR_PHASE_COMPLETION];
    for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
        rest_weights[anchor] =
            candidate->pose_anchor_resource_weights[anchor][EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN];
    }
    for (size_t index = 0; index < 3U; ++index) {
        const EidolonEprPoseAnchor *anchor = &program->poses[index];
        float target_hand[3];
        float target_pole[3];
        const float weight = clamp01(anchor->right_arm.weight);
        body_point(body, anchor->right_arm.hand_target, target_hand);
        body_point(body, anchor->right_arm.elbow_pole, target_pole);
        mix3(rest_hand, target_hand, weight, hands[index]);
        mix3(rest_pole, target_pole, weight, poles[index]);
        mix3(rest_wrist, anchor->right_arm.wrist_euler, weight, wrists[index]);
    }
    if (tick < preparation) {
        return;
    }
    size_t from = 0U;
    size_t to = 0U;
    float phase_weight = 0.0F;
    bool from_rest = false;
    bool to_rest = false;
    if (tick < onset) {
        phase_weight = smooth01((float)(tick - preparation) / (float)(onset - preparation));
        from_rest = true;
        mix3(rest_hand, hands[0], phase_weight, candidate->right_hand_target);
        mix3(rest_pole, poles[0], phase_weight, candidate->right_elbow_pole);
        mix3(rest_wrist, wrists[0], phase_weight, candidate->right_wrist_euler);
    } else if (tick < peak) {
        from = 0U;
        to = 1U;
        phase_weight = smooth01((float)(tick - onset) / (float)(peak - onset));
        mix3(hands[0], hands[1], phase_weight, candidate->right_hand_target);
        mix3(poles[0], poles[1], phase_weight, candidate->right_elbow_pole);
        mix3(wrists[0], wrists[1], phase_weight, candidate->right_wrist_euler);
    } else if (tick < recovery) {
        from = 1U;
        to = 2U;
        phase_weight = smooth01((float)(tick - peak) / (float)(recovery - peak));
        mix3(hands[1], hands[2], phase_weight, candidate->right_hand_target);
        mix3(poles[1], poles[2], phase_weight, candidate->right_elbow_pole);
        mix3(wrists[1], wrists[2], phase_weight, candidate->right_wrist_euler);
    } else {
        from = 2U;
        phase_weight = smooth01((float)(tick - recovery) / (float)(completion - recovery));
        to_rest = true;
        mix3(hands[2], rest_hand, phase_weight, candidate->right_hand_target);
        mix3(poles[2], rest_pole, phase_weight, candidate->right_elbow_pole);
        mix3(wrists[2], rest_wrist, phase_weight, candidate->right_wrist_euler);
    }
    const float from_strength = from_rest ? 0.0F : clamp01(program->poses[from].right_arm.weight);
    const float to_strength = to_rest ? 0.0F : clamp01(program->poses[to].right_arm.weight);
    const float rest_strength =
        (1.0F - phase_weight) * (1.0F - from_strength) + phase_weight * (1.0F - to_strength);
    for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
        candidate->pose_anchor_resource_weights[anchor][EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN] =
            rest_weights[anchor] * rest_strength;
    }
    if (!from_rest) {
        candidate->pose_anchor_resource_weights[program->pose_ids[from]]
                                               [EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN] +=
            (1.0F - phase_weight) * from_strength;
    }
    if (!to_rest) {
        candidate->pose_anchor_resource_weights[program->pose_ids[to]]
                                               [EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN] +=
            phase_weight * to_strength;
    }
}

void eidolon_epr_realize_right_arm(const EidolonEprBodyProfile *body,
                                   const EidolonRealizationProgram *program, EidolonEprTick tick,
                                   const EidolonCanonicalControl *settle_start,
                                   EidolonCanonicalControl *candidate) {
    if (program == NULL) {
        return;
    }
    if (program->modality == EIDOLON_EPR_MODALITY_GESTURE && program->pose_count == 3U) {
        realize_gesture(body, program, tick, candidate);
    } else if (program->modality == EIDOLON_EPR_MODALITY_SETTLE && settle_start != NULL) {
        const float posture_target[3] = {
            candidate->right_hand_target[0],
            candidate->right_hand_target[1],
            candidate->right_hand_target[2],
        };
        float posture_weights[EIDOLON_EPR_POSE_ANCHOR_COUNT];
        for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
            posture_weights[anchor] =
                candidate
                    ->pose_anchor_resource_weights[anchor][EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN];
        }
        const EidolonEprTick start = program->phase_ticks[EIDOLON_EPR_PHASE_INTERRUPT];
        const EidolonEprTick end = program->phase_ticks[EIDOLON_EPR_PHASE_SETTLE];
        const float weight = smooth01((float)(tick - start) / (float)(end - start));
        candidate->right_arm_ik_weight = 0.0F;
        candidate->right_arm_continuity_weight = 1.0F - weight;
        candidate->right_arm_continuity_id =
            candidate->right_arm_continuity_weight > 0.0F ? program->behavior : 0U;
        mix3(settle_start->right_hand_position, posture_target, weight,
             candidate->right_hand_target);
        mix3(settle_start->right_elbow_pole, candidate->right_elbow_pole, weight,
             candidate->right_elbow_pole);
        for (size_t index = 0; index < 3U; ++index) {
            candidate->right_wrist_euler[index] = mixf(settle_start->right_wrist_euler[index],
                                                       candidate->right_wrist_euler[index], weight);
        }
        for (size_t anchor = 0U; anchor < EIDOLON_EPR_POSE_ANCHOR_COUNT; ++anchor) {
            candidate
                ->pose_anchor_resource_weights[anchor][EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN] = mixf(
                settle_start
                    ->pose_anchor_resource_weights[anchor][EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN],
                posture_weights[anchor], weight);
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
