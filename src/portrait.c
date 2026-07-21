#include "portrait.h"

#include "log.h"
#include "portrait_motion.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define PORTRAIT_POLL_INTERVAL_MS 250U

struct EidolonPortraitRenderer {
    SDL_Renderer *renderer;
    SDL_Texture *textures[EIDOLON_PORTRAIT_MAX_EXPRESSIONS];
    EidolonPortraitConfig config;
    char config_path[EIDOLON_PORTRAIT_PATH_CAPACITY];
    char asset_directory[EIDOLON_PORTRAIT_PATH_CAPACITY];
    char error[EIDOLON_PORTRAIT_ERROR_CAPACITY];
    uint64_t config_hash;
    uint64_t attempted_hash;
    uint64_t revision;
    uint64_t expression_changed_ms;
    uint64_t performance_started_ms;
    uint64_t last_poll_ms;
    uint64_t last_draw_ms;
    unsigned int expression_serial;
    unsigned int accent_variant;
    float performance_intensity;
    EidolonPerformanceCue performance_cue;
    float attention_target;
    float attention_current;
    int current_expression;
    int automatic_expression;
    int override_expression;
    EidolonState state;
    EidolonPortraitSpring delivery_spring;
    int texture_width;
    int texture_height;
    bool ready;
    bool force_reload;
    bool face_mode;
    bool accent_interrupted;
};

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

static bool parse_unsigned(const char *text, unsigned int *value) {
    char *end = NULL;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (unsigned int)parsed;
    return true;
}

