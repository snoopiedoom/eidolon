#include "debug_ui.h"

#include "app.h"
#include "platform/overlay.h"
#include "pose.h"

#define DEBUG_PANEL_WIDTH 244.0F
#define DEBUG_PANEL_HEIGHT 332.0F
#define DEBUG_PANEL_MARGIN 14.0F
#define DEBUG_TRACK_HEIGHT 6.0F
#define DEBUG_TRACK_WIDTH 216.0F
#define DEBUG_SCALE_TRACK_WIDTH 138.0F
#define DEBUG_POSE_ROW_HEIGHT 18.0F
#define DEBUG_RESOLUTION_COUNT 4U

static const int debug_resolutions[DEBUG_RESOLUTION_COUNT] = {512, 1024, 1536, 2048};

static SDL_FRect panel_rect(const EidolonApp *app) {
    return (SDL_FRect){
        .x = DEBUG_PANEL_MARGIN + (float)(app->window_width - EIDOLON_WINDOW_WIDTH),
        .y = DEBUG_PANEL_MARGIN + (float)(app->window_height - EIDOLON_WINDOW_HEIGHT),
        .w = DEBUG_PANEL_WIDTH,
        .h = DEBUG_PANEL_HEIGHT,
    };
}

static SDL_FRect track_rect(const EidolonApp *app, float x_offset, float y_offset) {
    const SDL_FRect panel = panel_rect(app);
    return (SDL_FRect){panel.x + x_offset, panel.y + y_offset, DEBUG_TRACK_WIDTH,
                       DEBUG_TRACK_HEIGHT};
}

static SDL_FRect pose_button(const EidolonApp *app) {
    const SDL_FRect panel = panel_rect(app);
    return (SDL_FRect){panel.x + 14.0F, panel.y + 29.0F, panel.w - 28.0F, DEBUG_POSE_ROW_HEIGHT};
}

static SDL_FRect pose_dropdown_row(const EidolonApp *app, size_t row) {
    const SDL_FRect button = pose_button(app);
    return (SDL_FRect){button.x, button.y + button.h + 2.0F + (float)row * DEBUG_POSE_ROW_HEIGHT,
                       button.w, DEBUG_POSE_ROW_HEIGHT};
}

static SDL_FRect scale_track(const EidolonApp *app) {
    SDL_FRect track = track_rect(app, 14.0F, 67.0F);
    track.w = DEBUG_SCALE_TRACK_WIDTH;
    return track;
}

static SDL_FRect resolution_button(const EidolonApp *app) {
    const SDL_FRect panel = panel_rect(app);
    return (SDL_FRect){panel.x + 158.0F, panel.y + 52.0F, 72.0F, 22.0F};
}

static SDL_FRect resolution_dropdown_row(const EidolonApp *app, size_t row) {
    const SDL_FRect button = resolution_button(app);
    return (SDL_FRect){button.x, button.y + button.h + 2.0F + (float)row * DEBUG_POSE_ROW_HEIGHT,
                       button.w, DEBUG_POSE_ROW_HEIGHT};
}

static SDL_FRect yaw_track(const EidolonApp *app) { return track_rect(app, 14.0F, 93.0F); }

static SDL_FRect pitch_track(const EidolonApp *app) { return track_rect(app, 14.0F, 119.0F); }

static SDL_FRect roll_track(const EidolonApp *app) { return track_rect(app, 14.0F, 145.0F); }

static SDL_FRect arm_lower_track(const EidolonApp *app) { return track_rect(app, 14.0F, 171.0F); }

static SDL_FRect elbow_add_track(const EidolonApp *app) { return track_rect(app, 14.0F, 197.0F); }

static SDL_FRect semantic_hand_forward_track(const EidolonApp *app) {
    return track_rect(app, 14.0F, 223.0F);
}

static SDL_FRect semantic_pole_out_track(const EidolonApp *app) {
    return track_rect(app, 14.0F, 249.0F);
}

