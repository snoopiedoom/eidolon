#include "humanoid_pose.h"
#include "vrma_clip.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct AccessorFixture {
    cgltf_buffer buffer;
    cgltf_buffer_view view;
    cgltf_accessor accessor;
} AccessorFixture;

static void init_accessor(AccessorFixture *fixture, float *values, size_t count, cgltf_type type) {
    const size_t components = (size_t)cgltf_num_components(type);
    memset(fixture, 0, sizeof(*fixture));
    fixture->buffer.data = values;
    fixture->buffer.size = count * components * sizeof(float);
    fixture->view.buffer = &fixture->buffer;
    fixture->view.size = fixture->buffer.size;
    fixture->accessor.buffer_view = &fixture->view;
    fixture->accessor.component_type = cgltf_component_type_r_32f;
    fixture->accessor.type = type;
    fixture->accessor.count = count;
    fixture->accessor.stride = components * sizeof(float);
}

static void build_required_hierarchy(cgltf_node nodes[15]) {
    memset(nodes, 0, sizeof(cgltf_node) * 15U);
    nodes[0].has_translation = 1;
    nodes[0].translation[1] = 1.0F;
    nodes[1].parent = &nodes[0];
    nodes[1].has_translation = 1;
    nodes[1].translation[1] = 0.25F;
    nodes[2].parent = &nodes[1];
    nodes[2].has_translation = 1;
    nodes[2].translation[1] = 0.50F;

    nodes[3].parent = &nodes[0];
    nodes[4].parent = &nodes[3];
    nodes[5].parent = &nodes[4];
    nodes[6].parent = &nodes[0];
    nodes[7].parent = &nodes[6];
    nodes[8].parent = &nodes[7];

    nodes[9].parent = &nodes[1];
    nodes[10].parent = &nodes[9];
    nodes[11].parent = &nodes[10];
    nodes[12].parent = &nodes[1];
    nodes[13].parent = &nodes[12];
    nodes[14].parent = &nodes[13];
}

static void build_data(cgltf_data *data, cgltf_node nodes[15], cgltf_extension *extension,
                       cgltf_animation *animation, cgltf_animation_sampler samplers[2],
                       cgltf_animation_channel channels[2], AccessorFixture *times,
                       AccessorFixture *hips_values, AccessorFixture *arm_values,
                       float time_data[2], float hips_data[6], float arm_data[8]) {
    static char extension_json[] =
        "{\"specVersion\":\"1.0\",\"humanoid\":{\"humanBones\":{"
        "\"hips\":{\"node\":0},\"spine\":{\"node\":1},\"head\":{\"node\":2},"
        "\"leftUpperLeg\":{\"node\":3},\"leftLowerLeg\":{\"node\":4},"
        "\"leftFoot\":{\"node\":5},\"rightUpperLeg\":{\"node\":6},"
        "\"rightLowerLeg\":{\"node\":7},\"rightFoot\":{\"node\":8},"
        "\"leftUpperArm\":{\"node\":9},\"leftLowerArm\":{\"node\":10},"
        "\"leftHand\":{\"node\":11},\"rightUpperArm\":{\"node\":12},"
        "\"rightLowerArm\":{\"node\":13},\"rightHand\":{\"node\":14}}}}";

    memset(data, 0, sizeof(*data));
    build_required_hierarchy(nodes);
    extension->name = "VRMC_vrm_animation";
    extension->data = extension_json;
    data->nodes = nodes;
    data->nodes_count = 15U;
    data->data_extensions = extension;
    data->data_extensions_count = 1U;

    init_accessor(times, time_data, 2U, cgltf_type_scalar);
    init_accessor(hips_values, hips_data, 2U, cgltf_type_vec3);
    init_accessor(arm_values, arm_data, 2U, cgltf_type_vec4);
    memset(samplers, 0, sizeof(cgltf_animation_sampler) * 2U);
    samplers[0].input = &times->accessor;
    samplers[0].output = &hips_values->accessor;
    samplers[0].interpolation = cgltf_interpolation_type_linear;
    samplers[1].input = &times->accessor;
    samplers[1].output = &arm_values->accessor;
    samplers[1].interpolation = cgltf_interpolation_type_linear;

    memset(channels, 0, sizeof(cgltf_animation_channel) * 2U);
    channels[0].sampler = &samplers[0];
    channels[0].target_node = &nodes[0];
    channels[0].target_path = cgltf_animation_path_type_translation;
    channels[1].sampler = &samplers[1];
    channels[1].target_node = &nodes[12];
    channels[1].target_path = cgltf_animation_path_type_rotation;

    memset(animation, 0, sizeof(*animation));
    animation->samplers = samplers;
    animation->samplers_count = 2U;
    animation->channels = channels;
    animation->channels_count = 2U;
    data->animations = animation;
    data->animations_count = 1U;
}

