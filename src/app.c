#include "app.h"

#include "animation.h"
#include "bubble_layout.h"
#include "draw.h"
#include "log.h"
#include "platform/overlay.h"
#include "pose.h"
#include "settings_ui.h"

#define MODEL_ROTATION_DEGREES_PER_PIXEL 0.35F
#define PRESENTATION_RATE_HZ 30U
#define PRESENTATION_INTERVAL_NS ((Uint64)SDL_NS_PER_SECOND / PRESENTATION_RATE_HZ)
#define USER_SETTINGS_SAVE_DELAY_MS 500U
#define EXPRESSION_TRACK_TIMEOUT_MS 3000U

static void capture_runtime_settings(const EidolonApp *app, EidolonUserSettings *settings) {
    settings->overrides = 0U;
    settings->render_mode = (int)app->render_mode;
    settings->display_scale = app->model_scale;
    settings->portrait_face_mode = eidolon_portrait_face_mode(app->portrait);
    settings->model_render_resolution = app->model_render_resolution;
    settings->model_yaw_degrees = app->model_yaw_degrees;
    settings->model_pitch_degrees = app->model_pitch_degrees;
    settings->model_roll_degrees = app->model_roll_degrees;
    settings->dialogue_theme = (int)app->dialogue_theme;
    settings->dialogue_movement = (int)app->dialogue_movement;
    settings->dialogue_hold_ms = app->dialogue_hold_ms;
}

static void schedule_user_settings_save(EidolonApp *app) {
    if (!app->user_settings_ready || app->user_settings_applying) {
        return;
    }
    app->user_settings_dirty = true;
    app->user_settings_save_at_ms = SDL_GetTicks() + USER_SETTINGS_SAVE_DELAY_MS;
}

static void mark_user_settings_dirty(EidolonApp *app, EidolonUserSettingField field) {
    if (!app->user_settings_ready || app->user_settings_applying) {
        return;
    }
    switch (field) {
    case EIDOLON_USER_SETTING_RENDER_MODE:
        app->user_settings.render_mode = (int)app->render_mode;
        break;
    case EIDOLON_USER_SETTING_DISPLAY_SCALE:
        app->user_settings.display_scale = app->model_scale;
        break;
    case EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE:
        app->user_settings.portrait_face_mode = eidolon_portrait_face_mode(app->portrait);
        break;
    case EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION:
        app->user_settings.model_render_resolution = app->model_render_resolution;
        break;
    case EIDOLON_USER_SETTING_MODEL_YAW:
        app->user_settings.model_yaw_degrees = app->model_yaw_degrees;
        break;
    case EIDOLON_USER_SETTING_MODEL_PITCH:
        app->user_settings.model_pitch_degrees = app->model_pitch_degrees;
        break;
    case EIDOLON_USER_SETTING_MODEL_ROLL:
        app->user_settings.model_roll_degrees = app->model_roll_degrees;
        break;
    case EIDOLON_USER_SETTING_DIALOGUE_THEME:
        app->user_settings.dialogue_theme = (int)app->dialogue_theme;
        break;
    case EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT:
        app->user_settings.dialogue_movement = (int)app->dialogue_movement;
        break;
    case EIDOLON_USER_SETTING_DIALOGUE_HOLD:
        app->user_settings.dialogue_hold_ms = app->dialogue_hold_ms;
        break;
    }
    app->user_settings.overrides |= (uint32_t)field;
    schedule_user_settings_save(app);
}

static void flush_user_settings(EidolonApp *app, bool force) {
    if (!app->user_settings_ready || !app->user_settings_dirty ||
        (!force && SDL_GetTicks() < app->user_settings_save_at_ms)) {
        return;
    }
    char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    if (!eidolon_user_settings_save(app->user_settings_path, &app->user_settings, error,
                                    sizeof(error))) {
        eidolon_log_write("settings", "could not save preferences: %s", error);
        app->user_settings_save_at_ms = SDL_GetTicks() + 5000U;
        return;
    }
    app->user_settings_dirty = false;
    eidolon_log_write("settings", "user overrides saved path=%s fields=0x%x",
                      app->user_settings_path, app->user_settings.overrides);
}

static float session_attention_direction(const EidolonSessionEntry *entry) {
    return entry != NULL && (entry->layout_slot & 1) != 0 ? 1.0F : -1.0F;
}

#ifndef NDEBUG
static void performance_preview(const EidolonDialogue *dialogue,
                                const EidolonExpressionBeat *beat, char *output,
                                size_t capacity) {
    if (capacity == 0U) {
        return;
    }
    size_t written = 0U;
    size_t cursor = beat->text_start;
    const size_t limit = SDL_min(capacity - 1U, 96U);
    while (cursor < beat->text_end && written < limit) {
        const unsigned char byte = (unsigned char)dialogue->text[cursor];
        size_t character_bytes = 1U;
        if ((byte & 0xE0U) == 0xC0U) {
            character_bytes = 2U;
        } else if ((byte & 0xF0U) == 0xE0U) {
            character_bytes = 3U;
        } else if ((byte & 0xF8U) == 0xF0U) {
            character_bytes = 4U;
        }
        if (cursor + character_bytes > beat->text_end || written + character_bytes > limit) {
            break;
        }
        if (character_bytes == 1U) {
            output[written++] = byte < 32U || byte == '"' ? ' ' : (char)byte;
        } else {
            memcpy(output + written, dialogue->text + cursor, character_bytes);
            written += character_bytes;
        }
        cursor += character_bytes;
    }
    output[written] = '\0';
}
#endif

