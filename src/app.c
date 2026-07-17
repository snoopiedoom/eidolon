#include "app.h"

#include "animation.h"
#include "bubble_layout.h"
#include "debug_ui.h"
#include "draw.h"
#include "log.h"
#include "platform/overlay.h"
#include "pose.h"

#define MODEL_ROTATION_DEGREES_PER_PIXEL 0.35F
#define PRESENTATION_RATE_HZ 30U
#define PRESENTATION_INTERVAL_NS ((Uint64)SDL_NS_PER_SECOND / PRESENTATION_RATE_HZ)

static float session_attention_direction(const EidolonSessionEntry *entry) {
    return entry != NULL && (entry->layout_slot & 1) != 0 ? 1.0F : -1.0F;
}

static uint64_t begin_affect_text(EidolonApp *app, const char *text) {
    const uint64_t sequence = eidolon_affect_controller_begin_text(&app->affect);
    (void)eidolon_affect_client_submit(app->affect_client, sequence, text);
    return sequence;
}

static Sint32 presentation_wait_ms(Uint64 now_ns, Uint64 deadline_ns) {
    if (now_ns >= deadline_ns) {
        return 0;
    }
    const Uint64 remaining_ns = deadline_ns - now_ns;
    return (Sint32)((remaining_ns + (Uint64)SDL_NS_PER_MS - 1U) / (Uint64)SDL_NS_PER_MS);
}

static Uint64 advance_presentation_deadline(Uint64 deadline_ns, Uint64 completed_ns) {
    deadline_ns += PRESENTATION_INTERVAL_NS;
    if (deadline_ns <= completed_ns) {
        deadline_ns = completed_ns + PRESENTATION_INTERVAL_NS;
    }
    return deadline_ns;
}

static EidolonNeutralPose neutral_pose_from_config(const EidolonMotionConfig *config) {
    return (EidolonNeutralPose){
        .shoulder_lower_radians = config->neutral_arm_lower_degrees * SDL_PI_F / 180.0F,
        .elbow_bend_add_radians = config->neutral_elbow_add_degrees * SDL_PI_F / 180.0F,
    };
}

static EidolonIdleTuning idle_tuning_from_config(const EidolonMotionConfig *config) {
    return (EidolonIdleTuning){
        .breath_period_seconds = config->breath_period_seconds,
        .breath_chest_radians = config->breath_chest_degrees * SDL_PI_F / 180.0F,
        .breath_neck_counter_radians = config->breath_neck_counter_degrees * SDL_PI_F / 180.0F,
        .sway_period_seconds = config->sway_period_seconds,
        .sway_spine_radians = config->sway_spine_degrees * SDL_PI_F / 180.0F,
        .sway_chest_counter_radians = config->sway_chest_counter_degrees * SDL_PI_F / 180.0F,
        .sway_head_radians = config->sway_head_degrees * SDL_PI_F / 180.0F,
    };
}

static void apply_motion_config(EidolonApp *app) {
    const EidolonNeutralPose neutral = neutral_pose_from_config(&app->motion_config);
    eidolon_model_set_neutral_pose(app->model, neutral.shoulder_lower_radians,
                                   neutral.elbow_bend_add_radians);
    eidolon_model_set_idle_tuning(app->model, idle_tuning_from_config(&app->motion_config));
}

static void clear_semantic_pose(EidolonApp *app) {
    app->semantic_pose_index = EIDOLON_SEMANTIC_POSE_CUSTOM;
    app->drag_session_slot = -1;
    app->semantic_pose_dirty = false;
    eidolon_model_clear_semantic_pose(app->model);
}

static void poll_motion_config(EidolonApp *app, uint64_t now_ms) {
    const EidolonMotionConfigPollResult result = eidolon_motion_config_watch_poll(
        &app->motion_config_watch, EIDOLON_MOTION_CONFIG_PATH, now_ms, &app->motion_config);
    switch (result) {
    case EIDOLON_MOTION_CONFIG_APPLIED:
        app->motion_config_dirty = false;
        clear_semantic_pose(app);
        apply_motion_config(app);
        eidolon_log_write("motion", "config applied revision=%llu hash=%016llx seed=%llu",
                          (unsigned long long)app->motion_config_watch.revision,
                          (unsigned long long)app->motion_config_watch.active_hash,
                          (unsigned long long)app->motion_config.seed);
        break;
    case EIDOLON_MOTION_CONFIG_ERROR:
        eidolon_log_write("motion", "config rejected; retaining last good: %s",
                          app->motion_config_watch.error);
        break;
    case EIDOLON_MOTION_CONFIG_RECOVERED:
        eidolon_log_write("motion", "config file available again; active values unchanged");
        break;
    case EIDOLON_MOTION_CONFIG_UNCHANGED:
        break;
    }
}

