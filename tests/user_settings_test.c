#include "user_settings.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <string.h>

static void parses_complete_settings(void) {
    static const char text[] = "version = 1\n"
                               "render_mode = 2\n"
                               "presentation_preference = sdl_window_legacy\n"
                               "display_scale = 3.25\n"
                               "portrait_face_mode = true\n"
                               "model_render_resolution = 1536\n"
                               "model_yaw_degrees = -42.5\n"
                               "model_pitch_degrees = 12\n"
                               "model_roll_degrees = 4.5\n"
                               "dialogue_theme = 1\n"
                               "dialogue_movement = 0\n"
                               "dialogue_hold_ms = 1750\n"
                               "bubble_bounds_mode = custom\n"
                               "bubble_custom_x = -1920\n"
                               "bubble_custom_y = 0\n"
                               "bubble_custom_width = 1920\n"
                               "bubble_custom_height = 1040\n"
                               "vsync = false\n"
                               "fps_limit = 144\n"
                               "future_option = ignored\n";
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(eidolon_user_settings_parse(text, strlen(text), &settings, error, sizeof(error)));
    assert(settings.render_mode == 2);
    assert(settings.presentation_preference == 1);
    assert(settings.display_scale == 3.25F);
    assert(settings.portrait_face_mode);
    assert(settings.model_render_resolution == 1536);
    assert(settings.model_yaw_degrees == -42.5F);
    assert(settings.model_pitch_degrees == 12.0F);
    assert(settings.model_roll_degrees == 4.5F);
    assert(settings.dialogue_theme == 1);
    assert(settings.dialogue_movement == 0);
    assert(settings.dialogue_hold_ms == 1750U);
    assert(settings.bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_CUSTOM);
    assert(settings.bubble_custom_x == -1920);
    assert(settings.bubble_custom_y == 0);
    assert(settings.bubble_custom_width == 1920);
    assert(settings.bubble_custom_height == 1040);
    assert(!settings.vsync);
    assert(settings.fps_limit == 144);
    assert(settings.overrides ==
           (EIDOLON_USER_SETTING_RENDER_MODE | EIDOLON_USER_SETTING_DISPLAY_SCALE |
            EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE | EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION |
            EIDOLON_USER_SETTING_MODEL_YAW | EIDOLON_USER_SETTING_MODEL_PITCH |
            EIDOLON_USER_SETTING_MODEL_ROLL | EIDOLON_USER_SETTING_DIALOGUE_THEME |
            EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT | EIDOLON_USER_SETTING_DIALOGUE_HOLD |
            EIDOLON_USER_SETTING_BUBBLE_BOUNDS | EIDOLON_USER_SETTING_VSYNC |
            EIDOLON_USER_SETTING_FPS_LIMIT | EIDOLON_USER_SETTING_PRESENTATION));
}

static void rejects_invalid_file_without_partial_apply(void) {
    static const char text[] = "version = 1\n"
                               "display_scale = 3.0\n"
                               "portrait_face_mode = perhaps\n";
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(!eidolon_user_settings_parse(text, strlen(text), &settings, error, sizeof(error)));
    assert(settings.display_scale == 1.0F);
    assert(settings.overrides == 0U);
    assert(strstr(error, "portrait_face_mode") != NULL);
}

static void requires_version(void) {
    static const char text[] = "display_scale = 2.0\n";
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(!eidolon_user_settings_parse(text, strlen(text), &settings, error, sizeof(error)));
    assert(strstr(error, "version") != NULL);
}

