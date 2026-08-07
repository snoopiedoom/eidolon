#include "vrm_calibration.h"
#include "vrm_calibration_session.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void set_node(cgltf_node *nodes, size_t node, int parent, float x, float y, float z) {
    nodes[node].has_translation = 1;
    nodes[node].translation[0] = x;
    nodes[node].translation[1] = y;
    nodes[node].translation[2] = z;
    nodes[node].rotation[3] = 1.0F;
    nodes[node].scale[0] = 1.0F;
    nodes[node].scale[1] = 1.0F;
    nodes[node].scale[2] = 1.0F;
    nodes[node].parent = parent >= 0 ? &nodes[(size_t)parent] : NULL;
}

static void build_measurement_fixture(cgltf_data *data, cgltf_node nodes[EIDOLON_VRM_BONE_COUNT],
                                      EidolonVrmBody *body) {
    memset(data, 0, sizeof(*data));
    memset(nodes, 0, sizeof(cgltf_node) * EIDOLON_VRM_BONE_COUNT);
    memset(body, 0, sizeof(*body));
    data->nodes = nodes;
    data->nodes_count = EIDOLON_VRM_BONE_COUNT;
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        body->node_by_bone[bone] = (int)bone;
    }

    set_node(nodes, EIDOLON_VRM_BONE_HIPS, -1, 0.0F, 1.0F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_SPINE, EIDOLON_VRM_BONE_HIPS, 0.0F, 0.25F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_CHEST, EIDOLON_VRM_BONE_SPINE, 0.0F, 0.25F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_UPPER_CHEST, EIDOLON_VRM_BONE_CHEST, 0.0F, 0.20F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_NECK, EIDOLON_VRM_BONE_UPPER_CHEST, 0.0F, 0.15F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_HEAD, EIDOLON_VRM_BONE_NECK, 0.0F, 0.20F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_LEFT_EYE, EIDOLON_VRM_BONE_HEAD, -0.03F, 0.05F, 0.05F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_EYE, EIDOLON_VRM_BONE_HEAD, 0.03F, 0.05F, 0.05F);

    set_node(nodes, EIDOLON_VRM_BONE_LEFT_UPPER_LEG, EIDOLON_VRM_BONE_HIPS, -0.12F, 0.0F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_LEFT_LOWER_LEG, EIDOLON_VRM_BONE_LEFT_UPPER_LEG, 0.0F, -0.45F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_LEFT_FOOT, EIDOLON_VRM_BONE_LEFT_LOWER_LEG, 0.0F, -0.45F,
             0.05F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_UPPER_LEG, EIDOLON_VRM_BONE_HIPS, 0.12F, 0.0F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_LOWER_LEG, EIDOLON_VRM_BONE_RIGHT_UPPER_LEG, 0.0F,
             -0.45F, 0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_FOOT, EIDOLON_VRM_BONE_RIGHT_LOWER_LEG, 0.0F, -0.45F,
             0.05F);

    set_node(nodes, EIDOLON_VRM_BONE_LEFT_SHOULDER, EIDOLON_VRM_BONE_UPPER_CHEST, -0.20F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_LEFT_UPPER_ARM, EIDOLON_VRM_BONE_LEFT_SHOULDER, -0.10F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_LEFT_LOWER_ARM, EIDOLON_VRM_BONE_LEFT_UPPER_ARM, -0.30F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_LEFT_HAND, EIDOLON_VRM_BONE_LEFT_LOWER_ARM, -0.28F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_SHOULDER, EIDOLON_VRM_BONE_UPPER_CHEST, 0.20F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_UPPER_ARM, EIDOLON_VRM_BONE_RIGHT_SHOULDER, 0.10F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_LOWER_ARM, EIDOLON_VRM_BONE_RIGHT_UPPER_ARM, 0.30F, 0.0F,
             0.0F);
    set_node(nodes, EIDOLON_VRM_BONE_RIGHT_HAND, EIDOLON_VRM_BONE_RIGHT_LOWER_ARM, 0.28F, 0.0F,
             0.0F);
}