static bool load_atlas(EidolonApp *app) {
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%s/mutsuki-dress.png", EIDOLON_ASSET_DIR);

    SDL_Surface *surface = SDL_LoadPNG(path);
    if (surface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load %s: %s", path, SDL_GetError());
        return false;
    }

    const bool dimensions_are_valid = surface->w == EIDOLON_ATLAS_COLUMNS * EIDOLON_CELL_WIDTH &&
                                      surface->h == EIDOLON_ATLAS_ROWS * EIDOLON_CELL_HEIGHT;
    if (!dimensions_are_valid) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid v2 atlas size %dx%d", surface->w,
                     surface->h);
        SDL_DestroySurface(surface);
        return false;
    }

    app->atlas = SDL_CreateTextureFromSurface(app->renderer, surface);
    SDL_DestroySurface(surface);
    if (app->atlas == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create atlas texture: %s",
                     SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(app->atlas, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(app->atlas, SDL_BLENDMODE_BLEND);
    return true;
}

static void set_initial_position(SDL_Window *window) {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    SDL_Rect bounds;
    if (!SDL_GetDisplayUsableBounds(display, &bounds)) {
        return;
    }

    const int margin = 24;
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    SDL_SetWindowPosition(window, bounds.x + bounds.w - width - margin,
                          bounds.y + bounds.h - height - margin);
}

static void update_display_metrics(EidolonApp *app) {
    float display_scale = SDL_GetWindowDisplayScale(app->window);
    if (display_scale <= 0.0F) {
        display_scale = 1.0F;
    }
    float pixel_density = SDL_GetWindowPixelDensity(app->window);
    if (pixel_density <= 0.0F) {
        pixel_density = 1.0F;
    }

    app->display_scale = display_scale;
    app->window_coordinate_scale = display_scale / pixel_density;
    if (!SDL_SetRenderScale(app->renderer, display_scale, display_scale)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not apply display scale: %s",
                    SDL_GetError());
    }
}

