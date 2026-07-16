#include "motion_config.h"

#include <stdarg.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum MotionConfigField {
    FIELD_VERSION,
    FIELD_SEED,
    FIELD_NEUTRAL_ARM_LOWER,
    FIELD_NEUTRAL_ELBOW_ADD,
    FIELD_BREATH_PERIOD,
    FIELD_BREATH_CHEST,
    FIELD_BREATH_NECK_COUNTER,
    FIELD_SWAY_PERIOD,
    FIELD_SWAY_SPINE,
    FIELD_SWAY_CHEST_COUNTER,
    FIELD_SWAY_HEAD,
    FIELD_COUNT,
    FIELD_UNKNOWN = -1,
} MotionConfigField;

static const char *FIELD_NAMES[FIELD_COUNT] = {
    "version",
    "seed",
    "neutral.arm_lower_deg",
    "neutral.elbow_add_deg",
    "idle.breath.period_s",
    "idle.breath.chest_deg",
    "idle.breath.neck_counter_deg",
    "idle.sway.period_s",
    "idle.sway.spine_deg",
    "idle.sway.chest_counter_deg",
    "idle.sway.head_deg",
};

static void set_error(char *error, size_t capacity, const char *format, ...) {
    if (capacity == 0) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    SDL_vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static char *trim(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    char *end = text + SDL_strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static MotionConfigField field_from_name(const char *name) {
    for (int field = 0; field < FIELD_COUNT; ++field) {
        if (SDL_strcmp(name, FIELD_NAMES[field]) == 0) {
            return (MotionConfigField)field;
        }
    }
    return FIELD_UNKNOWN;
}

static bool parse_u64(const char *text, uint64_t *value) {
    if (*text == '-' || *text == '+') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, 10);
    if (text == end || errno == ERANGE) {
        return false;
    }
    end = trim(end);
    if (*end != '\0') {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_float_range(const char *text, float minimum, float maximum, float *value) {
    errno = 0;
    char *end = NULL;
    const float parsed = strtof(text, &end);
    if (text == end || errno == ERANGE || !isfinite(parsed)) {
        return false;
    }
    end = trim(end);
    if (*end != '\0' || parsed < minimum || parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool assign_field(EidolonMotionConfig *config, MotionConfigField field, const char *value) {
    uint64_t integer = 0;
    switch (field) {
    case FIELD_VERSION:
        if (!parse_u64(value, &integer) || integer > UINT32_MAX) {
            return false;
        }
        config->version = (uint32_t)integer;
        return true;
    case FIELD_SEED:
        return parse_u64(value, &config->seed);
    case FIELD_NEUTRAL_ARM_LOWER:
        return parse_float_range(value, EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES,
                                 EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES,
                                 &config->neutral_arm_lower_degrees);
    case FIELD_NEUTRAL_ELBOW_ADD:
        return parse_float_range(value, EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES,
                                 EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES,
                                 &config->neutral_elbow_add_degrees);
    case FIELD_BREATH_PERIOD:
        return parse_float_range(value, 1.0F, 20.0F, &config->breath_period_seconds);
    case FIELD_BREATH_CHEST:
        return parse_float_range(value, EIDOLON_IDLE_ROTATION_MIN_DEGREES,
                                 EIDOLON_IDLE_ROTATION_MAX_DEGREES, &config->breath_chest_degrees);
    case FIELD_BREATH_NECK_COUNTER:
        return parse_float_range(value, EIDOLON_IDLE_ROTATION_MIN_DEGREES,
                                 EIDOLON_IDLE_ROTATION_MAX_DEGREES,
                                 &config->breath_neck_counter_degrees);
    case FIELD_SWAY_PERIOD:
        return parse_float_range(value, 2.0F, 60.0F, &config->sway_period_seconds);
    case FIELD_SWAY_SPINE:
        return parse_float_range(value, EIDOLON_IDLE_ROTATION_MIN_DEGREES,
                                 EIDOLON_IDLE_ROTATION_MAX_DEGREES, &config->sway_spine_degrees);
    case FIELD_SWAY_CHEST_COUNTER:
        return parse_float_range(value, EIDOLON_IDLE_ROTATION_MIN_DEGREES,
                                 EIDOLON_IDLE_ROTATION_MAX_DEGREES,
                                 &config->sway_chest_counter_degrees);
    case FIELD_SWAY_HEAD:
        return parse_float_range(value, EIDOLON_IDLE_ROTATION_MIN_DEGREES,
                                 EIDOLON_IDLE_ROTATION_MAX_DEGREES, &config->sway_head_degrees);
    case FIELD_COUNT:
    case FIELD_UNKNOWN:
        break;
    }
    return false;
}

static bool field_float_range(MotionConfigField field, float *minimum, float *maximum) {
    switch (field) {
    case FIELD_NEUTRAL_ARM_LOWER:
        *minimum = EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES;
        *maximum = EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES;
        return true;
    case FIELD_NEUTRAL_ELBOW_ADD:
        *minimum = EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES;
        *maximum = EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES;
        return true;
    case FIELD_BREATH_PERIOD:
        *minimum = 1.0F;
        *maximum = 20.0F;
        return true;
    case FIELD_SWAY_PERIOD:
        *minimum = 2.0F;
        *maximum = 60.0F;
        return true;
    case FIELD_BREATH_CHEST:
    case FIELD_BREATH_NECK_COUNTER:
    case FIELD_SWAY_SPINE:
    case FIELD_SWAY_CHEST_COUNTER:
    case FIELD_SWAY_HEAD:
        *minimum = EIDOLON_IDLE_ROTATION_MIN_DEGREES;
        *maximum = EIDOLON_IDLE_ROTATION_MAX_DEGREES;
        return true;
    case FIELD_VERSION:
    case FIELD_SEED:
    case FIELD_COUNT:
    case FIELD_UNKNOWN:
        return false;
    }
    return false;
}

void eidolon_motion_config_defaults(EidolonMotionConfig *config) {
    *config = (EidolonMotionConfig){
        .version = EIDOLON_MOTION_CONFIG_VERSION,
        .seed = 1337U,
        .neutral_arm_lower_degrees = 0.0F,
        .neutral_elbow_add_degrees = 0.0F,
        .breath_period_seconds = 3.696F,
        .breath_chest_degrees = 0.688F,
        .breath_neck_counter_degrees = 0.286F,
        .sway_period_seconds = 14.612F,
        .sway_spine_degrees = 0.458F,
        .sway_chest_counter_degrees = 0.344F,
        .sway_head_degrees = 0.688F,
    };
}

bool eidolon_motion_config_parse(const char *text, size_t size, EidolonMotionConfig *config,
                                 char *error, size_t error_capacity) {
    if (error_capacity > 0) {
        error[0] = '\0';
    }
    if (text == NULL || size == SIZE_MAX) {
        set_error(error, error_capacity, "invalid config input");
        return false;
    }

    char *copy = SDL_malloc(size + 1U);
    if (copy == NULL) {
        set_error(error, error_capacity, "out of memory parsing config");
        return false;
    }
    SDL_memcpy(copy, text, size);
    copy[size] = '\0';

    EidolonMotionConfig candidate = {0};
    uint64_t fields_seen = 0;
    size_t line_number = 0;
    char *cursor = copy;
    bool valid = true;
    while (*cursor != '\0') {
        ++line_number;
        char *line = cursor;
        char *newline = SDL_strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += SDL_strlen(cursor);
        }

        char *comment = SDL_strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }
        line = trim(line);
        if (*line == '\0') {
            continue;
        }

        char *equals = SDL_strchr(line, '=');
        if (equals == NULL || SDL_strchr(equals + 1, '=') != NULL) {
            set_error(error, error_capacity, "line %zu: expected one '='", line_number);
            valid = false;
            break;
        }
        *equals = '\0';
        const char *key = trim(line);
        const char *value = trim(equals + 1);
        if (*key == '\0' || *value == '\0') {
            set_error(error, error_capacity, "line %zu: empty key or value", line_number);
            valid = false;
            break;
        }

        const MotionConfigField field = field_from_name(key);
        if (field == FIELD_UNKNOWN) {
            set_error(error, error_capacity, "line %zu: unknown key '%s'", line_number, key);
            valid = false;
            break;
        }
        const uint64_t bit = UINT64_C(1) << (unsigned int)field;
        if ((fields_seen & bit) != 0U) {
            set_error(error, error_capacity, "line %zu: duplicate key '%s'", line_number, key);
            valid = false;
            break;
        }
        if (!assign_field(&candidate, field, value)) {
            float minimum = 0.0F;
            float maximum = 0.0F;
            if (field_float_range(field, &minimum, &maximum)) {
                set_error(error, error_capacity,
                          "line %zu: invalid value for '%s' (allowed %.3g..%.3g)", line_number, key,
                          minimum, maximum);
            } else {
                set_error(error, error_capacity,
                          "line %zu: invalid value for '%s' (expected unsigned integer)",
                          line_number, key);
            }
            valid = false;
            break;
        }
        fields_seen |= bit;
    }

    if (valid) {
        const uint64_t required = (UINT64_C(1) << FIELD_COUNT) - 1U;
        if (fields_seen != required) {
            for (int field = 0; field < FIELD_COUNT; ++field) {
                if ((fields_seen & (UINT64_C(1) << (unsigned int)field)) == 0U) {
                    set_error(error, error_capacity, "missing required key '%s'",
                              FIELD_NAMES[field]);
                    break;
                }
            }
            valid = false;
        } else if (candidate.version != EIDOLON_MOTION_CONFIG_VERSION) {
            set_error(error, error_capacity, "unsupported config version %u", candidate.version);
            valid = false;
        }
    }

    SDL_free(copy);
    if (!valid) {
        return false;
    }
    *config = candidate;
    return true;
}

static uint64_t hash_bytes(const void *data, size_t size) {
    const Uint8 *bytes = data;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static EidolonMotionConfigPollResult watch_error(EidolonMotionConfigWatch *watch, bool parse_error,
                                                 const char *message) {
    if (watch->error[0] != '\0' && watch->error_is_parse == parse_error &&
        SDL_strcmp(watch->error, message) == 0) {
        return EIDOLON_MOTION_CONFIG_UNCHANGED;
    }
    SDL_strlcpy(watch->error, message, sizeof(watch->error));
    watch->error_is_parse = parse_error;
    return EIDOLON_MOTION_CONFIG_ERROR;
}

void eidolon_motion_config_watch_init(EidolonMotionConfigWatch *watch) { SDL_zero(*watch); }

void eidolon_motion_config_watch_force_reload(EidolonMotionConfigWatch *watch) {
    watch->has_observed_hash = false;
    watch->next_poll_ms = 0;
}

EidolonMotionConfigPollResult eidolon_motion_config_watch_poll(EidolonMotionConfigWatch *watch,
                                                               const char *path, uint64_t now_ms,
                                                               EidolonMotionConfig *active_config) {
    if (now_ms < watch->next_poll_ms) {
        return EIDOLON_MOTION_CONFIG_UNCHANGED;
    }
    watch->next_poll_ms = now_ms + EIDOLON_MOTION_CONFIG_POLL_INTERVAL_MS;

    size_t size = 0;
    void *data = SDL_LoadFile(path, &size);
    if (data == NULL) {
        char message[EIDOLON_MOTION_CONFIG_ERROR_CAPACITY];
        SDL_snprintf(message, sizeof(message), "could not load config: %s", SDL_GetError());
        SDL_ClearError();
        return watch_error(watch, false, message);
    }

    const uint64_t hash = hash_bytes(data, size);
    if (watch->has_observed_hash && watch->observed_hash == hash) {
        if (watch->error[0] != '\0' && !watch->error_is_parse) {
            if (watch->active_hash == hash) {
                SDL_free(data);
                watch->error[0] = '\0';
                return EIDOLON_MOTION_CONFIG_RECOVERED;
            }
        } else {
            SDL_free(data);
            return EIDOLON_MOTION_CONFIG_UNCHANGED;
        }
    }
    watch->has_observed_hash = true;
    watch->observed_hash = hash;

    EidolonMotionConfig candidate;
    char error[EIDOLON_MOTION_CONFIG_ERROR_CAPACITY];
    if (!eidolon_motion_config_parse(data, size, &candidate, error, sizeof(error))) {
        SDL_free(data);
        return watch_error(watch, true, error);
    }
    SDL_free(data);

    *active_config = candidate;
    watch->active_hash = hash;
    watch->revision += 1U;
    watch->error[0] = '\0';
    watch->error_is_parse = false;
    return EIDOLON_MOTION_CONFIG_APPLIED;
}
