#include "vrm_playback.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DESTINATION_NODE_COUNT 17U

static void quaternion_axis(float x, float y, float z, float radians, float result[4]) {
    const float sine = sinf(radians * 0.5F);
    result[0] = x * sine;
    result[1] = y * sine;
    result[2] = z * sine;
    result[3] = cosf(radians * 0.5F);
}

static bool quaternion_near(const float left[4], const float right[4]) {
    float direct = 0.0F;
    float negated = 0.0F;
    for (size_t component = 0U; component < 4U; ++component) {
        const float same = left[component] - right[component];
        const float opposite = left[component] + right[component];
        direct += same * same;
        negated += opposite * opposite;
    }
    return fminf(direct, negated) < 0.00001F;
}

static void initialize_body(EidolonVrmBody *body) {
    memset(body, 0, sizeof(*body));
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        body->node_by_role[role] = -1;
    }
    body->node_by_role[EIDOLON_HUMANOID_ROLE_HIPS] = 0;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_SPINE] = 1;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_NECK] = 2;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_HEAD] = 3;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG] = 4;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG] = 5;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_FOOT] = 6;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG] = 7;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG] = 8;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_FOOT] = 9;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM] = 10;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM] = 11;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_LEFT_HAND] = 12;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM] = 13;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM] = 14;
    body->node_by_role[EIDOLON_HUMANOID_ROLE_RIGHT_HAND] = 15;
}

static void set_node(EidolonMotionRig *rig, size_t index, const char *name, int parent, float x,
                     float y, float z) {
    const float translation[3] = {x, y, z};
    const float rotation[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    const float scale[3] = {1.0F, 1.0F, 1.0F};
    assert(eidolon_motion_set_node(rig, index, name, parent, translation, rotation, scale));
}

static void initialize_destination_rig(EidolonMotionRig *rig) {
    assert(eidolon_motion_init(rig, DESTINATION_NODE_COUNT));
    set_node(rig, 0U, "hips", -1, 0.0F, 2.0F, 0.0F);
    set_node(rig, 1U, "spine", 0, 0.0F, 0.25F, 0.0F);
    set_node(rig, 2U, "neck", 1, 0.0F, 0.50F, 0.0F);
    set_node(rig, 3U, "head", 2, 0.0F, 0.25F, 0.0F);
    set_node(rig, 4U, "leftUpperLeg", 0, -0.10F, -0.40F, 0.0F);
    set_node(rig, 5U, "leftLowerLeg", 4, 0.0F, -0.45F, 0.0F);
    set_node(rig, 6U, "leftFoot", 5, 0.0F, -0.40F, -0.05F);
    set_node(rig, 7U, "rightUpperLeg", 0, 0.10F, -0.40F, 0.0F);
    set_node(rig, 8U, "rightLowerLeg", 7, 0.0F, -0.45F, 0.0F);
    set_node(rig, 9U, "rightFoot", 8, 0.0F, -0.40F, -0.05F);
    set_node(rig, 10U, "leftUpperArm", 1, -0.25F, 0.35F, 0.0F);
    set_node(rig, 11U, "leftLowerArm", 10, -0.35F, 0.0F, 0.0F);
    set_node(rig, 12U, "leftHand", 11, -0.30F, 0.0F, 0.0F);
    set_node(rig, 13U, "rightUpperArm", 1, 0.25F, 0.35F, 0.0F);
    set_node(rig, 14U, "rightLowerArm", 13, 0.35F, 0.0F, 0.0F);
    set_node(rig, 15U, "rightHand", 14, 0.30F, 0.0F, 0.0F);
    set_node(rig, 16U, "headAccessory", 3, 0.0F, 0.20F, 0.0F);
    assert(eidolon_motion_rebuild_world(rig));
}

static uint64_t required_role_mask(void) {
    uint64_t mask = 0U;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        if (eidolon_humanoid_role_required((EidolonHumanoidRole)role)) {
            mask |= UINT64_C(1) << (uint32_t)role;
        }
    }
    return mask;
}

static void initialize_track(EidolonVrmaTrack *track, EidolonHumanoidRole role,
                             EidolonVrmaTrackPath path, size_t components) {
    memset(track, 0, sizeof(*track));
    track->role = role;
    track->path = path;
    track->interpolation = EIDOLON_VRMA_INTERPOLATION_LINEAR;
    track->key_count = 2U;
    track->component_count = components;
    track->values_per_key = 1U;
    track->times = calloc(track->key_count, sizeof(*track->times));
    track->values = calloc(track->key_count * components, sizeof(*track->values));
    assert(track->times != NULL && track->values != NULL);
    track->times[1] = 1.0F;
}

static void initialize_source(EidolonVrmaClip *clip) {
    memset(clip, 0, sizeof(*clip));
    clip->version = EIDOLON_VRMA_CLIP_VERSION;
    clip->mapped_roles = required_role_mask();
    clip->source_hips_height = 1.0F;
    clip->rest_hips_translation[1] = 1.0F;
    clip->duration_seconds = 1.0F;
    for (size_t role = 0U; role < EIDOLON_HUMANOID_ROLE_COUNT; ++role) {
        clip->rest_local_rotations[role][3] = 1.0F;
        clip->rest_world_rotations[role][3] = 1.0F;
    }
    clip->tracks = calloc(2U, sizeof(*clip->tracks));
    assert(clip->tracks != NULL);
    clip->track_count = 2U;
    initialize_track(&clip->tracks[0], EIDOLON_HUMANOID_ROLE_HEAD, EIDOLON_VRMA_TRACK_ROTATION, 4U);
    clip->tracks[0].values[3] = 1.0F;
    quaternion_axis(0.0F, 1.0F, 0.0F, 1.57079632679F, &clip->tracks[0].values[4]);
    initialize_track(&clip->tracks[1], EIDOLON_HUMANOID_ROLE_HIPS,
                     EIDOLON_VRMA_TRACK_HIPS_TRANSLATION, 3U);
    clip->tracks[1].values[1] = 1.0F;
    clip->tracks[1].values[3] = 1.0F;
    clip->tracks[1].values[4] = 1.5F;
    clip->tracks[1].values[5] = 2.0F;
}

