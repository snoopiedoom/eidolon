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

static const EidolonHumanoidRole LEGACY_ROLE_BY_BONE[EIDOLON_VRM_BONE_COUNT] = {
    [EIDOLON_VRM_BONE_HIPS] = EIDOLON_HUMANOID_ROLE_HIPS,
    [EIDOLON_VRM_BONE_SPINE] = EIDOLON_HUMANOID_ROLE_SPINE,
    [EIDOLON_VRM_BONE_CHEST] = EIDOLON_HUMANOID_ROLE_CHEST,
    [EIDOLON_VRM_BONE_UPPER_CHEST] = EIDOLON_HUMANOID_ROLE_UPPER_CHEST,
    [EIDOLON_VRM_BONE_NECK] = EIDOLON_HUMANOID_ROLE_NECK,
    [EIDOLON_VRM_BONE_HEAD] = EIDOLON_HUMANOID_ROLE_HEAD,
    [EIDOLON_VRM_BONE_LEFT_EYE] = EIDOLON_HUMANOID_ROLE_LEFT_EYE,
    [EIDOLON_VRM_BONE_RIGHT_EYE] = EIDOLON_HUMANOID_ROLE_RIGHT_EYE,
    [EIDOLON_VRM_BONE_LEFT_UPPER_LEG] = EIDOLON_HUMANOID_ROLE_LEFT_UPPER_LEG,
    [EIDOLON_VRM_BONE_LEFT_LOWER_LEG] = EIDOLON_HUMANOID_ROLE_LEFT_LOWER_LEG,
    [EIDOLON_VRM_BONE_LEFT_FOOT] = EIDOLON_HUMANOID_ROLE_LEFT_FOOT,
    [EIDOLON_VRM_BONE_RIGHT_UPPER_LEG] = EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_LEG,
    [EIDOLON_VRM_BONE_RIGHT_LOWER_LEG] = EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_LEG,
    [EIDOLON_VRM_BONE_RIGHT_FOOT] = EIDOLON_HUMANOID_ROLE_RIGHT_FOOT,
    [EIDOLON_VRM_BONE_LEFT_SHOULDER] = EIDOLON_HUMANOID_ROLE_LEFT_SHOULDER,
    [EIDOLON_VRM_BONE_LEFT_UPPER_ARM] = EIDOLON_HUMANOID_ROLE_LEFT_UPPER_ARM,
    [EIDOLON_VRM_BONE_LEFT_LOWER_ARM] = EIDOLON_HUMANOID_ROLE_LEFT_LOWER_ARM,
    [EIDOLON_VRM_BONE_LEFT_HAND] = EIDOLON_HUMANOID_ROLE_LEFT_HAND,
    [EIDOLON_VRM_BONE_RIGHT_SHOULDER] = EIDOLON_HUMANOID_ROLE_RIGHT_SHOULDER,
    [EIDOLON_VRM_BONE_RIGHT_UPPER_ARM] = EIDOLON_HUMANOID_ROLE_RIGHT_UPPER_ARM,
    [EIDOLON_VRM_BONE_RIGHT_LOWER_ARM] = EIDOLON_HUMANOID_ROLE_RIGHT_LOWER_ARM,
    [EIDOLON_VRM_BONE_RIGHT_HAND] = EIDOLON_HUMANOID_ROLE_RIGHT_HAND,
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

static bool json_bool(JsonSpan span, bool *value) {
    const char *begin = skip_space(span.begin, span.end);
    const size_t length = (size_t)(span.end - begin);
    if (value == NULL) {
        return false;
    }
    if (length == 4U && memcmp(begin, "true", 4U) == 0) {
        *value = true;
        return true;
    }
    if (length == 5U && memcmp(begin, "false", 5U) == 0) {
        *value = false;
        return true;
    }
    return false;
}

static bool json_array_count(JsonSpan array, size_t *count) {
    const char *cursor = skip_space(array.begin, array.end);
    size_t result = 0U;
    if (count == NULL || cursor >= array.end || *cursor != '[') {
        return false;
    }
    cursor += 1;
    for (;;) {
        const char *value_end;
        cursor = skip_space(cursor, array.end);
        if (cursor >= array.end) {
            return false;
        }
        if (*cursor == ']') {
            *count = result;
            return true;
        }
        value_end = skip_value(cursor, array.end, 0U);
        if (value_end == NULL) {
            return false;
        }
        result += 1U;
        cursor = skip_space(value_end, array.end);
        if (cursor < array.end && *cursor == ',') {
            cursor += 1;
            continue;
        }
        if (cursor < array.end && *cursor == ']') {
            *count = result;
            return true;
        }
        return false;
    }
}

static bool json_float_array(JsonSpan array, float *values, size_t value_count) {
    const char *cursor = skip_space(array.begin, array.end);
    if (values == NULL || cursor >= array.end || *cursor != '[') {
        return false;
    }
    cursor += 1;
    for (size_t index = 0; index < value_count; ++index) {
        JsonSpan value;
        cursor = skip_space(cursor, array.end);
        if (cursor >= array.end || *cursor == ']') {
            return false;
        }
        value.begin = cursor;
        value.end = skip_value(cursor, array.end, 0U);
        if (value.end == NULL || !json_float(value, &values[index])) {
            return false;
        }
        cursor = skip_space(value.end, array.end);
        if (index + 1U < value_count) {
            if (cursor >= array.end || *cursor != ',') {
                return false;
            }
            cursor += 1;
        }
    }
    cursor = skip_space(cursor, array.end);
    return cursor < array.end && *cursor == ']';
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
    if (codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
        return false;
    }
    if (codepoint <= 0x7fU) {
        bytes[0] = (unsigned char)codepoint;
        count = 1U;
    } else if (codepoint <= 0x7ffU) {
        bytes[0] = (unsigned char)(0xc0U | (codepoint >> 6U));
        bytes[1] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 2U;
    } else if (codepoint <= 0xffffU) {
        bytes[0] = (unsigned char)(0xe0U | (codepoint >> 12U));
        bytes[1] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        bytes[2] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 3U;
    } else {
        bytes[0] = (unsigned char)(0xf0U | (codepoint >> 18U));
        bytes[1] = (unsigned char)(0x80U | ((codepoint >> 12U) & 0x3fU));
        bytes[2] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        bytes[3] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 4U;
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
            if (codepoint >= 0xd800U && codepoint <= 0xdbffU) {
                uint32_t low = 0U;
                if (cursor + 6 > span.end - 1 || cursor[0] != '\\' || cursor[1] != 'u') {
                    return false;
                }
                cursor += 2;
                for (size_t index = 0; index < 4U; ++index) {
                    const int digit = hex_digit(cursor[index]);
                    if (digit < 0) {
                        return false;
                    }
                    low = (low << 4U) | (uint32_t)digit;
                }
                if (low < 0xdc00U || low > 0xdfffU) {
                    return false;
                }
                cursor += 4;
                codepoint = UINT32_C(0x10000) + ((codepoint - UINT32_C(0xd800)) << 10U) +
                            (low - UINT32_C(0xdc00));
            } else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU) {
                return false;
            }
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

static const cgltf_extension *find_scoped_extension(const cgltf_extension *extensions,
                                                    cgltf_size extension_count, const char *name) {
    for (cgltf_size index = 0; index < extension_count; ++index) {
        const cgltf_extension *extension = &extensions[index];
        if (extension->name != NULL && strcmp(extension->name, name) == 0) {
            return extension;
        }
    }
    return NULL;
}

static const cgltf_extension *find_extension(const cgltf_data *data, const char *name) {
    return find_scoped_extension(data->data_extensions, data->data_extensions_count, name);
}

const char *eidolon_vrm_capability_state_name(EidolonVrmCapabilityState state) {
    switch (state) {
    case EIDOLON_VRM_CAPABILITY_ABSENT:
        return "absent";
    case EIDOLON_VRM_CAPABILITY_DECLARED:
        return "declared";
    case EIDOLON_VRM_CAPABILITY_PARSED:
        return "parsed-not-executable";
    case EIDOLON_VRM_CAPABILITY_EXECUTABLE:
        return "executable";
    default:
        return "invalid";
    }
}

static bool parse_expression_binds(JsonSpan expression, EidolonVrmExpression *result) {
    JsonSpan array;
    const char *cursor;
    if (!object_member(expression, "morphTargetBinds", &array)) {
        return true;
    }
    cursor = skip_space(array.begin, array.end);
    if (cursor >= array.end || *cursor != '[') {
        return false;
    }
    cursor += 1;
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
        if (result->declared_morph_bind_count < EIDOLON_VRM_EXPRESSION_BIND_CAPACITY) {
            EidolonVrmExpressionBind *stored =
                &result->morph_binds[result->declared_morph_bind_count];
            stored->node = (size_t)node;
            stored->target = (size_t)target;
            stored->weight = weight;
            result->morph_bind_count += 1U;
        }
        result->declared_morph_bind_count += 1U;
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
    for (size_t index = 0U; index < EIDOLON_HUMANOID_ROLE_COUNT; ++index) {
        const EidolonHumanoidRole role = (EidolonHumanoidRole)index;
        const char *name = eidolon_humanoid_role_name(role);
        JsonSpan bone;
        JsonSpan node_span;
        int64_t node;
        if (!object_member(bones, name, &bone)) {
            if (eidolon_humanoid_role_required(role)) {
                set_error(error, error_capacity, "required VRM bone '%s' is missing", name);
                return false;
            }
            continue;
        }
        if (!object_member(bone, "node", &node_span) || !json_integer(node_span, &node) ||
            node < 0 || (uint64_t)node >= (uint64_t)data->nodes_count) {
            set_error(error, error_capacity, "VRM bone '%s' has an invalid node", name);
            return false;
        }
        body->node_by_role[index] = (int)node;
    }
    for (size_t bone = 0U; bone < EIDOLON_VRM_BONE_COUNT; ++bone) {
        body->node_by_bone[bone] = body->node_by_role[(size_t)LEGACY_ROLE_BY_BONE[bone]];
    }
    for (size_t left = 0U; left < EIDOLON_HUMANOID_ROLE_COUNT; ++left) {
        if (body->node_by_role[left] < 0) {
            continue;
        }
        for (size_t right = left + 1U; right < EIDOLON_HUMANOID_ROLE_COUNT; ++right) {
            if (body->node_by_role[left] == body->node_by_role[right]) {
                set_error(error, error_capacity, "VRM humanoid maps two roles to node %d",
                          body->node_by_role[left]);
                return false;
            }
        }
    }
    return true;
}

static const char *bone_name(EidolonVrmHumanBone bone) {
    for (size_t index = 0; index < sizeof(BONE_NAMES) / sizeof(BONE_NAMES[0]); ++index) {
        if (BONE_NAMES[index].bone == bone) {
            return BONE_NAMES[index].name;
        }
    }
    return "unknown";
}

static EidolonVrmHumanBone torso_parent(const EidolonVrmBody *body) {
    if (body->node_by_bone[EIDOLON_VRM_BONE_UPPER_CHEST] >= 0) {
        return EIDOLON_VRM_BONE_UPPER_CHEST;
    }
    if (body->node_by_bone[EIDOLON_VRM_BONE_CHEST] >= 0) {
        return EIDOLON_VRM_BONE_CHEST;
    }
    return EIDOLON_VRM_BONE_SPINE;
}

static EidolonVrmHumanBone expected_humanoid_parent(EidolonVrmHumanBone bone,
                                                    const EidolonVrmBody *body) {
    switch (bone) {
    case EIDOLON_VRM_BONE_HIPS:
        return EIDOLON_VRM_BONE_COUNT;
    case EIDOLON_VRM_BONE_SPINE:
        return EIDOLON_VRM_BONE_HIPS;
    case EIDOLON_VRM_BONE_CHEST:
        return EIDOLON_VRM_BONE_SPINE;
    case EIDOLON_VRM_BONE_UPPER_CHEST:
        return EIDOLON_VRM_BONE_CHEST;
    case EIDOLON_VRM_BONE_NECK:
        return torso_parent(body);
    case EIDOLON_VRM_BONE_HEAD:
        return body->node_by_bone[EIDOLON_VRM_BONE_NECK] >= 0 ? EIDOLON_VRM_BONE_NECK
                                                              : torso_parent(body);
    case EIDOLON_VRM_BONE_LEFT_EYE:
    case EIDOLON_VRM_BONE_RIGHT_EYE:
        return EIDOLON_VRM_BONE_HEAD;
    case EIDOLON_VRM_BONE_LEFT_UPPER_LEG:
    case EIDOLON_VRM_BONE_RIGHT_UPPER_LEG:
        return EIDOLON_VRM_BONE_HIPS;
    case EIDOLON_VRM_BONE_LEFT_LOWER_LEG:
        return EIDOLON_VRM_BONE_LEFT_UPPER_LEG;
    case EIDOLON_VRM_BONE_LEFT_FOOT:
        return EIDOLON_VRM_BONE_LEFT_LOWER_LEG;
    case EIDOLON_VRM_BONE_RIGHT_LOWER_LEG:
        return EIDOLON_VRM_BONE_RIGHT_UPPER_LEG;
    case EIDOLON_VRM_BONE_RIGHT_FOOT:
        return EIDOLON_VRM_BONE_RIGHT_LOWER_LEG;
    case EIDOLON_VRM_BONE_LEFT_SHOULDER:
    case EIDOLON_VRM_BONE_RIGHT_SHOULDER:
        return torso_parent(body);
    case EIDOLON_VRM_BONE_LEFT_UPPER_ARM:
        return body->node_by_bone[EIDOLON_VRM_BONE_LEFT_SHOULDER] >= 0
                   ? EIDOLON_VRM_BONE_LEFT_SHOULDER
                   : torso_parent(body);
    case EIDOLON_VRM_BONE_LEFT_LOWER_ARM:
        return EIDOLON_VRM_BONE_LEFT_UPPER_ARM;
    case EIDOLON_VRM_BONE_LEFT_HAND:
        return EIDOLON_VRM_BONE_LEFT_LOWER_ARM;
    case EIDOLON_VRM_BONE_RIGHT_UPPER_ARM:
        return body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_SHOULDER] >= 0
                   ? EIDOLON_VRM_BONE_RIGHT_SHOULDER
                   : torso_parent(body);
    case EIDOLON_VRM_BONE_RIGHT_LOWER_ARM:
        return EIDOLON_VRM_BONE_RIGHT_UPPER_ARM;
    case EIDOLON_VRM_BONE_RIGHT_HAND:
        return EIDOLON_VRM_BONE_RIGHT_LOWER_ARM;
    default:
        return EIDOLON_VRM_BONE_COUNT;
    }
}

static bool nearest_humanoid_parent(const cgltf_data *data, const EidolonVrmBody *body,
                                    EidolonVrmHumanBone bone, EidolonVrmHumanBone *parent_bone) {
    const int node_index = body->node_by_bone[(size_t)bone];
    const cgltf_node *node = &data->nodes[(size_t)node_index];
    size_t remaining = (size_t)data->nodes_count + 1U;
    node = node->parent;
    while (node != NULL && remaining > 0U) {
        const ptrdiff_t index = node - data->nodes;
        if (index < 0 || (size_t)index >= (size_t)data->nodes_count) {
            return false;
        }
        for (size_t candidate = 0; candidate < EIDOLON_VRM_BONE_COUNT; ++candidate) {
            if (body->node_by_bone[candidate] == (int)index) {
                *parent_bone = (EidolonVrmHumanBone)candidate;
                return true;
            }
        }
        node = node->parent;
        remaining -= 1U;
    }
    if (node != NULL) {
        return false;
    }
    *parent_bone = EIDOLON_VRM_BONE_COUNT;
    return true;
}

static bool validate_humanoid_structure(const cgltf_data *data, const EidolonVrmBody *body,
                                        char *error, size_t error_capacity) {
    for (size_t index = 0; index < (size_t)data->nodes_count; ++index) {
        if (data->nodes[index].has_matrix) {
            set_error(error, error_capacity,
                      "matrix node %zu is unsupported by the reference-avatar runtime", index);
            return false;
        }
    }
    for (size_t index = 0; index < EIDOLON_VRM_BONE_COUNT; ++index) {
        EidolonVrmHumanBone actual_parent;
        const EidolonVrmHumanBone bone = (EidolonVrmHumanBone)index;
        const int node_index = body->node_by_bone[index];
        if (node_index < 0) {
            continue;
        }
        const cgltf_node *node = &data->nodes[(size_t)node_index];
        for (size_t axis = 0; axis < 3U; ++axis) {
            if (!isfinite(node->scale[axis]) || node->scale[axis] <= 0.0F) {
                set_error(error, error_capacity,
                          "VRM humanoid bone '%s' has non-positive scale on axis %zu",
                          bone_name(bone), axis);
                return false;
            }
        }
        if (!nearest_humanoid_parent(data, body, bone, &actual_parent)) {
            set_error(error, error_capacity, "VRM humanoid bone '%s' has an invalid parent chain",
                      bone_name(bone));
            return false;
        }
        const EidolonVrmHumanBone expected_parent = expected_humanoid_parent(bone, body);
        if (actual_parent != expected_parent) {
            set_error(error, error_capacity,
                      "VRM humanoid bone '%s' must descend from '%s' (nearest humanoid parent is "
                      "'%s')",
                      bone_name(bone),
                      expected_parent < EIDOLON_VRM_BONE_COUNT ? bone_name(expected_parent)
                                                               : "root",
                      actual_parent < EIDOLON_VRM_BONE_COUNT ? bone_name(actual_parent) : "root");
            return false;
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

static bool expression_array_count(JsonSpan expression, const char *name, size_t *count) {
    JsonSpan array;
    *count = 0U;
    return !object_member(expression, name, &array) || json_array_count(array, count);
}

static void parse_expression(JsonSpan expression, const cgltf_data *data,
                             EidolonVrmExpression *result) {
    JsonSpan value;
    memset(result, 0, sizeof(*result));
    result->state = EIDOLON_VRM_CAPABILITY_DECLARED;
    if (object_member(expression, "isBinary", &value) && !json_bool(value, &result->is_binary)) {
        set_error(result->diagnostic, sizeof(result->diagnostic), "invalid isBinary value");
        return;
    }
    if (!parse_expression_binds(expression, result) ||
        !expression_array_count(expression, "materialColorBinds",
                                &result->material_color_bind_count) ||
        !expression_array_count(expression, "textureTransformBinds",
                                &result->texture_transform_bind_count)) {
        set_error(result->diagnostic, sizeof(result->diagnostic),
                  "expression bind declaration is malformed");
        return;
    }

    result->state = EIDOLON_VRM_CAPABILITY_PARSED;
    if (result->declared_morph_bind_count > EIDOLON_VRM_EXPRESSION_BIND_CAPACITY) {
        set_error(result->diagnostic, sizeof(result->diagnostic),
                  "morph bind count %zu exceeds executable capacity %u",
                  result->declared_morph_bind_count, EIDOLON_VRM_EXPRESSION_BIND_CAPACITY);
        return;
    }
    for (size_t index = 0; index < result->morph_bind_count; ++index) {
        if (!expression_bind_is_realizable(data, &result->morph_binds[index])) {
            set_error(result->diagnostic, sizeof(result->diagnostic),
                      "morph bind %zu has no executable position target", index);
            return;
        }
    }
    if (result->material_color_bind_count > 0U || result->texture_transform_bind_count > 0U) {
        result->state = EIDOLON_VRM_CAPABILITY_DECLARED;
        set_error(result->diagnostic, sizeof(result->diagnostic),
                  "material-color/texture-transform binds are declared but not parsed or "
                  "executable");
        return;
    }
    if (result->morph_bind_count == 0U) {
        set_error(result->diagnostic, sizeof(result->diagnostic),
                  "expression has no executable position-morph binds");
        return;
    }
    result->state = EIDOLON_VRM_CAPABILITY_EXECUTABLE;
    set_error(result->diagnostic, sizeof(result->diagnostic), "%zu position-morph bind(s)",
              result->morph_bind_count);
}

static void expression_absent(EidolonVrmExpression *expression) {
    memset(expression, 0, sizeof(*expression));
    expression->state = EIDOLON_VRM_CAPABILITY_ABSENT;
    set_error(expression->diagnostic, sizeof(expression->diagnostic), "not declared");
}

static void parse_expressions(JsonSpan root, const cgltf_data *data, EidolonVrmBody *body) {
    JsonSpan expressions;
    JsonSpan preset;
    JsonSpan expression;
    expression_absent(&body->neutral_expression);
    expression_absent(&body->relaxed_expression);
    if (!object_member(root, "expressions", &expressions) ||
        !object_member(expressions, "preset", &preset)) {
        return;
    }
    if (object_member(preset, "neutral", &expression)) {
        parse_expression(expression, data, &body->neutral_expression);
    }
    if (object_member(preset, "relaxed", &expression)) {
        parse_expression(expression, data, &body->relaxed_expression);
    }
}

static bool parse_range_map(JsonSpan look_at, const char *name, EidolonVrmLookAtRangeMap *map) {
    JsonSpan object;
    JsonSpan value;
    memset(map, 0, sizeof(*map));
    if (!object_member(look_at, name, &object)) {
        return true;
    }
    map->present = true;
    if (object_member(object, "inputMaxValue", &value)) {
        if (!json_float(value, &map->input_max_degrees) || map->input_max_degrees < 0.0F ||
            map->input_max_degrees > 180.0F) {
            return false;
        }
        map->has_input_max = true;
    }
    if (object_member(object, "outputScale", &value)) {
        if (!json_float(value, &map->output_scale)) {
            return false;
        }
        map->has_output_scale = true;
    }
    return true;
}

static void parse_look_at(JsonSpan root, EidolonVrmBody *body) {
    JsonSpan look_at;
    JsonSpan value;
    char type[16];
    memset(&body->look_at, 0, sizeof(body->look_at));
    body->look_at.state = EIDOLON_VRM_CAPABILITY_ABSENT;
    set_error(body->look_at.diagnostic, sizeof(body->look_at.diagnostic), "not declared");
    if (!object_member(root, "lookAt", &look_at)) {
        return;
    }
    body->look_at.state = EIDOLON_VRM_CAPABILITY_DECLARED;
    if (object_member(look_at, "type", &value)) {
        if (!json_string(value, type, sizeof(type))) {
            set_error(body->look_at.diagnostic, sizeof(body->look_at.diagnostic),
                      "lookAt.type is malformed");
            return;
        }
        if (strcmp(type, "bone") == 0) {
            body->look_at.type = EIDOLON_VRM_LOOK_AT_BONE;
        } else if (strcmp(type, "expression") == 0) {
            body->look_at.type = EIDOLON_VRM_LOOK_AT_EXPRESSION;
        } else {
            set_error(body->look_at.diagnostic, sizeof(body->look_at.diagnostic),
                      "lookAt.type '%s' is unsupported", type);
            return;
        }
    }
    if (object_member(look_at, "offsetFromHeadBone", &value)) {
        if (!json_float_array(value, body->look_at.offset_from_head, 3U)) {
            set_error(body->look_at.diagnostic, sizeof(body->look_at.diagnostic),
                      "lookAt.offsetFromHeadBone is malformed");
            return;
        }
        body->look_at.has_offset = true;
    }
    if (!parse_range_map(look_at, "rangeMapHorizontalInner", &body->look_at.horizontal_inner) ||
        !parse_range_map(look_at, "rangeMapHorizontalOuter", &body->look_at.horizontal_outer) ||
        !parse_range_map(look_at, "rangeMapVerticalDown", &body->look_at.vertical_down) ||
        !parse_range_map(look_at, "rangeMapVerticalUp", &body->look_at.vertical_up)) {
        set_error(body->look_at.diagnostic, sizeof(body->look_at.diagnostic),
                  "lookAt range map is malformed");
        return;
    }
    body->look_at.state = EIDOLON_VRM_CAPABILITY_PARSED;
    set_error(body->look_at.diagnostic, sizeof(body->look_at.diagnostic),
              "authored %s look-at parsed; runtime degrades to head-only",
              body->look_at.type == EIDOLON_VRM_LOOK_AT_BONE         ? "bone"
              : body->look_at.type == EIDOLON_VRM_LOOK_AT_EXPRESSION ? "expression"
                                                                     : "unspecified");
}

static bool parse_mtoon(const cgltf_data *data, EidolonVrmBody *body, char *error,
                        size_t error_capacity) {
    size_t parsed_count = 0U;
    body->material_count = (size_t)data->materials_count;
    if (body->material_count > 0U) {
        body->mtoon_material_states =
            calloc(body->material_count, sizeof(*body->mtoon_material_states));
        if (body->mtoon_material_states == NULL) {
            set_error(error, error_capacity, "out of memory while reporting MToon materials");
            return false;
        }
    }
    for (size_t index = 0; index < body->material_count; ++index) {
        const cgltf_material *material = &data->materials[index];
        const cgltf_extension *extension = find_scoped_extension(
            material->extensions, material->extensions_count, "VRMC_materials_mtoon");
        if (extension == NULL) {
            body->mtoon_material_states[index] = EIDOLON_VRM_CAPABILITY_ABSENT;
            continue;
        }
        body->mtoon_material_count += 1U;
        body->mtoon_material_states[index] = EIDOLON_VRM_CAPABILITY_DECLARED;
        if (extension->data != NULL) {
            JsonSpan root = {extension->data, extension->data + strlen(extension->data)};
            JsonSpan spec_span;
            char spec[16];
            if (skip_value(root.begin, root.end, 0U) != NULL &&
                object_member(root, "specVersion", &spec_span) &&
                json_string(spec_span, spec, sizeof(spec)) && strcmp(spec, "1.0") == 0) {
                body->mtoon_material_states[index] = EIDOLON_VRM_CAPABILITY_PARSED;
                parsed_count += 1U;
            }
        }
    }
    if (body->mtoon_material_count == 0U) {
        body->mtoon_state = EIDOLON_VRM_CAPABILITY_ABSENT;
    } else if (parsed_count == body->mtoon_material_count) {
        body->mtoon_state = EIDOLON_VRM_CAPABILITY_PARSED;
    } else {
        body->mtoon_state = EIDOLON_VRM_CAPABILITY_DECLARED;
    }
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
    for (size_t index = 0U; index < EIDOLON_HUMANOID_ROLE_COUNT; ++index) {
        body->node_by_role[index] = -1;
    }
    extension = find_extension(data, "VRMC_vrm");
    if (extension == NULL || extension->data == NULL) {
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
    if (!validate_humanoid_structure(data, body, error, error_capacity)) {
        return false;
    }
    body->eye_bones_present = body->node_by_bone[EIDOLON_VRM_BONE_LEFT_EYE] >= 0 &&
                              body->node_by_bone[EIDOLON_VRM_BONE_RIGHT_EYE] >= 0;
    parse_look_at(root, body);
    parse_expressions(root, data, body);
    body->spring_bones_state = find_extension(data, "VRMC_springBone") != NULL
                                   ? EIDOLON_VRM_CAPABILITY_DECLARED
                                   : EIDOLON_VRM_CAPABILITY_ABSENT;
    body->node_constraints_state = EIDOLON_VRM_CAPABILITY_ABSENT;
    for (size_t index = 0; index < (size_t)data->nodes_count; ++index) {
        const cgltf_node *node = &data->nodes[index];
        if (find_scoped_extension(node->extensions, node->extensions_count,
                                  "VRMC_node_constraint") != NULL) {
            body->node_constraints_state = EIDOLON_VRM_CAPABILITY_DECLARED;
            break;
        }
    }
    if (!parse_mtoon(data, body, error, error_capacity)) {
        eidolon_vrm_body_destroy(body);
        return false;
    }
    body->fingerprint = hash_text(extension->data);
    return true;
}

void eidolon_vrm_body_destroy(EidolonVrmBody *body) {
    if (body == NULL) {
        return;
    }
    free(body->mtoon_material_states);
    memset(body, 0, sizeof(*body));
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
    profile->has_eyes = body->look_at.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE;
    profile->has_expression = body->relaxed_expression.state == EIDOLON_VRM_CAPABILITY_EXECUTABLE;
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