bool eidolon_app_init(EidolonApp *app, EidolonAppMode mode) {
    SDL_zero(*app);
    app->snapshot_mode = mode == EIDOLON_APP_SNAPSHOT;
    app->model_scale = 1.0F;
    app->display_scale = 1.0F;
    app->window_coordinate_scale = 1.0F;
    app->window_width = EIDOLON_WINDOW_WIDTH;
    app->window_height = EIDOLON_WINDOW_HEIGHT;
    app->dialogue_theme = EIDOLON_DIALOGUE_THEME_CLASSIC;
    app->dialogue_movement = EIDOLON_DIALOGUE_MOVEMENT_FOLLOW;
    app->dialogue_hold_ms = EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS;
    app->semantic_pose_index = EIDOLON_SEMANTIC_POSE_CUSTOM;
    eidolon_motion_config_defaults(&app->motion_config);
    eidolon_motion_config_watch_init(&app->motion_config_watch);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    poll_motion_config(app, SDL_GetTicks());

    const SDL_WindowFlags flags =
        app->snapshot_mode ? SDL_WINDOW_HIDDEN
                           : SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
                                 SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    app->window = SDL_CreateWindow("Eidolon", EIDOLON_WINDOW_WIDTH, EIDOLON_WINDOW_HEIGHT, flags);
    if (app->window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

#if defined(_WIN32)
    app->renderer = SDL_CreateRenderer(app->window, "direct3d11");
    if (app->renderer == NULL) {
        eidolon_log_write("renderer", "direct3d11 unavailable; sprite fallback may be used: %s",
                          SDL_GetError());
        SDL_ClearError();
        app->renderer = SDL_CreateRenderer(app->window, NULL);
    }
#else
    app->renderer = SDL_CreateRenderer(app->window, NULL);
#endif
    if (app->renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    eidolon_log_write("renderer", "SDL backend=%s", SDL_GetRendererName(app->renderer));

    app->text_renderer = eidolon_text_renderer_create(app->renderer, EIDOLON_FONT_PATH, 12.0F);
    if (app->text_renderer == NULL) {
        eidolon_log_write("text", "Unicode renderer unavailable; debug text fallback active: %s",
                          SDL_GetError());
        SDL_ClearError();
    }

    if (!SDL_SetRenderVSync(app->renderer, app->snapshot_mode ? 0 : 1)) {
        eidolon_log_write("renderer", "vsync request failed; explicit %u Hz cap remains active: %s",
                          PRESENTATION_RATE_HZ, SDL_GetError());
        SDL_ClearError();
    }
    if (!app->snapshot_mode) {
        update_display_metrics(app);
    }

    app->portrait =
        eidolon_portrait_create(app->renderer, EIDOLON_CHARACTER_CONFIG_PATH, EIDOLON_ASSET_DIR);
    if (app->portrait == NULL) {
        eidolon_log_write("portrait", "initialization failed; trying legacy renderers: %s",
                          SDL_GetError());
        SDL_ClearError();
        if (!load_atlas(app)) {
            return false;
        }
        app->model = eidolon_model_create(app->renderer, EIDOLON_MODEL_PATH, EIDOLON_SHADER_DIR,
                                          neutral_pose_from_config(&app->motion_config),
                                          idle_tuning_from_config(&app->motion_config));
        if (app->model == NULL) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Rio 3D renderer unavailable; using sprite fallback: %s", SDL_GetError());
            eidolon_log_write("model", "initialization failed; sprite fallback active: %s",
                              SDL_GetError());
        }
    } else {
        eidolon_log_write("renderer", "portrait active; 3D initialization skipped");
        app->dialogue_theme = eidolon_portrait_dialogue_theme(app->portrait);
        app->dialogue_movement = eidolon_portrait_dialogue_movement(app->portrait);
        app->dialogue_hold_ms = eidolon_portrait_dialogue_hold_ms(app->portrait);
    }

    eidolon_app_set_model_scale(app, app->model_scale);
    if (!app->snapshot_mode) {
        if (!eidolon_platform_configure_overlay(app->window)) {
            return false;
        }
        set_initial_position(app->window);
    }

    if (!app->snapshot_mode) {
        if (!eidolon_ipc_server_init(&app->ipc)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Could not open the local state channel; is Eidolon already running?");
            return false;
        }
        eidolon_log_write("renderer", "ipc server ready");
        if (!eidolon_session_registry_init(&app->session_registry)) {
            eidolon_log_write("session", "background discovery unavailable: %s", SDL_GetError());
        }
        eidolon_session_registry_configure_dialogue(
            &app->session_registry, app->dialogue_movement, app->dialogue_hold_ms);
    }

    app->running = true;
    app->state = EIDOLON_STATE_IDLE;
    eidolon_affect_controller_init(&app->affect, app->state, SDL_GetTicks());
    if (!app->snapshot_mode) {
        app->affect_client = eidolon_affect_client_create(EIDOLON_AFFECT_WORKER_PATH);
    }
    eidolon_animation_set_state(&app->animation, app->state, SDL_GetTicks());
    if (!app->snapshot_mode) {
        eidolon_app_log_presentation_metrics(app);
    }
    return true;
}

void eidolon_app_set_state(EidolonApp *app, EidolonState state) {
    if (state == app->state) {
        return;
    }
    app->state = state;
    eidolon_affect_controller_set_state(&app->affect, state, SDL_GetTicks());
    eidolon_animation_set_state(&app->animation, app->state, SDL_GetTicks());
    eidolon_portrait_set_state(app->portrait, state, SDL_GetTicks());
}

void eidolon_app_select_portrait(EidolonApp *app, int expression) {
    eidolon_portrait_set_override(app->portrait, expression, SDL_GetTicks());
    app->hit_test_initialized = false;
}

void eidolon_app_set_model_scale(EidolonApp *app, float scale) {
    const float clamped = SDL_clamp(scale, EIDOLON_MODEL_SCALE_MIN, EIDOLON_MODEL_SCALE_MAX);
    int logical_width = 0;
    int logical_height = 0;
    if (eidolon_portrait_ready(app->portrait)) {
        const float portrait_width = eidolon_portrait_display_width(app->portrait) * clamped;
        const float portrait_height = eidolon_portrait_display_height(app->portrait) * clamped;
        logical_width = SDL_max(EIDOLON_WINDOW_WIDTH, (int)SDL_ceilf(portrait_width + 24.0F));
        logical_height = SDL_max(EIDOLON_WINDOW_HEIGHT, (int)SDL_ceilf(portrait_height + 24.0F));
        size_t visible = eidolon_session_registry_visible_count(&app->session_registry);
        if (visible == 0U && eidolon_portrait_face_mode(app->portrait)) {
            visible = 1U;
        }
        eidolon_bubble_layout_canvas(portrait_width, portrait_height, visible, &logical_width,
                                     &logical_height);
    } else {
        const float model_size = EIDOLON_MODEL_DISPLAY_SIZE * clamped;
        logical_width =
            SDL_max(EIDOLON_WINDOW_WIDTH, (int)SDL_ceilf((float)EIDOLON_WINDOW_WIDTH -
                                                         EIDOLON_MODEL_DISPLAY_SIZE + model_size));
        logical_height =
            SDL_max(EIDOLON_WINDOW_HEIGHT, (int)SDL_ceilf((float)EIDOLON_WINDOW_HEIGHT -
                                                          EIDOLON_MODEL_DISPLAY_SIZE + model_size));
    }
    const int native_width =
        SDL_max(1, (int)SDL_ceilf((float)logical_width * app->window_coordinate_scale));
    const int native_height =
        SDL_max(1, (int)SDL_ceilf((float)logical_height * app->window_coordinate_scale));

    int old_x = 0;
    int old_y = 0;
    int old_width = 0;
    int old_height = 0;
    SDL_GetWindowPosition(app->window, &old_x, &old_y);
    SDL_GetWindowSize(app->window, &old_width, &old_height);

    if (old_width != native_width || old_height != native_height) {
        if (!SDL_SetWindowSize(app->window, native_width, native_height)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not resize overlay: %s",
                        SDL_GetError());
            return;
        }
        if (!SDL_SetWindowPosition(app->window, old_x + old_width - native_width,
                                   old_y + old_height - native_height)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not preserve overlay anchor: %s",
                        SDL_GetError());
        }
    }
    app->model_scale = clamped;
    app->window_width = logical_width;
    app->window_height = logical_height;
    if (app->debug_drag_control != EIDOLON_DEBUG_CONTROL_MODEL_SCALE) {
        app->hit_test_initialized = false;
    }
}