static void log_performance_plan(const EidolonDialogue *dialogue) {
    const EidolonExpressionTrack *track = &dialogue->expression_track;
    eidolon_log_write("latency",
                      "session=%s phase=affect-ready detected_to_ready_ms=%llu beats=%zu",
                      track->owner,
                      (unsigned long long)(track->ready_ms - track->prepared_ms), track->count);
    eidolon_log_write("performance",
                      "track=%llu owner=%s ready beats=%zu latency_ms=%llu",
                      (unsigned long long)track->track_id, track->owner, track->count,
                      (unsigned long long)(track->ready_ms - track->prepared_ms));
    for (size_t index = 0U; index < track->count; ++index) {
        const EidolonExpressionBeat *beat = &track->beats[index];
#ifndef NDEBUG
        char preview[97];
        performance_preview(dialogue, beat, preview, sizeof(preview));
        eidolon_log_write(
            "performance",
            "track=%llu plan beat=%zu span=%zu..%zu boundary=%s infer_ms=%llu "
            "emotion=%s:%.2f,%s:%.2f,%s:%.2f affect=v%.2f/a%.2f/d%.2f/c%.2f/w%.2f/s%.2f "
            "raw_face=%s face=%s held=%s prev_advantage=%.3f runner=%s margin=%.3f "
            "cue=%s reason=%s intensity=%.2f text=\"%s\"",
            (unsigned long long)track->track_id, index, beat->text_start, beat->text_end,
            eidolon_beat_boundary_reason_name(beat->boundary_reason),
            (unsigned long long)(beat->classified_ms >= beat->submitted_ms
                                     ? beat->classified_ms - beat->submitted_ms
                                     : 0U),
            eidolon_goemotion_name(beat->top_emotions[0]), beat->top_probabilities[0],
            eidolon_goemotion_name(beat->top_emotions[1]), beat->top_probabilities[1],
            eidolon_goemotion_name(beat->top_emotions[2]), beat->top_probabilities[2],
            beat->affect.valence, beat->affect.arousal, beat->affect.dominance,
            beat->affect.certainty, beat->affect.warmth, beat->affect.surprise,
            eidolon_expression_intent_name(beat->raw_expression),
            eidolon_expression_intent_name(beat->expression),
            beat->expression_held ? "yes" : "no", beat->previous_expression_advantage,
            eidolon_expression_intent_name(beat->runner_up_expression),
            beat->expression_margin, eidolon_performance_cue_name(beat->cue),
            eidolon_cue_reason_name(beat->cue_reason), beat->intensity, preview);
#else
        eidolon_log_write(
            "performance",
            "track=%llu plan beat=%zu span=%zu..%zu boundary=%s infer_ms=%llu "
            "emotion=%s:%.2f,%s:%.2f,%s:%.2f affect=v%.2f/a%.2f/d%.2f/c%.2f/w%.2f/s%.2f "
            "raw_face=%s face=%s held=%s prev_advantage=%.3f runner=%s margin=%.3f "
            "cue=%s reason=%s intensity=%.2f",
            (unsigned long long)track->track_id, index, beat->text_start, beat->text_end,
            eidolon_beat_boundary_reason_name(beat->boundary_reason),
            (unsigned long long)(beat->classified_ms >= beat->submitted_ms
                                     ? beat->classified_ms - beat->submitted_ms
                                     : 0U),
            eidolon_goemotion_name(beat->top_emotions[0]), beat->top_probabilities[0],
            eidolon_goemotion_name(beat->top_emotions[1]), beat->top_probabilities[1],
            eidolon_goemotion_name(beat->top_emotions[2]), beat->top_probabilities[2],
            beat->affect.valence, beat->affect.arousal, beat->affect.dominance,
            beat->affect.certainty, beat->affect.warmth, beat->affect.surprise,
            eidolon_expression_intent_name(beat->raw_expression),
            eidolon_expression_intent_name(beat->expression),
            beat->expression_held ? "yes" : "no", beat->previous_expression_advantage,
            eidolon_expression_intent_name(beat->runner_up_expression),
            beat->expression_margin, eidolon_performance_cue_name(beat->cue),
            eidolon_cue_reason_name(beat->cue_reason), beat->intensity);
#endif
    }
}

static void apply_performance_event(EidolonApp *app, EidolonDialogue *dialogue,
                                    const EidolonPerformanceEvent *event, float attention,
                                    size_t text_offset, uint64_t now_ms) {
    EidolonExpressionTrack *track = &dialogue->expression_track;
    const EidolonExpressionBeat *beat = &track->beats[event->beat_index];
    const int previous_expression = eidolon_portrait_current_expression(app->portrait);
    const char *previous_label =
        previous_expression >= 0
            ? eidolon_portrait_expression_label(app->portrait, (size_t)previous_expression)
            : "none";
    eidolon_portrait_set_attention(app->portrait, attention);
    eidolon_affect_controller_perform(&app->affect, &event->affect, event->expression,
                                      event->evidence, now_ms);
    eidolon_portrait_set_expression_intent(app->portrait, event->expression, now_ms);
    eidolon_portrait_perform(app->portrait, event->cue, event->intensity, now_ms);
    const int current_expression = eidolon_portrait_current_expression(app->portrait);
    const char *current_label =
        current_expression >= 0
            ? eidolon_portrait_expression_label(app->portrait, (size_t)current_expression)
            : "none";
    const uint64_t since_previous =
        track->last_activation_ms == 0U ? 0U : now_ms - track->last_activation_ms;
    eidolon_log_write(
        "performance",
        "track=%llu owner=%s activate beat=%zu offset=%zu span=%zu..%zu since_ms=%llu "
        "portrait=%s->%s raw_face=%s face=%s held=%s prev_advantage=%.3f margin=%.3f "
        "cue=%s reason=%s intensity=%.2f",
        (unsigned long long)track->track_id, track->owner, event->beat_index, text_offset,
        beat->text_start, beat->text_end, (unsigned long long)since_previous, previous_label,
        current_label, eidolon_expression_intent_name(beat->raw_expression),
        eidolon_expression_intent_name(event->expression), beat->expression_held ? "yes" : "no",
        beat->previous_expression_advantage, beat->expression_margin,
        eidolon_performance_cue_name(event->cue), eidolon_cue_reason_name(beat->cue_reason),
        event->intensity);
    track->last_activation_ms = now_ms;
}

static void activate_dialogue_performance(EidolonApp *app, EidolonDialogue *dialogue,
                                          float attention, uint64_t now_ms) {
    EidolonPerformanceEvent event;
    const size_t offset = eidolon_dialogue_revealed_text_offset(dialogue);
    if (eidolon_expression_track_event(&dialogue->expression_track, offset, &event)) {
        apply_performance_event(app, dialogue, &event, attention, offset, now_ms);
    }
}