static void test_play_pause_seek_loop_and_failure_diagnostics(void) {
    EidolonMotionRig rig;
    EidolonVrmBody body;
    EidolonVrmaClip clip;
    EidolonVrmPlayback playback;
    EidolonVrmPlaybackReport report;
    const EidolonMotionRig *candidate = NULL;
    char error[EIDOLON_VRM_PLAYBACK_ERROR_CAPACITY];
    float expected_head[4];

    memset(&playback, 0, sizeof(playback));
    initialize_body(&body);
    initialize_destination_rig(&rig);
    initialize_source(&clip);
    assert(eidolon_vrm_playback_take_clip(&playback, &clip, "fixture/idle", &body, &rig,
                                          EIDOLON_VRM_ROOT_MOTION_IN_PLACE, error, sizeof(error)));
    assert(clip.version == 0U && clip.tracks == NULL);
    assert(eidolon_vrm_playback_play(&playback, 1000U));
    assert(eidolon_vrm_playback_update(&playback, 1000U, &rig, &candidate, error, sizeof(error)));
    assert(candidate != NULL);
    assert(eidolon_vrm_playback_note_published(&playback, playback.sample_revision));
    assert(quaternion_near(candidate->nodes[3].rotation, rig.nodes[3].bind_rotation));

    candidate = NULL;
    assert(eidolon_vrm_playback_update(&playback, 1500U, &rig, &candidate, error, sizeof(error)));
    assert(candidate != NULL);
    quaternion_axis(0.0F, 1.0F, 0.0F, 0.78539816339F, expected_head);
    assert(quaternion_near(candidate->nodes[3].rotation, expected_head));
    assert(fabsf(candidate->nodes[0].translation[0]) < 0.0001F);
    assert(fabsf(candidate->nodes[0].translation[1] - 2.5F) < 0.0001F);
    assert(fabsf(candidate->nodes[0].translation[2]) < 0.0001F);
    assert(quaternion_near(rig.nodes[3].rotation, rig.nodes[3].bind_rotation));
    assert(eidolon_vrm_playback_note_published(&playback, playback.sample_revision));

    assert(eidolon_vrm_playback_pause(&playback, 1750U));
    candidate = NULL;
    assert(eidolon_vrm_playback_update(&playback, 1750U, &rig, &candidate, error, sizeof(error)));
    assert(candidate != NULL);
    assert(eidolon_vrm_playback_note_published(&playback, playback.sample_revision));
    const uint64_t paused_revision = playback.sample_revision;
    candidate = NULL;
    assert(eidolon_vrm_playback_update(&playback, 2500U, &rig, &candidate, error, sizeof(error)));
    assert(candidate == NULL && playback.sample_revision == paused_revision);

    assert(eidolon_vrm_playback_seek(&playback, 1.0F, 2600U));
    assert(eidolon_vrm_playback_update(&playback, 2600U, &rig, &candidate, error, sizeof(error)));
    assert(candidate != NULL);
    assert(eidolon_vrm_playback_note_published(&playback, playback.sample_revision));
    assert(eidolon_vrm_playback_report(&playback, &report));
    assert(report.state == EIDOLON_VRM_PLAYBACK_PAUSED);
    assert(fabsf(report.phase - 1.0F) < 0.0001F);
    assert(strcmp(report.clip_identity, "fixture/idle") == 0);
    assert(report.coverage.version == EIDOLON_VRMA_COVERAGE_VERSION);
    assert(report.coverage.rotation_track_count == 1U);
    assert(report.coverage.varying_rotation_track_count == 1U);
    assert(report.coverage.has_hips_translation && report.coverage.hips_translation_varies);
    assert(report.coverage.varying_chain_mask ==
           (EIDOLON_VRMA_CHAIN_ROOT | EIDOLON_VRMA_CHAIN_HEAD));

    assert(eidolon_vrm_playback_set_loop(&playback, true));
    assert(eidolon_vrm_playback_play(&playback, 3000U));
    assert(eidolon_vrm_playback_update(&playback, 3500U, &rig, &candidate, error, sizeof(error)));
    assert(candidate != NULL);
    assert(eidolon_vrm_playback_note_published(&playback, playback.sample_revision));
    assert(eidolon_vrm_playback_report(&playback, &report));
    assert(report.state == EIDOLON_VRM_PLAYBACK_PLAYING);
    assert(report.loop && fabsf(report.phase - 0.5F) < 0.0001F);

    candidate = NULL;
    assert(!eidolon_vrm_playback_update(&playback, 3400U, &rig, &candidate, error, sizeof(error)));
    assert(candidate == NULL);
    assert(eidolon_vrm_playback_report(&playback, &report));
    assert(report.state == EIDOLON_VRM_PLAYBACK_FAILED);
    assert(strstr(report.failure, "clock") != NULL);
    assert(quaternion_near(rig.nodes[3].rotation, rig.nodes[3].bind_rotation));

    eidolon_vrm_playback_destroy(&playback);
    eidolon_motion_destroy(&rig);
}

int main(void) {
    test_play_pause_seek_loop_and_failure_diagnostics();
    puts("vrm playback tests passed");
    return 0;
}