static void test_complete_humanoid_vocabulary(void) {
    assert(EIDOLON_HUMANOID_ROLE_COUNT == 55);
    assert(strcmp(eidolon_humanoid_role_name(EIDOLON_HUMANOID_ROLE_HIPS), "hips") == 0);
    assert(eidolon_humanoid_role_from_name("rightLittleDistal", 17U) ==
           EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_DISTAL);
    assert(eidolon_humanoid_role_required(EIDOLON_HUMANOID_ROLE_LEFT_HAND));
    assert(!eidolon_humanoid_role_required(EIDOLON_HUMANOID_ROLE_LEFT_TOES));
    assert(!eidolon_humanoid_role_allowed_in_vrma(EIDOLON_HUMANOID_ROLE_LEFT_EYE));
}

static void test_parse_and_linear_sample(void) {
    cgltf_data data;
    cgltf_node nodes[15];
    cgltf_extension extension;
    cgltf_animation animation;
    cgltf_animation_sampler samplers[2];
    cgltf_animation_channel channels[2];
    AccessorFixture times;
    AccessorFixture hips_values;
    AccessorFixture arm_values;
    float time_data[2] = {0.0F, 1.0F};
    float hips_data[6] = {0.0F, 1.0F, 0.0F, 0.0F, 1.2F, 0.0F};
    float arm_data[8] = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.70710678F, 0.70710678F};
    EidolonVrmaClip clip;
    EidolonVrmaCoverage coverage;
    EidolonHumanoidPose pose;
    char error[EIDOLON_VRMA_ERROR_CAPACITY];
    memset(&clip, 0, sizeof(clip));
    build_data(&data, nodes, &extension, &animation, samplers, channels, &times, &hips_values,
               &arm_values, time_data, hips_data, arm_data);
    assert(eidolon_vrma_clip_parse(&data, &clip, error, sizeof(error)));
    assert(clip.track_count == 2U);
    assert(clip.auxiliary_channel_count == 0U);
    assert(fabsf(clip.duration_seconds - 1.0F) < 0.0001F);
    assert(fabsf(clip.source_hips_height - 1.0F) < 0.0001F);
    assert(eidolon_vrma_clip_coverage(&clip, &coverage));
    assert(coverage.version == EIDOLON_VRMA_COVERAGE_VERSION);
    assert(coverage.rotation_track_count == 1U);
    assert(coverage.varying_rotation_track_count == 1U);
    assert(coverage.rotation_roles == (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM));
    assert(coverage.varying_rotation_roles == coverage.rotation_roles);
    assert(coverage.has_hips_translation && coverage.hips_translation_varies);
    assert(coverage.tracked_chain_mask == (EIDOLON_VRMA_CHAIN_ROOT | EIDOLON_VRMA_CHAIN_RIGHT_ARM));
    assert(coverage.varying_chain_mask == coverage.tracked_chain_mask);
    assert(coverage.auxiliary_channel_count == 0U);
    assert(eidolon_vrma_clip_sample(&clip, 0.5F, false, &pose, error, sizeof(error)));
    assert(fabsf(pose.hips_translation[1] - 1.1F) < 0.0001F);
    assert(fabsf(pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM][2] - 0.38268343F) < 0.0001F);
    assert(fabsf(pose.rotations[EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM][3] - 0.92387953F) < 0.0001F);
    assert(eidolon_vrma_clip_sample(&clip, 1.5F, true, &pose, error, sizeof(error)));
    assert(fabsf(pose.hips_translation[1] - 1.1F) < 0.0001F);
    eidolon_vrma_clip_destroy(&clip);
}

static void test_cubic_and_step_sampling(void) {
    EidolonVrmaClip clip;
    EidolonVrmaTrack tracks[2];
    EidolonVrmaCoverage coverage;
    EidolonHumanoidPose pose;
    char error[EIDOLON_VRMA_ERROR_CAPACITY];
    float times[2] = {0.0F, 1.0F};
    float cubic_values[18] = {
        0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.2F, 0.0F,
        0.0F, 0.2F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F,
    };
    float step_values[8] = {
        0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F,
    };
    memset(&clip, 0, sizeof(clip));
    memset(tracks, 0, sizeof(tracks));
    clip.version = EIDOLON_VRMA_CLIP_VERSION;
    clip.duration_seconds = 1.0F;
    clip.mapped_roles =
        (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_HIPS) | (UINT64_C(1) << EIDOLON_HUMANOID_ROLE_HEAD);
    clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_HIPS][3] = 1.0F;
    clip.rest_local_rotations[EIDOLON_HUMANOID_ROLE_HEAD][3] = 1.0F;
    clip.tracks = tracks;
    clip.track_count = 2U;
    tracks[0].role = EIDOLON_HUMANOID_ROLE_HIPS;
    tracks[0].path = EIDOLON_VRMA_TRACK_HIPS_TRANSLATION;
    tracks[0].interpolation = EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE;
    tracks[0].times = times;
    tracks[0].values = cubic_values;
    tracks[0].key_count = 2U;
    tracks[0].component_count = 3U;
    tracks[0].values_per_key = 3U;
    tracks[1].role = EIDOLON_HUMANOID_ROLE_HEAD;
    tracks[1].path = EIDOLON_VRMA_TRACK_ROTATION;
    tracks[1].interpolation = EIDOLON_VRMA_INTERPOLATION_STEP;
    tracks[1].times = times;
    tracks[1].values = step_values;
    tracks[1].key_count = 2U;
    tracks[1].component_count = 4U;
    tracks[1].values_per_key = 1U;
    assert(eidolon_vrma_clip_sample(&clip, 0.5F, false, &pose, error, sizeof(error)));
    assert(fabsf(pose.hips_translation[1] - 1.0F) < 0.0001F);
    assert(fabsf(pose.rotations[EIDOLON_HUMANOID_ROLE_HEAD][3] - 1.0F) < 0.0001F);
    assert(eidolon_vrma_clip_sample(&clip, 0.25F, false, &pose, error, sizeof(error)));
    assert(fabsf(pose.hips_translation[1] - 1.01875F) < 0.0001F);
    assert(eidolon_vrma_clip_coverage(&clip, &coverage));
    assert(coverage.rotation_track_count == 1U);
    assert(coverage.varying_rotation_track_count == 1U);
    assert(coverage.has_hips_translation && coverage.hips_translation_varies);
    assert(coverage.tracked_chain_mask == (EIDOLON_VRMA_CHAIN_ROOT | EIDOLON_VRMA_CHAIN_HEAD));
    assert(coverage.varying_chain_mask == coverage.tracked_chain_mask);
    clip.tracks = NULL;
    clip.track_count = 0U;
}