bool eidolon_app_set_model_render_resolution(EidolonApp *app, int side) {
    if (!eidolon_model_set_render_resolution(app->model, side)) {
        eidolon_log_write("renderer", "could not set model render resolution to %d: %s", side,
                          SDL_GetError());
        SDL_ClearError();
        return false;
    }
    app->hit_test_initialized = false;
    if (!app->snapshot_mode) {
        eidolon_app_log_presentation_metrics(app);
    }
    return true;
}

void eidolon_app_log_presentation_metrics(const EidolonApp *app) {
    int window_width = 0;
    int window_height = 0;
    int output_width = 0;
    int output_height = 0;
    int vsync = 0;
    (void)SDL_GetWindowSize(app->window, &window_width, &window_height);
    (void)SDL_GetCurrentRenderOutputSize(app->renderer, &output_width, &output_height);
    (void)SDL_GetRenderVSync(app->renderer, &vsync);
    eidolon_log_write(
        "renderer",
        "presentation scale=%.2fx logical=%dx%d window=%dx%d output=%dx%d target=%d cap=%uHz "
        "vsync=%d",
        app->model_scale, app->window_width, app->window_height, window_width, window_height,
        output_width, output_height, eidolon_model_render_resolution(app->model),
        PRESENTATION_RATE_HZ, vsync);
}

static float normalize_degrees(float degrees) {
    while (degrees > EIDOLON_MODEL_ROTATION_MAX_DEGREES) {
        degrees -= 360.0F;
    }
    while (degrees < EIDOLON_MODEL_ROTATION_MIN_DEGREES) {
        degrees += 360.0F;
    }
    return degrees;
}

void eidolon_app_set_model_rotation(EidolonApp *app, float yaw_degrees, float pitch_degrees,
                                    float roll_degrees) {
    app->model_yaw_degrees = normalize_degrees(yaw_degrees);
    app->model_pitch_degrees = normalize_degrees(pitch_degrees);
    app->model_roll_degrees = normalize_degrees(roll_degrees);
    eidolon_model_set_rotation(app->model, app->model_yaw_degrees * SDL_PI_F / 180.0F,
                               app->model_pitch_degrees * SDL_PI_F / 180.0F,
                               app->model_roll_degrees * SDL_PI_F / 180.0F);
}

void eidolon_app_set_neutral_pose(EidolonApp *app, float arm_lower_degrees,
                                  float elbow_add_degrees) {
    app->motion_config.neutral_arm_lower_degrees =
        SDL_clamp(arm_lower_degrees, EIDOLON_NEUTRAL_ARM_LOWER_MIN_DEGREES,
                  EIDOLON_NEUTRAL_ARM_LOWER_MAX_DEGREES);
    app->motion_config.neutral_elbow_add_degrees =
        SDL_clamp(elbow_add_degrees, EIDOLON_NEUTRAL_ELBOW_ADD_MIN_DEGREES,
                  EIDOLON_NEUTRAL_ELBOW_ADD_MAX_DEGREES);
    app->motion_config_dirty = true;
    clear_semantic_pose(app);
    apply_motion_config(app);
}

void eidolon_app_select_semantic_pose(EidolonApp *app, int pose_index) {
    if (pose_index < 0) {
        clear_semantic_pose(app);
        apply_motion_config(app);
        return;
    }
    const EidolonSemanticPose *pose = eidolon_semantic_pose((size_t)pose_index);
    if (pose == NULL) {
        return;
    }
    app->semantic_pose = *pose;
    app->semantic_pose_index = pose_index;
    app->semantic_pose_dirty = false;
    eidolon_model_set_neutral_pose(app->model, 0.0F, 0.0F);
    eidolon_model_set_semantic_pose(app->model, &app->semantic_pose);
    eidolon_log_write(
        "motion", "semantic pose selected name=%s hand=%+.2f,%+.2f,%+.2f pole=%+.2f,%+.2f,%+.2f",
        pose->name, pose->arms[0].hand[0], pose->arms[0].hand[1], pose->arms[0].hand[2],
        pose->arms[0].elbow_pole[0], pose->arms[0].elbow_pole[1], pose->arms[0].elbow_pole[2]);
}