static void fallback_dialogue_performance(EidolonApp *app, EidolonDialogue *dialogue,
                                          float attention, uint64_t now_ms, const char *reason) {
    eidolon_expression_track_fallback(&dialogue->expression_track, dialogue->text);
    dialogue->expression_track.ready_ms = now_ms;
    log_performance_plan(dialogue);
    eidolon_dialogue_resume(dialogue, now_ms);
    activate_dialogue_performance(app, dialogue, attention, now_ms);
    eidolon_log_write("performance", "track=%llu owner=%s fallback reason=%s",
                      (unsigned long long)dialogue->expression_track.track_id,
                      dialogue->expression_track.owner, reason);
}

static void prepare_dialogue_performance(EidolonApp *app, EidolonDialogue *dialogue,
                                         EidolonState state, float attention, const char *owner,
                                         uint64_t now_ms) {
    eidolon_expression_track_compile(&dialogue->expression_track, dialogue->text, state);
    app->next_performance_track_id += 1U;
    dialogue->expression_track.track_id = app->next_performance_track_id;
    dialogue->expression_track.prepared_ms = now_ms;
    SDL_strlcpy(dialogue->expression_track.owner, owner != NULL ? owner : "unknown",
                sizeof(dialogue->expression_track.owner));
    if (dialogue->expression_track.count == 0U) {
        eidolon_dialogue_resume(dialogue, now_ms);
        return;
    }
    dialogue->expression_track.deadline_ms = now_ms + EXPRESSION_TRACK_TIMEOUT_MS;
    if (app->affect_client == NULL) {
        fallback_dialogue_performance(app, dialogue, attention, now_ms, "worker unavailable");
        return;
    }
    char text[EIDOLON_EXPRESSION_BEAT_TEXT_CAPACITY];
    for (size_t index = 0U; index < dialogue->expression_track.count; ++index) {
        app->next_affect_sequence += 1U;
        if (app->next_affect_sequence == 0U) {
            app->next_affect_sequence += 1U;
        }
        if (!eidolon_expression_track_set_sequence(&dialogue->expression_track, index,
                                                   app->next_affect_sequence) ||
            !eidolon_expression_track_copy_text(&dialogue->expression_track, index,
                                                dialogue->text, text, sizeof(text)) ||
            !eidolon_affect_client_submit(app->affect_client, app->next_affect_sequence, text)) {
            fallback_dialogue_performance(app, dialogue, attention, now_ms,
                                          "classification queue full");
            return;
        }
        dialogue->expression_track.beats[index].submitted_ms = now_ms;
    }
    eidolon_log_write("performance", "track=%llu owner=%s compiled beats=%zu bytes=%zu",
                      (unsigned long long)dialogue->expression_track.track_id,
                      dialogue->expression_track.owner, dialogue->expression_track.count,
                      strlen(dialogue->text));
}

static bool apply_classification(EidolonApp *app, EidolonDialogue *dialogue, uint64_t sequence,
                                 const float probabilities[EIDOLON_GOEMOTIONS_COUNT],
                                 float attention, uint64_t now_ms) {
    const bool was_ready = eidolon_expression_track_ready(&dialogue->expression_track);
    if (!eidolon_expression_track_apply(&dialogue->expression_track, sequence, probabilities,
                                        now_ms)) {
        return false;
    }
    if (!was_ready && eidolon_expression_track_ready(&dialogue->expression_track)) {
        dialogue->expression_track.ready_ms = now_ms;
        log_performance_plan(dialogue);
        eidolon_dialogue_resume(dialogue, now_ms);
        activate_dialogue_performance(app, dialogue, attention, now_ms);
    }
    return true;
}

static void service_expression_director(EidolonApp *app, uint64_t now_ms) {
    uint64_t sequence = 0U;
    float probabilities[EIDOLON_GOEMOTIONS_COUNT];
    while (eidolon_affect_client_poll(app->affect_client, &sequence, probabilities)) {
        bool applied = apply_classification(app, &app->dialogue, sequence, probabilities, -1.0F,
                                            now_ms);
        for (size_t index = 0U; !applied && index < EIDOLON_SESSION_CAPACITY; ++index) {
            EidolonSessionEntry *entry = &app->session_registry.entries[index];
            if (entry->occupied) {
                applied = apply_classification(app, &entry->dialogue, sequence, probabilities,
                                               session_attention_direction(entry), now_ms);
            }
        }
    }

    if (app->dialogue.expression_track.waiting &&
        now_ms >= app->dialogue.expression_track.deadline_ms) {
        fallback_dialogue_performance(app, &app->dialogue, -1.0F, now_ms,
                                      "classification timeout");
    }
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        EidolonSessionEntry *entry = &app->session_registry.entries[index];
        if (entry->occupied && entry->dialogue.expression_track.waiting &&
            now_ms >= entry->dialogue.expression_track.deadline_ms) {
            fallback_dialogue_performance(app, &entry->dialogue,
                                          session_attention_direction(entry), now_ms,
                                          "classification timeout");
        }
    }
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
    app->primary_session_slot = -1;
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
    if (app->atlas != NULL) {
        return true;
    }
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

static bool ensure_portrait(EidolonApp *app) {
    if (eidolon_portrait_ready(app->portrait)) {
        return true;
    }
    app->portrait =
        eidolon_portrait_create(app->renderer, EIDOLON_CHARACTER_CONFIG_PATH, EIDOLON_ASSET_DIR);
    if (app->portrait == NULL) {
        eidolon_log_write("portrait", "could not activate portrait renderer: %s", SDL_GetError());
        SDL_ClearError();
        return false;
    }
    return true;
}