static bool parse_float(const char *text, float *value) {
    char *end = NULL;
    const float parsed = strtof(text, &end);
    if (end == text || *end != '\0') {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_crop(const char *text, SDL_FRect *crop) {
    float *components[4] = {&crop->x, &crop->y, &crop->w, &crop->h};
    const char *cursor = text;
    for (size_t index = 0U; index < 4U; ++index) {
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        char *end = NULL;
        *components[index] = strtof(cursor, &end);
        if (end == cursor) {
            return false;
        }
        cursor = end;
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (index < 3U && *cursor++ != ',') {
            return false;
        }
    }
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    return *cursor == '\0' && crop->x >= 0.0F && crop->y >= 0.0F && crop->w > 0.0F &&
           crop->h > 0.0F;
}

static int state_index(const char *name) {
    EidolonState state;
    return eidolon_state_parse(name, &state) ? (int)state : -1;
}

static bool parse_dialogue_theme(const char *text, EidolonDialogueTheme *theme) {
    if (strcmp(text, "classic") == 0) {
        *theme = EIDOLON_DIALOGUE_THEME_CLASSIC;
        return true;
    }
    if (strcmp(text, "academy_heart") == 0) {
        *theme = EIDOLON_DIALOGUE_THEME_ACADEMY_HEART;
        return true;
    }
    return false;
}

static bool parse_dialogue_movement(const char *text, EidolonDialogueMovement *movement) {
    if (strcmp(text, "manual") == 0) {
        *movement = EIDOLON_DIALOGUE_MOVEMENT_MANUAL;
        return true;
    }
    if (strcmp(text, "paged") == 0) {
        *movement = EIDOLON_DIALOGUE_MOVEMENT_PAGED;
        return true;
    }
    if (strcmp(text, "follow") == 0) {
        *movement = EIDOLON_DIALOGUE_MOVEMENT_FOLLOW;
        return true;
    }
    return false;
}

static bool parse_expression_key(const char *key, size_t *index, const char **field) {
    if (strncmp(key, "expression.", 11U) != 0) {
        return false;
    }
    char *end = NULL;
    const unsigned long parsed = strtoul(key + 11U, &end, 10);
    if (end == key + 11U || *end != '.' || parsed >= EIDOLON_PORTRAIT_MAX_EXPRESSIONS) {
        return false;
    }
    *index = (size_t)parsed;
    *field = end + 1;
    return true;
}

static bool parse_line(EidolonPortraitConfig *config, const char *key, const char *value,
                       bool *file_seen, bool *label_seen, bool *crop_seen, char *error,
                       size_t error_capacity) {
    if (strcmp(key, "version") == 0) {
        if (!parse_unsigned(value, &config->version)) {
            set_error(error, error_capacity, "invalid version");
            return false;
        }
        return true;
    }
    if (strcmp(key, "name") == 0) {
        SDL_strlcpy(config->name, value, sizeof(config->name));
        return true;
    }
    if (strcmp(key, "directory") == 0) {
        SDL_strlcpy(config->directory, value, sizeof(config->directory));
        return true;
    }
    if (strcmp(key, "expression_count") == 0) {
        unsigned int count = 0U;
        if (!parse_unsigned(value, &count) || count == 0U ||
            count > EIDOLON_PORTRAIT_MAX_EXPRESSIONS) {
            set_error(error, error_capacity, "invalid expression_count");
            return false;
        }
        config->expression_count = (size_t)count;
        return true;
    }
    if (strcmp(key, "display_height") == 0) {
        if (!parse_float(value, &config->display_height)) {
            set_error(error, error_capacity, "invalid display_height");
            return false;
        }
        return true;
    }
    if (strcmp(key, "portrait_display_height") == 0) {
        if (!parse_float(value, &config->portrait_display_height)) {
            set_error(error, error_capacity, "invalid portrait_display_height");
            return false;
        }
        return true;
    }
    if (strcmp(key, "framing") == 0) {
        if (strcmp(value, "full") == 0) {
            config->default_face_mode = false;
        } else if (strcmp(value, "portrait") == 0 || strcmp(value, "bust") == 0) {
            config->default_face_mode = true;
        } else {
            set_error(error, error_capacity, "framing must be full or portrait");
            return false;
        }
        return true;
    }
    if (strcmp(key, "dialogue.theme") == 0) {
        if (!parse_dialogue_theme(value, &config->dialogue_theme)) {
            set_error(error, error_capacity, "dialogue.theme must be classic or academy_heart");
            return false;
        }
        return true;
    }
    if (strcmp(key, "dialogue.movement") == 0) {
        if (!parse_dialogue_movement(value, &config->dialogue_movement)) {
            set_error(error, error_capacity, "dialogue.movement must be manual, paged, or follow");
            return false;
        }
        return true;
    }
    if (strcmp(key, "dialogue.hold_ms") == 0) {
        if (!parse_unsigned(value, &config->dialogue_hold_ms)) {
            set_error(error, error_capacity, "invalid dialogue.hold_ms");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.breath_amount") == 0) {
        if (!parse_float(value, &config->breath_amount)) {
            set_error(error, error_capacity, "invalid motion.breath_amount");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.breath_period_s") == 0) {
        if (!parse_float(value, &config->breath_period_seconds)) {
            set_error(error, error_capacity, "invalid motion.breath_period_s");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.sway_pixels") == 0) {
        if (!parse_float(value, &config->sway_pixels)) {
            set_error(error, error_capacity, "invalid motion.sway_pixels");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.sway_degrees") == 0) {
        if (!parse_float(value, &config->sway_degrees)) {
            set_error(error, error_capacity, "invalid motion.sway_degrees");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.accent_strength") == 0) {
        if (!parse_float(value, &config->accent_strength)) {
            set_error(error, error_capacity, "invalid motion.accent_strength");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.accent_duration_ms") == 0) {
        if (!parse_unsigned(value, &config->accent_duration_ms)) {
            set_error(error, error_capacity, "invalid motion.accent_duration_ms");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.posture_strength") == 0) {
        if (!parse_float(value, &config->posture_strength)) {
            set_error(error, error_capacity, "invalid motion.posture_strength");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.speech_strength") == 0) {
        if (!parse_float(value, &config->speech_strength)) {
            set_error(error, error_capacity, "invalid motion.speech_strength");
            return false;
        }
        return true;
    }
    if (strcmp(key, "motion.attention_strength") == 0) {
        if (!parse_float(value, &config->attention_strength)) {
            set_error(error, error_capacity, "invalid motion.attention_strength");
            return false;
        }
        return true;
    }
    if (strncmp(key, "state.", 6U) == 0) {
        const int state = state_index(key + 6U);
        unsigned int expression = 0U;
        if (state < 0 || !parse_unsigned(value, &expression) ||
            expression >= EIDOLON_PORTRAIT_MAX_EXPRESSIONS) {
            set_error(error, error_capacity, "invalid state mapping: %s", key);
            return false;
        }
        config->state_expressions[state] = (int)expression;
        return true;
    }

    size_t index = 0U;
    const char *field = NULL;
    if (parse_expression_key(key, &index, &field)) {
        if (strcmp(field, "file") == 0) {
            SDL_strlcpy(config->expressions[index].file, value,
                        sizeof(config->expressions[index].file));
            file_seen[index] = true;
            return true;
        }
        if (strcmp(field, "label") == 0) {
            SDL_strlcpy(config->expressions[index].label, value,
                        sizeof(config->expressions[index].label));
            label_seen[index] = true;
            return true;
        }
        if (strcmp(field, "portrait_crop") == 0) {
            SDL_FRect crop;
            if (!parse_crop(value, &crop)) {
                set_error(error, error_capacity, "invalid portrait crop: %s", key);
                return false;
            }
            config->expressions[index].portrait_crop = crop;
            crop_seen[index] = true;
            return true;
        }
    }
    set_error(error, error_capacity, "unknown key: %s", key);
    return false;
}

bool eidolon_portrait_config_parse(const char *text, size_t length, EidolonPortraitConfig *config,
                                   char *error, size_t error_capacity) {
    if (text == NULL || config == NULL) {
        set_error(error, error_capacity, "missing portrait config input");
        return false;
    }
    SDL_zero(*config);
    config->accent_strength = 1.0F;
    config->accent_duration_ms = 520U;
    config->posture_strength = 1.0F;
    config->speech_strength = 1.0F;
    config->attention_strength = 1.0F;
    config->dialogue_theme = EIDOLON_DIALOGUE_THEME_CLASSIC;
    config->dialogue_movement = EIDOLON_DIALOGUE_MOVEMENT_FOLLOW;
    config->dialogue_hold_ms = EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS;
    for (size_t state = 0U; state < EIDOLON_STATE_COUNT; ++state) {
        config->state_expressions[state] = -1;
    }
    bool file_seen[EIDOLON_PORTRAIT_MAX_EXPRESSIONS] = {false};
    bool label_seen[EIDOLON_PORTRAIT_MAX_EXPRESSIONS] = {false};
    bool crop_seen[EIDOLON_PORTRAIT_MAX_EXPRESSIONS] = {false};
    char *copy = SDL_malloc(length + 1U);
    if (copy == NULL) {
        set_error(error, error_capacity, "out of memory");
        return false;
    }
    SDL_memcpy(copy, text, length);
    copy[length] = '\0';

    bool valid = true;
    unsigned int line_number = 0U;
    char *cursor = copy;
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
        char *equals = strchr(line, '=');
        if (equals == NULL) {
            set_error(error, error_capacity, "line %u: expected key = value", line_number);
            valid = false;
            break;
        }
        *equals = '\0';
        char *key = trim(line);
        char *value = trim(equals + 1);
        if (!parse_line(config, key, value, file_seen, label_seen, crop_seen, error,
                        error_capacity)) {
            char detail[EIDOLON_PORTRAIT_ERROR_CAPACITY];
            SDL_strlcpy(detail, error, sizeof(detail));
            set_error(error, error_capacity, "line %u: %s", line_number, detail);
            valid = false;
            break;
        }
    }
    SDL_free(copy);
    if (!valid) {
        return false;
    }
    if (config->version != 1U || config->name[0] == '\0' || config->directory[0] == '\0' ||
        config->expression_count == 0U) {
        set_error(error, error_capacity, "missing required portrait metadata");
        return false;
    }
    if (config->display_height < 128.0F || config->display_height > 2048.0F ||
        config->portrait_display_height < 128.0F || config->portrait_display_height > 2048.0F ||
        config->breath_amount < 0.0F || config->breath_amount > 0.05F ||
        config->breath_period_seconds < 1.0F || config->breath_period_seconds > 30.0F ||
        config->sway_pixels < 0.0F || config->sway_pixels > 32.0F || config->sway_degrees < 0.0F ||
        config->sway_degrees > 5.0F || config->accent_strength < 0.0F ||
        config->accent_strength > 3.0F || config->accent_duration_ms < 100U ||
        config->accent_duration_ms > 2000U || config->posture_strength < 0.0F ||
        config->posture_strength > 3.0F || config->speech_strength < 0.0F ||
        config->speech_strength > 3.0F || config->attention_strength < 0.0F ||
        config->attention_strength > 3.0F || config->dialogue_hold_ms > 30000U) {
        set_error(error, error_capacity, "portrait motion or display value out of range");
        return false;
    }
    for (size_t index = 0U; index < config->expression_count; ++index) {
        if (!file_seen[index] || !label_seen[index] || !crop_seen[index]) {
            set_error(error, error_capacity, "expression.%zu is incomplete", index);
            return false;
        }
    }
    for (size_t state = 0U; state < EIDOLON_STATE_COUNT; ++state) {
        if (config->state_expressions[state] < 0 ||
            (size_t)config->state_expressions[state] >= config->expression_count) {
            set_error(error, error_capacity, "missing or invalid state.%s mapping",
                      eidolon_state_name((EidolonState)state));
            return false;
        }
    }
    if (error != NULL && error_capacity > 0U) {
        error[0] = '\0';
    }
    return true;
}

static uint64_t hash_bytes(const void *data, size_t length) {
    const unsigned char *bytes = data;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0U; index < length; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void destroy_textures(SDL_Texture **textures, size_t count) {
    for (size_t index = 0U; index < count; ++index) {
        SDL_DestroyTexture(textures[index]);
        textures[index] = NULL;
    }
}

static bool load_textures(EidolonPortraitRenderer *portrait, const EidolonPortraitConfig *config,
                          SDL_Texture **textures, int *width, int *height) {
    for (size_t index = 0U; index < config->expression_count; ++index) {
        char path[1024];
        SDL_snprintf(path, sizeof(path), "%s/%s/%s", portrait->asset_directory, config->directory,
                     config->expressions[index].file);
        SDL_Surface *surface = SDL_LoadPNG(path);
        if (surface == NULL) {
            set_error(portrait->error, sizeof(portrait->error), "could not load %s: %s", path,
                      SDL_GetError());
            destroy_textures(textures, config->expression_count);
            return false;
        }
        if (index == 0U) {
            *width = surface->w;
            *height = surface->h;
        } else if (surface->w != *width || surface->h != *height) {
            set_error(portrait->error, sizeof(portrait->error),
                      "portrait canvases differ: %s is %dx%d, expected %dx%d", path, surface->w,
                      surface->h, *width, *height);
            SDL_DestroySurface(surface);
            destroy_textures(textures, config->expression_count);
            return false;
        }
        textures[index] = SDL_CreateTextureFromSurface(portrait->renderer, surface);
        SDL_DestroySurface(surface);
        if (textures[index] == NULL) {
            set_error(portrait->error, sizeof(portrait->error), "could not upload %s: %s", path,
                      SDL_GetError());
            destroy_textures(textures, config->expression_count);
            return false;
        }
        SDL_SetTextureScaleMode(textures[index], SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(textures[index], SDL_BLENDMODE_BLEND);
    }
    return true;
}

static void select_expression(EidolonPortraitRenderer *portrait, int expression, uint64_t now_ms,
                              const char *source) {
    if (!portrait->ready || expression < 0 ||
        (size_t)expression >= portrait->config.expression_count ||
        expression == portrait->current_expression) {
        return;
    }
    const int previous = portrait->current_expression;
    const uint64_t since_previous =
        portrait->expression_changed_ms == 0U || now_ms < portrait->expression_changed_ms
            ? 0U
            : now_ms - portrait->expression_changed_ms;
    portrait->accent_interrupted =
        portrait->current_expression >= 0 && now_ms >= portrait->expression_changed_ms &&
        now_ms - portrait->expression_changed_ms < portrait->config.accent_duration_ms;
    portrait->current_expression = expression;
    portrait->expression_changed_ms = now_ms;
    eidolon_portrait_spring_reset(&portrait->delivery_spring, now_ms);
    portrait->expression_serial += 1U;
    portrait->accent_variant = (portrait->expression_serial + (unsigned int)expression * 5U) % 3U;
    portrait->revision += 1U;
    const char *previous_label =
        previous >= 0 && (size_t)previous < portrait->config.expression_count
            ? portrait->config.expressions[previous].label
            : "none";
    const char *current_label = portrait->config.expressions[expression].label;
    eidolon_log_write("portrait", "expression %s->%s source=%s since_ms=%llu interrupted=%s",
                      previous_label, current_label, source != NULL ? source : "unknown",
                      (unsigned long long)since_previous,
                      portrait->accent_interrupted ? "yes" : "no");
}

static bool reload_config(EidolonPortraitRenderer *portrait, uint64_t now_ms) {
    size_t length = 0U;
    void *bytes = SDL_LoadFile(portrait->config_path, &length);
    if (bytes == NULL) {
        set_error(portrait->error, sizeof(portrait->error), "could not read %s: %s",
                  portrait->config_path, SDL_GetError());
        return false;
    }
    const uint64_t hash = hash_bytes(bytes, length);
    if (!portrait->force_reload && portrait->attempted_hash == hash) {
        SDL_free(bytes);
        return true;
    }
    portrait->attempted_hash = hash;
    EidolonPortraitConfig config;
    if (!eidolon_portrait_config_parse(bytes, length, &config, portrait->error,
                                       sizeof(portrait->error))) {
        SDL_free(bytes);
        return false;
    }
    SDL_free(bytes);

    SDL_Texture *textures[EIDOLON_PORTRAIT_MAX_EXPRESSIONS] = {NULL};
    int width = 0;
    int height = 0;
    if (!load_textures(portrait, &config, textures, &width, &height)) {
        return false;
    }
    for (size_t index = 0U; index < config.expression_count; ++index) {
        const SDL_FRect crop = config.expressions[index].portrait_crop;
        if (crop.x + crop.w > (float)width || crop.y + crop.h > (float)height) {
            set_error(portrait->error, sizeof(portrait->error),
                      "expression.%zu portrait crop exceeds %dx%d canvas", index, width, height);
            destroy_textures(textures, config.expression_count);
            return false;
        }
    }
    destroy_textures(portrait->textures, portrait->config.expression_count);
    SDL_memcpy(portrait->textures, textures, sizeof(textures));
    portrait->config = config;
    portrait->face_mode = config.default_face_mode;
    portrait->texture_width = width;
    portrait->texture_height = height;
    portrait->config_hash = hash;
    portrait->ready = true;
    portrait->force_reload = false;
    portrait->error[0] = '\0';
    if (portrait->override_expression >= (int)config.expression_count) {
        portrait->override_expression = -1;
    }
    if (portrait->automatic_expression < 0 ||
        portrait->automatic_expression >= (int)config.expression_count) {
        portrait->automatic_expression = config.state_expressions[portrait->state];
    }
    const int selected = portrait->override_expression >= 0 ? portrait->override_expression
                                                            : portrait->automatic_expression;
    portrait->current_expression = selected;
    portrait->expression_changed_ms = now_ms;
    portrait->accent_interrupted = false;
    portrait->revision += 1U;
    eidolon_log_write("portrait", "loaded %s expressions=%zu canvas=%dx%d", config.name,
                      config.expression_count, width, height);
    return true;
}

EidolonPortraitRenderer *eidolon_portrait_create(SDL_Renderer *renderer, const char *config_path,
                                                 const char *asset_directory) {
    if (renderer == NULL || config_path == NULL || asset_directory == NULL) {
        SDL_SetError("missing portrait renderer input");
        return NULL;
    }
    EidolonPortraitRenderer *portrait = SDL_calloc(1, sizeof(*portrait));
    if (portrait == NULL) {
        SDL_SetError("out of memory while creating portrait renderer");
        return NULL;
    }
    portrait->renderer = renderer;
    portrait->override_expression = -1;
    portrait->automatic_expression = -1;
    portrait->current_expression = -1;
    portrait->state = EIDOLON_STATE_IDLE;
    portrait->force_reload = true;
    SDL_strlcpy(portrait->config_path, config_path, sizeof(portrait->config_path));
    SDL_strlcpy(portrait->asset_directory, asset_directory, sizeof(portrait->asset_directory));
    if (!reload_config(portrait, SDL_GetTicks())) {
        SDL_SetError("%s", portrait->error);
        eidolon_portrait_destroy(portrait);
        return NULL;
    }
    return portrait;
}

void eidolon_portrait_destroy(EidolonPortraitRenderer *portrait) {
    if (portrait == NULL) {
        return;
    }
    destroy_textures(portrait->textures, portrait->config.expression_count);
    SDL_free(portrait);
}

void eidolon_portrait_update(EidolonPortraitRenderer *portrait, uint64_t now_ms) {
    if (portrait == NULL ||
        (!portrait->force_reload && now_ms - portrait->last_poll_ms < PORTRAIT_POLL_INTERVAL_MS)) {
        return;
    }
    portrait->last_poll_ms = now_ms;
    if (!reload_config(portrait, now_ms) && portrait->error[0] != '\0') {
        eidolon_log_write("portrait", "config rejected; retaining last good: %s", portrait->error);
        portrait->force_reload = false;
    }
}

void eidolon_portrait_set_state(EidolonPortraitRenderer *portrait, EidolonState state,
                                uint64_t now_ms) {
    if (portrait == NULL || state < 0 || state >= EIDOLON_STATE_COUNT) {
        return;
    }
    portrait->state = state;
    portrait->automatic_expression = portrait->config.state_expressions[state];
    if (portrait->override_expression < 0) {
        char source[48];
        SDL_snprintf(source, sizeof(source), "state:%s", eidolon_state_name(state));
        select_expression(portrait, portrait->automatic_expression, now_ms, source);
    }
}

void eidolon_portrait_set_expression_intent(EidolonPortraitRenderer *portrait,
                                            EidolonExpressionIntent intent, uint64_t now_ms) {
    if (portrait == NULL) {
        return;
    }
    const char *label = eidolon_expression_intent_name(intent);
    int expression = -1;
    for (size_t index = 0U; index < portrait->config.expression_count; ++index) {
        if (strcmp(portrait->config.expressions[index].label, label) == 0) {
            expression = (int)index;
            break;
        }
    }
    if (expression < 0) {
        expression = portrait->config.state_expressions[portrait->state];
    }
    portrait->automatic_expression = expression;
    if (portrait->override_expression < 0) {
        char source[64];
        SDL_snprintf(source, sizeof(source), "intent:%s", label);
        select_expression(portrait, expression, now_ms, source);
    }
}

void eidolon_portrait_set_override(EidolonPortraitRenderer *portrait, int expression,
                                   uint64_t now_ms) {
    if (portrait == NULL || expression < -1 ||
        (expression >= 0 && (size_t)expression >= portrait->config.expression_count)) {
        return;
    }
    portrait->override_expression = expression;
    select_expression(portrait, expression >= 0 ? expression : portrait->automatic_expression,
                      now_ms, expression >= 0 ? "debug-override" : "override-cleared");
}

void eidolon_portrait_deliver(EidolonPortraitRenderer *portrait, EidolonDeliveryCue cue,
                              float intensity, float direction, uint64_t now_ms) {
    if (portrait == NULL || !portrait->ready || intensity <= 0.0F) {
        return;
    }
    eidolon_portrait_spring_update(&portrait->delivery_spring, now_ms);
    const char *label = portrait->config.expressions[portrait->current_expression].label;
    float energy = 1.0F;
    float vertical = 1.0F;
    float lateral = 1.0F;
    float rotation = 1.0F;
    if (strcmp(label, "cheerful") == 0 || strcmp(label, "delighted") == 0 ||
        strcmp(label, "ecstatic") == 0) {
        energy = 1.12F;
        vertical = 1.18F;
    } else if (strcmp(label, "gentle") == 0) {
        energy = 0.82F;
        lateral = 1.12F;
    } else if (strcmp(label, "annoyed") == 0) {
        energy = 0.96F;
        vertical = 0.72F;
        lateral = 1.24F;
        rotation = 1.20F;
    } else if (strcmp(label, "worried") == 0) {
        energy = 0.76F;
        vertical = 0.82F;
    } else if (strcmp(label, "serious") == 0) {
        energy = 0.62F;
        rotation = 0.72F;
    } else if (strcmp(label, "embarrassed") == 0) {
        energy = 0.86F;
        rotation = 1.18F;
    }
    const float amount =
        SDL_clamp(intensity, 0.0F, 1.0F) * portrait->config.speech_strength * energy;
    const float signed_direction = direction < 0.0F ? -1.0F : 1.0F;
    float velocity_x = 0.0F;
    float velocity_y = 0.0F;
    float velocity_scale = 0.0F;
    float velocity_angle = 0.0F;
    switch (cue) {
    case EIDOLON_DELIVERY_CUE_PHRASE:
        velocity_y = -24.0F * vertical;
        velocity_angle = 0.70F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_ACCENT:
        velocity_y = -44.0F * vertical;
        velocity_scale = 0.030F;
        velocity_angle = 1.25F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_CONTRAST:
        velocity_x = 52.0F * signed_direction * lateral;
        velocity_y = -12.0F * vertical;
        velocity_angle = -2.10F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_HESITATE:
        velocity_x = 20.0F * signed_direction * lateral;
        velocity_y = 12.0F * vertical;
        velocity_angle = 1.35F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_PAUSE:
        velocity_y = 12.0F * vertical;
        velocity_scale = -0.010F;
        velocity_angle = -0.45F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_LAND:
        velocity_y = 20.0F * vertical;
        velocity_scale = -0.018F;
        velocity_angle = -0.62F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_QUESTION:
        velocity_y = -32.0F * vertical;
        velocity_scale = 0.018F;
        velocity_angle = 1.85F * signed_direction * rotation;
        break;
    case EIDOLON_DELIVERY_CUE_EXCLAIM:
        velocity_y = -76.0F * vertical;
        velocity_scale = 0.065F;
        velocity_angle = 2.50F * signed_direction * rotation;
        break;
    }
    eidolon_portrait_spring_impulse(&portrait->delivery_spring, velocity_x * amount,
                                    velocity_y * amount, velocity_scale * amount,
                                    velocity_angle * amount);
}

void eidolon_portrait_perform(EidolonPortraitRenderer *portrait, EidolonPerformanceCue cue,
                              float intensity, uint64_t now_ms) {
    if (portrait == NULL || cue == EIDOLON_PERFORMANCE_CUE_NONE || intensity <= 0.0F) {
        return;
    }
    portrait->performance_cue = cue;
    portrait->performance_intensity = SDL_clamp(intensity, 0.0F, 1.0F);
    portrait->performance_started_ms = now_ms;
}

void eidolon_portrait_set_attention(EidolonPortraitRenderer *portrait, float direction) {
    if (portrait != NULL) {
        portrait->attention_target = SDL_clamp(direction, -1.0F, 1.0F);
    }
}

void eidolon_portrait_force_reload(EidolonPortraitRenderer *portrait) {
    if (portrait != NULL) {
        portrait->force_reload = true;
        portrait->last_poll_ms = 0U;
    }
}

void eidolon_portrait_set_face_mode(EidolonPortraitRenderer *portrait, bool enabled) {
    if (portrait != NULL && portrait->face_mode != enabled) {
        portrait->face_mode = enabled;
        portrait->revision += 1U;
    }
}

bool eidolon_portrait_face_mode(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL && portrait->face_mode;
}

typedef struct PortraitMotionProfile {
    float accent_x;
    float accent_y;
    float accent_scale;
    float accent_degrees;
    float posture_x;
    float posture_y;
    float posture_scale;
    float posture_degrees;
} PortraitMotionProfile;

static PortraitMotionProfile motion_for_label(const char *label) {
    if (strcmp(label, "cheerful") == 0) {
        return (PortraitMotionProfile){0.0F, -10.0F, 0.016F, -0.40F, 0.0F, -1.4F, 0.003F, -0.08F};
    }
    if (strcmp(label, "responding") == 0) {
        return (PortraitMotionProfile){0.0F, -5.0F, 0.008F, 0.12F, 0.0F, -0.5F, 0.001F, 0.04F};
    }
    if (strcmp(label, "delighted") == 0) {
        return (PortraitMotionProfile){0.0F, -12.0F, 0.018F, 0.45F, 0.7F, -2.0F, 0.004F, 0.12F};
    }
    if (strcmp(label, "embarrassed") == 0) {
        return (PortraitMotionProfile){5.0F, 2.0F, -0.006F, 0.55F, 1.6F, 1.0F, -0.002F, 0.16F};
    }
    if (strcmp(label, "serious") == 0) {
        return (PortraitMotionProfile){-5.0F, 0.0F, 0.003F, -0.25F, -1.0F, 0.3F, 0.0F, -0.12F};
    }
    if (strcmp(label, "worried") == 0) {
        return (PortraitMotionProfile){0.0F, 7.0F, -0.012F, -0.25F, 0.0F, 2.0F, -0.003F, -0.10F};
    }
    if (strcmp(label, "annoyed") == 0) {
        return (PortraitMotionProfile){-8.0F, -1.0F, 0.005F, -0.65F, -1.5F, -0.2F, 0.001F, -0.20F};
    }
    if (strcmp(label, "gentle") == 0) {
        return (PortraitMotionProfile){4.0F, -4.0F, 0.010F, 0.35F, 1.5F, -0.8F, 0.002F, 0.12F};
    }
    if (strcmp(label, "ecstatic") == 0) {
        return (PortraitMotionProfile){0.0F, -15.0F, 0.025F, -0.75F, 0.0F, -2.5F, 0.005F, -0.16F};
    }
    return (PortraitMotionProfile){0.0F, -3.0F, 0.004F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
}

static float damped_channel(float progress, float cycles, float power) {
    const float remaining = 1.0F - progress;
    return SDL_sinf(progress * SDL_PI_F * cycles) * powf(remaining, power);
}

bool eidolon_portrait_draw(EidolonPortraitRenderer *portrait, SDL_Renderer *renderer,
                           const SDL_FRect *destination, uint64_t now_ms) {
    if (!eidolon_portrait_ready(portrait) || renderer == NULL || destination == NULL) {
        return false;
    }
    const float seconds = (float)now_ms / 1000.0F;
    const float breath_phase = seconds * (2.0F * SDL_PI_F) / portrait->config.breath_period_seconds;
    const float sway_phase = seconds * (2.0F * SDL_PI_F) / 8.7F;
    const float breath = 1.0F + SDL_sinf(breath_phase) * portrait->config.breath_amount;
    SDL_FRect animated = *destination;
    animated.w *= breath;
    animated.h *= breath;
    animated.x += (destination->w - animated.w) * 0.5F +
                  SDL_sinf(sway_phase) * portrait->config.sway_pixels * 0.4F;
    animated.y +=
        destination->h - animated.h + SDL_sinf(sway_phase * 0.73F) * portrait->config.sway_pixels;
    double angle = (double)(SDL_sinf(sway_phase) * portrait->config.sway_degrees);

    const char *label = portrait->config.expressions[portrait->current_expression].label;
    const PortraitMotionProfile motion = motion_for_label(label);
    const float reference_height = portrait->face_mode ? portrait->config.portrait_display_height
                                                       : portrait->config.display_height;
    const float unit = destination->h / reference_height;
    float translation_x = motion.posture_x * unit * portrait->config.posture_strength;
    float translation_y = motion.posture_y * unit * portrait->config.posture_strength;
    float scale_delta = motion.posture_scale * portrait->config.posture_strength;
    float degrees_delta = motion.posture_degrees * portrait->config.posture_strength;

    eidolon_portrait_spring_update(&portrait->delivery_spring, now_ms);
    translation_x += portrait->delivery_spring.x * unit;
    translation_y += portrait->delivery_spring.y * unit;
    scale_delta += portrait->delivery_spring.scale;
    degrees_delta += portrait->delivery_spring.angle;

    if (portrait->last_draw_ms != 0U && now_ms >= portrait->last_draw_ms) {
        const float delta_seconds =
            SDL_min((float)(now_ms - portrait->last_draw_ms) / 1000.0F, 0.1F);
        const float attention_blend = 1.0F - expf(-7.0F * delta_seconds);
        portrait->attention_current +=
            (portrait->attention_target - portrait->attention_current) * attention_blend;
    }
    portrait->last_draw_ms = now_ms;
    translation_x +=
        portrait->attention_current * 2.2F * unit * portrait->config.attention_strength;
    degrees_delta += portrait->attention_current * 0.18F * portrait->config.attention_strength;

    const float accent_progress = SDL_clamp((float)(now_ms - portrait->expression_changed_ms) /
                                                (float)portrait->config.accent_duration_ms,
                                            0.0F, 1.0F);
    if (accent_progress < 1.0F && portrait->config.accent_strength > 0.0F) {
        static const float variant_amplitude[3] = {0.88F, 1.0F, 1.12F};
        const float variant = (float)portrait->accent_variant - 1.0F;
        const float amplitude =
            variant_amplitude[portrait->accent_variant] * portrait->config.accent_strength;
        float anticipation = 0.0F;
        if (portrait->accent_interrupted && accent_progress < 0.18F) {
            const float anticipation_progress = accent_progress / 0.18F;
            anticipation = -0.30F * SDL_sinf(anticipation_progress * SDL_PI_F) *
                           (1.0F - anticipation_progress);
        }
        const float pulse_x =
            damped_channel(accent_progress, 2.10F + variant * 0.10F, 2.15F) + anticipation;
        const float pulse_y =
            damped_channel(accent_progress, 2.48F + variant * 0.12F, 1.85F) + anticipation;
        const float pulse_scale =
            damped_channel(accent_progress, 1.92F + variant * 0.08F, 2.35F) + anticipation;
        const float pulse_angle =
            damped_channel(accent_progress, 2.82F + variant * 0.14F, 2.0F) + anticipation;
        translation_x += motion.accent_x * unit * pulse_x * amplitude;
        translation_y += motion.accent_y * unit * pulse_y * amplitude;
        scale_delta += motion.accent_scale * pulse_scale * amplitude;
        degrees_delta += motion.accent_degrees * pulse_angle * amplitude;
    }

    if (portrait->performance_started_ms != 0U && now_ms >= portrait->performance_started_ms) {
        const float cue_progress = (float)(now_ms - portrait->performance_started_ms) / 560.0F;
        if (cue_progress < 1.0F) {
            const float pulse =
                damped_channel(cue_progress, 2.15F, 1.75F) * portrait->performance_intensity;
            const float direction =
                fabsf(portrait->attention_current) > 0.1F ? portrait->attention_current : -1.0F;
            switch (portrait->performance_cue) {
            case EIDOLON_PERFORMANCE_CUE_LIFT:
                translation_y -= 10.0F * unit * pulse;
                scale_delta += 0.012F * pulse;
                degrees_delta -= 0.24F * direction * pulse;
                break;
            case EIDOLON_PERFORMANCE_CUE_RECOIL:
                translation_x -= 7.0F * unit * direction * pulse;
                translation_y += 3.0F * unit * pulse;
                scale_delta -= 0.008F * pulse;
                degrees_delta -= 0.38F * direction * pulse;
                break;
            case EIDOLON_PERFORMANCE_CUE_LEAN:
                translation_x += 5.0F * unit * direction * pulse;
                translation_y -= 4.0F * unit * pulse;
                scale_delta += 0.007F * pulse;
                degrees_delta += 0.28F * direction * pulse;
                break;
            case EIDOLON_PERFORMANCE_CUE_SURPRISE:
                translation_y -= 19.0F * unit * pulse;
                scale_delta += 0.026F * pulse;
                degrees_delta -= 0.48F * direction * pulse;
                break;
            case EIDOLON_PERFORMANCE_CUE_ACCENT:
                translation_y -= 4.0F * unit * pulse;
                degrees_delta += 0.12F * direction * pulse;
                break;
            case EIDOLON_PERFORMANCE_CUE_NONE:
                break;
            }
        }
    }

    const float transform_scale = 1.0F + scale_delta;
    const float old_width = animated.w;
    const float old_height = animated.h;
    animated.w *= transform_scale;
    animated.h *= transform_scale;
    animated.x += (old_width - animated.w) * 0.5F + translation_x;
    animated.y += old_height - animated.h + translation_y;
    angle += (double)degrees_delta;
    const SDL_FPoint pivot = {animated.w * 0.5F, animated.h * 0.94F};

    SDL_Texture *current = portrait->textures[portrait->current_expression];
    const SDL_FRect *current_source =
        portrait->face_mode
            ? &portrait->config.expressions[portrait->current_expression].portrait_crop
            : NULL;
    return SDL_RenderTextureRotated(renderer, current, current_source, &animated, angle, &pivot,
                                    SDL_FLIP_NONE);
}

bool eidolon_portrait_ready(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL && portrait->ready && portrait->current_expression >= 0;
}

size_t eidolon_portrait_expression_count(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->config.expression_count : 0U;
}

const char *eidolon_portrait_expression_label(const EidolonPortraitRenderer *portrait,
                                              size_t expression) {
    return portrait != NULL && expression < portrait->config.expression_count
               ? portrait->config.expressions[expression].label
               : "unknown";
}

int eidolon_portrait_current_expression(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->current_expression : -1;
}

int eidolon_portrait_override_expression(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->override_expression : -1;
}

float eidolon_portrait_display_width(const EidolonPortraitRenderer *portrait) {
    if (!eidolon_portrait_ready(portrait) || portrait->texture_height <= 0) {
        return 0.0F;
    }
    if (portrait->face_mode) {
        const SDL_FRect crop =
            portrait->config.expressions[portrait->current_expression].portrait_crop;
        return portrait->config.portrait_display_height * crop.w / crop.h;
    }
    return portrait->config.display_height * (float)portrait->texture_width /
           (float)portrait->texture_height;
}

float eidolon_portrait_display_height(const EidolonPortraitRenderer *portrait) {
    if (!eidolon_portrait_ready(portrait)) {
        return 0.0F;
    }
    return portrait->face_mode ? portrait->config.portrait_display_height
                               : portrait->config.display_height;
}

uint64_t eidolon_portrait_revision(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->revision : 0U;
}

EidolonDialogueTheme eidolon_portrait_dialogue_theme(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->config.dialogue_theme : EIDOLON_DIALOGUE_THEME_CLASSIC;
}

EidolonDialogueMovement
eidolon_portrait_dialogue_movement(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->config.dialogue_movement : EIDOLON_DIALOGUE_MOVEMENT_FOLLOW;
}

unsigned int eidolon_portrait_dialogue_hold_ms(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->config.dialogue_hold_ms : EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS;
}

const char *eidolon_portrait_error(const EidolonPortraitRenderer *portrait) {
    return portrait != NULL ? portrait->error : "portrait unavailable";
}