static EidolonVrmMeasurements measured_fixture(void) {
    cgltf_data data;
    cgltf_node nodes[EIDOLON_VRM_BONE_COUNT];
    EidolonVrmBody body;
    EidolonVrmMeasurements measurements;
    char error[256];
    build_measurement_fixture(&data, nodes, &body);
    if (!eidolon_vrm_measure(&data, &body, &measurements, error, sizeof(error))) {
        fprintf(stderr, "measurement fixture failed: %s\n", error);
        assert(false);
    }
    assert(measurements.version == EIDOLON_VRM_MEASUREMENTS_VERSION);
    assert(measurements.anatomy_fingerprint != 0U);
    assert(fabsf(measurements.shoulder_width - 0.60F) < 0.0001F);
    assert(fabsf(measurements.upper_arm_length[EIDOLON_VRM_CALIBRATION_LEFT] - 0.30F) < 0.0001F);
    assert(fabsf(measurements.lower_arm_length[EIDOLON_VRM_CALIBRATION_RIGHT] - 0.28F) < 0.0001F);
    assert(fabsf(measurements.upper_leg_length[EIDOLON_VRM_CALIBRATION_LEFT] - 0.45F) < 0.0001F);
    assert(measurements.skeleton_height > 1.7F);
    assert(measurements.right[0] > 0.99F);
    assert(measurements.up[1] > 0.99F);
    return measurements;
}

static void test_measurements_fingerprint_anatomy(void) {
    const EidolonVrmMeasurements first = measured_fixture();
    cgltf_data data;
    cgltf_node nodes[EIDOLON_VRM_BONE_COUNT];
    EidolonVrmBody body;
    EidolonVrmMeasurements changed;
    char error[256];
    build_measurement_fixture(&data, nodes, &body);
    nodes[EIDOLON_VRM_BONE_RIGHT_HAND].translation[0] = 0.31F;
    assert(eidolon_vrm_measure(&data, &body, &changed, error, sizeof(error)));
    assert(first.anatomy_fingerprint != changed.anatomy_fingerprint);
}