void eidolon_app_set_semantic_arm_component(EidolonApp *app, bool pole, size_t component,
                                            float value) {
    if (app->semantic_pose_index < 0 || component >= 3U) {
        return;
    }
    const float clamped = SDL_clamp(value, EIDOLON_POSE_TARGET_MIN, EIDOLON_POSE_TARGET_MAX);
    for (size_t side = 0; side < EIDOLON_POSE_ARM_COUNT; ++side) {
        float *target =
            pole ? app->semantic_pose.arms[side].elbow_pole : app->semantic_pose.arms[side].hand;
        target[component] = clamped;
        if (app->semantic_pose.arms[side].weight <= 0.0F) {
            app->semantic_pose.arms[side].weight = 1.0F;
        }
    }
    app->semantic_pose_dirty = true;
    eidolon_model_set_semantic_pose(app->model, &app->semantic_pose);
}

bool eidolon_app_copy_semantic_pose(EidolonApp *app) {
    if (app->semantic_pose_index < 0) {
        return false;
    }
    const EidolonSemanticPose *pose = &app->semantic_pose;
    char initializer[1024];
    SDL_snprintf(initializer, sizeof(initializer),
                 "{\n"
                 "    .name = \"%s\",\n"
                 "    .intent = \"%s\",\n"
                 "    .arms = {{{%+.3fF, %+.3fF, %+.3fF}, {%+.3fF, %+.3fF, %+.3fF}, %.3fF},\n"
                 "             {{%+.3fF, %+.3fF, %+.3fF}, {%+.3fF, %+.3fF, %+.3fF}, %.3fF}},\n"
                 "    .soften_ratio = %.3fF,\n"
                 "},",
                 pose->name, pose->intent, pose->arms[0].hand[0], pose->arms[0].hand[1],
                 pose->arms[0].hand[2], pose->arms[0].elbow_pole[0], pose->arms[0].elbow_pole[1],
                 pose->arms[0].elbow_pole[2], pose->arms[0].weight, pose->arms[1].hand[0],
                 pose->arms[1].hand[1], pose->arms[1].hand[2], pose->arms[1].elbow_pole[0],
                 pose->arms[1].elbow_pole[1], pose->arms[1].elbow_pole[2], pose->arms[1].weight,
                 pose->soften_ratio);
    if (!SDL_SetClipboardText(initializer)) {
        eidolon_log_write("motion", "could not copy semantic pose: %s", SDL_GetError());
        return false;
    }
    app->semantic_pose_copied_until_ms = SDL_GetTicks() + 1500U;
    eidolon_log_write("motion", "semantic pose initializer copied name=%s", pose->name);
    return true;
}

static SDL_FRect portrait_character_rect(const EidolonApp *app) {
    const float width = eidolon_portrait_display_width(app->portrait) * app->model_scale;
    const float height = eidolon_portrait_display_height(app->portrait) * app->model_scale;
    const size_t visible = eidolon_session_registry_visible_count(&app->session_registry);
    return eidolon_bubble_layout_character(app->window_width, app->window_height, width, height,
                                           visible);
}

static void toggle_portrait_framing(EidolonApp *app) {
    if (!eidolon_portrait_ready(app->portrait)) {
        return;
    }
    int old_window_x = 0;
    int old_window_y = 0;
    SDL_GetWindowPosition(app->window, &old_window_x, &old_window_y);
    const SDL_FRect old_character = portrait_character_rect(app);
    const float old_center_x = (float)old_window_x + (old_character.x + old_character.w * 0.5F) *
                                                         app->window_coordinate_scale;
    const float old_center_y = (float)old_window_y + (old_character.y + old_character.h * 0.5F) *
                                                         app->window_coordinate_scale;

    eidolon_portrait_set_face_mode(app->portrait, !eidolon_portrait_face_mode(app->portrait));
    eidolon_app_set_model_scale(app, app->model_scale);

    const SDL_FRect new_character = portrait_character_rect(app);
    const int new_window_x = (int)SDL_roundf(
        old_center_x - (new_character.x + new_character.w * 0.5F) * app->window_coordinate_scale);
    const int new_window_y = (int)SDL_roundf(
        old_center_y - (new_character.y + new_character.h * 0.5F) * app->window_coordinate_scale);
    if (!SDL_SetWindowPosition(app->window, new_window_x, new_window_y)) {
        eidolon_log_write("portrait", "could not preserve character center while reframing: %s",
                          SDL_GetError());
    }
    app->hit_test_initialized = false;
}