static bool ensure_model(EidolonApp *app) {
    if (app->model != NULL) {
        return true;
    }
    app->model = eidolon_model_create(app->renderer, EIDOLON_MODEL_PATH, EIDOLON_SHADER_DIR,
                                      neutral_pose_from_config(&app->motion_config),
                                      idle_tuning_from_config(&app->motion_config));
    if (app->model == NULL) {
        eidolon_log_write("model", "could not activate 3D renderer: %s", SDL_GetError());
        SDL_ClearError();
        return false;
    }
    eidolon_app_set_model_rotation(app, app->model_yaw_degrees, app->model_pitch_degrees,
                                   app->model_roll_degrees);
    if (!eidolon_model_set_render_resolution(app->model, app->model_render_resolution)) {
        eidolon_log_write("renderer", "could not restore model resolution %d: %s",
                          app->model_render_resolution, SDL_GetError());
        SDL_ClearError();
        app->model_render_resolution = eidolon_model_render_resolution(app->model);
    }
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

static void apply_settings_layer(EidolonApp *app, const EidolonUserSettings *settings) {
    const bool was_applying = app->user_settings_applying;
    app->user_settings_applying = true;

    const float yaw = eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_MODEL_YAW)
                          ? settings->model_yaw_degrees
                          : app->model_yaw_degrees;
    const float pitch =
        eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_MODEL_PITCH)
            ? settings->model_pitch_degrees
            : app->model_pitch_degrees;
    const float roll =
        eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_MODEL_ROLL)
            ? settings->model_roll_degrees
            : app->model_roll_degrees;
    eidolon_app_set_model_rotation(app, yaw, pitch, roll);

    if (eidolon_user_settings_is_overridden(settings,
                                            EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION)) {
        const int resolution =
            SDL_clamp(settings->model_render_resolution, EIDOLON_MODEL_RENDER_RESOLUTION_MIN,
                      EIDOLON_MODEL_RENDER_RESOLUTION_MAX);
        if (app->model != NULL) {
            (void)eidolon_app_set_model_render_resolution(app, resolution);
        } else {
            app->model_render_resolution = resolution;
        }
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_RENDER_MODE) &&
        settings->render_mode >= 0 && settings->render_mode < (int)EIDOLON_RENDER_MODE_COUNT) {
        (void)eidolon_app_set_render_mode(app, (EidolonRenderMode)settings->render_mode);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE)) {
        eidolon_app_set_portrait_framing(app, settings->portrait_face_mode);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DISPLAY_SCALE)) {
        eidolon_app_set_model_scale(app, settings->display_scale);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DIALOGUE_THEME) &&
        settings->dialogue_theme >= 0 &&
        settings->dialogue_theme < (int)EIDOLON_DIALOGUE_THEME_COUNT) {
        eidolon_app_set_dialogue_theme(app, (EidolonDialogueTheme)settings->dialogue_theme);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DIALOGUE_HOLD)) {
        eidolon_app_set_dialogue_hold_ms(app, settings->dialogue_hold_ms);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT) &&
        settings->dialogue_movement >= 0 &&
        settings->dialogue_movement < (int)EIDOLON_DIALOGUE_MOVEMENT_COUNT) {
        eidolon_app_set_dialogue_movement(app,
                                          (EidolonDialogueMovement)settings->dialogue_movement);
    }
    app->user_settings_applying = was_applying;
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
    app->primary_session_slot = -1;
    app->render_mode = EIDOLON_RENDER_MODE_PORTRAIT;
    app->model_render_resolution = EIDOLON_MODEL_RENDER_RESOLUTION_DEFAULT;
    eidolon_user_settings_defaults(&app->system_settings);
    eidolon_user_settings_defaults(&app->user_settings);
    eidolon_motion_config_defaults(&app->motion_config);
    eidolon_motion_config_watch_init(&app->motion_config_watch);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    poll_motion_config(app, SDL_GetTicks());

    EidolonUserSettings configured_defaults;
    eidolon_user_settings_defaults(&configured_defaults);
    char system_settings_error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
    const bool configured_defaults_loaded =
        eidolon_user_settings_load(EIDOLON_SYSTEM_SETTINGS_PATH, &configured_defaults,
                                   system_settings_error, sizeof(system_settings_error));
    if (!configured_defaults_loaded) {
        eidolon_log_write("settings", "system defaults rejected; using built-ins: %s",
                          system_settings_error);
    }

    bool stored_settings_loaded = false;
    if (!app->snapshot_mode && eidolon_user_settings_resolve_path(
                                   app->user_settings_path, sizeof(app->user_settings_path))) {
        SDL_PathInfo path_info;
        if (SDL_GetPathInfo(app->user_settings_path, &path_info)) {
            char error[EIDOLON_USER_SETTINGS_ERROR_CAPACITY];
            if (eidolon_user_settings_load(app->user_settings_path, &app->user_settings, error,
                                           sizeof(error))) {
                stored_settings_loaded = true;
            } else {
                eidolon_log_write("settings", "preferences rejected: %s", error);
            }
        } else {
            SDL_ClearError();
        }
    }

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

    if (!ensure_portrait(app)) {
        eidolon_log_write("portrait", "initialization failed; trying legacy renderers: %s",
                          SDL_GetError());
        SDL_ClearError();
        if (!load_atlas(app)) {
            return false;
        }
        if (!ensure_model(app)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Rio 3D renderer unavailable; using sprite fallback: %s", SDL_GetError());
            eidolon_log_write("model", "initialization failed; sprite fallback active: %s",
                              SDL_GetError());
            app->render_mode = EIDOLON_RENDER_MODE_SPRITE;
        } else {
            app->render_mode = EIDOLON_RENDER_MODE_MODEL_3D;
        }
    } else {
        eidolon_log_write("renderer", "portrait active; 3D initialization skipped");
        app->render_mode = EIDOLON_RENDER_MODE_PORTRAIT;
        app->dialogue_theme = eidolon_portrait_dialogue_theme(app->portrait);
        app->dialogue_movement = eidolon_portrait_dialogue_movement(app->portrait);
        app->dialogue_hold_ms = eidolon_portrait_dialogue_hold_ms(app->portrait);
    }

    if (configured_defaults_loaded) {
        apply_settings_layer(app, &configured_defaults);
    }
    capture_runtime_settings(app, &app->system_settings);

    if (stored_settings_loaded) {
        apply_settings_layer(app, &app->user_settings);
        eidolon_log_write("settings", "user overrides loaded fields=0x%x",
                          app->user_settings.overrides);
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
        app->conversation_sources =
            eidolon_conversation_sources_create(EIDOLON_PROVIDER_CONFIG_PATH);
        if (app->conversation_sources == NULL) {
            eidolon_log_write("provider", "live provider manager unavailable; legacy inputs remain");
        } else {
            eidolon_session_registry_set_legacy_transcripts(
                &app->session_registry,
                eidolon_conversation_sources_legacy_transcripts(app->conversation_sources));
        }
        eidolon_session_registry_configure_dialogue(&app->session_registry, app->dialogue_movement,
                                                    app->dialogue_hold_ms);
        app->settings_ui = eidolon_settings_ui_create(EIDOLON_FONT_PATH);
        if (app->settings_ui == NULL) {
            eidolon_log_write("settings", "window unavailable: %s", SDL_GetError());
            SDL_ClearError();
        }
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
        app->user_settings_ready = app->user_settings_path[0] != '\0';
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

const char *eidolon_render_mode_name(EidolonRenderMode mode) {
    switch (mode) {
    case EIDOLON_RENDER_MODE_SPRITE:
        return "Sprite";
    case EIDOLON_RENDER_MODE_PORTRAIT:
        return "2D Portrait";
    case EIDOLON_RENDER_MODE_MODEL_3D:
        return "3D Model";
    case EIDOLON_RENDER_MODE_COUNT:
        break;
    }
    return "Unknown";
}

const char *eidolon_app_model_name(const EidolonApp *app) {
    switch (app->render_mode) {
    case EIDOLON_RENDER_MODE_SPRITE:
        return "Mutsuki Dress";
    case EIDOLON_RENDER_MODE_PORTRAIT:
        return "Asuna (Bunny Girl)";
    case EIDOLON_RENDER_MODE_MODEL_3D:
        return "Rio (Battle)";
    case EIDOLON_RENDER_MODE_COUNT:
        break;
    }
    return "Unknown";
}

bool eidolon_app_set_render_mode(EidolonApp *app, EidolonRenderMode mode) {
    if (mode < 0 || mode >= EIDOLON_RENDER_MODE_COUNT) {
        return false;
    }
    bool ready = false;
    switch (mode) {
    case EIDOLON_RENDER_MODE_SPRITE:
        ready = load_atlas(app);
        break;
    case EIDOLON_RENDER_MODE_PORTRAIT:
        ready = ensure_portrait(app);
        break;
    case EIDOLON_RENDER_MODE_MODEL_3D:
        ready = ensure_model(app);
        break;
    case EIDOLON_RENDER_MODE_COUNT:
        break;
    }
    if (!ready) {
        return false;
    }
    const bool changed = app->render_mode != mode;
    app->render_mode = mode;
    eidolon_app_set_model_scale(app, app->model_scale);
    app->hit_test_initialized = false;
    eidolon_log_write("renderer", "active mode=%s model=%s", eidolon_render_mode_name(mode),
                      eidolon_app_model_name(app));
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_RENDER_MODE);
    }
    return true;
}