static void test_partial_sidecar_is_transactional(void) {
    const EidolonVrmMeasurements measurements = measured_fixture();
    char text[4096];
    const int written = snprintf(text, sizeof(text),
                                 "version = 1\n"
                                 "anatomy_fingerprint = 0x%llx\n"
                                 "anchor.neutral.resources = torso,head,left_arm,right_arm\n"
                                 "anchor.neutral.torso = 0, 0, 0\n"
                                 "anchor.neutral.head = 0, 0, 0\n"
                                 "anchor.neutral.left.hand = 0.05, -0.80, 0.10\n"
                                 "anchor.neutral.left.pole = 0.30, -0.35, -0.20\n"
                                 "anchor.neutral.left.wrist = 0, 0, 0\n"
                                 "anchor.neutral.left.weight = 1\n"
                                 "anchor.neutral.right.hand = 0.05, -0.80, 0.10\n"
                                 "anchor.neutral.right.pole = 0.30, -0.35, -0.20\n"
                                 "anchor.neutral.right.wrist = 0, 0, 0\n"
                                 "anchor.neutral.right.weight = 1\n"
                                 "anchor.neutral.residual.rightHand = 0, 0, 0, 1\n",
                                 (unsigned long long)measurements.anatomy_fingerprint);
    assert(written > 0 && (size_t)written < sizeof(text));

    EidolonVrmCalibration calibration;
    char error[256];
    assert(eidolon_vrm_calibration_parse(text, (size_t)written, &measurements, &calibration, error,
                                         sizeof(error)));
    assert(calibration.anchor_mask == (UINT32_C(1) << EIDOLON_VRM_CALIBRATION_NEUTRAL));
    const EidolonVrmCalibrationAnchor *neutral =
        eidolon_vrm_calibration_anchor(&calibration, EIDOLON_VRM_CALIBRATION_NEUTRAL);
    assert(neutral != NULL);
    assert(neutral->resource_mask & (UINT32_C(1) << EIDOLON_EPR_RESOURCE_LEFT_ARM_CHAIN));
    assert(neutral->residual_bone_mask & (UINT32_C(1) << EIDOLON_VRM_BONE_RIGHT_HAND));
    assert(eidolon_vrm_calibration_anchor(&calibration, EIDOLON_VRM_CALIBRATION_ATTENTIVE) == NULL);

    EidolonVrmMeasurements stale = measurements;
    stale.anatomy_fingerprint += 1U;
    EidolonVrmCalibration unchanged = calibration;
    assert(!eidolon_vrm_calibration_parse(text, (size_t)written, &stale, &unchanged, error,
                                          sizeof(error)));
    assert(strstr(error, "fingerprint") != NULL);
    assert(unchanged.anatomy_fingerprint == calibration.anatomy_fingerprint);

    char serialized[EIDOLON_VRM_CALIBRATION_TEXT_CAPACITY];
    size_t serialized_size = 0U;
    assert(eidolon_vrm_calibration_serialize(&calibration, &measurements, serialized,
                                             sizeof(serialized), &serialized_size, error,
                                             sizeof(error)));
    EidolonVrmCalibration round_trip;
    assert(eidolon_vrm_calibration_parse(serialized, serialized_size, &measurements, &round_trip,
                                         error, sizeof(error)));
    char repeated[EIDOLON_VRM_CALIBRATION_TEXT_CAPACITY];
    size_t repeated_size = 0U;
    assert(eidolon_vrm_calibration_serialize(&round_trip, &measurements, repeated, sizeof(repeated),
                                             &repeated_size, error, sizeof(error)));
    assert(serialized_size == repeated_size);
    assert(memcmp(serialized, repeated, serialized_size) == 0);
    assert(!eidolon_vrm_calibration_serialize(&calibration, &measurements, repeated, 8U,
                                              &repeated_size, error, sizeof(error)));

    const char *path = "build/windows/tests/debug/vrm-calibration-roundtrip.epr-calibration";
    (void)SDL_RemovePath(path);
    assert(eidolon_vrm_calibration_save(&calibration, &measurements, path, error, sizeof(error)));
    assert(eidolon_vrm_calibration_save(&round_trip, &measurements, path, error, sizeof(error)));
    size_t loaded_size = 0U;
    void *loaded = SDL_LoadFile(path, &loaded_size);
    assert(loaded != NULL);
    EidolonVrmCalibration loaded_calibration;
    assert(eidolon_vrm_calibration_parse(loaded, loaded_size, &measurements, &loaded_calibration,
                                         error, sizeof(error)));
    SDL_free(loaded);
    assert(loaded_calibration.anchor_mask == calibration.anchor_mask);
    assert(SDL_RemovePath(path));
}

static void test_invalid_anchor_does_not_commit(void) {
    const EidolonVrmMeasurements measurements = measured_fixture();
    EidolonVrmCalibration calibration;
    eidolon_vrm_calibration_init(&calibration, measurements.anatomy_fingerprint);
    EidolonVrmCalibrationAnchor invalid;
    memset(&invalid, 0, sizeof(invalid));
    invalid.resource_mask = UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN;
    invalid.arms[EIDOLON_VRM_CALIBRATION_RIGHT].weight = 2.0F;
    char error[256];
    assert(!eidolon_vrm_calibration_set_anchor(&calibration, EIDOLON_VRM_CALIBRATION_ATTENTIVE,
                                               &invalid, error, sizeof(error)));
    assert(calibration.anchor_mask == 0U);
}