static void handle_key(EidolonApp *app, SDL_Keycode key) {
    switch (key) {
    case SDLK_ESCAPE:
        if (app->debug_visible) {
            app->debug_visible = false;
            app->debug_pose_dropdown_open = false;
            app->debug_resolution_dropdown_open = false;
            app->debug_portrait_dropdown_open = false;
            app->debug_drag_control = EIDOLON_DEBUG_CONTROL_NONE;
            app->hit_test_initialized = false;
        } else {
            app->running = false;
        }
        break;
    case SDLK_F1:
        app->debug_visible = !app->debug_visible;
        app->debug_pose_dropdown_open = false;
        app->debug_resolution_dropdown_open = false;
        app->debug_portrait_dropdown_open = false;
        app->debug_drag_control = EIDOLON_DEBUG_CONTROL_NONE;
        app->hit_test_initialized = false;
        break;
    case SDLK_F5:
        eidolon_motion_config_watch_force_reload(&app->motion_config_watch);
        eidolon_portrait_force_reload(app->portrait);
        break;
    case SDLK_P:
        toggle_portrait_framing(app);
        break;
    case SDLK_T:
        app->dialogue_theme = (EidolonDialogueTheme)(
            ((int)app->dialogue_theme + 1) % (int)EIDOLON_DIALOGUE_THEME_COUNT);
        app->hit_test_initialized = false;
        break;
    case SDLK_C:
        if (app->debug_visible) {
            (void)eidolon_app_copy_semantic_pose(app);
        }
        break;
    case SDLK_1:
        eidolon_app_set_state(app, EIDOLON_STATE_IDLE);
        break;
    case SDLK_2:
        eidolon_app_set_state(app, EIDOLON_STATE_RUNNING);
        break;
    case SDLK_3:
        eidolon_app_set_state(app, EIDOLON_STATE_WAITING);
        break;
    case SDLK_4:
        eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
        break;
    case SDLK_5:
        eidolon_app_set_state(app, EIDOLON_STATE_FAILED);
        break;
    case SDLK_SPACE:
        eidolon_app_set_state(app, (EidolonState)((app->state + 1) % EIDOLON_STATE_COUNT));
        break;
    default:
        break;
    }
}

static void begin_drag(EidolonApp *app, const SDL_MouseButtonEvent *button) {
    app->dragging = true;
    app->drag_moved = false;
    const size_t visible = eidolon_session_registry_visible_count(&app->session_registry);
    app->drag_session_slot = -1;
    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        if (eidolon_session_registry_at_slot(&app->session_registry, slot) == NULL) {
            continue;
        }
        const SDL_FRect bubble =
            eidolon_bubble_layout_rect(app->window_width, app->window_height, slot, visible);
        if (button->x >= bubble.x && button->x < bubble.x + bubble.w && button->y >= bubble.y &&
            button->y < bubble.y + bubble.h) {
            app->drag_session_slot = slot;
            break;
        }
    }
    SDL_GetGlobalMouseState(&app->drag_global_x, &app->drag_global_y);
    SDL_GetWindowPosition(app->window, &app->drag_window_x, &app->drag_window_y);
}

static void update_drag(EidolonApp *app) {
    float global_x = 0.0F;
    float global_y = 0.0F;
    SDL_GetGlobalMouseState(&global_x, &global_y);
    const int x = app->drag_window_x + (int)(global_x - app->drag_global_x);
    const int y = app->drag_window_y + (int)(global_y - app->drag_global_y);
    if (SDL_abs((int)(global_x - app->drag_global_x)) > 3 ||
        SDL_abs((int)(global_y - app->drag_global_y)) > 3) {
        app->drag_moved = true;
    }
    SDL_SetWindowPosition(app->window, x, y);
}

static bool point_in_model_rect(const EidolonApp *app, float x, float y) {
    const float size = EIDOLON_MODEL_DISPLAY_SIZE * app->model_scale;
    const float left = (float)app->window_width - size;
    const float top = (float)app->window_height - size;
    return app->model != NULL && x >= left && y >= top && x < left + size && y < top + size;
}

static void end_model_rotation_drag(EidolonApp *app) {
    if (app->model_rotation_dragging) {
        app->model_rotation_dragging = false;
        app->model_rotation_roll_dragging = false;
        app->hit_test_initialized = false;
        SDL_CaptureMouse(false);
    }
}

static void begin_model_rotation_drag(EidolonApp *app, const SDL_MouseButtonEvent *button) {
    if (!point_in_model_rect(app, button->x, button->y)) {
        return;
    }
    if (button->clicks >= 2U) {
        end_model_rotation_drag(app);
        eidolon_app_set_model_rotation(app, 0.0F, 0.0F, 0.0F);
        return;
    }

    app->model_rotation_dragging = true;
    app->model_rotation_roll_dragging = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    eidolon_platform_suspend_hit_test(app->window);
    if (!SDL_CaptureMouse(true)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not capture model rotation drag: %s",
                    SDL_GetError());
    }
}

