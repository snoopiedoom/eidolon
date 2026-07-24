#include "vrm_body.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct JsonSpan {
    const char *begin;
    const char *end;
} JsonSpan;

typedef struct BoneName {
    EidolonVrmHumanBone bone;
    const char *name;
    bool required;
} BoneName;

static const BoneName BONE_NAMES[] = {
    {EIDOLON_VRM_BONE_HIPS, "hips", true},
    {EIDOLON_VRM_BONE_SPINE, "spine", true},
    {EIDOLON_VRM_BONE_CHEST, "chest", false},
    {EIDOLON_VRM_BONE_UPPER_CHEST, "upperChest", false},
    {EIDOLON_VRM_BONE_NECK, "neck", false},
    {EIDOLON_VRM_BONE_HEAD, "head", true},
    {EIDOLON_VRM_BONE_LEFT_EYE, "leftEye", false},
    {EIDOLON_VRM_BONE_RIGHT_EYE, "rightEye", false},
    {EIDOLON_VRM_BONE_LEFT_UPPER_LEG, "leftUpperLeg", true},
    {EIDOLON_VRM_BONE_LEFT_LOWER_LEG, "leftLowerLeg", true},
    {EIDOLON_VRM_BONE_LEFT_FOOT, "leftFoot", true},
    {EIDOLON_VRM_BONE_RIGHT_UPPER_LEG, "rightUpperLeg", true},
    {EIDOLON_VRM_BONE_RIGHT_LOWER_LEG, "rightLowerLeg", true},
    {EIDOLON_VRM_BONE_RIGHT_FOOT, "rightFoot", true},
    {EIDOLON_VRM_BONE_LEFT_SHOULDER, "leftShoulder", false},
    {EIDOLON_VRM_BONE_LEFT_UPPER_ARM, "leftUpperArm", true},
    {EIDOLON_VRM_BONE_LEFT_LOWER_ARM, "leftLowerArm", true},
    {EIDOLON_VRM_BONE_LEFT_HAND, "leftHand", true},
    {EIDOLON_VRM_BONE_RIGHT_SHOULDER, "rightShoulder", false},
    {EIDOLON_VRM_BONE_RIGHT_UPPER_ARM, "rightUpperArm", true},
    {EIDOLON_VRM_BONE_RIGHT_LOWER_ARM, "rightLowerArm", true},
    {EIDOLON_VRM_BONE_RIGHT_HAND, "rightHand", true},
};

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
            cursor += 1;
            if (cursor >= end) {
                return NULL;
            }
            cursor += 1;
        } else if (*cursor == '"') {
            return cursor + 1;
        } else {
            cursor += 1;
        }
    }
    return NULL;
}