static void saves_and_loads_round_trip(void) {
    EidolonUserSettings expected;
    eidolon_user_settings_defaults(&expected);
    expected.render_mode = 0;
    expected.presentation_preference = 1;
    expected.display_scale = 2.75F;
    expected.portrait_face_mode = true;
    expected.model_render_resolution = 2048;
    expected.dialogue_theme = 1;
    expected.dialogue_movement = 1;
    expected.dialogue_hold_ms = 2250U;
    expected.bubble_bounds_mode = EIDOLON_BUBBLE_BOUNDS_CUSTOM;
    expected.bubble_custom_x = -2560;
    expected.bubble_custom_y = 40;
    expected.bubble_custom_width = 2560;
    expected.bubble_custom_height = 1400;
    expected.vsync = false;
    expected.fps_limit = 165;
    expected.overrides =
        EIDOLON_USER_SETTING_RENDER_MODE | EIDOLON_USER_SETTING_DISPLAY_SCALE |
        EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE | EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION |
        EIDOLON_USER_SETTING_DIALOGUE_THEME | EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT |
        EIDOLON_USER_SETTING_DIALOGUE_HOLD | EIDOLON_USER_SETTING_BUBBLE_BOUNDS |
        EIDOLON_USER_SETTING_VSYNC | EIDOLON_USER_SETTING_FPS_LIMIT |
        EIDOLON_USER_SETTING_PRESENTATION;
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(eidolon_user_settings_save(EIDOLON_TEST_SETTINGS_PATH, &expected, error, sizeof(error)));

    EidolonUserSettings actual;
    eidolon_user_settings_defaults(&actual);
    assert(eidolon_user_settings_load(EIDOLON_TEST_SETTINGS_PATH, &actual, error, sizeof(error)));
    assert(actual.render_mode == expected.render_mode);
    assert(actual.presentation_preference == expected.presentation_preference);
    assert(actual.display_scale == expected.display_scale);
    assert(actual.portrait_face_mode == expected.portrait_face_mode);
    assert(actual.model_render_resolution == expected.model_render_resolution);
    assert(actual.dialogue_theme == expected.dialogue_theme);
    assert(actual.dialogue_movement == expected.dialogue_movement);
    assert(actual.dialogue_hold_ms == expected.dialogue_hold_ms);
    assert(actual.bubble_bounds_mode == expected.bubble_bounds_mode);
    assert(actual.bubble_custom_x == expected.bubble_custom_x);
    assert(actual.bubble_custom_y == expected.bubble_custom_y);
    assert(actual.bubble_custom_width == expected.bubble_custom_width);
    assert(actual.bubble_custom_height == expected.bubble_custom_height);
    assert(actual.vsync == expected.vsync);
    assert(actual.fps_limit == expected.fps_limit);
    assert(actual.overrides == expected.overrides);
    assert(SDL_RemovePath(EIDOLON_TEST_SETTINGS_PATH));
}

static void sparse_file_inherits_missing_fields(void) {
    static const char text[] = "version = 1\npreferred_renderer = sprite\n";
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    settings.overrides = EIDOLON_USER_SETTING_DIALOGUE_THEME;
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(eidolon_user_settings_parse(text, strlen(text), &settings, error, sizeof(error)));
    assert(settings.overrides == EIDOLON_USER_SETTING_RENDER_MODE);
    assert(settings.render_mode == 0);
    assert(settings.display_scale == 1.0F);
    assert(settings.dialogue_theme == 0);
}

static void sparse_save_omits_inherited_values(void) {
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    settings.overrides =
        EIDOLON_USER_SETTING_RENDER_MODE | EIDOLON_USER_SETTING_PRESENTATION;
    settings.render_mode = 2;
    settings.presentation_preference = 1;
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(eidolon_user_settings_save(EIDOLON_TEST_SETTINGS_PATH, &settings, error, sizeof(error)));
    size_t size = 0U;
    char *text = SDL_LoadFile(EIDOLON_TEST_SETTINGS_PATH, &size);
    assert(text != NULL);
    assert(strstr(text, "preferred_renderer = model_3d") != NULL);
    assert(strstr(text, "presentation_preference = sdl_window_legacy") != NULL);
    assert(strstr(text, "display_scale") == NULL);
    SDL_free(text);
    assert(SDL_RemovePath(EIDOLON_TEST_SETTINGS_PATH));
}

static void rejects_invalid_fps_limit(void) {
    static const char text[] = "version = 1\nfps_limit = 1001\n";
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(!eidolon_user_settings_parse(text, strlen(text), &settings, error, sizeof(error)));
    assert(settings.fps_limit == 0);
    assert(settings.overrides == 0U);
    assert(strstr(error, "fps_limit") != NULL);
}

static void rejects_invalid_presentation_preference(void) {
    static const char text[] = "version = 1\npresentation_preference = 4\n";
    EidolonUserSettings settings;
    eidolon_user_settings_defaults(&settings);
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    assert(!eidolon_user_settings_parse(text, strlen(text), &settings, error, sizeof(error)));
    assert(settings.presentation_preference == 0);
    assert(settings.overrides == 0U);
    assert(strstr(error, "presentation_preference") != NULL);
}

int main(void) {
    parses_complete_settings();
    rejects_invalid_file_without_partial_apply();
    requires_version();
    saves_and_loads_round_trip();
    sparse_file_inherits_missing_fields();
    sparse_save_omits_inherited_values();
    rejects_invalid_fps_limit();
    rejects_invalid_presentation_preference();
    return 0;
}