void eidolon_app_select_portrait(EidolonApp *app, int expression) {
    eidolon_portrait_set_override(app->portrait, expression, SDL_GetTicks());
    app->hit_test_initialized = false;
}

void eidolon_app_set_model_scale(EidolonApp *app, float scale) {
    const float clamped = SDL_clamp(scale, EIDOLON_MODEL_SCALE_MIN, EIDOLON_MODEL_SCALE_MAX);
    const bool changed = app->model_scale != clamped;
    int logical_width = 0;
    int logical_height = 0;
    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait)) {
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
    app->hit_test_initialized = false;
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_DISPLAY_SCALE);
    }
}

bool eidolon_app_set_model_render_resolution(EidolonApp *app, int side) {
    const bool changed = app->model_render_resolution != side;
    if (!eidolon_model_set_render_resolution(app->model, side)) {
        eidolon_log_write("renderer", "could not set model render resolution to %d: %s", side,
                          SDL_GetError());
        SDL_ClearError();
        return false;
    }
    app->hit_test_initialized = false;
    app->model_render_resolution = side;
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION);
    }
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
    const float old_yaw = app->model_yaw_degrees;
    const float old_pitch = app->model_pitch_degrees;
    const float old_roll = app->model_roll_degrees;
    app->model_yaw_degrees = normalize_degrees(yaw_degrees);
    app->model_pitch_degrees = normalize_degrees(pitch_degrees);
    app->model_roll_degrees = normalize_degrees(roll_degrees);
    eidolon_model_set_rotation(app->model, app->model_yaw_degrees * SDL_PI_F / 180.0F,
                               app->model_pitch_degrees * SDL_PI_F / 180.0F,
                               app->model_roll_degrees * SDL_PI_F / 180.0F);
    if (old_yaw != app->model_yaw_degrees) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_MODEL_YAW);
    }
    if (old_pitch != app->model_pitch_degrees) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_MODEL_PITCH);
    }
    if (old_roll != app->model_roll_degrees) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_MODEL_ROLL);
    }
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

void eidolon_app_set_portrait_framing(EidolonApp *app, bool face_mode) {
    if (!eidolon_portrait_ready(app->portrait)) {
        return;
    }
    if (eidolon_portrait_face_mode(app->portrait) == face_mode) {
        return;
    }
    if (app->render_mode != EIDOLON_RENDER_MODE_PORTRAIT) {
        eidolon_portrait_set_face_mode(app->portrait, face_mode);
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE);
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

    eidolon_portrait_set_face_mode(app->portrait, face_mode);
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
    mark_user_settings_dirty(app, EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE);
}

void eidolon_app_reload_configs(EidolonApp *app) {
    eidolon_motion_config_watch_force_reload(&app->motion_config_watch);
    eidolon_portrait_force_reload(app->portrait);
}

