#ifndef EIDOLON_USER_SETTINGS_H
#define EIDOLON_USER_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_USER_SETTINGS_VERSION 1
#define EIDOLON_USER_SETTINGS_PATH_CAPACITY 1024U
#define EIDOLON_USER_SETTINGS_ERROR_CAPACITY 256U
#define EIDOLON_FPS_LIMIT_MIN 0
#define EIDOLON_FPS_LIMIT_MAX 1000

typedef enum EidolonBubbleBoundsMode {
    EIDOLON_BUBBLE_BOUNDS_AVATAR = 0,
    EIDOLON_BUBBLE_BOUNDS_PRIMARY,
    EIDOLON_BUBBLE_BOUNDS_VIRTUAL,
    EIDOLON_BUBBLE_BOUNDS_CUSTOM,
    EIDOLON_BUBBLE_BOUNDS_COUNT,
} EidolonBubbleBoundsMode;

typedef enum EidolonUserSettingField {
    EIDOLON_USER_SETTING_RENDER_MODE = 1U << 0,
    EIDOLON_USER_SETTING_DISPLAY_SCALE = 1U << 1,
    EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE = 1U << 2,
    EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION = 1U << 3,
    EIDOLON_USER_SETTING_MODEL_YAW = 1U << 4,
    EIDOLON_USER_SETTING_MODEL_PITCH = 1U << 5,
    EIDOLON_USER_SETTING_MODEL_ROLL = 1U << 6,
    EIDOLON_USER_SETTING_DIALOGUE_THEME = 1U << 7,
    EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT = 1U << 8,
    EIDOLON_USER_SETTING_DIALOGUE_HOLD = 1U << 9,
    EIDOLON_USER_SETTING_BUBBLE_BOUNDS = 1U << 10,
    EIDOLON_USER_SETTING_VSYNC = 1U << 11,
    EIDOLON_USER_SETTING_FPS_LIMIT = 1U << 12,
    EIDOLON_USER_SETTING_PRESENTATION = 1U << 13,
} EidolonUserSettingField;

typedef struct EidolonUserSettings {
    uint32_t overrides;
    int render_mode;
    int presentation_preference;
    float display_scale;
    bool portrait_face_mode;
    int model_render_resolution;
    float model_yaw_degrees;
    float model_pitch_degrees;
    float model_roll_degrees;
    int dialogue_theme;
    int dialogue_movement;
    unsigned int dialogue_hold_ms;
    int bubble_bounds_mode;
    int bubble_custom_x;
    int bubble_custom_y;
    int bubble_custom_width;
    int bubble_custom_height;
    bool vsync;
    int fps_limit;
} EidolonUserSettings;

const char *eidolon_bubble_bounds_mode_name(EidolonBubbleBoundsMode mode);
void eidolon_user_settings_defaults(EidolonUserSettings *settings);
bool eidolon_user_settings_is_overridden(const EidolonUserSettings *settings,
                                         EidolonUserSettingField field);
bool eidolon_user_settings_parse(const char *text, size_t length, EidolonUserSettings *settings,
                                 char *error, size_t error_capacity);
bool eidolon_user_settings_resolve_path(char *path, size_t capacity);
bool eidolon_user_settings_load(const char *path, EidolonUserSettings *settings, char *error,
                                size_t error_capacity);
bool eidolon_user_settings_save(const char *path, const EidolonUserSettings *settings, char *error,
                                size_t error_capacity);

#endif