static EidolonEprBodyProfile body_profile(const EidolonVrmMeasurements *measurements) {
    EidolonEprBodyProfile body;
    memset(&body, 0, sizeof(body));
    body.version = EIDOLON_EPR_BODY_PROFILE_VERSION;
    body.fingerprint = measurements->anatomy_fingerprint;
    memcpy(body.shoulder, measurements->bones[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM].bind_position,
           sizeof(body.shoulder));
    memcpy(body.head, measurements->bones[EIDOLON_VRM_BONE_HEAD].bind_position, sizeof(body.head));
    memcpy(body.right, measurements->right, sizeof(body.right));
    memcpy(body.up, measurements->up, sizeof(body.up));
    memcpy(body.forward, measurements->forward, sizeof(body.forward));
    body.right_upper_arm_length = measurements->upper_arm_length[EIDOLON_VRM_CALIBRATION_RIGHT];
    body.right_lower_arm_length = measurements->lower_arm_length[EIDOLON_VRM_CALIBRATION_RIGHT];
    body.maximum_reach_ratio = 0.1F;
    body.shoulder_limit_radians = 1.0F;
    body.elbow_limit_radians = 2.8F;
    body.has_required_humanoid = true;
    body.has_right_arm = true;
    return body;
}

static EidolonVrmCalibrationAnchor test_anchor(uint32_t resources, float torso_pitch,
                                               float head_yaw, float hand_outward) {
    EidolonVrmCalibrationAnchor anchor;
    memset(&anchor, 0, sizeof(anchor));
    anchor.resource_mask = resources;
    anchor.torso_euler[0] = torso_pitch;
    anchor.head_euler[1] = head_yaw;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].hand_target[0] = hand_outward;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].hand_target[1] = -0.70F;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].hand_target[2] = 0.10F;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].elbow_pole[0] = 0.50F;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].elbow_pole[1] = 0.05F;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].elbow_pole[2] = -0.45F;
    anchor.arms[EIDOLON_VRM_CALIBRATION_RIGHT].weight = 1.0F;
    anchor.calibrated = true;
    return anchor;
}

static void test_calibration_compiles_transactionally_to_epr_profile(void) {
    const EidolonVrmMeasurements measurements = measured_fixture();
    EidolonEprBodyProfile body = body_profile(&measurements);
    EidolonVrmCalibration calibration;
    EidolonEprRealizationProfile realization;
    EidolonEprRealizationProfile unchanged;
    char error[256];
    const uint32_t resources = (UINT32_C(1) << EIDOLON_EPR_RESOURCE_TORSO) |
                               (UINT32_C(1) << EIDOLON_EPR_RESOURCE_HEAD) |
                               (UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN);
    eidolon_vrm_calibration_init(&calibration, measurements.anatomy_fingerprint);
    memset(&realization, 0x5a, sizeof(realization));
    unchanged = realization;
    assert(!eidolon_vrm_calibration_compile_realization(
        &calibration, &measurements, &body, &realization, error, sizeof(error)));
    assert(memcmp(&realization, &unchanged, sizeof(realization)) == 0);

    EidolonVrmCalibrationAnchor neutral = test_anchor(resources, 0.01F, -0.02F, 0.40F);
    EidolonVrmCalibrationAnchor attentive = test_anchor(resources, 0.08F, 0.12F, 0.55F);
    assert(eidolon_vrm_calibration_set_anchor(&calibration, EIDOLON_VRM_CALIBRATION_NEUTRAL,
                                              &neutral, error, sizeof(error)));
    assert(eidolon_vrm_calibration_set_anchor(&calibration, EIDOLON_VRM_CALIBRATION_ATTENTIVE,
                                              &attentive, error, sizeof(error)));
    assert(eidolon_vrm_calibration_compile_realization(
        &calibration, &measurements, &body, &realization, error, sizeof(error)));
    assert(realization.version == EIDOLON_EPR_REALIZATION_PROFILE_VERSION);
    assert(realization.body_fingerprint == measurements.anatomy_fingerprint);
    assert(realization.anchor_mask ==
           ((UINT32_C(1) << EIDOLON_EPR_POSE_NEUTRAL) |
            (UINT32_C(1) << EIDOLON_EPR_POSE_ATTENTIVE)));
    assert(fabsf(realization.anchors[EIDOLON_EPR_POSE_ATTENTIVE].torso_euler[0] - 0.08F) <
           0.0001F);
    assert(fabsf(realization.anchors[EIDOLON_EPR_POSE_ATTENTIVE].right_arm.hand_target[0] -
                 0.55F) < 0.0001F);

    unchanged = realization;
    body.fingerprint += 1U;
    assert(!eidolon_vrm_calibration_compile_realization(
        &calibration, &measurements, &body, &realization, error, sizeof(error)));
    assert(memcmp(&realization, &unchanged, sizeof(realization)) == 0);
}