static SDL_FRect semantic_pole_up_track(const EidolonApp *app) {
    return track_rect(app, 14.0F, 275.0F);
}

static SDL_FRect semantic_pole_forward_track(const EidolonApp *app) {
    return track_rect(app, 14.0F, 301.0F);
}

static SDL_FRect track_hit_rect(SDL_FRect rect) {
    rect.y -= 7.0F;
    rect.h += 14.0F;
    return rect;
}

static bool point_in_rect(float x, float y, const SDL_FRect *rect) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static float pointer_normalized(float x, const SDL_FRect *track) {
    return SDL_clamp((x - track->x) / track->w, 0.0F, 1.0F);
}

static float rotation_from_pointer(float x, const SDL_FRect *track) {
    const float normalized = pointer_normalized(x, track);
    float degrees =
        EIDOLON_MODEL_ROTATION_MIN_DEGREES +
        normalized * (EIDOLON_MODEL_ROTATION_MAX_DEGREES - EIDOLON_MODEL_ROTATION_MIN_DEGREES);
    if (SDL_fabsf(degrees) < 2.0F) {
        degrees = 0.0F;
    }
    return degrees;
}

static float semantic_value_from_pointer(float x, const SDL_FRect *track) {
    return EIDOLON_POSE_TARGET_MIN +
           pointer_normalized(x, track) * (EIDOLON_POSE_TARGET_MAX - EIDOLON_POSE_TARGET_MIN);
}

