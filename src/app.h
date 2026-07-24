#ifndef EIDOLON_APP_H
#define EIDOLON_APP_H

#include "eidolon.h"

typedef enum EidolonAppMode {
    EIDOLON_APP_INTERACTIVE,
    EIDOLON_APP_SNAPSHOT,
} EidolonAppMode;

bool eidolon_app_init(EidolonApp *app, EidolonAppMode mode);
void eidolon_app_run(EidolonApp *app);
void eidolon_app_set_state(EidolonApp *app, EidolonState state);
bool eidolon_app_set_render_mode(EidolonApp *app, EidolonRenderMode mode);
void eidolon_app_set_presentation_preference(EidolonApp *app,
                                             EidolonPresentationPreference preference);
const char *eidolon_render_mode_name(EidolonRenderMode mode);
const char *eidolon_app_model_name(const EidolonApp *app);
void eidolon_app_select_portrait(EidolonApp *app, int expression);
void eidolon_app_set_portrait_framing(EidolonApp *app, bool face_mode);
void eidolon_app_reload_configs(EidolonApp *app);
void eidolon_app_set_dialogue_theme(EidolonApp *app, EidolonDialogueTheme theme);
void eidolon_app_set_dialogue_movement(EidolonApp *app, EidolonDialogueMovement movement);
void eidolon_app_set_dialogue_hold_ms(EidolonApp *app, unsigned int hold_ms);
void eidolon_app_set_bubble_bounds_mode(EidolonApp *app, EidolonBubbleBoundsMode mode);
void eidolon_app_set_bubble_custom_bounds(EidolonApp *app, SDL_Rect bounds);
void eidolon_app_set_vsync(EidolonApp *app, bool enabled);
void eidolon_app_set_fps_limit(EidolonApp *app, int fps_limit);
bool eidolon_app_reset_user_setting(EidolonApp *app, EidolonUserSettingField field);
void eidolon_app_set_model_scale(EidolonApp *app, float scale);
bool eidolon_app_set_model_render_resolution(EidolonApp *app, int side);
bool eidolon_app_update_performance_fixture(EidolonApp *app, uint64_t now_ms);
void eidolon_app_log_presentation_metrics(const EidolonApp *app);
void eidolon_app_set_model_rotation(EidolonApp *app, float yaw_degrees, float pitch_degrees,
                                    float roll_degrees);
void eidolon_app_set_neutral_pose(EidolonApp *app, float arm_lower_degrees,
                                  float elbow_add_degrees);
void eidolon_app_select_semantic_pose(EidolonApp *app, int pose_index);
void eidolon_app_set_semantic_arm_component(EidolonApp *app, bool pole, size_t component,
                                            float value);
bool eidolon_app_copy_semantic_pose(EidolonApp *app);
void eidolon_app_destroy(EidolonApp *app);

#endif