static void update_model_rotation_drag(EidolonApp *app, const SDL_MouseMotionEvent *motion) {
    if (app->model_rotation_roll_dragging) {
        eidolon_app_set_model_rotation(app, app->model_yaw_degrees, app->model_pitch_degrees,
                                       app->model_roll_degrees +
                                           motion->xrel * MODEL_ROTATION_DEGREES_PER_PIXEL);
    } else {
        eidolon_app_set_model_rotation(
            app, app->model_yaw_degrees + motion->xrel * MODEL_ROTATION_DEGREES_PER_PIXEL,
            app->model_pitch_degrees + motion->yrel * MODEL_ROTATION_DEGREES_PER_PIXEL,
            app->model_roll_degrees);
    }
}

static void handle_event(EidolonApp *app, const SDL_Event *event) {
    SDL_Event render_event = *event;
    if (!SDL_ConvertEventToRenderCoordinates(app->renderer, &render_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not convert input coordinates: %s",
                    SDL_GetError());
    }
    event = &render_event;

    if (eidolon_debug_ui_handle_event(app, event)) {
        return;
    }

    switch (event->type) {
    case SDL_EVENT_QUIT:
        app->running = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (!event->key.repeat) {
            handle_key(app, event->key.key);
        }
        break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        update_display_metrics(app);
        eidolon_app_set_model_scale(app, app->model_scale);
        break;
    case SDL_EVENT_RENDER_TARGETS_RESET:
        app->hit_test_initialized = false;
        eidolon_model_request_redraw(app->model);
        eidolon_log_write("renderer", "render targets reset; model redraw requested");
        break;
    case SDL_EVENT_RENDER_DEVICE_RESET:
    case SDL_EVENT_RENDER_DEVICE_LOST:
        eidolon_log_write("renderer", "D3D11 device reset/lost; restart required");
        app->running = false;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (app->debug_drag_control != EIDOLON_DEBUG_CONTROL_NONE) {
            app->hit_test_initialized = false;
        }
        app->debug_drag_control = EIDOLON_DEBUG_CONTROL_NONE;
        app->dragging = false;
        end_model_rotation_drag(app);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            begin_drag(app, &event->button);
        } else if (event->button.button == SDL_BUTTON_MIDDLE) {
            begin_model_rotation_drag(app, &event->button);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (app->dragging && !app->drag_moved && app->drag_session_slot >= 0) {
                if (eidolon_session_registry_advance(&app->session_registry, app->drag_session_slot,
                                                     SDL_GetTicks())) {
                    EidolonSessionEntry *entry = eidolon_session_registry_at_slot(
                        &app->session_registry, app->drag_session_slot);
                    (void)begin_affect_text(app, entry != NULL ? entry->dialogue.page : "");
                }
            } else if (app->dragging && !app->drag_moved && app->state == EIDOLON_STATE_REVIEW) {
                const size_t previous_cursor = app->dialogue.cursor;
                eidolon_dialogue_advance(&app->dialogue, SDL_GetTicks());
                if (app->dialogue.cursor != previous_cursor) {
                    (void)begin_affect_text(app, app->dialogue.page);
                }
            }
            app->dragging = false;
            app->drag_session_slot = -1;
        } else if (event->button.button == SDL_BUTTON_MIDDLE) {
            end_model_rotation_drag(app);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (app->model_rotation_dragging) {
            update_model_rotation_drag(app, &event->motion);
        } else if (app->dragging) {
            update_drag(app);
        }
        break;
    default:
        break;
    }
}

