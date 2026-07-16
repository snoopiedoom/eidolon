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
void eidolon_app_set_model_scale(EidolonApp *app, float scale);
bool eidolon_app_set_model_render_resolution(EidolonApp *app, int side);
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