static const char *skip_value(const char *cursor, const char *end, unsigned depth) {
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
        cursor = skip_value(cursor, end, depth + 1U);
        if (cursor == NULL) {
            return NULL;
        }
        cursor = skip_space(cursor, end);
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

static bool json_float(JsonSpan span, float *value) {
    char buffer[64];
    char *parsed_end = NULL;
    const char *begin = skip_space(span.begin, span.end);
    const size_t length = (size_t)(span.end - begin);
    float parsed;
    if (value == NULL || length == 0U || length >= sizeof(buffer)) {
        return false;
    }
    memcpy(buffer, begin, length);
    buffer[length] = '\0';
    parsed = strtof(buffer, &parsed_end);
    if (parsed_end == buffer || *parsed_end != '\0' || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

static int hex_digit(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool append_utf8(char *output, size_t capacity, size_t *length, uint32_t codepoint) {
    unsigned char bytes[4];
    size_t count;
    if (codepoint <= 0x7fU) {
        bytes[0] = (unsigned char)codepoint;
        count = 1U;
    } else if (codepoint <= 0x7ffU) {
        bytes[0] = (unsigned char)(0xc0U | (codepoint >> 6U));
        bytes[1] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 2U;
    } else {
        bytes[0] = (unsigned char)(0xe0U | (codepoint >> 12U));
        bytes[1] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        bytes[2] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 3U;
    }
    if (*length + count >= capacity) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        output[*length] = (char)bytes[index];
        *length += 1U;
    }
    return true;
}

static bool json_string(JsonSpan span, char *output, size_t capacity) {
    const char *cursor = span.begin;
    size_t length = 0U;
    if (output == NULL || capacity == 0U || cursor >= span.end || *cursor != '"' ||
        span.end[-1] != '"') {
        return false;
    }
    cursor += 1;
    while (cursor < span.end - 1) {
        unsigned char character = (unsigned char)*cursor;
        cursor += 1;
        if (character != '\\') {
            if (length + 1U >= capacity) {
                return false;
            }
            output[length] = (char)character;
            length += 1U;
            continue;
        }
        if (cursor >= span.end - 1) {
            return false;
        }
        character = (unsigned char)*cursor;
        cursor += 1;
        if (character == 'u') {
            uint32_t codepoint = 0U;
            if (cursor + 4 > span.end - 1) {
                return false;
            }
            for (size_t index = 0; index < 4U; ++index) {
                const int digit = hex_digit(cursor[index]);
                if (digit < 0) {
                    return false;
                }
                codepoint = (codepoint << 4U) | (uint32_t)digit;
            }
            cursor += 4;
            if (!append_utf8(output, capacity, &length, codepoint)) {
                return false;
            }
        } else {
            const char decoded = character == 'n'   ? '\n'
                                 : character == 'r' ? '\r'
                                 : character == 't' ? '\t'
                                                    : (char)character;
            if (length + 1U >= capacity) {
                return false;
            }
            output[length] = decoded;
            length += 1U;
        }
    }
    output[length] = '\0';
    return true;
}

static bool first_array_string(JsonSpan array, char *output, size_t capacity) {
    const char *cursor = skip_space(array.begin, array.end);
    JsonSpan first;
    if (cursor >= array.end || *cursor != '[') {
        return false;
    }
    cursor = skip_space(cursor + 1, array.end);
    first.begin = cursor;
    first.end = skip_string(cursor, array.end);
    return first.end != NULL && json_string(first, output, capacity);
}

static const cgltf_extension *find_extension(const cgltf_data *data, const char *name) {
    for (cgltf_size index = 0; index < data->data_extensions_count; ++index) {
        const cgltf_extension *extension = &data->data_extensions[index];
        if (extension->name != NULL && extension->data != NULL &&
            strcmp(extension->name, name) == 0) {
            return extension;
        }
    }
    return NULL;
}

static bool parse_expression_binds(JsonSpan expression, EidolonVrmExpressionBind *binds,
                                   size_t *bind_count) {
    JsonSpan array;
    const char *cursor;
    if (!object_member(expression, "morphTargetBinds", &array)) {
        *bind_count = 0U;
        return true;
    }
    cursor = skip_space(array.begin, array.end);
    if (cursor >= array.end || *cursor != '[') {
        return false;
    }
    cursor += 1;
    *bind_count = 0U;
    for (;;) {
        JsonSpan bind;
        JsonSpan node_span;
        JsonSpan index_span;
        JsonSpan weight_span;
        int64_t node;
        int64_t target;
        float weight;
        const char *end;
        cursor = skip_space(cursor, array.end);
        if (cursor >= array.end || *cursor == ']') {
            return true;
        }
        if (*bind_count >= EIDOLON_VRM_EXPRESSION_BIND_CAPACITY) {
            return false;
        }
        bind.begin = cursor;
        end = skip_value(cursor, array.end, 0U);
        if (end == NULL) {
            return false;
        }
        bind.end = end;
        if (!object_member(bind, "node", &node_span) ||
            !object_member(bind, "index", &index_span) ||
            !object_member(bind, "weight", &weight_span) || !json_integer(node_span, &node) ||
            !json_integer(index_span, &target) || !json_float(weight_span, &weight) || node < 0 ||
            target < 0 || weight < 0.0F || weight > 1.0F) {
            return false;
        }
        binds[*bind_count].node = (size_t)node;
        binds[*bind_count].target = (size_t)target;
        binds[*bind_count].weight = weight;
        *bind_count += 1U;
        cursor = skip_space(end, array.end);
        if (cursor < array.end && *cursor == ',') {
            cursor += 1;
        }
    }
}

static uint64_t hash_text(const char *text) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool parse_metadata(JsonSpan root, EidolonVrmBody *body) {
    JsonSpan meta;
    JsonSpan value;
    if (!object_member(root, "meta", &meta) || !object_member(meta, "name", &value) ||
        !json_string(value, body->name, sizeof(body->name)) ||
        !object_member(meta, "authors", &value) ||
        !first_array_string(value, body->author, sizeof(body->author)) ||
        !object_member(meta, "licenseUrl", &value) ||
        !json_string(value, body->license_url, sizeof(body->license_url))) {
        return false;
    }
    if (object_member(meta, "commercialUsage", &value)) {
        (void)json_string(value, body->commercial_usage, sizeof(body->commercial_usage));
    }
    if (object_member(meta, "creditNotation", &value)) {
        (void)json_string(value, body->credit_notation, sizeof(body->credit_notation));
    }
    return true;
}

static bool parse_bones(JsonSpan root, const cgltf_data *data, EidolonVrmBody *body, char *error,
                        size_t error_capacity) {
    JsonSpan humanoid;
    JsonSpan bones;
    if (!object_member(root, "humanoid", &humanoid) ||
        !object_member(humanoid, "humanBones", &bones)) {
        set_error(error, error_capacity, "VRMC_vrm humanoid.humanBones is missing");
        return false;
    }
    for (size_t index = 0; index < sizeof(BONE_NAMES) / sizeof(BONE_NAMES[0]); ++index) {
        const BoneName *mapping = &BONE_NAMES[index];
        JsonSpan bone;
        JsonSpan node_span;
        int64_t node;
        if (!object_member(bones, mapping->name, &bone)) {
            if (mapping->required) {
                set_error(error, error_capacity, "required VRM bone '%s' is missing",
                          mapping->name);
                return false;
            }
            continue;
        }
        if (!object_member(bone, "node", &node_span) || !json_integer(node_span, &node) ||
            node < 0 || (uint64_t)node >= (uint64_t)data->nodes_count) {
            set_error(error, error_capacity, "VRM bone '%s' has an invalid node", mapping->name);
            return false;
        }
        body->node_by_bone[(size_t)mapping->bone] = (int)node;
    }
    for (size_t left = 0; left < EIDOLON_VRM_BONE_COUNT; ++left) {
        if (body->node_by_bone[left] < 0) {
            continue;
        }
        for (size_t right = left + 1U; right < EIDOLON_VRM_BONE_COUNT; ++right) {
            if (body->node_by_bone[left] == body->node_by_bone[right]) {
                set_error(error, error_capacity, "VRM humanoid maps two roles to node %d",
                          body->node_by_bone[left]);
                return false;
            }
        }
    }
    return true;
}

static const cgltf_accessor *morph_position_accessor(const cgltf_primitive *primitive,
                                                     size_t target) {
    if (target >= (size_t)primitive->targets_count) {
        return NULL;
    }
    const cgltf_morph_target *morph = &primitive->targets[target];
    for (cgltf_size index = 0; index < morph->attributes_count; ++index) {
        const cgltf_attribute *attribute = &morph->attributes[index];
        if (attribute->type == cgltf_attribute_type_position && attribute->index == 0) {
            return attribute->data;
        }
    }
    return NULL;
}

static bool expression_bind_is_realizable(const cgltf_data *data,
                                          const EidolonVrmExpressionBind *bind) {
    if (bind->node >= (size_t)data->nodes_count) {
        return false;
    }
    const cgltf_node *node = &data->nodes[bind->node];
    if (node->mesh == NULL || node->mesh->primitives_count == 0U) {
        return false;
    }
    bool has_position_delta = false;
    for (cgltf_size index = 0; index < node->mesh->primitives_count; ++index) {
        const cgltf_primitive *primitive = &node->mesh->primitives[index];
        if (bind->target >= (size_t)primitive->targets_count) {
            return false;
        }
        const cgltf_accessor *delta = morph_position_accessor(primitive, bind->target);
        if (delta != NULL) {
            has_position_delta = true;
        }
    }
    return has_position_delta;
}

static bool parse_expressions(JsonSpan root, const cgltf_data *data, EidolonVrmBody *body) {
    JsonSpan expressions;
    JsonSpan preset;
    JsonSpan neutral;
    JsonSpan focused;
    if (!object_member(root, "expressions", &expressions) ||
        !object_member(expressions, "preset", &preset) ||
        !object_member(preset, "neutral", &neutral) ||
        !object_member(preset, "relaxed", &focused) ||
        !parse_expression_binds(neutral, body->neutral_binds, &body->neutral_bind_count) ||
        !parse_expression_binds(focused, body->focused_binds, &body->focused_bind_count)) {
        return false;
    }
    for (size_t index = 0; index < body->neutral_bind_count; ++index) {
        if (!expression_bind_is_realizable(data, &body->neutral_binds[index])) {
            return false;
        }
    }
    for (size_t index = 0; index < body->focused_bind_count; ++index) {
        if (!expression_bind_is_realizable(data, &body->focused_binds[index])) {
            return false;
        }
    }
    body->has_expression = body->neutral_bind_count > 0U && body->focused_bind_count > 0U;
    return true;
}

bool eidolon_vrm_body_parse(const cgltf_data *data, EidolonVrmBody *body, char *error,
                            size_t error_capacity) {
    const cgltf_extension *extension;
    JsonSpan root;
    JsonSpan spec_span;
    char spec[16];
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    if (data == NULL || body == NULL) {
        set_error(error, error_capacity, "VRM parse requires data and output");
        return false;
    }
    memset(body, 0, sizeof(*body));
    for (size_t index = 0; index < EIDOLON_VRM_BONE_COUNT; ++index) {
        body->node_by_bone[index] = -1;
    }
    extension = find_extension(data, "VRMC_vrm");
    if (extension == NULL) {
        set_error(error, error_capacity, "model is not VRM 1.0 (VRMC_vrm is missing)");
        return false;
    }
    root.begin = extension->data;
    root.end = extension->data + strlen(extension->data);
    if (skip_value(root.begin, root.end, 0U) == NULL ||
        !object_member(root, "specVersion", &spec_span) ||
        !json_string(spec_span, spec, sizeof(spec)) || strcmp(spec, "1.0") != 0) {
        set_error(error, error_capacity, "unsupported VRMC_vrm specVersion");
        return false;
    }
    if (!parse_metadata(root, body)) {
        set_error(error, error_capacity, "VRM 1.0 metadata is incomplete");
        return false;
    }
    if (!parse_bones(root, data, body, error, error_capacity)) {
        return false;
    }
    body->has_look_at = body->node_by_bone[EIDOLON_VRM_BONE_LEFT_EYE] >= 0 &&
                        body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_EYE] >= 0 &&
                        object_member(root, "lookAt", &spec_span);
    if (!parse_expressions(root, data, body)) {
        body->neutral_bind_count = 0U;
        body->focused_bind_count = 0U;
        body->has_expression = false;
    }
    body->has_spring_bones = find_extension(data, "VRMC_springBone") != NULL;
    body->has_mtoon = find_extension(data, "VRMC_materials_mtoon") != NULL;
    body->fingerprint = hash_text(extension->data);
    return true;
}

static bool node_position(const cgltf_data *data, int index, float position[3]) {
    cgltf_float world[16];
    if (index < 0 || (size_t)index >= (size_t)data->nodes_count) {
        return false;
    }
    cgltf_node_transform_world(&data->nodes[(size_t)index], world);
    position[0] = world[12];
    position[1] = world[13];
    position[2] = world[14];
    return isfinite(position[0]) && isfinite(position[1]) && isfinite(position[2]);
}

static float distance3(const float left[3], const float right[3]) {
    const float x = left[0] - right[0];
    const float y = left[1] - right[1];
    const float z = left[2] - right[2];
    return sqrtf(x * x + y * y + z * z);
}

static bool direction3(const float from[3], const float to[3], float result[3]) {
    const float length = distance3(from, to);
    if (length <= 0.0001F) {
        return false;
    }
    for (size_t axis = 0; axis < 3U; ++axis) {
        result[axis] = (to[axis] - from[axis]) / length;
    }
    return true;
}

static float dot3(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static bool cross3(const float left[3], const float right[3], float result[3]) {
    const float value[3] = {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
    const float length = sqrtf(dot3(value, value));
    if (length <= 0.0001F) {
        return false;
    }
    for (size_t axis = 0; axis < 3U; ++axis) {
        result[axis] = value[axis] / length;
    }
    return true;
}

bool eidolon_vrm_body_make_profile(const cgltf_data *data, const EidolonVrmBody *body,
                                   EidolonEprBodyProfile *profile, char *error,
                                   size_t error_capacity) {
    float upper[3];
    float lower[3];
    float hand[3];
    float left_upper[3];
    float hips[3];
    float head[3];
    if (data == NULL || body == NULL || profile == NULL ||
        !node_position(data, body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_UPPER_ARM], upper) ||
        !node_position(data, body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_LOWER_ARM], lower) ||
        !node_position(data, body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_HAND], hand) ||
        !node_position(data, body->node_by_bone[EIDOLON_VRM_BONE_LEFT_UPPER_ARM], left_upper) ||
        !node_position(data, body->node_by_bone[EIDOLON_VRM_BONE_HIPS], hips) ||
        !node_position(data, body->node_by_bone[EIDOLON_VRM_BONE_HEAD], head)) {
        set_error(error, error_capacity, "could not measure VRM right arm");
        return false;
    }
    memset(profile, 0, sizeof(*profile));
    profile->version = EIDOLON_EPR_BODY_PROFILE_VERSION;
    profile->fingerprint = body->fingerprint;
    memcpy(profile->shoulder, upper, sizeof(upper));
    memcpy(profile->head, head, sizeof(head));
    if (!direction3(left_upper, upper, profile->right) || !direction3(hips, head, profile->up)) {
        set_error(error, error_capacity, "VRM body axes are degenerate");
        return false;
    }
    {
        const float vertical = dot3(profile->right, profile->up);
        float length;
        for (size_t axis = 0; axis < 3U; ++axis) {
            profile->right[axis] -= profile->up[axis] * vertical;
        }
        length = sqrtf(dot3(profile->right, profile->right));
        if (length <= 0.0001F) {
            set_error(error, error_capacity, "VRM shoulder axis is degenerate");
            return false;
        }
        for (size_t axis = 0; axis < 3U; ++axis) {
            profile->right[axis] /= length;
        }
    }
    if (!cross3(profile->right, profile->up, profile->forward)) {
        set_error(error, error_capacity, "VRM forward axis is degenerate");
        return false;
    }
    profile->right_upper_arm_length = distance3(upper, lower);
    profile->right_lower_arm_length = distance3(lower, hand);
    profile->maximum_reach_ratio = 0.98F;
    profile->shoulder_limit_radians = 2.60F;
    profile->elbow_limit_radians = 2.70F;
    profile->has_required_humanoid = true;
    profile->has_right_arm = true;
    profile->has_eyes = body->has_look_at;
    profile->has_expression = body->has_expression;
    if (!isfinite(profile->right_upper_arm_length) || !isfinite(profile->right_lower_arm_length) ||
        profile->right_upper_arm_length <= 0.0001F || profile->right_lower_arm_length <= 0.0001F) {
        set_error(error, error_capacity, "VRM right arm has invalid measurements");
        return false;
    }
    return true;
}

int eidolon_vrm_body_node(const EidolonVrmBody *body, EidolonVrmHumanBone bone) {
    if (body == NULL || bone < EIDOLON_VRM_BONE_HIPS || bone >= EIDOLON_VRM_BONE_COUNT) {
        return -1;
    }
    return body->node_by_bone[(size_t)bone];
}