static void test_forbidden_humanoid_scale_is_rejected(void) {
    cgltf_data data;
    cgltf_node nodes[15];
    cgltf_extension extension;
    cgltf_animation animation;
    cgltf_animation_sampler samplers[2];
    cgltf_animation_channel channels[2];
    AccessorFixture times;
    AccessorFixture hips_values;
    AccessorFixture arm_values;
    float time_data[2] = {0.0F, 1.0F};
    float hips_data[6] = {0.0F, 1.0F, 0.0F, 0.0F, 1.2F, 0.0F};
    float arm_data[8] = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.70710678F, 0.70710678F};
    EidolonVrmaClip clip;
    char error[EIDOLON_VRMA_ERROR_CAPACITY];
    memset(&clip, 0, sizeof(clip));
    build_data(&data, nodes, &extension, &animation, samplers, channels, &times, &hips_values,
               &arm_values, time_data, hips_data, arm_data);
    channels[1].target_path = cgltf_animation_path_type_scale;
    assert(!eidolon_vrma_clip_parse(&data, &clip, error, sizeof(error)));
    assert(strstr(error, "scale") != NULL);
}

int main(int argument_count, char **arguments) {
    test_complete_humanoid_vocabulary();
    test_parse_and_linear_sample();
    test_cubic_and_step_sampling();
    test_forbidden_humanoid_scale_is_rejected();
    if (argument_count == 2) {
        EidolonVrmaClip clip;
        EidolonVrmaCoverage coverage;
        EidolonHumanoidPose pose;
        char error[EIDOLON_VRMA_ERROR_CAPACITY];
        memset(&clip, 0, sizeof(clip));
        if (!eidolon_vrma_clip_load(arguments[1], &clip, error, sizeof(error)) ||
            !eidolon_vrma_clip_sample(&clip, 0.0F, false, &pose, error, sizeof(error))) {
            fprintf(stderr, "VRMA check failed: %s\n", error);
            eidolon_vrma_clip_destroy(&clip);
            return 1;
        }
        if (!eidolon_vrma_clip_coverage(&clip, &coverage)) {
            fputs("VRMA check failed: could not classify humanoid track coverage\n", stderr);
            eidolon_vrma_clip_destroy(&clip);
            return 1;
        }
        printf("VRMA ready duration=%.3fs tracks=%zu auxiliary=%zu mapped=0x%016llx "
               "rotation_tracks=%zu varying=%zu chains=0x%02x varying_chains=0x%02x\n",
               clip.duration_seconds, clip.track_count, clip.auxiliary_channel_count,
               (unsigned long long)clip.mapped_roles, coverage.rotation_track_count,
               coverage.varying_rotation_track_count, (unsigned int)coverage.tracked_chain_mask,
               (unsigned int)coverage.varying_chain_mask);
        for (size_t index = 0U; index < clip.track_count; ++index) {
            const EidolonVrmaTrack *track = &clip.tracks[index];
            const bool varies = track->path == EIDOLON_VRMA_TRACK_HIPS_TRANSLATION
                                    ? coverage.hips_translation_varies
                                    : (coverage.varying_rotation_roles &
                                       (UINT64_C(1) << (uint32_t)track->role)) != 0U;
            printf("  track[%zu] role=%s path=%s interpolation=%s keys=%zu varying=%s\n", index,
                   eidolon_humanoid_role_name(track->role),
                   eidolon_vrma_track_path_name(track->path),
                   eidolon_vrma_interpolation_name(track->interpolation), track->key_count,
                   varies ? "yes" : "no");
        }
        eidolon_vrma_clip_destroy(&clip);
    }
    puts("VRMA clip tests passed");
    return 0;
}
