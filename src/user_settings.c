#include "user_settings.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t capacity, const char *format, ...) {
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(error, capacity, format, args);
    va_end(args);
}

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        --end;
    }
    *end = '\0';
    return text;
}

static bool parse_int(const char *text, int *value) {
    char *end = NULL;
    errno = 0;
    const long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool parse_uint(const char *text, unsigned int *value) {
    char *end = NULL;
    errno = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT_MAX) {
        return false;
    }
    *value = (unsigned int)parsed;
    return true;
}

static bool parse_float(const char *text, float *value) {
    char *end = NULL;
    errno = 0;
    const float parsed = strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_bool(const char *text, bool *value) {
    if (SDL_strcasecmp(text, "true") == 0 || strcmp(text, "1") == 0) {
        *value = true;
        return true;
    }
    if (SDL_strcasecmp(text, "false") == 0 || strcmp(text, "0") == 0) {
        *value = false;
        return true;
    }
    return false;
}

static bool parse_render_mode(const char *text, int *value) {
    if (SDL_strcasecmp(text, "sprite") == 0) {
        *value = 0;
        return true;
    }
    if (SDL_strcasecmp(text, "portrait") == 0 || SDL_strcasecmp(text, "2d") == 0) {
        *value = 1;
        return true;
    }
    if (SDL_strcasecmp(text, "model_3d") == 0 || SDL_strcasecmp(text, "3d") == 0) {
        *value = 2;
        return true;
    }
    return parse_int(text, value);
}

static const char *render_mode_name(int value) {
    switch (value) {
    case 0:
        return "sprite";
    case 1:
        return "portrait";
    case 2:
        return "model_3d";
    default:
        return "invalid";
    }
}

bool eidolon_user_settings_is_overridden(const EidolonUserSettings *settings,
                                         EidolonUserSettingField field) {
    return settings != NULL && (settings->overrides & (uint32_t)field) != 0U;
}

void eidolon_user_settings_defaults(EidolonUserSettings *settings) {
    *settings = (EidolonUserSettings){
        .render_mode = 1,
        .display_scale = 1.0F,
        .portrait_face_mode = false,
        .model_render_resolution = 1024,
        .dialogue_theme = 0,
        .dialogue_movement = 2,
        .dialogue_hold_ms = 3000U,
    };
}

bool eidolon_user_settings_parse(const char *text, size_t length, EidolonUserSettings *settings,
                                 char *error, size_t error_capacity) {
    if (text == NULL || settings == NULL) {
        set_error(error, error_capacity, "settings input is null");
        return false;
    }
    char *copy = malloc(length + 1U);
    if (copy == NULL) {
        set_error(error, error_capacity, "out of memory");
        return false;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';

    EidolonUserSettings candidate = *settings;
    candidate.overrides = 0U;
    int version = 0;
    bool version_seen = false;
    size_t line_number = 0U;
    char *cursor = copy;
    bool valid = true;
    while (*cursor != '\0') {
        ++line_number;
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }
        line = trim(line);
        if (*line == '\0' || *line == '#') {
            continue;
        }
        char *separator = strchr(line, '=');
        if (separator == NULL) {
            set_error(error, error_capacity, "line %zu: expected key = value", line_number);
            valid = false;
            break;
        }
        *separator = '\0';
        const char *key = trim(line);
        const char *value = trim(separator + 1);
        bool parsed = true;
        if (strcmp(key, "version") == 0) {
            parsed = parse_int(value, &version);
            version_seen = parsed;
        } else if (strcmp(key, "preferred_renderer") == 0 || strcmp(key, "render_mode") == 0) {
            parsed = parse_render_mode(value, &candidate.render_mode);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_RENDER_MODE;
            }
        } else if (strcmp(key, "display_scale") == 0) {
            parsed = parse_float(value, &candidate.display_scale);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_DISPLAY_SCALE;
            }
        } else if (strcmp(key, "portrait_face_mode") == 0) {
            parsed = parse_bool(value, &candidate.portrait_face_mode);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE;
            }
        } else if (strcmp(key, "model_render_resolution") == 0) {
            parsed = parse_int(value, &candidate.model_render_resolution);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION;
            }
        } else if (strcmp(key, "model_yaw_degrees") == 0) {
            parsed = parse_float(value, &candidate.model_yaw_degrees);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_MODEL_YAW;
            }
        } else if (strcmp(key, "model_pitch_degrees") == 0) {
            parsed = parse_float(value, &candidate.model_pitch_degrees);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_MODEL_PITCH;
            }
        } else if (strcmp(key, "model_roll_degrees") == 0) {
            parsed = parse_float(value, &candidate.model_roll_degrees);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_MODEL_ROLL;
            }
        } else if (strcmp(key, "dialogue_theme") == 0) {
            parsed = parse_int(value, &candidate.dialogue_theme);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_DIALOGUE_THEME;
            }
        } else if (strcmp(key, "dialogue_movement") == 0) {
            parsed = parse_int(value, &candidate.dialogue_movement);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT;
            }
        } else if (strcmp(key, "dialogue_hold_ms") == 0) {
            parsed = parse_uint(value, &candidate.dialogue_hold_ms);
            if (parsed) {
                candidate.overrides |= EIDOLON_USER_SETTING_DIALOGUE_HOLD;
            }
        }
        if (!parsed) {
            set_error(error, error_capacity, "line %zu: invalid value for %s", line_number, key);
            valid = false;
            break;
        }
    }
    if (valid && (!version_seen || version != EIDOLON_USER_SETTINGS_VERSION)) {
        set_error(error, error_capacity, "unsupported or missing settings version");
        valid = false;
    }
    if (valid) {
        *settings = candidate;
        if (error != NULL && error_capacity > 0U) {
            error[0] = '\0';
        }
    }
    free(copy);
    return valid;
}