static void update_dragged_control(EidolonApp *app, float x) {
    switch (app->debug_drag_control) {
    case EIDOLON_DEBUG_CONTROL_MODEL_SCALE: {
        const SDL_FRect track = scale_track(app);
        const float normalized = pointer_normalized(x, &track);
        eidolon_app_set_model_scale(
            app, EIDOLON_MODEL_SCALE_MIN +
                     normalized * (EIDOLON_MODEL_SCALE_MAX - EIDOLON_MODEL_SCALE_MIN));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_MODEL_YAW: {
        const SDL_FRect track = yaw_track(app);
        eidolon_app_set_model_rotation(app, rotation_from_pointer(x, &track),
                                       app->model_pitch_degrees, app->model_roll_degrees);
        break;
    }
    case EIDOLON_DEBUG_CONTROL_MODEL_PITCH: {
        const SDL_FRect track = pitch_track(app);
        eidolon_app_set_model_rotation(app, app->model_yaw_degrees,
                                       rotation_from_pointer(x, &track), app->model_roll_degrees);
        break;
    }
    case EIDOLON_DEBUG_CONTROL_MODEL_ROLL: {
        const SDL_FRect track = roll_track(app);
        eidolon_app_set_model_rotation(app, app->model_yaw_degrees, app->model_pitch_degrees,
                                       rotation_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_NEUTRAL_ARM_LOWER: {
        const SDL_FRect track = arm_lower_track(app);
        const float normalized = pointer_normalized(x, &track);
        eidolon_app_set_neutral_pose(app,
                                     EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES +
                                         normalized * (EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES -
                                                       EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES),
                                     app->motion_config.neutral_elbow_add_degrees);
        break;
    }
    case EIDOLON_DEBUG_CONTROL_NEUTRAL_ELBOW_ADD: {
        const SDL_FRect track = elbow_add_track(app);
        const float normalized = pointer_normalized(x, &track);
        eidolon_app_set_neutral_pose(app, app->motion_config.neutral_arm_lower_degrees,
                                     EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES +
                                         normalized * (EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES -
                                                       EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_OUT: {
        const SDL_FRect track = arm_lower_track(app);
        eidolon_app_set_semantic_arm_component(app, false, 0U,
                                               semantic_value_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_UP: {
        const SDL_FRect track = elbow_add_track(app);
        eidolon_app_set_semantic_arm_component(app, false, 1U,
                                               semantic_value_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_FORWARD: {
        const SDL_FRect track = semantic_hand_forward_track(app);
        eidolon_app_set_semantic_arm_component(app, false, 2U,
                                               semantic_value_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_OUT: {
        const SDL_FRect track = semantic_pole_out_track(app);
        eidolon_app_set_semantic_arm_component(app, true, 0U,
                                               semantic_value_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_UP: {
        const SDL_FRect track = semantic_pole_up_track(app);
        eidolon_app_set_semantic_arm_component(app, true, 1U,
                                               semantic_value_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_FORWARD: {
        const SDL_FRect track = semantic_pole_forward_track(app);
        eidolon_app_set_semantic_arm_component(app, true, 2U,
                                               semantic_value_from_pointer(x, &track));
        break;
    }
    case EIDOLON_DEBUG_CONTROL_NONE:
        break;
    }
}

static EidolonDebugControl control_at_pointer(const EidolonApp *app, float x, float y) {
    const SDL_FRect scale = track_hit_rect(scale_track(app));
    const SDL_FRect yaw = track_hit_rect(yaw_track(app));
    const SDL_FRect pitch = track_hit_rect(pitch_track(app));
    const SDL_FRect roll = track_hit_rect(roll_track(app));
    if (point_in_rect(x, y, &scale)) {
        return EIDOLON_DEBUG_CONTROL_MODEL_SCALE;
    }
    if (point_in_rect(x, y, &yaw)) {
        return EIDOLON_DEBUG_CONTROL_MODEL_YAW;
    }
    if (point_in_rect(x, y, &pitch)) {
        return EIDOLON_DEBUG_CONTROL_MODEL_PITCH;
    }
    if (point_in_rect(x, y, &roll)) {
        return EIDOLON_DEBUG_CONTROL_MODEL_ROLL;
    }
    const SDL_FRect first_left = track_hit_rect(arm_lower_track(app));
    const SDL_FRect first_right = track_hit_rect(elbow_add_track(app));
    if (app->semantic_pose_index < 0) {
        if (point_in_rect(x, y, &first_left)) {
            return EIDOLON_DEBUG_CONTROL_NEUTRAL_ARM_LOWER;
        }
        if (point_in_rect(x, y, &first_right)) {
            return EIDOLON_DEBUG_CONTROL_NEUTRAL_ELBOW_ADD;
        }
        return EIDOLON_DEBUG_CONTROL_NONE;
    }
    if (point_in_rect(x, y, &first_left)) {
        return EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_OUT;
    }
    if (point_in_rect(x, y, &first_right)) {
        return EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_UP;
    }
    const SDL_FRect second_left = track_hit_rect(semantic_hand_forward_track(app));
    const SDL_FRect second_right = track_hit_rect(semantic_pole_out_track(app));
    const SDL_FRect third_left = track_hit_rect(semantic_pole_up_track(app));
    const SDL_FRect third_right = track_hit_rect(semantic_pole_forward_track(app));
    if (point_in_rect(x, y, &second_left)) {
        return EIDOLON_DEBUG_CONTROL_SEMANTIC_HAND_FORWARD;
    }
    if (point_in_rect(x, y, &second_right)) {
        return EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_OUT;
    }
    if (point_in_rect(x, y, &third_left)) {
        return EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_UP;
    }
    if (point_in_rect(x, y, &third_right)) {
        return EIDOLON_DEBUG_CONTROL_SEMANTIC_POLE_FORWARD;
    }
    return EIDOLON_DEBUG_CONTROL_NONE;
}

bool eidolon_debug_ui_handle_event(EidolonApp *app, const SDL_Event *event) {
    if (!app->debug_visible) {
        return false;
    }

    switch (event->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        const SDL_FRect panel = panel_rect(app);
        if (event->button.button == SDL_BUTTON_LEFT) {
            const SDL_FRect button = pose_button(app);
            if (point_in_rect(event->button.x, event->button.y, &button)) {
                app->debug_pose_dropdown_open = !app->debug_pose_dropdown_open;
                app->debug_resolution_dropdown_open = false;
                app->debug_drag_control = EIDOLON_DEBUG_CONTROL_NONE;
                return true;
            }
            const SDL_FRect render_resolution = resolution_button(app);
            if (point_in_rect(event->button.x, event->button.y, &render_resolution)) {
                app->debug_resolution_dropdown_open = !app->debug_resolution_dropdown_open;
                app->debug_pose_dropdown_open = false;
                app->debug_drag_control = EIDOLON_DEBUG_CONTROL_NONE;
                return true;
            }
            if (app->debug_pose_dropdown_open) {
                const size_t row_count = eidolon_semantic_pose_count() + 1U;
                for (size_t row = 0; row < row_count; ++row) {
                    const SDL_FRect item = pose_dropdown_row(app, row);
                    if (!point_in_rect(event->button.x, event->button.y, &item)) {
                        continue;
                    }
                    if (row == 0U) {
                        eidolon_motion_config_watch_force_reload(&app->motion_config_watch);
                    } else {
                        eidolon_app_select_semantic_pose(app, (int)(row - 1U));
                    }
                    app->debug_pose_dropdown_open = false;
                    return true;
                }
                app->debug_pose_dropdown_open = false;
                return true;
            }
            if (app->debug_resolution_dropdown_open) {
                for (size_t row = 0; row < DEBUG_RESOLUTION_COUNT; ++row) {
                    const SDL_FRect item = resolution_dropdown_row(app, row);
                    if (!point_in_rect(event->button.x, event->button.y, &item)) {
                        continue;
                    }
                    (void)eidolon_app_set_model_render_resolution(app, debug_resolutions[row]);
                    app->debug_resolution_dropdown_open = false;
                    return true;
                }
                app->debug_resolution_dropdown_open = false;
                return true;
            }
        }
        if (point_in_rect(event->button.x, event->button.y, &panel)) {
            if (event->button.button == SDL_BUTTON_LEFT) {
                app->debug_drag_control = control_at_pointer(app, event->button.x, event->button.y);
                if (app->debug_drag_control != EIDOLON_DEBUG_CONTROL_NONE) {
                    eidolon_platform_suspend_hit_test(app->window);
                }
                update_dragged_control(app, event->button.x);
            }
            return true;
        }
        if (app->debug_pose_dropdown_open || app->debug_resolution_dropdown_open) {
            app->debug_pose_dropdown_open = false;
            app->debug_resolution_dropdown_open = false;
            return true;
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (app->debug_pose_dropdown_open || app->debug_resolution_dropdown_open) {
            return true;
        }
        if (app->debug_drag_control != EIDOLON_DEBUG_CONTROL_NONE) {
            update_dragged_control(app, event->motion.x);
            return true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT &&
            app->debug_drag_control != EIDOLON_DEBUG_CONTROL_NONE) {
            const EidolonDebugControl completed_control = app->debug_drag_control;
            update_dragged_control(app, event->button.x);
            app->debug_drag_control = EIDOLON_DEBUG_CONTROL_NONE;
            app->hit_test_initialized = false;
            if (completed_control == EIDOLON_DEBUG_CONTROL_MODEL_SCALE) {
                eidolon_app_log_presentation_metrics(app);
            }
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

static void draw_slider(SDL_Renderer *renderer, const SDL_FRect *track, float normalized) {
    normalized = SDL_clamp(normalized, 0.0F, 1.0F);
    SDL_SetRenderDrawColor(renderer, 55, 56, 67, 255);
    SDL_RenderFillRect(renderer, track);
    const SDL_FRect filled = {track->x, track->y, track->w * normalized, track->h};
    SDL_SetRenderDrawColor(renderer, 112, 156, 255, 255);
    SDL_RenderFillRect(renderer, &filled);
    const SDL_FRect knob = {track->x + track->w * normalized - 3.0F, track->y - 3.0F, 6.0F, 12.0F};
    SDL_SetRenderDrawColor(renderer, 235, 236, 244, 255);
    SDL_RenderFillRect(renderer, &knob);
}

static float rotation_normalized(float degrees) {
    return (degrees - EIDOLON_MODEL_ROTATION_MIN_DEGREES) /
           (EIDOLON_MODEL_ROTATION_MAX_DEGREES - EIDOLON_MODEL_ROTATION_MIN_DEGREES);
}

static float semantic_value_normalized(float value) {
    return (value - EIDOLON_POSE_TARGET_MIN) / (EIDOLON_POSE_TARGET_MAX - EIDOLON_POSE_TARGET_MIN);
}

static const char *selected_pose_name(const EidolonApp *app) {
    if (app->semantic_pose_index >= 0) {
        const EidolonSemanticPose *pose = eidolon_semantic_pose((size_t)app->semantic_pose_index);
        if (pose != NULL) {
            return pose->name;
        }
    }
    return "CUSTOM / FILE";
}

static void draw_pose_selector(EidolonApp *app) {
    const SDL_FRect button = pose_button(app);
    SDL_SetRenderDrawColor(app->renderer, 27, 28, 36, 255);
    SDL_RenderFillRect(app->renderer, &button);
    SDL_SetRenderDrawColor(app->renderer, 74, 76, 91, 255);
    SDL_RenderRect(app->renderer, &button);
    SDL_SetRenderDrawColor(app->renderer, 194, 193, 204, 255);
    SDL_RenderDebugTextFormat(app->renderer, button.x + 6.0F, button.y + 5.0F, "%s%s",
                              selected_pose_name(app), app->semantic_pose_dirty ? " *" : "");
    SDL_RenderDebugText(app->renderer, button.x + button.w - 14.0F, button.y + 5.0F,
                        app->debug_pose_dropdown_open ? "^" : "v");
}

static void draw_pose_dropdown(EidolonApp *app) {
    if (!app->debug_pose_dropdown_open) {
        return;
    }
    const SDL_FRect first_row = pose_dropdown_row(app, 0U);
    const size_t row_count = eidolon_semantic_pose_count() + 1U;
    const SDL_FRect backdrop = {
        first_row.x,
        first_row.y,
        first_row.w,
        (float)row_count * DEBUG_POSE_ROW_HEIGHT,
    };
    SDL_SetRenderDrawColor(app->renderer, 12, 12, 17, 255);
    SDL_RenderFillRect(app->renderer, &backdrop);
    for (size_t row = 0; row < row_count; ++row) {
        const SDL_FRect item = pose_dropdown_row(app, row);
        const bool selected =
            row == 0U ? app->semantic_pose_index < 0 : app->semantic_pose_index == (int)(row - 1U);
        SDL_SetRenderDrawColor(app->renderer, selected ? 64 : 22, selected ? 78 : 23,
                               selected ? 112 : 31, 255);
        SDL_RenderFillRect(app->renderer, &item);
        SDL_SetRenderDrawColor(app->renderer, 74, 76, 91, 255);
        SDL_RenderRect(app->renderer, &item);
        SDL_SetRenderDrawColor(app->renderer, 226, 225, 234, 255);
        if (row == 0U) {
            SDL_RenderDebugText(app->renderer, item.x + 6.0F, item.y + 5.0F,
                                "CUSTOM / FILE (RELOAD)");
        } else {
            const EidolonSemanticPose *pose = eidolon_semantic_pose(row - 1U);
            SDL_RenderDebugText(app->renderer, item.x + 6.0F, item.y + 5.0F, pose->name);
        }
    }
}

static void draw_resolution_selector(EidolonApp *app) {
    const SDL_FRect button = resolution_button(app);
    SDL_SetRenderDrawColor(app->renderer, 27, 28, 36, 255);
    SDL_RenderFillRect(app->renderer, &button);
    SDL_SetRenderDrawColor(app->renderer, 74, 76, 91, 255);
    SDL_RenderRect(app->renderer, &button);
    SDL_SetRenderDrawColor(app->renderer, 226, 225, 234, 255);
    const int resolution = eidolon_model_render_resolution(app->model);
    if (resolution > 0) {
        SDL_RenderDebugTextFormat(app->renderer, button.x + 6.0F, button.y + 7.0F, "%d %s",
                                  resolution, app->debug_resolution_dropdown_open ? "^" : "v");
    } else {
        SDL_RenderDebugText(app->renderer, button.x + 6.0F, button.y + 7.0F, "SPRITE");
    }
}

static void draw_resolution_dropdown(EidolonApp *app) {
    if (!app->debug_resolution_dropdown_open) {
        return;
    }
    const int active = eidolon_model_render_resolution(app->model);
    for (size_t row = 0; row < DEBUG_RESOLUTION_COUNT; ++row) {
        const SDL_FRect item = resolution_dropdown_row(app, row);
        const bool selected = active == debug_resolutions[row];
        SDL_SetRenderDrawColor(app->renderer, selected ? 64 : 22, selected ? 78 : 23,
                               selected ? 112 : 31, 255);
        SDL_RenderFillRect(app->renderer, &item);
        SDL_SetRenderDrawColor(app->renderer, 74, 76, 91, 255);
        SDL_RenderRect(app->renderer, &item);
        SDL_SetRenderDrawColor(app->renderer, 226, 225, 234, 255);
        SDL_RenderDebugTextFormat(app->renderer, item.x + 6.0F, item.y + 5.0F, "%d",
                                  debug_resolutions[row]);
    }
}

void eidolon_debug_ui_draw(EidolonApp *app) {
    if (!app->debug_visible) {
        return;
    }

    const SDL_FRect panel = panel_rect(app);
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(app->renderer, 12, 12, 17, 242);
    SDL_RenderFillRect(app->renderer, &panel);
    SDL_SetRenderDrawColor(app->renderer, 112, 156, 255, 255);
    SDL_RenderRect(app->renderer, &panel);

    SDL_SetRenderDrawColor(app->renderer, 245, 244, 248, 255);
    SDL_RenderDebugText(app->renderer, panel.x + 14.0F, panel.y + 10.0F, "EIDOLON / F1");
    SDL_SetRenderDrawColor(app->renderer, 96, 95, 106, 255);
    SDL_RenderDebugTextFormat(app->renderer, panel.x + 142.0F, panel.y + 10.0F, "R%llu%s %.2fx",
                              (unsigned long long)app->motion_config_watch.revision,
                              app->motion_config_dirty ? "*" : "", app->display_scale);

    draw_pose_selector(app);

    SDL_SetRenderDrawColor(app->renderer, 168, 166, 177, 255);
    SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 54.0F, "SCALE  %.2fx",
                              app->model_scale);
    const SDL_FRect scale = scale_track(app);
    draw_slider(app->renderer, &scale,
                (app->model_scale - EIDOLON_MODEL_SCALE_MIN) /
                    (EIDOLON_MODEL_SCALE_MAX - EIDOLON_MODEL_SCALE_MIN));
    draw_resolution_selector(app);

    SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 80.0F, "YAW    %+.1f DEG",
                              app->model_yaw_degrees);
    const SDL_FRect yaw = yaw_track(app);
    draw_slider(app->renderer, &yaw, rotation_normalized(app->model_yaw_degrees));

    SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 106.0F, "PITCH  %+.1f DEG",
                              app->model_pitch_degrees);
    const SDL_FRect pitch = pitch_track(app);
    draw_slider(app->renderer, &pitch, rotation_normalized(app->model_pitch_degrees));

    SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 132.0F, "ROLL   %+.1f DEG",
                              app->model_roll_degrees);
    const SDL_FRect roll = roll_track(app);
    draw_slider(app->renderer, &roll, rotation_normalized(app->model_roll_degrees));

    const SDL_FRect first_left = arm_lower_track(app);
    const SDL_FRect first_right = elbow_add_track(app);
    if (app->semantic_pose_index < 0) {
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 158.0F,
                                  "ARM LOWER  %+.1f DEG",
                                  app->motion_config.neutral_arm_lower_degrees);
        draw_slider(
            app->renderer, &first_left,
            (app->motion_config.neutral_arm_lower_degrees - EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES) /
                (EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES - EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES));
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 184.0F,
                                  "ELBOW ADD  %+.1f DEG",
                                  app->motion_config.neutral_elbow_add_degrees);
        draw_slider(
            app->renderer, &first_right,
            (app->motion_config.neutral_elbow_add_degrees - EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES) /
                (EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES - EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES));
        SDL_RenderDebugText(app->renderer, panel.x + 14.0F, panel.y + 218.0F,
                            "SELECT A POSE FOR TARGETS");
    } else {
        const EidolonArmPoseGoal *goal = &app->semantic_pose.arms[0];
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 158.0F,
                                  "HAND OUT  %+.2f", goal->hand[0]);
        draw_slider(app->renderer, &first_left, semantic_value_normalized(goal->hand[0]));
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 184.0F,
                                  "HAND UP   %+.2f", goal->hand[1]);
        draw_slider(app->renderer, &first_right, semantic_value_normalized(goal->hand[1]));

        const SDL_FRect hand_forward = semantic_hand_forward_track(app);
        const SDL_FRect pole_out = semantic_pole_out_track(app);
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 210.0F,
                                  "HAND FWD  %+.2f", goal->hand[2]);
        draw_slider(app->renderer, &hand_forward, semantic_value_normalized(goal->hand[2]));
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 236.0F,
                                  "POLE OUT  %+.2f", goal->elbow_pole[0]);
        draw_slider(app->renderer, &pole_out, semantic_value_normalized(goal->elbow_pole[0]));

        const SDL_FRect pole_up = semantic_pole_up_track(app);
        const SDL_FRect pole_forward = semantic_pole_forward_track(app);
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 262.0F,
                                  "POLE UP   %+.2f", goal->elbow_pole[1]);
        draw_slider(app->renderer, &pole_up, semantic_value_normalized(goal->elbow_pole[1]));
        SDL_RenderDebugTextFormat(app->renderer, panel.x + 14.0F, panel.y + 288.0F,
                                  "POLE FWD  %+.2f", goal->elbow_pole[2]);
        draw_slider(app->renderer, &pole_forward, semantic_value_normalized(goal->elbow_pole[2]));
    }

    if (app->motion_config_watch.error[0] != '\0') {
        char error[28];
        SDL_strlcpy(error, app->motion_config_watch.error, sizeof(error));
        SDL_SetRenderDrawColor(app->renderer, 255, 105, 105, 255);
        SDL_RenderDebugText(app->renderer, panel.x + 14.0F, panel.y + 318.0F, error);
    } else {
        SDL_SetRenderDrawColor(app->renderer, 136, 135, 147, 255);
        const char *help = "F5 CFG  MMB ROT  SHIFT ROLL";
        if (app->semantic_pose_copied_until_ms > SDL_GetTicks()) {
            help = "POSE INITIALIZER COPIED";
        } else if (app->semantic_pose_index >= 0) {
            help = "C COPY  MMB ROT  SHIFT ROLL";
        }
        SDL_RenderDebugText(app->renderer, panel.x + 14.0F, panel.y + 318.0F, help);
    }
    draw_pose_dropdown(app);
    draw_resolution_dropdown(app);
}