void eidolon_app_set_dialogue_theme(EidolonApp *app, EidolonDialogueTheme theme) {
    if (theme < 0 || theme >= EIDOLON_DIALOGUE_THEME_COUNT) {
        return;
    }
    const bool changed = app->dialogue_theme != theme;
    app->dialogue_theme = theme;
    app->hit_test_initialized = false;
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_DIALOGUE_THEME);
    }
}

void eidolon_app_set_dialogue_movement(EidolonApp *app, EidolonDialogueMovement movement) {
    if (movement < 0 || movement >= EIDOLON_DIALOGUE_MOVEMENT_COUNT) {
        return;
    }
    const bool changed = app->dialogue_movement != movement;
    app->dialogue_movement = movement;
    eidolon_dialogue_configure(&app->dialogue, movement, app->dialogue_hold_ms);
    eidolon_session_registry_configure_dialogue(&app->session_registry, movement,
                                                app->dialogue_hold_ms);
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT);
    }
}

void eidolon_app_set_dialogue_hold_ms(EidolonApp *app, unsigned int hold_ms) {
    const unsigned int clamped = SDL_clamp(hold_ms, 250U, 10000U);
    const bool changed = app->dialogue_hold_ms != clamped;
    app->dialogue_hold_ms = clamped;
    eidolon_dialogue_configure(&app->dialogue, app->dialogue_movement, app->dialogue_hold_ms);
    eidolon_session_registry_configure_dialogue(&app->session_registry, app->dialogue_movement,
                                                app->dialogue_hold_ms);
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_DIALOGUE_HOLD);
    }
}

bool eidolon_app_reset_user_setting(EidolonApp *app, EidolonUserSettingField field) {
    if (!eidolon_user_settings_is_overridden(&app->user_settings, field)) {
        return false;
    }
    app->user_settings.overrides &= ~(uint32_t)field;
    app->user_settings_applying = true;
    switch (field) {
    case EIDOLON_USER_SETTING_RENDER_MODE:
        (void)eidolon_app_set_render_mode(app, (EidolonRenderMode)app->system_settings.render_mode);
        break;
    case EIDOLON_USER_SETTING_DISPLAY_SCALE:
        eidolon_app_set_model_scale(app, app->system_settings.display_scale);
        break;
    case EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE:
        eidolon_app_set_portrait_framing(app, app->system_settings.portrait_face_mode);
        break;
    case EIDOLON_USER_SETTING_MODEL_RENDER_RESOLUTION:
        if (app->model != NULL) {
            (void)eidolon_app_set_model_render_resolution(
                app, app->system_settings.model_render_resolution);
        } else {
            app->model_render_resolution = app->system_settings.model_render_resolution;
        }
        break;
    case EIDOLON_USER_SETTING_MODEL_YAW:
        eidolon_app_set_model_rotation(app, app->system_settings.model_yaw_degrees,
                                       app->model_pitch_degrees, app->model_roll_degrees);
        break;
    case EIDOLON_USER_SETTING_MODEL_PITCH:
        eidolon_app_set_model_rotation(app, app->model_yaw_degrees,
                                       app->system_settings.model_pitch_degrees,
                                       app->model_roll_degrees);
        break;
    case EIDOLON_USER_SETTING_MODEL_ROLL:
        eidolon_app_set_model_rotation(app, app->model_yaw_degrees, app->model_pitch_degrees,
                                       app->system_settings.model_roll_degrees);
        break;
    case EIDOLON_USER_SETTING_DIALOGUE_THEME:
        eidolon_app_set_dialogue_theme(app,
                                       (EidolonDialogueTheme)app->system_settings.dialogue_theme);
        break;
    case EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT:
        eidolon_app_set_dialogue_movement(
            app, (EidolonDialogueMovement)app->system_settings.dialogue_movement);
        break;
    case EIDOLON_USER_SETTING_DIALOGUE_HOLD:
        eidolon_app_set_dialogue_hold_ms(app, app->system_settings.dialogue_hold_ms);
        break;
    }
    app->user_settings_applying = false;
    schedule_user_settings_save(app);
    eidolon_log_write("settings", "override reset field=0x%x", (unsigned int)field);
    return true;
}

static void handle_key(EidolonApp *app, SDL_Keycode key) {
    switch (key) {
    case SDLK_ESCAPE:
        app->running = false;
        break;
    case SDLK_F1:
        eidolon_settings_ui_open(app->settings_ui);
        break;
    case SDLK_F5:
        eidolon_app_reload_configs(app);
        break;
    default:
        break;
    }
}

static bool point_in_rect(float x, float y, const SDL_FRect *rect) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static SDL_FRect character_rect(const EidolonApp *app) {
    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT) {
        return portrait_character_rect(app);
    }
    if (app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D) {
        const float size = EIDOLON_MODEL_DISPLAY_SIZE * app->model_scale;
        return (SDL_FRect){(float)app->window_width - size, (float)app->window_height - size, size,
                           size};
    }
    const float width = (float)EIDOLON_CELL_WIDTH * app->model_scale;
    const float height = (float)EIDOLON_CELL_HEIGHT * app->model_scale;
    return (SDL_FRect){(float)app->window_width - 18.0F - width,
                       (float)app->window_height - 24.0F - height, width, height};
}

static void begin_primary_interaction(EidolonApp *app, const SDL_MouseButtonEvent *button) {
    app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_NONE;
    app->primary_moved = false;
    app->primary_local_x = button->x;
    app->primary_local_y = button->y;
    app->primary_session_slot = -1;
    const size_t visible = eidolon_session_registry_visible_count(&app->session_registry);
    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        if (eidolon_session_registry_at_slot(&app->session_registry, slot) == NULL) {
            continue;
        }
        const SDL_FRect bubble =
            eidolon_bubble_layout_rect(app->window_width, app->window_height, slot, visible);
        if (button->x >= bubble.x && button->x < bubble.x + bubble.w && button->y >= bubble.y &&
            button->y < bubble.y + bubble.h) {
            app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_SESSION_BUBBLE;
            app->primary_session_slot = slot;
            return;
        }
    }
    if (visible == 0U && app->state == EIDOLON_STATE_REVIEW &&
        eidolon_dialogue_is_active(&app->dialogue)) {
        const SDL_FRect bubble = {17.0F, 16.0F, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
        if (point_in_rect(button->x, button->y, &bubble)) {
            app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_DIALOGUE_BUBBLE;
            return;
        }
    }
    const SDL_FRect character = character_rect(app);
    if (!point_in_rect(button->x, button->y, &character)) {
        return;
    }
    app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG;
    SDL_GetGlobalMouseState(&app->drag_global_x, &app->drag_global_y);
    SDL_GetWindowPosition(app->window, &app->drag_window_x, &app->drag_window_y);
    if (!SDL_CaptureMouse(true)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not capture character drag: %s",
                    SDL_GetError());
    }
}