static EidolonCanonicalControl source_control(const EidolonEprBodyProfile *body,
                                              EidolonEprTick tick) {
    EidolonCanonicalControl control;
    memset(&control, 0, sizeof(control));
    control.version = EIDOLON_EPR_CONTROL_VERSION;
    control.revision = 4U;
    control.tick = tick;
    control.right_hand_target[0] = body->shoulder[0] + 0.28F;
    control.right_hand_target[1] = body->shoulder[1] - 0.28F;
    control.right_hand_target[2] = body->shoulder[2] + 0.08F;
    control.right_elbow_pole[0] = body->shoulder[0] + 0.20F;
    control.right_elbow_pole[1] = body->shoulder[1] - 0.10F;
    control.right_elbow_pole[2] = body->shoulder[2] - 0.30F;
    control.valid = true;
    return control;
}

static void test_calibration_session_is_model_relative_and_transactional(void) {
    const EidolonVrmMeasurements measurements = measured_fixture();
    const EidolonEprBodyProfile body = body_profile(&measurements);
    EidolonVrmCalibration calibration;
    eidolon_vrm_calibration_init(&calibration, measurements.anatomy_fingerprint);
    EidolonVrmCalibrationSession session;
    assert(eidolon_vrm_calibration_session_init(&session, &measurements, &body, &calibration,
                                                "unused.epr-calibration", 10U));
    const EidolonCanonicalControl source = source_control(
        &body, eidolon_vrm_calibration_anchor_tick(EIDOLON_VRM_CALIBRATION_ATTENTIVE));
    assert(eidolon_vrm_calibration_session_select(&session, EIDOLON_VRM_CALIBRATION_ATTENTIVE,
                                                  &source));
    assert(!session.dirty);
    assert(!session.source_was_calibrated);
    EidolonCanonicalControl projected;
    assert(eidolon_vrm_calibration_session_make_control(&session, &projected));
    assert(projected.revision == 11U);
    assert(fabsf(projected.right_hand_target[0] - source.right_hand_target[0]) < 0.0001F);
    session.draft.arms[EIDOLON_VRM_CALIBRATION_RIGHT].hand_target[1] += 0.1F;
    session.dirty = true;
    assert(eidolon_vrm_calibration_session_make_control(&session, &projected));
    assert(projected.right_hand_target[1] > source.right_hand_target[1]);
    assert(eidolon_vrm_calibration_session_commit(&session));
    assert((session.working.anchor_mask & (UINT32_C(1) << EIDOLON_VRM_CALIBRATION_ATTENTIVE)) !=
           0U);
    assert(!session.dirty);

    const EidolonCanonicalControl contrast = source_control(
        &body, eidolon_vrm_calibration_anchor_tick(EIDOLON_VRM_CALIBRATION_CONTRAST_PEAK));
    assert(eidolon_vrm_calibration_session_select(&session, EIDOLON_VRM_CALIBRATION_CONTRAST_PEAK,
                                                  &contrast));
    assert(session.draft.resource_mask == (UINT32_C(1) << EIDOLON_EPR_RESOURCE_RIGHT_ARM_CHAIN));
    eidolon_vrm_calibration_session_revert(&session);
    assert(!session.dirty);
}

int main(void) {
    test_measurements_fingerprint_anatomy();
    test_partial_sidecar_is_transactional();
    test_invalid_anchor_does_not_commit();
    test_calibration_compiles_transactionally_to_epr_profile();
    test_calibration_session_is_model_relative_and_transactional();
    puts("vrm calibration tests passed");
    return 0;
}