void eidolon_app_run(EidolonApp *app) {
    Uint64 next_presentation_ns = SDL_GetTicksNS();
    Uint64 previous_presentation_ns = 0U;
    while (app->running) {
        SDL_Event event;
        const Sint32 wait_ms = presentation_wait_ms(SDL_GetTicksNS(), next_presentation_ns);
        if (SDL_WaitEventTimeout(&event, wait_ms)) {
            handle_event(app, &event);
        }
        while (SDL_PollEvent(&event)) {
            handle_event(app, &event);
        }
        if (!app->running) {
            break;
        }

        const Uint64 now_ns = SDL_GetTicksNS();
        if (now_ns < next_presentation_ns) {
            continue;
        }
        if (previous_presentation_ns != 0U &&
            now_ns - previous_presentation_ns > PRESENTATION_INTERVAL_NS * 2U) {
            eidolon_log_write("renderer", "presentation hitch gap_ms=%.2f",
                              (double)(now_ns - previous_presentation_ns) / (double)SDL_NS_PER_MS);
        }
        previous_presentation_ns = now_ns;

        EidolonState received_state;
        char received_text[EIDOLON_IPC_TEXT_CAPACITY + 1];
        while (eidolon_ipc_server_poll(&app->ipc, &received_state, received_text,
                                       sizeof(received_text))) {
            eidolon_log_write("renderer", "ipc receive state=%s bytes=%zu",
                              eidolon_state_name(received_state), strlen(received_text));
            eidolon_app_set_state(app, received_state);
            if (received_text[0] != '\0') {
                eidolon_dialogue_set(&app->dialogue, received_text, SDL_GetTicks());
                eidolon_dialogue_configure(&app->dialogue, app->dialogue_movement,
                                           app->dialogue_hold_ms);
                eidolon_portrait_set_attention(app->portrait, -1.0F);
                (void)begin_affect_text(app, app->dialogue.page);
            }
        }

        const uint64_t now_ms = SDL_GetTicks();
        poll_motion_config(app, now_ms);
        const EidolonSessionPoll sessions =
            eidolon_session_registry_poll(&app->session_registry, now_ms);
        if (sessions.new_message) {
            eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
            eidolon_portrait_set_attention(
                app->portrait, session_attention_direction(sessions.message_session));
            sessions.message_session->affect_sequence =
                begin_affect_text(app, sessions.message_session->dialogue.page);
        }
        if (sessions.page_advanced && sessions.advanced_session != NULL) {
            eidolon_portrait_set_attention(
                app->portrait, session_attention_direction(sessions.advanced_session));
            sessions.advanced_session->affect_sequence =
                begin_affect_text(app, sessions.advanced_session->dialogue.page);
        }
        if (sessions.speech_beat > 0.0F) {
            eidolon_portrait_set_attention(
                app->portrait, session_attention_direction(sessions.speaking_session));
            eidolon_portrait_speak(app->portrait, sessions.speech_beat, now_ms);
        }
        if (sessions.changed) {
            eidolon_app_set_model_scale(app, app->model_scale);
            app->hit_test_initialized = false;
            if (eidolon_session_registry_visible_count(&app->session_registry) == 0U) {
                eidolon_portrait_set_attention(app->portrait, 0.0F);
            }
        }
        eidolon_animation_update(&app->animation, app->state, now_ms);
        if (eidolon_session_registry_visible_count(&app->session_registry) == 0U) {
            const size_t previous_revealed = app->dialogue.revealed;
            eidolon_dialogue_update(&app->dialogue, now_ms);
            const float local_speech_beat =
                eidolon_dialogue_reveal_emphasis(&app->dialogue, previous_revealed);
            if (local_speech_beat > 0.0F) {
                eidolon_portrait_set_attention(app->portrait, -1.0F);
                eidolon_portrait_speak(app->portrait, local_speech_beat, now_ms);
            }
            if (eidolon_dialogue_autoplay(&app->dialogue, now_ms)) {
                (void)begin_affect_text(app, app->dialogue.page);
            }
        }
        uint64_t affect_sequence = 0U;
        float affect_probabilities[EIDOLON_GOEMOTIONS_COUNT];
        while (eidolon_affect_client_poll(app->affect_client, &affect_sequence,
                                          affect_probabilities)) {
            if (eidolon_affect_controller_apply_goemotions(&app->affect, affect_sequence,
                                                           affect_probabilities, now_ms)) {
                eidolon_log_write("affect", "classification applied sequence=%llu evidence=%.3f",
                                  (unsigned long long)affect_sequence, app->affect.evidence);
            }
        }
        eidolon_affect_controller_update(&app->affect, 1.0F / (float)PRESENTATION_RATE_HZ, now_ms);
        eidolon_portrait_set_expression_intent(app->portrait, app->affect.expression_intent,
                                               now_ms);
        const uint64_t portrait_revision = eidolon_portrait_revision(app->portrait);
        eidolon_portrait_update(app->portrait, now_ms);
        if (eidolon_portrait_revision(app->portrait) != portrait_revision) {
            app->dialogue_theme = eidolon_portrait_dialogue_theme(app->portrait);
            app->dialogue_movement = eidolon_portrait_dialogue_movement(app->portrait);
            app->dialogue_hold_ms = eidolon_portrait_dialogue_hold_ms(app->portrait);
            eidolon_dialogue_configure(&app->dialogue, app->dialogue_movement,
                                       app->dialogue_hold_ms);
            eidolon_session_registry_configure_dialogue(
                &app->session_registry, app->dialogue_movement, app->dialogue_hold_ms);
            eidolon_app_set_model_scale(app, app->model_scale);
        }
        eidolon_model_update(app->model, now_ms);
        eidolon_draw_frame(app);
        next_presentation_ns =
            advance_presentation_deadline(next_presentation_ns, SDL_GetTicksNS());
    }
}

void eidolon_app_destroy(EidolonApp *app) {
    end_model_rotation_drag(app);
    eidolon_affect_client_destroy(app->affect_client);
    eidolon_session_registry_destroy(&app->session_registry);
    if (!app->snapshot_mode) {
        eidolon_ipc_server_destroy(&app->ipc);
    }
    eidolon_platform_destroy_overlay(app->window);
    eidolon_model_destroy(app->model);
    eidolon_portrait_destroy(app->portrait);
    eidolon_text_renderer_destroy(app->text_renderer);
    SDL_DestroyTexture(app->atlas);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
    SDL_zero(*app);
}
