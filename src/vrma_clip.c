#include "vrma_clip.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VRMA_EPSILON 0.000001F

typedef struct JsonSpan {
    const char *begin;
    const char *end;
} JsonSpan;

static void set_error(char *error, size_t capacity, const char *format, ...) {
    va_list arguments;
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static const char *skip_space(const char *cursor, const char *end) {
    while (cursor < end &&
           (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
        cursor += 1;
    }
    return cursor;
}

static const char *skip_string(const char *cursor, const char *end) {
    if (cursor >= end || *cursor != '"') {
        return NULL;
    }
    cursor += 1;
    while (cursor < end) {
        if (*cursor == '\\') {
            cursor += 2;
        } else if (*cursor == '"') {
            return cursor + 1;
        } else {
            cursor += 1;
        }
    }
    return NULL;
}

static const char *skip_value(const char *cursor, const char *end, unsigned int depth) {
    char close = '\0';
    if (depth > 64U) {
        return NULL;
    }
    cursor = skip_space(cursor, end);
    if (cursor >= end) {
        return NULL;
    }
    if (*cursor == '"') {
        return skip_string(cursor, end);
    }
    if (*cursor == '{') {
        close = '}';
    } else if (*cursor == '[') {
        close = ']';
    } else {
        while (cursor < end && *cursor != ',' && *cursor != '}' && *cursor != ']' &&
               *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
            cursor += 1;
        }
        return cursor;
    }
    cursor += 1;
    for (;;) {
        const char *value_end;
        cursor = skip_space(cursor, end);
        if (cursor >= end) {
            return NULL;
        }
        if (*cursor == close) {
            return cursor + 1;
        }
        if (close == '}') {
            cursor = skip_string(cursor, end);
            if (cursor == NULL) {
                return NULL;
            }
            cursor = skip_space(cursor, end);
            if (cursor >= end || *cursor != ':') {
                return NULL;
            }
            cursor += 1;
        }
        value_end = skip_value(cursor, end, depth + 1U);
        if (value_end == NULL) {
            return NULL;
        }
        cursor = skip_space(value_end, end);
        if (cursor < end && *cursor == ',') {
            cursor += 1;
            continue;
        }
        if (cursor < end && *cursor == close) {
            return cursor + 1;
        }
        return NULL;
    }
}

static bool string_equals(JsonSpan string, const char *expected) {
    const size_t length = strlen(expected);
    return string.end > string.begin + 1 && string.begin[0] == '"' && string.end[-1] == '"' &&
           (size_t)(string.end - string.begin - 2) == length &&
           memcmp(string.begin + 1, expected, length) == 0;
}

static bool object_member(JsonSpan object, const char *key, JsonSpan *value) {
    const char *cursor = skip_space(object.begin, object.end);
    if (cursor >= object.end || *cursor != '{') {
        return false;
    }
    cursor += 1;
    for (;;) {
        JsonSpan name;
        const char *value_end;
        cursor = skip_space(cursor, object.end);
        if (cursor >= object.end || *cursor == '}') {
            return false;
        }
        name.begin = cursor;
        name.end = skip_string(cursor, object.end);
        if (name.end == NULL) {
            return false;
        }
        cursor = skip_space(name.end, object.end);
        if (cursor >= object.end || *cursor != ':') {
            return false;
        }
        cursor = skip_space(cursor + 1, object.end);
        value_end = skip_value(cursor, object.end, 0U);
        if (value_end == NULL) {
            return false;
        }
        if (string_equals(name, key)) {
            value->begin = cursor;
            value->end = value_end;
            return true;
        }
        cursor = skip_space(value_end, object.end);
        if (cursor < object.end && *cursor == ',') {
            cursor += 1;
        }
    }
}

static bool json_integer(JsonSpan span, int64_t *value) {
    char buffer[64];
    char *parsed_end = NULL;
    const char *begin = skip_space(span.begin, span.end);
    const size_t length = (size_t)(span.end - begin);
    long long parsed;
    if (value == NULL || length == 0U || length >= sizeof(buffer)) {
        return false;
    }
    memcpy(buffer, begin, length);
    buffer[length] = '\0';
    parsed = strtoll(buffer, &parsed_end, 10);
    if (parsed_end == buffer || *parsed_end != '\0') {
        return false;
    }
    *value = (int64_t)parsed;
    return true;
}

static const cgltf_extension *find_extension(const cgltf_data *data, const char *name) {
    if (data == NULL || name == NULL) {
        return NULL;
    }
    for (cgltf_size index = 0; index < data->data_extensions_count; ++index) {
        const cgltf_extension *extension = &data->data_extensions[index];
        if (extension->name != NULL && strcmp(extension->name, name) == 0) {
            return extension;
        }
    }
    return NULL;
}

static void quaternion_multiply(const float left[4], const float right[4], float result[4]) {
    result[0] = left[3] * right[0] + left[0] * right[3] + left[1] * right[2] - left[2] * right[1];
    result[1] = left[3] * right[1] - left[0] * right[2] + left[1] * right[3] + left[2] * right[0];
    result[2] = left[3] * right[2] + left[0] * right[1] - left[1] * right[0] + left[2] * right[3];
    result[3] = left[3] * right[3] - left[0] * right[0] - left[1] * right[1] - left[2] * right[2];
}

static bool quaternion_normalize(float value[4]) {
    const float length_squared =
        value[0] * value[0] + value[1] * value[1] + value[2] * value[2] + value[3] * value[3];
    if (!isfinite(length_squared) || length_squared <= VRMA_EPSILON) {
        return false;
    }
    const float scale = 1.0F / sqrtf(length_squared);
    for (size_t index = 0U; index < 4U; ++index) {
        value[index] *= scale;
    }
    return true;
}

static void node_local_rotation(const cgltf_node *node, float result[4]) {
    result[0] = 0.0F;
    result[1] = 0.0F;
    result[2] = 0.0F;
    result[3] = 1.0F;
    if (node != NULL && node->has_rotation) {
        for (size_t index = 0U; index < 4U; ++index) {
            result[index] = node->rotation[index];
        }
    }
}

static bool node_world_rotation(const cgltf_node *node, float result[4], size_t depth) {
    float local[4];
    if (node == NULL || depth > 512U) {
        return false;
    }
    node_local_rotation(node, local);
    if (!quaternion_normalize(local)) {
        return false;
    }
    if (node->parent == NULL) {
        memcpy(result, local, sizeof(local));
        return true;
    }
    float parent[4];
    if (!node_world_rotation(node->parent, parent, depth + 1U)) {
        return false;
    }
    quaternion_multiply(parent, local, result);
    return quaternion_normalize(result);
}

static int bone_for_node(const EidolonVrmaClip *clip, const cgltf_data *data,
                         const cgltf_node *node) {
    if (clip == NULL || data == NULL || node == NULL || node < data->nodes ||
        node >= data->nodes + data->nodes_count) {
        return -1;
    }
    const ptrdiff_t node_index = node - data->nodes;
    for (size_t bone = 0U; bone < EIDOLON_HUMANOID_ROLE_COUNT; ++bone) {
        if (clip->source_node_by_role[bone] == (int)node_index) {
            return (int)bone;
        }
    }
    return -1;
}

static bool parse_humanoid(const cgltf_data *data, JsonSpan root, EidolonVrmaClip *clip,
                           char *error, size_t error_capacity) {
    JsonSpan humanoid;
    JsonSpan bones;
    if (!object_member(root, "humanoid", &humanoid) ||
        !object_member(humanoid, "humanBones", &bones)) {
        set_error(error, error_capacity, "VRMA humanoid.humanBones is missing");
        return false;
    }
    for (size_t bone = 0U; bone < EIDOLON_HUMANOID_ROLE_COUNT; ++bone) {
        JsonSpan binding;
        JsonSpan node_value;
        const EidolonHumanoidRole role = (EidolonHumanoidRole)bone;
        clip->source_node_by_role[bone] = -1;
        if (!object_member(bones, eidolon_humanoid_role_name(role), &binding)) {
            if (eidolon_humanoid_role_required(role)) {
                set_error(error, error_capacity, "VRMA required humanoid bone '%s' is missing",
                          eidolon_humanoid_role_name(role));
                return false;
            }
            continue;
        }
        if (!eidolon_humanoid_role_allowed_in_vrma(role)) {
            set_error(error, error_capacity, "VRMA must represent '%s' through look-at",
                      eidolon_humanoid_role_name(role));
            return false;
        }
        int64_t node_index = -1;
        if (!object_member(binding, "node", &node_value) ||
            !json_integer(node_value, &node_index) || node_index < 0 ||
            (uint64_t)node_index >= (uint64_t)data->nodes_count) {
            set_error(error, error_capacity, "VRMA humanoid bone '%s' has an invalid node",
                      eidolon_humanoid_role_name(role));
            return false;
        }
        for (size_t prior = 0U; prior < bone; ++prior) {
            if (clip->source_node_by_role[prior] == (int)node_index) {
                set_error(error, error_capacity, "VRMA maps two humanoid roles to node %lld",
                          (long long)node_index);
                return false;
            }
        }
        const cgltf_node *node = &data->nodes[(size_t)node_index];
        if (node->has_matrix) {
            set_error(error, error_capacity, "VRMA humanoid bone '%s' uses a matrix node",
                      eidolon_humanoid_role_name(role));
            return false;
        }
        clip->source_node_by_role[bone] = (int)node_index;
        clip->mapped_roles |= UINT64_C(1) << (uint32_t)bone;
        node_local_rotation(node, clip->rest_local_rotations[bone]);
        if (!quaternion_normalize(clip->rest_local_rotations[bone]) ||
            !node_world_rotation(node, clip->rest_world_rotations[bone], 0U)) {
            set_error(error, error_capacity, "VRMA humanoid bone '%s' has an invalid rest rotation",
                      eidolon_humanoid_role_name(role));
            return false;
        }
    }
    const cgltf_node *hips =
        &data->nodes[(size_t)clip->source_node_by_role[EIDOLON_HUMANOID_ROLE_HIPS]];
    if (hips->has_translation) {
        memcpy(clip->rest_hips_translation, hips->translation, sizeof(clip->rest_hips_translation));
    }
    cgltf_float world[16];
    cgltf_node_transform_world(hips, world);
    clip->source_hips_height = world[13];
    if (!isfinite(clip->source_hips_height) || clip->source_hips_height <= VRMA_EPSILON) {
        set_error(error, error_capacity, "VRMA T-pose hips height must be positive");
        return false;
    }
    return true;
}

static bool map_interpolation(cgltf_interpolation_type source, EidolonVrmaInterpolation *result) {
    switch (source) {
    case cgltf_interpolation_type_step:
        *result = EIDOLON_VRMA_INTERPOLATION_STEP;
        return true;
    case cgltf_interpolation_type_linear:
        *result = EIDOLON_VRMA_INTERPOLATION_LINEAR;
        return true;
    case cgltf_interpolation_type_cubic_spline:
        *result = EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE;
        return true;
    case cgltf_interpolation_type_max_enum:
        return false;
    }
    return false;
}

static bool track_duplicate(const EidolonVrmaClip *clip, EidolonHumanoidRole bone,
                            EidolonVrmaTrackPath path) {
    for (size_t index = 0U; index < clip->track_count; ++index) {
        if (clip->tracks[index].role == bone && clip->tracks[index].path == path) {
            return true;
        }
    }
    return false;
}

static bool copy_track(const cgltf_animation_channel *channel, EidolonHumanoidRole bone,
                       EidolonVrmaTrackPath path, EidolonVrmaTrack *track, char *error,
                       size_t error_capacity) {
    const cgltf_animation_sampler *sampler = channel->sampler;
    const cgltf_accessor *input = sampler != NULL ? sampler->input : NULL;
    const cgltf_accessor *output = sampler != NULL ? sampler->output : NULL;
    const size_t components = path == EIDOLON_VRMA_TRACK_ROTATION ? 4U : 3U;
    EidolonVrmaInterpolation interpolation;
    if (input == NULL || output == NULL || input->type != cgltf_type_scalar ||
        input->component_type != cgltf_component_type_r_32f || input->is_sparse ||
        output->component_type != cgltf_component_type_r_32f || output->is_sparse ||
        input->count == 0U ||
        output->type != (path == EIDOLON_VRMA_TRACK_ROTATION ? cgltf_type_vec4 : cgltf_type_vec3) ||
        !map_interpolation(sampler->interpolation, &interpolation)) {
        set_error(error, error_capacity, "VRMA track for '%s' has unsupported accessors",
                  eidolon_humanoid_role_name(bone));
        return false;
    }
    const size_t values_per_key =
        interpolation == EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE ? 3U : 1U;
    if ((size_t)output->count != (size_t)input->count * values_per_key) {
        set_error(error, error_capacity, "VRMA track for '%s' has mismatched key/value counts",
                  eidolon_humanoid_role_name(bone));
        return false;
    }
    const size_t key_count = (size_t)input->count;
    if (key_count > SIZE_MAX / values_per_key ||
        key_count * values_per_key > SIZE_MAX / components) {
        set_error(error, error_capacity, "VRMA track for '%s' is too large",
                  eidolon_humanoid_role_name(bone));
        return false;
    }
    track->times = calloc(key_count, sizeof(*track->times));
    track->values = calloc(key_count * values_per_key * components, sizeof(*track->values));
    if (track->times == NULL || track->values == NULL) {
        set_error(error, error_capacity, "out of memory copying VRMA track");
        return false;
    }
    track->role = bone;
    track->path = path;
    track->interpolation = interpolation;
    track->key_count = key_count;
    track->component_count = components;
    track->values_per_key = values_per_key;
    for (size_t index = 0U; index < key_count; ++index) {
        if (!cgltf_accessor_read_float(input, (cgltf_size)index, &track->times[index], 1U) ||
            !isfinite(track->times[index]) || track->times[index] < 0.0F ||
            (index > 0U && track->times[index] <= track->times[index - 1U])) {
            set_error(error, error_capacity, "VRMA track for '%s' has invalid key times",
                      eidolon_humanoid_role_name(bone));
            return false;
        }
    }
    const size_t value_count = key_count * values_per_key;
    for (size_t index = 0U; index < value_count; ++index) {
        float *destination = &track->values[index * components];
        if (!cgltf_accessor_read_float(output, (cgltf_size)index, destination,
                                       (cgltf_size)components)) {
            set_error(error, error_capacity, "VRMA track for '%s' has unreadable values",
                      eidolon_humanoid_role_name(bone));
            return false;
        }
        for (size_t component = 0U; component < components; ++component) {
            if (!isfinite(destination[component])) {
                set_error(error, error_capacity, "VRMA track for '%s' has non-finite values",
                          eidolon_humanoid_role_name(bone));
                return false;
            }
        }
    }
    return true;
}

static bool parse_tracks(const cgltf_data *data, EidolonVrmaClip *clip, char *error,
                         size_t error_capacity) {
    if (data->animations_count == 0U) {
        set_error(error, error_capacity, "VRMA contains no glTF animation");
        return false;
    }
    const cgltf_animation *animation = &data->animations[0];
    clip->tracks = calloc((size_t)animation->channels_count, sizeof(*clip->tracks));
    if (animation->channels_count > 0U && clip->tracks == NULL) {
        set_error(error, error_capacity, "out of memory allocating VRMA tracks");
        return false;
    }
    for (cgltf_size index = 0; index < animation->channels_count; ++index) {
        const cgltf_animation_channel *channel = &animation->channels[index];
        const int bone_value = bone_for_node(clip, data, channel->target_node);
        if (bone_value < 0) {
            clip->auxiliary_channel_count += 1U;
            continue;
        }
        const EidolonHumanoidRole bone = (EidolonHumanoidRole)bone_value;
        EidolonVrmaTrackPath path;
        if (channel->target_path == cgltf_animation_path_type_rotation) {
            path = EIDOLON_VRMA_TRACK_ROTATION;
        } else if (channel->target_path == cgltf_animation_path_type_translation &&
                   bone == EIDOLON_HUMANOID_ROLE_HIPS) {
            path = EIDOLON_VRMA_TRACK_HIPS_TRANSLATION;
        } else if (channel->target_path == cgltf_animation_path_type_scale) {
            set_error(error, error_capacity, "VRMA humanoid scale animation is forbidden");
            return false;
        } else if (channel->target_path == cgltf_animation_path_type_translation) {
            set_error(error, error_capacity, "VRMA translation is allowed only on hips, not '%s'",
                      eidolon_humanoid_role_name(bone));
            return false;
        } else {
            set_error(error, error_capacity, "VRMA humanoid channel for '%s' has an invalid path",
                      eidolon_humanoid_role_name(bone));
            return false;
        }
        if (track_duplicate(clip, bone, path)) {
            set_error(error, error_capacity, "VRMA has duplicate '%s' tracks",
                      eidolon_humanoid_role_name(bone));
            return false;
        }
        EidolonVrmaTrack *track = &clip->tracks[clip->track_count];
        if (!copy_track(channel, bone, path, track, error, error_capacity)) {
            free(track->times);
            free(track->values);
            track->times = NULL;
            track->values = NULL;
            return false;
        }
        clip->track_count += 1U;
        const float end = track->times[track->key_count - 1U];
        if (end > clip->duration_seconds) {
            clip->duration_seconds = end;
        }
    }
    if (clip->track_count == 0U) {
        set_error(error, error_capacity, "VRMA contains no humanoid motion channels");
        return false;
    }
    return true;
}

bool eidolon_vrma_clip_parse(const cgltf_data *data, EidolonVrmaClip *clip, char *error,
                             size_t error_capacity) {
    EidolonVrmaClip candidate;
    const cgltf_extension *extension = find_extension(data, "VRMC_vrm_animation");
    if (data == NULL || clip == NULL || extension == NULL || extension->data == NULL) {
        set_error(error, error_capacity, "VRMC_vrm_animation 1.0 extension is missing");
        return false;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.version = EIDOLON_VRMA_CLIP_VERSION;
    for (size_t bone = 0U; bone < EIDOLON_HUMANOID_ROLE_COUNT; ++bone) {
        candidate.source_node_by_role[bone] = -1;
        candidate.rest_local_rotations[bone][3] = 1.0F;
        candidate.rest_world_rotations[bone][3] = 1.0F;
    }
    const JsonSpan root = {extension->data, extension->data + strlen(extension->data)};
    JsonSpan version;
    if (!object_member(root, "specVersion", &version) || !string_equals(version, "1.0")) {
        set_error(error, error_capacity, "VRMC_vrm_animation specVersion must be '1.0'");
        return false;
    }
    if (!parse_humanoid(data, root, &candidate, error, error_capacity) ||
        !parse_tracks(data, &candidate, error, error_capacity)) {
        eidolon_vrma_clip_destroy(&candidate);
        return false;
    }
    *clip = candidate;
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

bool eidolon_vrma_clip_load(const char *path, EidolonVrmaClip *clip, char *error,
                            size_t error_capacity) {
    cgltf_options options = {0};
    cgltf_data *data = NULL;
    if (path == NULL || clip == NULL) {
        set_error(error, error_capacity, "invalid VRMA path or output");
        return false;
    }
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        set_error(error, error_capacity, "could not parse VRMA (cgltf result %d)", (int)result);
        return false;
    }
    result = cgltf_load_buffers(&options, data, path);
    if (result == cgltf_result_success) {
        result = cgltf_validate(data);
    }
    if (result != cgltf_result_success) {
        set_error(error, error_capacity, "could not load or validate VRMA (cgltf result %d)",
                  (int)result);
        cgltf_free(data);
        return false;
    }
    const bool parsed = eidolon_vrma_clip_parse(data, clip, error, error_capacity);
    cgltf_free(data);
    return parsed;
}

static const float *track_value(const EidolonVrmaTrack *track, size_t key, size_t part) {
    return &track->values[(key * track->values_per_key + part) * track->component_count];
}

static bool quaternion_slerp(const float first[4], const float second[4], float weight,
                             float result[4]) {
    float target[4];
    float dot = 0.0F;
    for (size_t index = 0U; index < 4U; ++index) {
        target[index] = second[index];
        dot += first[index] * second[index];
    }
    if (dot < 0.0F) {
        dot = -dot;
        for (size_t index = 0U; index < 4U; ++index) {
            target[index] = -target[index];
        }
    }
    if (dot > 0.9995F) {
        for (size_t index = 0U; index < 4U; ++index) {
            result[index] = first[index] + (target[index] - first[index]) * weight;
        }
        return quaternion_normalize(result);
    }
    dot = fmaxf(-1.0F, fminf(1.0F, dot));
    const float angle = acosf(dot);
    const float denominator = sinf(angle);
    if (fabsf(denominator) <= VRMA_EPSILON) {
        memcpy(result, first, sizeof(float) * 4U);
        return quaternion_normalize(result);
    }
    const float first_weight = sinf((1.0F - weight) * angle) / denominator;
    const float second_weight = sinf(weight * angle) / denominator;
    for (size_t index = 0U; index < 4U; ++index) {
        result[index] = first[index] * first_weight + target[index] * second_weight;
    }
    return quaternion_normalize(result);
}

static bool sample_track(const EidolonVrmaTrack *track, float time, float result[4]) {
    size_t left = 0U;
    size_t right = 0U;
    float weight = 0.0F;
    if (track->key_count == 0U) {
        return false;
    }
    if (time <= track->times[0]) {
        left = 0U;
        right = 0U;
    } else if (time >= track->times[track->key_count - 1U]) {
        left = track->key_count - 1U;
        right = left;
    } else {
        size_t lower = 1U;
        size_t upper = track->key_count - 1U;
        while (lower < upper) {
            const size_t middle = lower + (upper - lower) / 2U;
            if (time < track->times[middle]) {
                upper = middle;
            } else {
                lower = middle + 1U;
            }
        }
        right = lower;
        left = right - 1U;
        const float span = track->times[right] - track->times[left];
        weight = (time - track->times[left]) / span;
    }
    const size_t value_part =
        track->interpolation == EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE ? 1U : 0U;
    const float *first = track_value(track, left, value_part);
    if (left == right || track->interpolation == EIDOLON_VRMA_INTERPOLATION_STEP) {
        memcpy(result, first, sizeof(float) * track->component_count);
    } else if (track->interpolation == EIDOLON_VRMA_INTERPOLATION_LINEAR &&
               track->path == EIDOLON_VRMA_TRACK_ROTATION) {
        const float *second = track_value(track, right, 0U);
        if (!quaternion_slerp(first, second, weight, result)) {
            return false;
        }
    } else if (track->interpolation == EIDOLON_VRMA_INTERPOLATION_LINEAR) {
        const float *second = track_value(track, right, 0U);
        for (size_t component = 0U; component < track->component_count; ++component) {
            result[component] = first[component] + (second[component] - first[component]) * weight;
        }
    } else {
        const float *out_tangent = track_value(track, left, 2U);
        const float *second = track_value(track, right, 1U);
        const float *in_tangent = track_value(track, right, 0U);
        const float span = track->times[right] - track->times[left];
        const float t2 = weight * weight;
        const float t3 = t2 * weight;
        const float h00 = 2.0F * t3 - 3.0F * t2 + 1.0F;
        const float h10 = t3 - 2.0F * t2 + weight;
        const float h01 = -2.0F * t3 + 3.0F * t2;
        const float h11 = t3 - t2;
        for (size_t component = 0U; component < track->component_count; ++component) {
            result[component] = h00 * first[component] + h10 * span * out_tangent[component] +
                                h01 * second[component] + h11 * span * in_tangent[component];
        }
    }
    if (track->path == EIDOLON_VRMA_TRACK_ROTATION) {
        return quaternion_normalize(result);
    }
    for (size_t component = 0U; component < track->component_count; ++component) {
        if (!isfinite(result[component])) {
            return false;
        }
    }
    return true;
}

bool eidolon_vrma_clip_sample(const EidolonVrmaClip *clip, float seconds, bool loop,
                              EidolonHumanoidPose *pose, char *error, size_t error_capacity) {
    float time = seconds;
    if (clip == NULL || pose == NULL || clip->version != EIDOLON_VRMA_CLIP_VERSION ||
        !isfinite(seconds) || seconds < 0.0F) {
        set_error(error, error_capacity, "invalid VRMA sample request");
        return false;
    }
    if (loop && clip->duration_seconds > VRMA_EPSILON) {
        time = fmodf(time, clip->duration_seconds);
    } else if (time > clip->duration_seconds) {
        time = clip->duration_seconds;
    }
    eidolon_humanoid_pose_init(pose);
    pose->rotation_mask = clip->mapped_roles;
    for (size_t bone = 0U; bone < EIDOLON_HUMANOID_ROLE_COUNT; ++bone) {
        if ((clip->mapped_roles & (UINT64_C(1) << (uint32_t)bone)) != 0U) {
            memcpy(pose->rotations[bone], clip->rest_local_rotations[bone],
                   sizeof(pose->rotations[bone]));
        }
    }
    memcpy(pose->hips_translation, clip->rest_hips_translation, sizeof(pose->hips_translation));
    pose->has_hips_translation = true;
    for (size_t index = 0U; index < clip->track_count; ++index) {
        const EidolonVrmaTrack *track = &clip->tracks[index];
        float value[4] = {0.0F, 0.0F, 0.0F, 1.0F};
        if (!sample_track(track, time, value)) {
            set_error(error, error_capacity, "could not sample VRMA track for '%s'",
                      eidolon_humanoid_role_name(track->role));
            return false;
        }
        if (track->path == EIDOLON_VRMA_TRACK_ROTATION) {
            memcpy(pose->rotations[(size_t)track->role], value, sizeof(float) * 4U);
        } else {
            memcpy(pose->hips_translation, value, sizeof(float) * 3U);
        }
    }
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

static uint32_t role_chain(EidolonHumanoidRole role) {
    if (role >= EIDOLON_HUMANOID_ROLE_HIPS && role <= EIDOLON_HUMANOID_ROLE_UPPER_CHEST) {
        return EIDOLON_VRMA_CHAIN_TORSO;
    }
    if (role == EIDOLON_HUMANOID_ROLE_NECK || role == EIDOLON_HUMANOID_ROLE_HEAD ||
        role == EIDOLON_HUMANOID_ROLE_LEFT_EYE || role == EIDOLON_HUMANOID_ROLE_RIGHT_EYE ||
        role == EIDOLON_HUMANOID_ROLE_JAW) {
        return EIDOLON_VRMA_CHAIN_HEAD;
    }
    if ((role >= EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG && role <= EIDOLON_HUMANOID_ROLE_LEFT_TOES)) {
        return EIDOLON_VRMA_CHAIN_LEFT_LEG;
    }
    if ((role >= EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG &&
         role <= EIDOLON_HUMANOID_ROLE_RIGHT_TOES)) {
        return EIDOLON_VRMA_CHAIN_RIGHT_LEG;
    }
    if ((role >= EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER && role <= EIDOLON_HUMANOID_ROLE_LEFT_HAND) ||
        (role >= EIDOLON_HUMANOID_ROLE_LEFT_THUMB_METACARPAL &&
         role <= EIDOLON_HUMANOID_ROLE_LEFT_LITTLE_DISTAL)) {
        return EIDOLON_VRMA_CHAIN_LEFT_ARM;
    }
    if ((role >= EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER &&
         role <= EIDOLON_HUMANOID_ROLE_RIGHT_HAND) ||
        (role >= EIDOLON_HUMANOID_ROLE_RIGHT_THUMB_METACARPAL &&
         role <= EIDOLON_HUMANOID_ROLE_RIGHT_LITTLE_DISTAL)) {
        return EIDOLON_VRMA_CHAIN_RIGHT_ARM;
    }
    return 0U;
}

static bool sampled_values_differ(EidolonVrmaTrackPath path, const float first[4],
                                  const float second[4]) {
    float direct = 0.0F;
    float negated = 0.0F;
    const size_t components = path == EIDOLON_VRMA_TRACK_ROTATION ? 4U : 3U;
    for (size_t component = 0U; component < components; ++component) {
        const float same = first[component] - second[component];
        direct += same * same;
        if (path == EIDOLON_VRMA_TRACK_ROTATION) {
            const float opposite = first[component] + second[component];
            negated += opposite * opposite;
        }
    }
    const float difference = path == EIDOLON_VRMA_TRACK_ROTATION ? fminf(direct, negated) : direct;
    return difference > 0.0000000001F;
}

static bool track_varies(const EidolonVrmaTrack *track, bool *varies) {
    float baseline[4];
    float sample[4];
    if (track == NULL || varies == NULL || track->times == NULL || track->values == NULL ||
        track->key_count == 0U ||
        (track->path == EIDOLON_VRMA_TRACK_ROTATION && track->component_count != 4U) ||
        (track->path == EIDOLON_VRMA_TRACK_HIPS_TRANSLATION &&
         (track->role != EIDOLON_HUMANOID_ROLE_HIPS || track->component_count != 3U)) ||
        (track->interpolation == EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE
             ? track->values_per_key != 3U
             : track->values_per_key != 1U) ||
        !sample_track(track, track->times[0], baseline)) {
        return false;
    }
    *varies = false;
    for (size_t key = 1U; key < track->key_count; ++key) {
        if (!isfinite(track->times[key]) || track->times[key] <= track->times[key - 1U] ||
            !sample_track(track, track->times[key], sample)) {
            return false;
        }
        if (sampled_values_differ(track->path, baseline, sample)) {
            *varies = true;
        }
        if (track->interpolation == EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE) {
            const float start = track->times[key - 1U];
            const float span = track->times[key] - start;
            for (size_t probe = 1U; probe < 4U; ++probe) {
                const float probe_time = start + span * (float)probe * 0.25F;
                if (!sample_track(track, probe_time, sample)) {
                    return false;
                }
                if (sampled_values_differ(track->path, baseline, sample)) {
                    *varies = true;
                }
            }
        }
    }
    return true;
}

bool eidolon_vrma_clip_coverage(const EidolonVrmaClip *clip, EidolonVrmaCoverage *coverage) {
    if (clip == NULL || coverage == NULL || clip->version != EIDOLON_VRMA_CLIP_VERSION ||
        clip->tracks == NULL || clip->track_count == 0U) {
        return false;
    }
    memset(coverage, 0, sizeof(*coverage));
    coverage->version = EIDOLON_VRMA_COVERAGE_VERSION;
    coverage->auxiliary_channel_count = clip->auxiliary_channel_count;
    for (size_t index = 0U; index < clip->track_count; ++index) {
        const EidolonVrmaTrack *track = &clip->tracks[index];
        bool varies = false;
        if (track->role < EIDOLON_HUMANOID_ROLE_HIPS ||
            track->role >= EIDOLON_HUMANOID_ROLE_COUNT || !track_varies(track, &varies)) {
            memset(coverage, 0, sizeof(*coverage));
            return false;
        }
        if (track->path == EIDOLON_VRMA_TRACK_HIPS_TRANSLATION) {
            coverage->has_hips_translation = true;
            coverage->hips_translation_varies = varies;
            coverage->tracked_chain_mask |= EIDOLON_VRMA_CHAIN_ROOT;
            if (varies) {
                coverage->varying_chain_mask |= EIDOLON_VRMA_CHAIN_ROOT;
            }
            continue;
        }
        const uint64_t role_bit = UINT64_C(1) << (uint32_t)track->role;
        const uint32_t chain = role_chain(track->role);
        coverage->rotation_roles |= role_bit;
        coverage->rotation_track_count += 1U;
        coverage->tracked_chain_mask |= chain;
        if (varies) {
            coverage->varying_rotation_roles |= role_bit;
            coverage->varying_rotation_track_count += 1U;
            coverage->varying_chain_mask |= chain;
        }
    }
    return true;
}

const char *eidolon_vrma_track_path_name(EidolonVrmaTrackPath path) {
    switch (path) {
    case EIDOLON_VRMA_TRACK_ROTATION:
        return "rotation";
    case EIDOLON_VRMA_TRACK_HIPS_TRANSLATION:
        return "hips_translation";
    }
    return "unknown";
}

const char *eidolon_vrma_interpolation_name(EidolonVrmaInterpolation interpolation) {
    switch (interpolation) {
    case EIDOLON_VRMA_INTERPOLATION_STEP:
        return "STEP";
    case EIDOLON_VRMA_INTERPOLATION_LINEAR:
        return "LINEAR";
    case EIDOLON_VRMA_INTERPOLATION_CUBIC_SPLINE:
        return "CUBICSPLINE";
    }
    return "unknown";
}

void eidolon_vrma_clip_destroy(EidolonVrmaClip *clip) {
    if (clip == NULL) {
        return;
    }
    for (size_t index = 0U; index < clip->track_count; ++index) {
        free(clip->tracks[index].times);
        free(clip->tracks[index].values);
    }
    free(clip->tracks);
    memset(clip, 0, sizeof(*clip));
}