static void update_primary_interaction(EidolonApp *app, const SDL_MouseMotionEvent *motion) {
    if (SDL_fabsf(motion->x - app->primary_local_x) > 3.0F ||
        SDL_fabsf(motion->y - app->primary_local_y) > 3.0F) {
        app->primary_moved = true;
    }
    if (app->primary_interaction != EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG) {
        return;
    }
    float global_x = 0.0F;
    float global_y = 0.0F;
    SDL_GetGlobalMouseState(&global_x, &global_y);
    const int x = app->drag_window_x + (int)(global_x - app->drag_global_x);
    const int y = app->drag_window_y + (int)(global_y - app->drag_global_y);
    if (SDL_abs((int)(global_x - app->drag_global_x)) > 3 ||
        SDL_abs((int)(global_y - app->drag_global_y)) > 3) {
        app->primary_moved = true;
    }
    SDL_SetWindowPosition(app->window, x, y);
}

static void end_primary_interaction(EidolonApp *app) {
    if (app->primary_interaction == EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG) {
        SDL_CaptureMouse(false);
    }
    app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_NONE;
    app->primary_session_slot = -1;
}

static bool point_in_model_rect(const EidolonApp *app, float x, float y) {
    const float size = EIDOLON_MODEL_DISPLAY_SIZE * app->model_scale;
    const float left = (float)app->window_width - size;
    const float top = (float)app->window_height - size;
    return app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D && app->model != NULL && x >= left &&
           y >= top && x < left + size && y < top + size;
}

static void end_model_rotation_drag(EidolonApp *app) {
    if (app->model_rotation_dragging) {
        if (app->model_rotation_hit_test_suspended) {
            app->hit_test_initialized = false;
        }
        app->model_rotation_dragging = false;
        app->model_rotation_roll_dragging = false;
        app->model_rotation_hit_test_suspended = false;
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
    app->model_rotation_hit_test_suspended = false;
    if (!SDL_CaptureMouse(true)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not capture model rotation drag: %s",
                    SDL_GetError());
    }
}

