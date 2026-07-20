#ifndef EIDOLON_USER_SETTINGS_H
#define EIDOLON_USER_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_USER_SETTINGS_VERSION 1
#define EIDOLON_USER_SETTINGS_PATH_CAPACITY 1024U
#define EIDOLON_USER_SETTINGS_ERROR_CAPACITY 256U

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
} EidolonUserSettingField;

typedef struct EidolonUserSettings {
    uint32_t overrides;
    int render_mode;
    float display_scale;
    bool portrait_face_mode;
    int model_render_resolution;
    float model_yaw_degrees;
    float model_pitch_degrees;
    float model_roll_degrees;
    int dialogue_theme;
    int dialogue_movement;
    unsigned int dialogue_hold_ms;
} EidolonUserSettings;

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