bool eidolon_user_settings_resolve_path(char *path, size_t capacity) {
    char *directory = SDL_GetPrefPath("snoopiedoom", "Eidolon");
    if (directory == NULL) {
        return false;
    }
    const int written = SDL_snprintf(path, capacity, "%ssettings.cfg", directory);
    SDL_free(directory);
    return written > 0 && (size_t)written < capacity;
}

bool eidolon_user_settings_load(const char *path, EidolonUserSettings *settings, char *error,
                                size_t error_capacity) {
    size_t size = 0U;
    char *text = SDL_LoadFile(path, &size);
    if (text == NULL) {
        set_error(error, error_capacity, "%s", SDL_GetError());
        return false;
    }
    const bool parsed = eidolon_user_settings_parse(text, size, settings, error, error_capacity);
    SDL_free(text);
    return parsed;
}

bool eidolon_user_settings_save(const char *path, const EidolonUserSettings *settings, char *error,
                                size_t error_capacity) {
    char text[1024];
    size_t used = 0U;
#define APPEND_SETTING(...)                                                                        \
    do {                                                                                           \
        const int written = SDL_snprintf(text + used, sizeof(text) - used, __VA_ARGS__);           \
        if (written <= 0 || (size_t)written >= sizeof(text) - used) {                              \
            set_error(error, error_capacity, "serialized settings exceed buffer");                 \
            return false;                                                                          \
        }                                                                                          \
        used += (size_t)written;                                                                   \
    } while (0)

    APPEND_SETTING("# Sparse Eidolon user overrides. Missing keys inherit system defaults.\n");
    APPEND_SETTING("version = %d\n", EIDOLON_USER_SETTINGS_VERSION);
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_RENDER_MODE)) {
        APPEND_SETTING("preferred_renderer = %s\n", render_mode_name(settings->render_mode));
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DISPLAY_SCALE)) {
        APPEND_SETTING("display_scale = %.4f\n", settings->display_scale);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE)) {
        APPEND_SETTING("portrait_face_mode = %s\n",
                       settings->portrait_face_mode ? "true" : "false");
    }
    if (eidolon_user_settings_is_overridden(settings,
                                            EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION)) {
        APPEND_SETTING("model_render_resolution = %d\n", settings->model_render_resolution);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_MODEL_YAW)) {
        APPEND_SETTING("model_yaw_degrees = %.4f\n", settings->model_yaw_degrees);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_MODEL_PITCH)) {
        APPEND_SETTING("model_pitch_degrees = %.4f\n", settings->model_pitch_degrees);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_MODEL_ROLL)) {
        APPEND_SETTING("model_roll_degrees = %.4f\n", settings->model_roll_degrees);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DIALOGUE_THEME)) {
        APPEND_SETTING("dialogue_theme = %d\n", settings->dialogue_theme);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT)) {
        APPEND_SETTING("dialogue_movement = %d\n", settings->dialogue_movement);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DIALOGUE_HOLD)) {
        APPEND_SETTING("dialogue_hold_ms = %u\n", settings->dialogue_hold_ms);
    }
#undef APPEND_SETTING

    if (used == 0U) {
        set_error(error, error_capacity, "serialized settings exceed buffer");
        return false;
    }
    if (!SDL_SaveFile(path, text, used)) {
        set_error(error, error_capacity, "%s", SDL_GetError());
        return false;
    }
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}