static void update_model_rotation_drag(EidolonApp *app, const SDL_MouseMotionEvent *motion) {
    if (motion->xrel == 0.0F && motion->yrel == 0.0F) {
        return;
    }
    if (!app->model_rotation_hit_test_suspended) {
        eidolon_platform_suspend_hit_test(app->window);
        app->model_rotation_hit_test_suspended = true;
    }
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
    if (eidolon_settings_ui_handle_event(app->settings_ui, event)) {
        return;
    }
    SDL_Event render_event = *event;
    if (!SDL_ConvertEventToRenderCoordinates(app->renderer, &render_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not convert input coordinates: %s",
                    SDL_GetError());
    }
    event = &render_event;

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
        end_primary_interaction(app);
        end_model_rotation_drag(app);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            begin_primary_interaction(app, &event->button);
        } else if (event->button.button == SDL_BUTTON_MIDDLE) {
            begin_model_rotation_drag(app, &event->button);
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            const SDL_FRect character = character_rect(app);
            if (point_in_rect(event->button.x, event->button.y, &character)) {
                eidolon_settings_ui_open(app->settings_ui);
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (!app->primary_moved &&
                app->primary_interaction == EIDOLON_PRIMARY_INTERACTION_SESSION_BUBBLE) {
                if (eidolon_session_registry_advance(&app->session_registry,
                                                     app->primary_session_slot, SDL_GetTicks())) {
                    EidolonSessionEntry *entry = eidolon_session_registry_at_slot(
                        &app->session_registry, app->primary_session_slot);
                    if (entry != NULL) {
                        activate_dialogue_performance(app, &entry->dialogue,
                                                      session_attention_direction(entry),
                                                      SDL_GetTicks());
                    }
                }
            } else if (!app->primary_moved &&
                       app->primary_interaction == EIDOLON_PRIMARY_INTERACTION_DIALOGUE_BUBBLE) {
                const size_t previous_cursor = app->dialogue.cursor;
                eidolon_dialogue_advance(&app->dialogue, SDL_GetTicks());
                if (app->dialogue.cursor != previous_cursor) {
                    activate_dialogue_performance(app, &app->dialogue, -1.0F, SDL_GetTicks());
                }
            }
            end_primary_interaction(app);
        } else if (event->button.button == SDL_BUTTON_MIDDLE) {
            end_model_rotation_drag(app);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (app->model_rotation_dragging) {
            update_model_rotation_drag(app, &event->motion);
        } else if (app->primary_interaction != EIDOLON_PRIMARY_INTERACTION_NONE) {
            update_primary_interaction(app, &event->motion);
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

        const uint64_t now_ms = SDL_GetTicks();
        EidolonConversationEvent conversation_event;
        bool conversation_layout_changed = false;
        while (eidolon_conversation_sources_poll(app->conversation_sources,
                                                 &conversation_event)) {
            const EidolonSessionPoll live = eidolon_session_registry_apply_event(
                &app->session_registry, &conversation_event, now_ms);
            conversation_layout_changed = conversation_layout_changed || live.changed;
            if (live.stream_started && live.message_session != NULL) {
                eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
                const float attention = session_attention_direction(live.message_session);
                eidolon_portrait_set_attention(app->portrait, attention);
                const EidolonAffect responding = eidolon_affect_for_state(EIDOLON_STATE_REVIEW);
                eidolon_affect_controller_perform(&app->affect, &responding,
                                                  EIDOLON_EXPRESSION_RESPONDING, 0.65F, now_ms);
            } else if (live.new_message && live.message_session != NULL) {
                eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
                const float attention = session_attention_direction(live.message_session);
                eidolon_portrait_set_attention(app->portrait, attention);
                prepare_dialogue_performance(app, &live.message_session->dialogue, app->state,
                                             attention, live.message_session->id, now_ms);
            }
        }

        EidolonState received_state;
        char received_text[EIDOLON_IPC_TEXT_CAPACITY + 1];
        const bool legacy_hooks = app->conversation_sources == NULL ||
                                  eidolon_conversation_sources_legacy_hooks(
                                      app->conversation_sources);
        while (eidolon_ipc_server_poll(&app->ipc, &received_state, received_text,
                                       sizeof(received_text))) {
            if (!legacy_hooks) {
                continue;
            }
            eidolon_log_write("renderer", "ipc receive state=%s bytes=%zu",
                              eidolon_state_name(received_state), strlen(received_text));
            eidolon_app_set_state(app, received_state);
            if (received_text[0] != '\0') {
                eidolon_dialogue_set(&app->dialogue, received_text, SDL_GetTicks());
                eidolon_dialogue_configure(&app->dialogue, app->dialogue_movement,
                                           app->dialogue_hold_ms);
                eidolon_portrait_set_attention(app->portrait, -1.0F);
                prepare_dialogue_performance(app, &app->dialogue, app->state, -1.0F, "ipc",
                                             SDL_GetTicks());
            }
        }

        poll_motion_config(app, now_ms);
        const EidolonSessionPoll sessions =
            eidolon_session_registry_poll(&app->session_registry, now_ms);
        if (sessions.new_message) {
            eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
            eidolon_portrait_set_attention(app->portrait,
                                           session_attention_direction(sessions.message_session));
            prepare_dialogue_performance(
                app, &sessions.message_session->dialogue, app->state,
                session_attention_direction(sessions.message_session),
                sessions.message_session->id, now_ms);
        }
        if (sessions.page_advanced && sessions.advanced_session != NULL) {
            eidolon_portrait_set_attention(app->portrait,
                                           session_attention_direction(sessions.advanced_session));
            activate_dialogue_performance(
                app, &sessions.advanced_session->dialogue,
                session_attention_direction(sessions.advanced_session), now_ms);
        }
        if (sessions.speech_beat > 0.0F) {
            eidolon_portrait_set_attention(app->portrait,
                                           session_attention_direction(sessions.speaking_session));
            eidolon_portrait_speak(app->portrait, sessions.speech_beat, now_ms);
        }
        for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
            EidolonSessionEntry *entry = &app->session_registry.entries[index];
            if (entry->visible) {
                activate_dialogue_performance(app, &entry->dialogue,
                                              session_attention_direction(entry), now_ms);
            }
        }
        if (sessions.changed || conversation_layout_changed) {
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
                activate_dialogue_performance(app, &app->dialogue, -1.0F, now_ms);
            }
            activate_dialogue_performance(app, &app->dialogue, -1.0F, now_ms);
        }
        service_expression_director(app, now_ms);
        eidolon_affect_controller_update(&app->affect, 1.0F / (float)PRESENTATION_RATE_HZ, now_ms);
        eidolon_portrait_set_expression_intent(app->portrait, app->affect.expression_intent,
                                               now_ms);
        const uint64_t portrait_revision = eidolon_portrait_revision(app->portrait);
        eidolon_portrait_update(app->portrait, now_ms);
        if (eidolon_portrait_revision(app->portrait) != portrait_revision) {
            app->system_settings.portrait_face_mode = eidolon_portrait_face_mode(app->portrait);
            app->system_settings.dialogue_theme =
                (int)eidolon_portrait_dialogue_theme(app->portrait);
            app->system_settings.dialogue_movement =
                (int)eidolon_portrait_dialogue_movement(app->portrait);
            app->system_settings.dialogue_hold_ms =
                eidolon_portrait_dialogue_hold_ms(app->portrait);
            app->user_settings_applying = true;
            if (eidolon_user_settings_is_overridden(&app->user_settings,
                                                    EIDOLON_USER_SETTING_PORTRAIT_FACE_MODE)) {
                eidolon_app_set_portrait_framing(app, app->user_settings.portrait_face_mode);
            }
            eidolon_app_set_dialogue_theme(
                app,
                (EidolonDialogueTheme)(eidolon_user_settings_is_overridden(
                                           &app->user_settings, EIDOLON_USER_SETTING_DIALOGUE_THEME)
                                           ? app->user_settings.dialogue_theme
                                           : app->system_settings.dialogue_theme));
            eidolon_app_set_dialogue_hold_ms(
                app, eidolon_user_settings_is_overridden(&app->user_settings,
                                                         EIDOLON_USER_SETTING_DIALOGUE_HOLD)
                         ? app->user_settings.dialogue_hold_ms
                         : app->system_settings.dialogue_hold_ms);
            eidolon_app_set_dialogue_movement(
                app, (EidolonDialogueMovement)(eidolon_user_settings_is_overridden(
                                                   &app->user_settings,
                                                   EIDOLON_USER_SETTING_DIALOGUE_MOVEMENT)
                                                   ? app->user_settings.dialogue_movement
                                                   : app->system_settings.dialogue_movement));
            app->user_settings_applying = false;
            eidolon_app_set_model_scale(app, app->model_scale);
        }
        eidolon_model_update(app->model, now_ms);
        eidolon_draw_frame(app);
        eidolon_settings_ui_draw(app->settings_ui, app);
        flush_user_settings(app, false);
        next_presentation_ns =
            advance_presentation_deadline(next_presentation_ns, SDL_GetTicksNS());
    }
}

void eidolon_app_destroy(EidolonApp *app) {
    end_model_rotation_drag(app);
    flush_user_settings(app, true);
    eidolon_settings_ui_destroy(app->settings_ui);
    eidolon_affect_client_destroy(app->affect_client);
    eidolon_conversation_sources_destroy(app->conversation_sources);
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
