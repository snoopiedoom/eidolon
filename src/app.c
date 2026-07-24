#include "app.h"

#include "animation.h"
#include "bubble_layout.h"
#include "draw.h"
#include "event_pump.h"
#include "frame_clock.h"
#include "log.h"
#include "pose.h"
#include "presentation_sdl_legacy.h"
#include "settings_ui.h"

#if defined(_WIN32)
#include "platform/windows_dcomp.h"
#endif

#define MODEL_ROTATION_DEGREES_PER_PIXEL 0.35F
#define EVENT_BATCH_LIMIT 128U
#define EVENT_PRESSURE_LOG_INTERVAL_MS 1000U
#define USER_SETTINGS_SAVE_DELAY_MS 500U
#define EXPRESSION_TRACK_TIMEOUT_MS 3000U
#define EXPRESSION_SUBMISSION_TIMEOUT_MS 10000U
#define EXPRESSION_CLASSIFY_BUDGET_PER_BEAT_MS 40U
#define EXPRESSION_SUBMISSION_BURST 8U
#define DELIVERY_EVENT_BURST 8U

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
    settings->bubble_bounds_mode = (int)app->bubble_bounds_mode;
    settings->bubble_custom_x = app->bubble_custom_bounds.x;
    settings->bubble_custom_y = app->bubble_custom_bounds.y;
    settings->bubble_custom_width = app->bubble_custom_bounds.w;
    settings->bubble_custom_height = app->bubble_custom_bounds.h;
    settings->vsync = app->vsync_enabled;
    settings->fps_limit = app->fps_limit;
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
    case EIDOLON_USER_SETTING_BUBBLE_BOUNDS:
        app->user_settings.bubble_bounds_mode = (int)app->bubble_bounds_mode;
        app->user_settings.bubble_custom_x = app->bubble_custom_bounds.x;
        app->user_settings.bubble_custom_y = app->bubble_custom_bounds.y;
        app->user_settings.bubble_custom_width = app->bubble_custom_bounds.w;
        app->user_settings.bubble_custom_height = app->bubble_custom_bounds.h;
        break;
    case EIDOLON_USER_SETTING_VSYNC:
        app->user_settings.vsync = app->vsync_enabled;
        break;
    case EIDOLON_USER_SETTING_FPS_LIMIT:
        app->user_settings.fps_limit = app->fps_limit;
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

static float session_attention_direction(const EidolonApp *app, const EidolonSessionEntry *entry) {
    if (entry == NULL || entry->layout_slot < 0 ||
        entry->layout_slot >= (int)EIDOLON_VISIBLE_SESSION_CAPACITY) {
        return -1.0F;
    }
    const int slot = entry->layout_slot;
    if (app->bubble_rect_valid[slot]) {
        return app->bubble_sides[slot] == EIDOLON_BUBBLE_SIDE_RIGHT ? 1.0F : -1.0F;
    }
    return (slot & 1) != 0 ? 1.0F : -1.0F;
}

#ifndef NDEBUG
static void performance_preview(const EidolonDialogue *dialogue, const EidolonExpressionBeat *beat,
                                char *output, size_t capacity) {
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

static void log_performance_plan(EidolonDialogue *dialogue) {
    EidolonExpressionTrack *track = &dialogue->expression_track;
    if (track->logged_count >= track->count) {
        return;
    }
    const uint64_t batch_started_ms =
        track->batch_prepared_ms != 0U ? track->batch_prepared_ms : track->prepared_ms;
    if (track->logged_count == 0U) {
        eidolon_log_write(
            "latency", "session=%s phase=affect-ready detected_to_ready_ms=%llu beats=%zu",
            track->owner, (unsigned long long)(track->ready_ms - track->prepared_ms), track->count);
    }
    eidolon_log_write("performance", "track=%llu owner=%s ready beats=%zu new=%zu latency_ms=%llu",
                      (unsigned long long)track->track_id, track->owner, track->count,
                      track->count - track->logged_count,
                      (unsigned long long)(track->ready_ms - batch_started_ms));
    for (size_t index = track->logged_count; index < track->count; ++index) {
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
            eidolon_expression_intent_name(beat->expression), beat->expression_held ? "yes" : "no",
            beat->previous_expression_advantage,
            eidolon_expression_intent_name(beat->runner_up_expression), beat->expression_margin,
            eidolon_performance_cue_name(beat->cue), eidolon_cue_reason_name(beat->cue_reason),
            beat->intensity, preview);
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
            eidolon_expression_intent_name(beat->expression), beat->expression_held ? "yes" : "no",
            beat->previous_expression_advantage,
            eidolon_expression_intent_name(beat->runner_up_expression), beat->expression_margin,
            eidolon_performance_cue_name(beat->cue), eidolon_cue_reason_name(beat->cue_reason),
            beat->intensity);
#endif
    }
    track->logged_count = track->count;
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

static void activate_dialogue_delivery(EidolonApp *app, EidolonDialogue *dialogue, float attention,
                                       uint64_t now_ms) {
    if (dialogue == NULL) {
        return;
    }
    EidolonDeliveryMark events[DELIVERY_EVENT_BURST];
    const size_t offset = eidolon_dialogue_revealed_text_offset(dialogue);
    const size_t count = eidolon_delivery_track_collect(&dialogue->delivery_track, offset, events,
                                                        SDL_arraysize(events));
    for (size_t index = 0U; index < count; ++index) {
        const float direction =
            attention < 0.0F ? -events[index].direction : events[index].direction;
        eidolon_portrait_set_attention(app->portrait, attention);
        eidolon_portrait_deliver(app->portrait, events[index].cue, events[index].intensity,
                                 direction, now_ms);
        eidolon_log_write(
            "delivery", "owner=%s offset=%zu cue=%s intensity=%.2f direction=%.0f",
            dialogue->expression_track.owner[0] != '\0' ? dialogue->expression_track.owner : "live",
            events[index].text_offset, eidolon_delivery_cue_name(events[index].cue),
            events[index].intensity, direction);
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

static bool submit_dialogue_performance(EidolonApp *app, EidolonDialogue *dialogue,
                                        uint64_t now_ms) {
    EidolonExpressionTrack *track = &dialogue->expression_track;
    char text[EIDOLON_EXPRESSION_BEAT_TEXT_CAPACITY];
    size_t submitted = 0U;
    while (track->next_submit_index < track->count && submitted < EXPRESSION_SUBMISSION_BURST) {
        const size_t index = track->next_submit_index;
        if (!eidolon_expression_track_copy_text(track, index, dialogue->text, text, sizeof(text))) {
            return false;
        }
        app->next_affect_sequence += 1U;
        if (app->next_affect_sequence == 0U) {
            app->next_affect_sequence += 1U;
        }
        if (!eidolon_affect_client_submit(app->affect_client, app->next_affect_sequence, text)) {
            if (!track->submission_deferred) {
                eidolon_log_write("performance",
                                  "track=%llu owner=%s submissions deferred queued=%zu/%zu",
                                  (unsigned long long)track->track_id, track->owner,
                                  track->next_submit_index, track->count);
                track->submission_deferred = true;
            }
            return true;
        }
        if (!eidolon_expression_track_set_sequence(track, index, app->next_affect_sequence)) {
            return false;
        }
        track->beats[index].submitted_ms = now_ms;
        track->next_submit_index += 1U;
        submitted += 1U;
    }
    if (track->next_submit_index == track->count && !track->submission_complete) {
        track->submission_complete = true;
        track->submission_deferred = false;
        track->deadline_ms = now_ms + EXPRESSION_TRACK_TIMEOUT_MS +
                             (uint64_t)track->count * EXPRESSION_CLASSIFY_BUDGET_PER_BEAT_MS;
        eidolon_log_write("performance", "track=%llu owner=%s queued beats=%zu",
                          (unsigned long long)track->track_id, track->owner, track->count);
    }
    return true;
}

static void prepare_dialogue_performance(EidolonApp *app, EidolonDialogue *dialogue,
                                         EidolonState state, float attention, const char *owner,
                                         uint64_t now_ms) {
    eidolon_expression_track_compile(&dialogue->expression_track, dialogue->text, state);
    app->next_performance_track_id += 1U;
    dialogue->expression_track.track_id = app->next_performance_track_id;
    dialogue->expression_track.prepared_ms = now_ms;
    dialogue->expression_track.batch_prepared_ms = now_ms;
    SDL_strlcpy(dialogue->expression_track.owner, owner != NULL ? owner : "unknown",
                sizeof(dialogue->expression_track.owner));
    if (dialogue->expression_track.count == 0U) {
        eidolon_dialogue_resume(dialogue, now_ms);
        return;
    }
    dialogue->expression_track.deadline_ms = now_ms + EXPRESSION_SUBMISSION_TIMEOUT_MS;
    eidolon_log_write(
        "performance", "track=%llu owner=%s compiled beats=%zu delivery=%zu bytes=%zu",
        (unsigned long long)dialogue->expression_track.track_id, dialogue->expression_track.owner,
        dialogue->expression_track.count, dialogue->delivery_track.count, strlen(dialogue->text));
    if (app->affect_client == NULL) {
        fallback_dialogue_performance(app, dialogue, attention, now_ms, "worker unavailable");
        return;
    }
    if (!submit_dialogue_performance(app, dialogue, now_ms)) {
        fallback_dialogue_performance(app, dialogue, attention, now_ms,
                                      "classification submission failed");
    }
}

static void begin_stream_performance(EidolonApp *app, EidolonDialogue *dialogue, EidolonState state,
                                     const char *owner, uint64_t now_ms) {
    eidolon_expression_track_compile(&dialogue->expression_track, "", state);
    app->next_performance_track_id += 1U;
    dialogue->expression_track.track_id = app->next_performance_track_id;
    dialogue->expression_track.prepared_ms = now_ms;
    dialogue->expression_track.batch_prepared_ms = now_ms;
    SDL_strlcpy(dialogue->expression_track.owner, owner != NULL ? owner : "unknown",
                sizeof(dialogue->expression_track.owner));
    eidolon_log_write("performance", "track=%llu owner=%s stream opened",
                      (unsigned long long)dialogue->expression_track.track_id,
                      dialogue->expression_track.owner);
}

static void extend_stream_performance(EidolonApp *app, EidolonDialogue *dialogue,
                                      EidolonState state, float attention, bool completed,
                                      uint64_t now_ms) {
    const size_t previous_count = dialogue->expression_track.count;
    if (!eidolon_expression_track_extend(&dialogue->expression_track, dialogue->text, state,
                                         completed)) {
        return;
    }
    dialogue->expression_track.batch_prepared_ms = now_ms;
    dialogue->expression_track.deadline_ms = now_ms + EXPRESSION_SUBMISSION_TIMEOUT_MS;
    eidolon_log_write(
        "performance", "track=%llu owner=%s stream extended beats=%zu->%zu final=%s bytes=%zu",
        (unsigned long long)dialogue->expression_track.track_id, dialogue->expression_track.owner,
        previous_count, dialogue->expression_track.count, completed ? "yes" : "no",
        strlen(dialogue->text));
    if (dialogue->expression_track.count == 0U) {
        return;
    }
    if (app->affect_client == NULL) {
        fallback_dialogue_performance(app, dialogue, attention, now_ms, "worker unavailable");
        return;
    }
    if (!submit_dialogue_performance(app, dialogue, now_ms)) {
        fallback_dialogue_performance(app, dialogue, attention, now_ms,
                                      "classification submission failed");
    }
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
        bool applied =
            apply_classification(app, &app->dialogue, sequence, probabilities, -1.0F, now_ms);
        for (size_t index = 0U; !applied && index < EIDOLON_SESSION_CAPACITY; ++index) {
            EidolonSessionEntry *entry = &app->session_registry.entries[index];
            if (entry->occupied) {
                applied = apply_classification(app, &entry->dialogue, sequence, probabilities,
                                               session_attention_direction(app, entry), now_ms);
            }
        }
    }

    if (app->dialogue.expression_track.waiting &&
        !app->dialogue.expression_track.submission_complete &&
        !submit_dialogue_performance(app, &app->dialogue, now_ms)) {
        fallback_dialogue_performance(app, &app->dialogue, -1.0F, now_ms,
                                      "classification submission failed");
    }
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        EidolonSessionEntry *entry = &app->session_registry.entries[index];
        if (entry->occupied && entry->dialogue.expression_track.waiting &&
            !entry->dialogue.expression_track.submission_complete &&
            !submit_dialogue_performance(app, &entry->dialogue, now_ms)) {
            fallback_dialogue_performance(app, &entry->dialogue,
                                          session_attention_direction(app, entry), now_ms,
                                          "classification submission failed");
        }
    }

    if (app->dialogue.expression_track.waiting &&
        now_ms >= app->dialogue.expression_track.deadline_ms) {
        fallback_dialogue_performance(app, &app->dialogue, -1.0F, now_ms,
                                      app->dialogue.expression_track.submission_complete
                                          ? "classification timeout"
                                          : "classification submission timeout");
    }
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        EidolonSessionEntry *entry = &app->session_registry.entries[index];
        if (entry->occupied && entry->dialogue.expression_track.waiting &&
            now_ms >= entry->dialogue.expression_track.deadline_ms) {
            fallback_dialogue_performance(app, &entry->dialogue,
                                          session_attention_direction(app, entry), now_ms,
                                          entry->dialogue.expression_track.submission_complete
                                              ? "classification timeout"
                                              : "classification submission timeout");
        }
    }
}

static uint64_t current_display_interval_ns(const EidolonApp *app) {
    if (app->presentation_environment_valid && (app->presentation_environment.valid_fields &
                                                EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH) != 0U) {
        return eidolon_frame_interval_ns(0, 0, app->presentation_environment.nominal_refresh_hz);
    }
    return eidolon_frame_interval_ns(0, 0, 60.0F);
}

static bool presentation_position(const EidolonApp *app, int *x, int *y) {
    EidolonPresentationGeometry geometry;
    if (!eidolon_presentation_get_geometry(app->presentation, &geometry)) {
        return false;
    }
    *x = geometry.x;
    *y = geometry.y;
    return true;
}

static bool set_presentation_position(EidolonApp *app, int x, int y) {
    EidolonPresentationGeometry geometry;
    if (!eidolon_presentation_get_geometry(app->presentation, &geometry)) {
        return false;
    }
    geometry.x = x;
    geometry.y = y;
    return eidolon_presentation_set_geometry(app->presentation, &geometry);
}

static void update_presentation_policy(EidolonApp *app) {
    app->display_interval_ns = current_display_interval_ns(app);
    app->presentation_interval_ns = eidolon_frame_policy_interval_ns(
        app->display_interval_ns, app->vsync_enabled, app->vsync_active, app->fps_limit,
        &app->presentation_uncapped, &app->presentation_software_paced);
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
    if (app->renderer == NULL) {
        SDL_SetError("sprite rendering requires the SDL legacy presentation");
        return false;
    }
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
    if (app->renderer == NULL) {
        SDL_SetError("3D rendering requires the SDL legacy presentation");
        return false;
    }
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

static void body_dimensions(const EidolonApp *app, float scale, float *width, float *height) {
    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait)) {
        *width = eidolon_portrait_display_width(app->portrait) * scale;
        *height = eidolon_portrait_display_height(app->portrait) * scale;
        return;
    }
    if (app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D) {
        *width = EIDOLON_MODEL_DISPLAY_SIZE * scale;
        *height = *width;
        return;
    }
    *width = (float)EIDOLON_CELL_WIDTH * scale;
    *height = (float)EIDOLON_CELL_HEIGHT * scale;
}

static SDL_FRect legacy_body_rect(const EidolonApp *app, float scale, int canvas_width,
                                  int canvas_height, size_t visible_count) {
    float width = 0.0F;
    float height = 0.0F;
    body_dimensions(app, scale, &width, &height);
    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait)) {
        return eidolon_bubble_layout_character(canvas_width, canvas_height, width, height,
                                               visible_count);
    }
    if (app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D) {
        return (SDL_FRect){(float)canvas_width - width, (float)canvas_height - height, width,
                           height};
    }
    return (SDL_FRect){(float)canvas_width - 18.0F - width, (float)canvas_height - 24.0F - height,
                       width, height};
}

static SDL_FPoint current_body_global_center(const EidolonApp *app) {
    int window_x = 0;
    int window_y = 0;
    (void)presentation_position(app, &window_x, &window_y);
    const SDL_FRect body =
        app->body_rect_initialized
            ? app->body_rect
            : legacy_body_rect(app, app->model_scale, app->window_width, app->window_height,
                               eidolon_session_registry_visible_count(&app->session_registry));
    return (SDL_FPoint){
        (float)window_x + (body.x + body.w * 0.5F) * app->window_coordinate_scale,
        (float)window_y + (body.y + body.h * 0.5F) * app->window_coordinate_scale,
    };
}

static void restore_body_global_center(EidolonApp *app, SDL_FPoint center) {
    if (!app->body_rect_initialized || app->window_coordinate_scale <= 0.0F) {
        return;
    }
    int window_x = 0;
    int window_y = 0;
    (void)presentation_position(app, &window_x, &window_y);
    app->body_rect.x =
        (center.x - (float)window_x) / app->window_coordinate_scale - app->body_rect.w * 0.5F;
    app->body_rect.y =
        (center.y - (float)window_y) / app->window_coordinate_scale - app->body_rect.h * 0.5F;
}

static bool apply_window_geometry(EidolonApp *app, SDL_Rect window_bounds) {
    if (window_bounds.w <= 0 || window_bounds.h <= 0) {
        return false;
    }
    EidolonPresentationGeometry current = {
        .x = window_bounds.x,
        .y = window_bounds.y,
        .width = window_bounds.w,
        .height = window_bounds.h,
    };
    const bool have_geometry = eidolon_presentation_get_geometry(app->presentation, &current);
    EidolonPresentationGeometry requested = current;
    requested.width = window_bounds.w;
    requested.height = window_bounds.h;
    if (!app->snapshot_mode) {
        requested.x = window_bounds.x;
        requested.y = window_bounds.y;
    }
    const bool geometry_requested = !have_geometry || current.x != requested.x ||
                                    current.y != requested.y || current.width != requested.width ||
                                    current.height != requested.height;
    if (geometry_requested && !eidolon_presentation_set_geometry(app->presentation, &requested)) {
        eidolon_log_write("layout", "could not apply overlay geometry: %s", SDL_GetError());
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not apply overlay geometry: %s",
                    SDL_GetError());
        return false;
    }
    if (geometry_requested && !eidolon_presentation_sync_host(app->presentation)) {
        eidolon_log_write("layout", "overlay geometry synchronization timed out: %s",
                          SDL_GetError());
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Overlay geometry synchronization timed out: %s",
                    SDL_GetError());
        return false;
    }
    app->window_width =
        SDL_max(1, (int)SDL_ceilf((float)window_bounds.w / app->window_coordinate_scale));
    app->window_height =
        SDL_max(1, (int)SDL_ceilf((float)window_bounds.h / app->window_coordinate_scale));
    return true;
}

static float output_intersection_area(SDL_FRect body, EidolonPresentationRect output) {
    const float left = SDL_max(body.x, output.x);
    const float top = SDL_max(body.y, output.y);
    const float right = SDL_min(body.x + body.w, output.x + output.width);
    const float bottom = SDL_min(body.y + body.h, output.y + output.height);
    return SDL_max(0.0F, right - left) * SDL_max(0.0F, bottom - top);
}

static bool presentation_rect_to_sdl(const EidolonPresentationRect *source, SDL_Rect *result) {
    if (source->width <= 0.0F || source->height <= 0.0F) {
        return false;
    }
    *result = (SDL_Rect){
        (int)SDL_floorf(source->x),
        (int)SDL_floorf(source->y),
        SDL_max(1, (int)SDL_ceilf(source->x + source->width) - (int)SDL_floorf(source->x)),
        SDL_max(1, (int)SDL_ceilf(source->y + source->height) - (int)SDL_floorf(source->y)),
    };
    return true;
}

static bool query_presentation_outputs(EidolonApp *app, EidolonPresentationOutputInfo **outputs,
                                       size_t *output_count, uint64_t *topology_revision) {
    *outputs = NULL;
    *output_count = 0U;
    *topology_revision = 0U;
    EidolonPresentationTopologyResult result =
        eidolon_presentation_copy_outputs(app->presentation, NULL, 0U);
    if (result.status == EIDOLON_PRESENTATION_TOPOLOGY_UNAVAILABLE ||
        result.status == EIDOLON_PRESENTATION_TOPOLOGY_ERROR) {
        return false;
    }

    for (unsigned int attempt = 0U; attempt < 3U; ++attempt) {
        if (result.required_count == 0U) {
            *topology_revision = result.revision;
            return result.status == EIDOLON_PRESENTATION_TOPOLOGY_OK;
        }
        if (result.required_count > SIZE_MAX / sizeof(EidolonPresentationOutputInfo)) {
            return false;
        }
        EidolonPresentationOutputInfo *candidate =
            SDL_malloc(result.required_count * sizeof(*candidate));
        if (candidate == NULL) {
            return false;
        }
        result =
            eidolon_presentation_copy_outputs(app->presentation, candidate, result.required_count);
        if (result.status == EIDOLON_PRESENTATION_TOPOLOGY_OK) {
            *outputs = candidate;
            *output_count = result.copied_count;
            *topology_revision = result.revision;
            return true;
        }
        SDL_free(candidate);
        if (result.status != EIDOLON_PRESENTATION_TOPOLOGY_CHANGED &&
            result.status != EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY) {
            return false;
        }
    }
    return false;
}

static bool resolve_presentation_bubble_bounds(EidolonApp *app, SDL_FRect body, SDL_Rect *result) {
    const EidolonPresentationEnvironment *environment = &app->presentation_environment;
    if (!app->presentation_environment_valid ||
        (environment->coordinate_space != EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL &&
         environment->coordinate_space != EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_LOGICAL)) {
        return false;
    }

    EidolonPresentationOutputInfo *outputs = NULL;
    size_t output_count = 0U;
    uint64_t topology_revision = 0U;
    if (!query_presentation_outputs(app, &outputs, &output_count, &topology_revision) ||
        ((environment->valid_fields & EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY) != 0U &&
         topology_revision != environment->topology_revision)) {
        SDL_free(outputs);
        return false;
    }

    const EidolonPresentationOutputInfo *selected = NULL;
    float selected_area = -1.0F;
    float previous_area = -1.0F;
    bool found = false;
    for (size_t index = 0U; index < output_count; ++index) {
        const EidolonPresentationOutputInfo *output = &outputs[index];
        if (output->coordinate_space != environment->coordinate_space ||
            (output->valid_fields & EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS) == 0U) {
            continue;
        }
        if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_AVATAR) {
            const float area = output_intersection_area(body, output->bounds);
            if (area > selected_area) {
                selected = output;
                selected_area = area;
            }
            if (output->output.value == app->bubble_output.value) {
                previous_area = area;
            }
        } else if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_PRIMARY) {
            if ((output->flags & EIDOLON_PRESENTATION_OUTPUT_PRIMARY) != 0U) {
                selected = output;
                break;
            }
        } else if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_VIRTUAL) {
            SDL_Rect usable;
            if (!presentation_rect_to_sdl(&output->usable_bounds, &usable)) {
                continue;
            }
            if (!found) {
                *result = usable;
                found = true;
                continue;
            }
            const int left = SDL_min(result->x, usable.x);
            const int top = SDL_min(result->y, usable.y);
            const int right = SDL_max(result->x + result->w, usable.x + usable.w);
            const int bottom = SDL_max(result->y + result->h, usable.y + usable.h);
            *result = (SDL_Rect){left, top, right - left, bottom - top};
        }
    }
    if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_AVATAR && app->bubble_output.value != 0U &&
        previous_area >= 0.0F && selected_area <= previous_area + body.w * body.h * 0.15F) {
        for (size_t index = 0U; index < output_count; ++index) {
            if (outputs[index].output.value == app->bubble_output.value) {
                selected = &outputs[index];
                break;
            }
        }
    }
    if (selected != NULL) {
        found = presentation_rect_to_sdl(&selected->usable_bounds, result);
        if (found) {
            app->bubble_output = selected->output;
        }
    } else if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_VIRTUAL) {
        app->bubble_output.value = 0U;
    }
    SDL_free(outputs);
    return found;
}

static bool resolve_bubble_bounds(EidolonApp *app, SDL_FRect body, SDL_Rect *result) {
    if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_CUSTOM) {
        if (app->bubble_custom_bounds.w > 0 && app->bubble_custom_bounds.h > 0) {
            *result = app->bubble_custom_bounds;
            app->bubble_output.value = 0U;
            return true;
        }
        return false;
    }
    return resolve_presentation_bubble_bounds(app, body, result);
}

static void clear_bubble_rects(EidolonApp *app) {
    for (size_t slot = 0U; slot < EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        app->bubble_rect_valid[slot] = false;
        app->bubble_sides[slot] = EIDOLON_BUBBLE_SIDE_NONE;
    }
}

static void apply_legacy_layout(EidolonApp *app, float scale, SDL_FPoint body_center,
                                size_t visible_count) {
    float body_width = 0.0F;
    float body_height = 0.0F;
    body_dimensions(app, scale, &body_width, &body_height);
    int logical_width = EIDOLON_WINDOW_WIDTH;
    int logical_height = EIDOLON_WINDOW_HEIGHT;
    if (app->render_mode == EIDOLON_RENDER_MODE_PORTRAIT && eidolon_portrait_ready(app->portrait)) {
        logical_width = SDL_max(EIDOLON_WINDOW_WIDTH, (int)SDL_ceilf(body_width + 24.0F));
        logical_height = SDL_max(EIDOLON_WINDOW_HEIGHT, (int)SDL_ceilf(body_height + 24.0F));
        eidolon_bubble_layout_canvas(body_width, body_height, visible_count, &logical_width,
                                     &logical_height);
    } else {
        logical_width =
            SDL_max(EIDOLON_WINDOW_WIDTH, (int)SDL_ceilf((float)EIDOLON_WINDOW_WIDTH -
                                                         EIDOLON_MODEL_DISPLAY_SIZE + body_width));
        logical_height = SDL_max(EIDOLON_WINDOW_HEIGHT,
                                 (int)SDL_ceilf((float)EIDOLON_WINDOW_HEIGHT -
                                                EIDOLON_MODEL_DISPLAY_SIZE + body_height));
    }
    const SDL_FRect local_body =
        legacy_body_rect(app, scale, logical_width, logical_height, visible_count);
    const int native_width =
        SDL_max(1, (int)SDL_ceilf((float)logical_width * app->window_coordinate_scale));
    const int native_height =
        SDL_max(1, (int)SDL_ceilf((float)logical_height * app->window_coordinate_scale));
    const SDL_Rect window_bounds = {
        (int)SDL_roundf(body_center.x -
                        (local_body.x + local_body.w * 0.5F) * app->window_coordinate_scale),
        (int)SDL_roundf(body_center.y -
                        (local_body.y + local_body.h * 0.5F) * app->window_coordinate_scale),
        native_width,
        native_height,
    };
    if (!apply_window_geometry(app, window_bounds)) {
        return;
    }
    app->body_rect = local_body;
    app->body_rect_initialized = true;
    clear_bubble_rects(app);
    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        if (eidolon_session_registry_at_slot(&app->session_registry, slot) == NULL) {
            continue;
        }
        app->bubble_rects[slot] =
            eidolon_bubble_layout_rect(logical_width, logical_height, slot, visible_count);
        app->bubble_sides[slot] =
            (slot & 1) == 0 ? EIDOLON_BUBBLE_SIDE_LEFT : EIDOLON_BUBBLE_SIDE_RIGHT;
        app->bubble_rect_valid[slot] = true;
    }
}

static void reflow_overlay_layout(EidolonApp *app, float scale) {
    if (app->window_coordinate_scale <= 0.0F) {
        return;
    }
    const SDL_FPoint body_center = current_body_global_center(app);
    const size_t visible_count = eidolon_session_registry_visible_count(&app->session_registry);
    if (app->snapshot_mode || visible_count == 0U) {
        apply_legacy_layout(app, scale, body_center, visible_count);
        return;
    }

    float body_width = 0.0F;
    float body_height = 0.0F;
    body_dimensions(app, scale, &body_width, &body_height);
    const float coordinate_scale = app->window_coordinate_scale;
    const SDL_FRect global_body = {
        body_center.x - body_width * coordinate_scale * 0.5F,
        body_center.y - body_height * coordinate_scale * 0.5F,
        body_width * coordinate_scale,
        body_height * coordinate_scale,
    };
    SDL_Rect usable_bounds;
    if (!resolve_bubble_bounds(app, global_body, &usable_bounds)) {
        eidolon_log_write("layout", "could not resolve bubble bounds; using legacy layout");
        apply_legacy_layout(app, scale, body_center, visible_count);
        return;
    }

    int old_window_x = 0;
    int old_window_y = 0;
    (void)presentation_position(app, &old_window_x, &old_window_y);
    EidolonBubbleLayoutInput input = {
        .usable_bounds = {(float)usable_bounds.x, (float)usable_bounds.y, (float)usable_bounds.w,
                          (float)usable_bounds.h},
        .body_render_bounds = global_body,
        .visible_body_bounds = global_body,
        .spacing_scale = coordinate_scale,
        .has_face_bounds = true,
        .face_bounds = {global_body.x, global_body.y, global_body.w, global_body.h * 0.45F},
        .bubble_count = visible_count,
    };
    int slot_map[EIDOLON_VISIBLE_SESSION_CAPACITY];
    size_t bubble_index = 0U;
    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        if (eidolon_session_registry_at_slot(&app->session_registry, slot) == NULL) {
            continue;
        }
        slot_map[bubble_index] = slot;
        EidolonBubbleLayoutItem *item = &input.bubbles[bubble_index];
        item->width = EIDOLON_BUBBLE_WIDTH * coordinate_scale;
        item->height = EIDOLON_BUBBLE_HEIGHT * coordinate_scale;
        if (app->bubble_rect_valid[slot]) {
            item->has_previous = true;
            item->previous_rect = (SDL_FRect){
                (float)old_window_x + app->bubble_rects[slot].x * coordinate_scale,
                (float)old_window_y + app->bubble_rects[slot].y * coordinate_scale,
                app->bubble_rects[slot].w * coordinate_scale,
                app->bubble_rects[slot].h * coordinate_scale,
            };
            item->previous_side = app->bubble_sides[slot];
        }
        ++bubble_index;
    }

    EidolonBubbleLayoutResult layout;
    if (!eidolon_bubble_layout_solve(&input, &layout)) {
        eidolon_log_write("layout", "monitor-aware solve failed; using legacy layout");
        apply_legacy_layout(app, scale, body_center, visible_count);
        return;
    }
    const int window_x = (int)SDL_floorf(layout.canvas_bounds.x);
    const int window_y = (int)SDL_floorf(layout.canvas_bounds.y);
    const SDL_Rect window_bounds = {
        window_x,
        window_y,
        SDL_max(1, (int)SDL_ceilf(layout.canvas_bounds.x + layout.canvas_bounds.w) - window_x),
        SDL_max(1, (int)SDL_ceilf(layout.canvas_bounds.y + layout.canvas_bounds.h) - window_y),
    };
    if (!apply_window_geometry(app, window_bounds)) {
        return;
    }

    app->body_rect = (SDL_FRect){
        (global_body.x - (float)window_x) / coordinate_scale,
        (global_body.y - (float)window_y) / coordinate_scale,
        global_body.w / coordinate_scale,
        global_body.h / coordinate_scale,
    };
    app->body_rect_initialized = true;
    clear_bubble_rects(app);
    for (size_t index = 0U; index < layout.bubble_count; ++index) {
        const int slot = slot_map[index];
        app->bubble_rects[slot] = (SDL_FRect){
            (layout.bubbles[index].rect.x - (float)window_x) / coordinate_scale,
            (layout.bubbles[index].rect.y - (float)window_y) / coordinate_scale,
            layout.bubbles[index].rect.w / coordinate_scale,
            layout.bubbles[index].rect.h / coordinate_scale,
        };
        app->bubble_sides[slot] = layout.bubbles[index].side;
        app->bubble_rect_valid[slot] = true;
    }
    app->bubble_resolved_bounds = usable_bounds;
    app->hit_test_initialized = false;
#ifndef NDEBUG
    eidolon_log_write(
        "layout",
        "mode=%s bounds=%d,%d %dx%d body=%.0f,%.0f %.0fx%.0f bubbles=%zu window=%d,%d %dx%d",
        eidolon_bubble_bounds_mode_name(app->bubble_bounds_mode), usable_bounds.x, usable_bounds.y,
        usable_bounds.w, usable_bounds.h, global_body.x, global_body.y, global_body.w,
        global_body.h, layout.bubble_count, window_bounds.x, window_bounds.y, window_bounds.w,
        window_bounds.h);
#endif
}

static void set_initial_position(EidolonApp *app) {
    SDL_Rect bounds;
    if (!app->presentation_environment_valid ||
        (app->presentation_environment.valid_fields & EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS) ==
            0U ||
        !presentation_rect_to_sdl(&app->presentation_environment.usable_bounds, &bounds)) {
        return;
    }

    const int margin = 24;
    EidolonPresentationGeometry geometry;
    if (!eidolon_presentation_get_geometry(app->presentation, &geometry)) {
        return;
    }
    geometry.x = bounds.x + bounds.w - geometry.width - margin;
    geometry.y = bounds.y + bounds.h - geometry.height - margin;
    (void)eidolon_presentation_set_geometry(app->presentation, &geometry);
}

static bool update_display_metrics(EidolonApp *app) {
    const float previous_coordinate_scale = app->window_coordinate_scale;
    float display_scale =
        app->presentation_environment_valid && (app->presentation_environment.valid_fields &
                                                EIDOLON_PRESENTATION_ENV_CONTENT_SCALE) != 0U
            ? app->presentation_environment.content_scale
            : eidolon_presentation_display_scale(app->presentation);
    if (display_scale <= 0.0F) {
        display_scale = 1.0F;
    }
    float pixel_density =
        app->presentation_environment_valid && (app->presentation_environment.valid_fields &
                                                EIDOLON_PRESENTATION_ENV_PIXEL_SCALE) != 0U
            ? app->presentation_environment.pixel_scale
            : 1.0F;
    if (pixel_density <= 0.0F) {
        pixel_density = 1.0F;
    }

    app->display_scale = display_scale;
    app->window_coordinate_scale = display_scale / pixel_density;
    if (app->renderer != NULL && !SDL_SetRenderScale(app->renderer, display_scale, display_scale)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not apply display scale: %s",
                    SDL_GetError());
    }
    return SDL_fabsf(previous_coordinate_scale - app->window_coordinate_scale) > 0.0001F;
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
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_BUBBLE_BOUNDS) &&
        settings->bubble_bounds_mode >= 0 &&
        settings->bubble_bounds_mode < (int)EIDOLON_BUBBLE_BOUNDS_COUNT) {
        eidolon_app_set_bubble_custom_bounds(
            app, (SDL_Rect){settings->bubble_custom_x, settings->bubble_custom_y,
                            settings->bubble_custom_width, settings->bubble_custom_height});
        eidolon_app_set_bubble_bounds_mode(app,
                                           (EidolonBubbleBoundsMode)settings->bubble_bounds_mode);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_VSYNC)) {
        eidolon_app_set_vsync(app, settings->vsync);
    }
    if (eidolon_user_settings_is_overridden(settings, EIDOLON_USER_SETTING_FPS_LIMIT)) {
        eidolon_app_set_fps_limit(app, settings->fps_limit);
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
    app->bubble_bounds_mode = EIDOLON_BUBBLE_BOUNDS_AVATAR;
    app->bubble_custom_bounds = (SDL_Rect){0, 0, 1920, 1080};
    app->vsync_enabled = true;
    app->fps_limit = 0;
    eidolon_user_settings_defaults(&app->system_settings);
    eidolon_user_settings_defaults(&app->user_settings);
    eidolon_motion_config_defaults(&app->motion_config);
    eidolon_motion_config_watch_init(&app->motion_config_watch);
    eidolon_scene_init(&app->scene);

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

    const char *presentation_override =
        !app->snapshot_mode ? SDL_getenv("EIDOLON_PRESENTATION_BACKEND") : NULL;
    const bool native_presentation_requested =
        presentation_override != NULL && strcmp(presentation_override, "win32_dcomp") == 0;
#if defined(_WIN32)
    if (native_presentation_requested) {
        const EidolonWin32DcompConfig presentation_config = {
            .title = "Eidolon",
            .x = 0,
            .y = 0,
            .width = EIDOLON_WINDOW_WIDTH,
            .height = EIDOLON_WINDOW_HEIGHT,
            .visible = true,
        };
        app->presentation = eidolon_win32_dcomp_presentation_create(&presentation_config);
        if (app->presentation == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "DirectComposition presentation creation failed: %s", SDL_GetError());
            return false;
        }
    }
#else
    (void)presentation_override;
    if (native_presentation_requested) {
        SDL_SetError("win32_dcomp is available only on Windows");
        return false;
    }
#endif
    if (!native_presentation_requested) {
        const SDL_WindowFlags flags =
            app->snapshot_mode ? SDL_WINDOW_HIDDEN
                               : SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
                                     SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        const EidolonSdlLegacyConfig presentation_config = {
            .title = "Eidolon",
            .width = EIDOLON_WINDOW_WIDTH,
            .height = EIDOLON_WINDOW_HEIGHT,
            .window_flags = flags,
        };
        app->presentation = eidolon_sdl_legacy_presentation_create(&presentation_config);
    }
    if (app->presentation == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Presentation creation failed: %s",
                     SDL_GetError());
        return false;
    }
    EidolonPresentationEnvironment initial_environment;
    if (eidolon_presentation_get_environment(app->presentation, &initial_environment)) {
        app->presentation_environment = initial_environment;
        app->presentation_environment_valid = true;
        app->applied_environment_revision = initial_environment.revision;
    } else if (native_presentation_requested) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Presentation environment bootstrap failed: %s",
                     SDL_GetError());
        return false;
    } else {
        SDL_ClearError();
    }
    if (!native_presentation_requested) {
        app->window = eidolon_sdl_legacy_window(app->presentation);
        app->renderer = eidolon_sdl_legacy_renderer(app->presentation);
    }
    eidolon_log_write("renderer", "presentation=%s graphics=%s capabilities=0x%llx",
                      eidolon_presentation_backend_name(app->presentation),
                      app->renderer != NULL ? SDL_GetRendererName(app->renderer) : "direct3d11",
                      (unsigned long long)eidolon_presentation_capabilities(app->presentation));

    app->text_renderer = eidolon_text_renderer_create(app->renderer, EIDOLON_FONT_PATH, 12.0F);
    if (app->text_renderer == NULL) {
        eidolon_log_write("text", "Unicode renderer unavailable; debug text fallback active: %s",
                          SDL_GetError());
        SDL_ClearError();
    }

    eidolon_app_set_vsync(app, app->vsync_enabled);
    if (!app->snapshot_mode) {
        (void)update_display_metrics(app);
    }

    if (!ensure_portrait(app)) {
        if (app->renderer == NULL) {
            eidolon_log_write("portrait", "native presentation requires a portrait body");
            return false;
        }
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
        if (!eidolon_presentation_configure_host(app->presentation)) {
            return false;
        }
        set_initial_position(app);
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
            eidolon_log_write("provider",
                              "live provider manager unavailable; legacy inputs remain");
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
        app->event_pump = eidolon_event_pump_create(app->presentation, app->settings_ui);
        if (app->event_pump == NULL) {
            eidolon_log_write("input", "event pump unavailable: %s", SDL_GetError());
            return false;
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
    if (app->renderer == NULL && mode != EIDOLON_RENDER_MODE_PORTRAIT) {
        SDL_SetError("the native presentation proof currently supports portrait bodies only");
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
    app->model_scale = clamped;
    reflow_overlay_layout(app, clamped);
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
    const double display_rate = app->display_interval_ns > 0U
                                    ? (double)SDL_NS_PER_SECOND / (double)app->display_interval_ns
                                    : 0.0;
    const double presentation_rate =
        app->presentation_interval_ns > 0U
            ? (double)SDL_NS_PER_SECOND / (double)app->presentation_interval_ns
            : 0.0;
    EidolonPresentationGeometry geometry;
    if (eidolon_presentation_get_geometry(app->presentation, &geometry)) {
        window_width = geometry.width;
        window_height = geometry.height;
    }
    if (app->renderer != NULL) {
        (void)SDL_GetCurrentRenderOutputSize(app->renderer, &output_width, &output_height);
    } else {
        output_width = window_width;
        output_height = window_height;
    }
    eidolon_log_write(
        "renderer",
        "presentation scale=%.2fx logical=%dx%d window=%dx%d output=%dx%d target=%d "
        "display=%.2fHz vsync=requested:%s/active:%s fps_limit=%d effective=%s%.2fHz owner=%s",
        app->model_scale, app->window_width, app->window_height, window_width, window_height,
        output_width, output_height, eidolon_model_render_resolution(app->model), display_rate,
        app->vsync_enabled ? "yes" : "no", app->vsync_active ? "yes" : "no", app->fps_limit,
        app->presentation_uncapped ? "uncapped/" : "", presentation_rate,
        app->presentation_software_paced ? "software"
        : app->vsync_active              ? "vsync"
                                         : "none");
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
    return app->body_rect_initialized
               ? app->body_rect
               : legacy_body_rect(app, app->model_scale, app->window_width, app->window_height,
                                  eidolon_session_registry_visible_count(&app->session_registry));
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
    (void)presentation_position(app, &old_window_x, &old_window_y);
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
    if (!set_presentation_position(app, new_window_x, new_window_y)) {
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

void eidolon_app_set_bubble_bounds_mode(EidolonApp *app, EidolonBubbleBoundsMode mode) {
    if (mode < 0 || mode >= EIDOLON_BUBBLE_BOUNDS_COUNT) {
        return;
    }
    const bool changed = app->bubble_bounds_mode != mode;
    app->bubble_bounds_mode = mode;
    app->bubble_output.value = 0U;
    reflow_overlay_layout(app, app->model_scale);
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_BUBBLE_BOUNDS);
    }
}

void eidolon_app_set_bubble_custom_bounds(EidolonApp *app, SDL_Rect bounds) {
    if (bounds.w <= 0 || bounds.h <= 0) {
        return;
    }
    const bool changed =
        app->bubble_custom_bounds.x != bounds.x || app->bubble_custom_bounds.y != bounds.y ||
        app->bubble_custom_bounds.w != bounds.w || app->bubble_custom_bounds.h != bounds.h;
    app->bubble_custom_bounds = bounds;
    if (app->bubble_bounds_mode == EIDOLON_BUBBLE_BOUNDS_CUSTOM) {
        reflow_overlay_layout(app, app->model_scale);
    }
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_BUBBLE_BOUNDS);
    }
}

void eidolon_app_set_vsync(EidolonApp *app, bool enabled) {
    const bool changed = app->vsync_enabled != enabled;
    app->vsync_enabled = enabled;
    const int requested = !app->snapshot_mode && enabled ? 1 : 0;
    if (!eidolon_presentation_set_vsync(app->presentation, requested)) {
        eidolon_log_write("renderer", "could not set vsync=%d: %s", requested, SDL_GetError());
        SDL_ClearError();
    }
    int active = 0;
    if (app->renderer == NULL) {
        active = 0;
    } else if (!SDL_GetRenderVSync(app->renderer, &active)) {
        eidolon_log_write("renderer", "could not query active vsync: %s", SDL_GetError());
        SDL_ClearError();
        active = 0;
    }
    app->vsync_active = active != 0;
    update_presentation_policy(app);
    if (enabled && !app->snapshot_mode && !app->vsync_active) {
        eidolon_log_write("renderer", "vsync unavailable; active-display refresh fallback enabled");
    }
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_VSYNC);
    }
}

void eidolon_app_set_fps_limit(EidolonApp *app, int fps_limit) {
    const int clamped = SDL_clamp(fps_limit, EIDOLON_FPS_LIMIT_MIN, EIDOLON_FPS_LIMIT_MAX);
    const bool changed = app->fps_limit != clamped;
    app->fps_limit = clamped;
    update_presentation_policy(app);
    if (changed) {
        mark_user_settings_dirty(app, EIDOLON_USER_SETTING_FPS_LIMIT);
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
    case EIDOLON_USER_SETTING_BUBBLE_BOUNDS:
        eidolon_app_set_bubble_custom_bounds(app,
                                             (SDL_Rect){app->system_settings.bubble_custom_x,
                                                        app->system_settings.bubble_custom_y,
                                                        app->system_settings.bubble_custom_width,
                                                        app->system_settings.bubble_custom_height});
        eidolon_app_set_bubble_bounds_mode(
            app, (EidolonBubbleBoundsMode)app->system_settings.bubble_bounds_mode);
        break;
    case EIDOLON_USER_SETTING_VSYNC:
        eidolon_app_set_vsync(app, app->system_settings.vsync);
        break;
    case EIDOLON_USER_SETTING_FPS_LIMIT:
        eidolon_app_set_fps_limit(app, app->system_settings.fps_limit);
        break;
    }
    app->user_settings_applying = false;
    schedule_user_settings_save(app);
    eidolon_log_write("settings", "override reset field=0x%x", (unsigned int)field);
    return true;
}

static bool point_in_rect(float x, float y, const SDL_FRect *rect) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static SDL_FRect character_rect(const EidolonApp *app) {
    return app->body_rect_initialized
               ? app->body_rect
               : legacy_body_rect(app, app->model_scale, app->window_width, app->window_height,
                                  eidolon_session_registry_visible_count(&app->session_registry));
}

static void begin_primary_interaction(EidolonApp *app, const EidolonAppPointerEvent *pointer) {
    app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_NONE;
    app->primary_moved = false;
    app->primary_local_x = pointer->x;
    app->primary_local_y = pointer->y;
    app->primary_session_slot = -1;
    const size_t visible = eidolon_session_registry_visible_count(&app->session_registry);
    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        if (eidolon_session_registry_at_slot(&app->session_registry, slot) == NULL) {
            continue;
        }
        const SDL_FRect bubble = app->bubble_rects[slot];
        if (app->bubble_rect_valid[slot] && pointer->x >= bubble.x &&
            pointer->x < bubble.x + bubble.w && pointer->y >= bubble.y &&
            pointer->y < bubble.y + bubble.h) {
            app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_SESSION_BUBBLE;
            app->primary_session_slot = slot;
            return;
        }
    }
    if (visible == 0U && app->state == EIDOLON_STATE_REVIEW &&
        eidolon_dialogue_is_active(&app->dialogue)) {
        const SDL_FRect bubble = {17.0F, 16.0F, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
        if (point_in_rect(pointer->x, pointer->y, &bubble)) {
            app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_DIALOGUE_BUBBLE;
            return;
        }
    }
    const SDL_FRect character = character_rect(app);
    if (!point_in_rect(pointer->x, pointer->y, &character)) {
        return;
    }
    int original_x = 0;
    int original_y = 0;
    (void)presentation_position(app, &original_x, &original_y);
    if (eidolon_presentation_begin_interactive_move(app->presentation)) {
        int current_x = 0;
        int current_y = 0;
        (void)presentation_position(app, &current_x, &current_y);
        app->native_drag_completed = true;
        app->primary_moved = current_x != original_x || current_y != original_y;
        if (app->primary_moved) {
            reflow_overlay_layout(app, app->model_scale);
        }
        return;
    }
    app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG;
    if (pointer->global_position_valid) {
        app->drag_global_x = pointer->global_x;
        app->drag_global_y = pointer->global_y;
    } else {
        (void)SDL_GetGlobalMouseState(&app->drag_global_x, &app->drag_global_y);
    }
    (void)presentation_position(app, &app->drag_window_x, &app->drag_window_y);
    app->drag_target_window_x = app->drag_window_x;
    app->drag_target_window_y = app->drag_window_y;
    app->drag_position_pending = false;
    if (!SDL_CaptureMouse(true)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not capture character drag: %s",
                    SDL_GetError());
    }
}

static void update_primary_interaction(EidolonApp *app, const EidolonAppPointerEvent *pointer) {
    if (SDL_fabsf(pointer->x - app->primary_local_x) > 3.0F ||
        SDL_fabsf(pointer->y - app->primary_local_y) > 3.0F) {
        app->primary_moved = true;
    }
    if (app->primary_interaction != EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG) {
        return;
    }
    float global_x = pointer->global_x;
    float global_y = pointer->global_y;
    if (!pointer->global_position_valid) {
        (void)SDL_GetGlobalMouseState(&global_x, &global_y);
    }
    const int x = app->drag_window_x + (int)(global_x - app->drag_global_x);
    const int y = app->drag_window_y + (int)(global_y - app->drag_global_y);
    if (SDL_abs((int)(global_x - app->drag_global_x)) > 3 ||
        SDL_abs((int)(global_y - app->drag_global_y)) > 3) {
        app->primary_moved = true;
    }
    app->drag_target_window_x = x;
    app->drag_target_window_y = y;
    app->drag_position_pending = true;
}

static void commit_character_drag(EidolonApp *app) {
    if (app->primary_interaction != EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG ||
        !app->drag_position_pending) {
        return;
    }
    app->drag_position_pending = false;
    const Uint64 started_ns = SDL_GetTicksNS();
    if (!set_presentation_position(app, app->drag_target_window_x, app->drag_target_window_y)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not move character window: %s",
                    SDL_GetError());
        return;
    }
    const Uint64 elapsed_ns = SDL_GetTicksNS() - started_ns;
    if (elapsed_ns > app->presentation_interval_ns) {
        eidolon_log_write("input", "window move stalled duration_ms=%.2f target=%d,%d",
                          (double)elapsed_ns / (double)SDL_NS_PER_MS, app->drag_target_window_x,
                          app->drag_target_window_y);
    }
}

static void end_primary_interaction(EidolonApp *app) {
    if (app->primary_interaction == EIDOLON_PRIMARY_INTERACTION_CHARACTER_DRAG) {
        commit_character_drag(app);
        SDL_CaptureMouse(false);
        if (app->primary_moved) {
            reflow_overlay_layout(app, app->model_scale);
        }
    }
    app->drag_position_pending = false;
    app->primary_interaction = EIDOLON_PRIMARY_INTERACTION_NONE;
    app->primary_session_slot = -1;
}

static void advance_session_bubble(EidolonApp *app, int slot, uint64_t now_ms) {
    if (!eidolon_session_registry_advance(&app->session_registry, slot, now_ms)) {
        return;
    }
    EidolonSessionEntry *entry = eidolon_session_registry_at_slot(&app->session_registry, slot);
    if (entry != NULL) {
        activate_dialogue_performance(app, &entry->dialogue,
                                      session_attention_direction(app, entry), now_ms);
    }
}

static void advance_fallback_dialogue(EidolonApp *app, uint64_t now_ms) {
    const size_t previous_cursor = app->dialogue.cursor;
    eidolon_dialogue_advance(&app->dialogue, now_ms);
    if (app->dialogue.cursor != previous_cursor) {
        activate_dialogue_performance(app, &app->dialogue, -1.0F, now_ms);
    }
}

static void stage_presentation_environment(EidolonApp *app,
                                           const EidolonPresentationEnvironment *environment) {
    if (app->presentation_environment_valid &&
        environment->revision <= app->presentation_environment.revision) {
        return;
    }
    app->presentation_environment = *environment;
    app->presentation_environment_valid = true;
    app->presentation_environment_pending =
        environment->revision > app->applied_environment_revision;
}

static void apply_presentation_updates(EidolonApp *app) {
    if (app->presentation_resync_pending) {
        EidolonPresentationEnvironment environment;
        if (eidolon_presentation_get_environment(app->presentation, &environment)) {
            stage_presentation_environment(app, &environment);
        } else {
            eidolon_log_write("presentation", "environment resync failed: %s", SDL_GetError());
            SDL_ClearError();
        }
        app->presentation_resync_pending = false;
    }
    if (!app->presentation_environment_pending && !app->presentation_move_completion_pending) {
        return;
    }

    const SDL_FPoint body_center = current_body_global_center(app);
    if (app->presentation_environment_pending) {
        update_presentation_policy(app);
        if (update_display_metrics(app)) {
            restore_body_global_center(app, body_center);
        }
        app->applied_environment_revision = app->presentation_environment.revision;
        app->presentation_environment_pending = false;
        eidolon_log_write("presentation",
                          "environment applied revision=%llu topology=%llu fields=0x%llx output=%u",
                          (unsigned long long)app->presentation_environment.revision,
                          (unsigned long long)app->presentation_environment.topology_revision,
                          (unsigned long long)app->presentation_environment.changed_fields,
                          app->presentation_environment.active_output.value);
    }
    app->presentation_move_completion_pending = false;
    app->bubble_output.value = 0U;
    reflow_overlay_layout(app, app->model_scale);
    app->hit_test_initialized = false;
    app->native_drag_completed = true;
}

static void handle_presentation_event(EidolonApp *app, const EidolonPresentationEvent *event) {
    const uint64_t now_ms = SDL_GetTicks();
    switch (event->kind) {
    case EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED:
        for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
            if (app->bubble_layers[slot].value == event->data.layer.layer.value) {
                advance_session_bubble(app, slot, now_ms);
                return;
            }
        }
        if (app->fallback_dialogue_layer.value == event->data.layer.layer.value) {
            advance_fallback_dialogue(app, now_ms);
        }
        break;
    case EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED:
        for (size_t index = 0U; index < app->scene_snapshot.layer_count; ++index) {
            const EidolonSceneLayerSnapshot *layer = &app->scene_snapshot.layers[index];
            if (layer->id.value == event->data.layer.layer.value &&
                layer->kind == EIDOLON_SCENE_LAYER_BODY) {
                eidolon_settings_ui_open(app->settings_ui);
                return;
            }
        }
        break;
    case EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED:
        app->presentation_move_completion_pending = true;
        break;
    case EIDOLON_PRESENTATION_EVENT_MOVE_CANCELED:
        app->native_drag_completed = true;
        break;
    case EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED:
        app->presentation_resync_pending = true;
        app->presentation_move_completion_pending = true;
        eidolon_log_write("input", "presentation event queue resynchronized");
        break;
    case EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED:
        stage_presentation_environment(app, &event->data.environment.environment);
        break;
    case EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED:
        app->running = false;
        break;
    case EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED:
        if (event->data.graphics.reset_kind == EIDOLON_PRESENTATION_GRAPHICS_RESET_TARGETS) {
            app->hit_test_initialized = false;
            eidolon_model_request_redraw(app->model);
            eidolon_log_write("renderer",
                              "presentation targets reset; model redraw requested");
        } else {
            eidolon_log_write(
                "renderer", "presentation graphics reset kind=%d; restart required",
                (int)event->data.graphics.reset_kind);
            app->running = false;
        }
        break;
    case EIDOLON_PRESENTATION_EVENT_MOVE_STARTED:
    case EIDOLON_PRESENTATION_EVENT_NONE:
        break;
    }
}

static size_t drain_presentation_events(EidolonApp *app, size_t limit) {
    size_t count = 0U;
    EidolonPresentationEvent event;
    while (count < limit && eidolon_presentation_poll_event(app->presentation, &event)) {
        handle_presentation_event(app, &event);
        ++count;
    }
    apply_presentation_updates(app);
    return count;
}

static bool point_in_model_rect(const EidolonApp *app, float x, float y) {
    return app->render_mode == EIDOLON_RENDER_MODE_MODEL_3D && app->model != NULL &&
           point_in_rect(x, y, &app->body_rect);
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

static void begin_model_rotation_drag(EidolonApp *app, const EidolonAppPointerEvent *pointer) {
    if (!point_in_model_rect(app, pointer->x, pointer->y)) {
        return;
    }
    if (pointer->clicks >= 2U) {
        end_model_rotation_drag(app);
        eidolon_app_set_model_rotation(app, 0.0F, 0.0F, 0.0F);
        return;
    }

    app->model_rotation_dragging = true;
    app->model_rotation_roll_dragging = (pointer->modifiers & EIDOLON_APP_MODIFIER_SHIFT) != 0U;
    app->model_rotation_hit_test_suspended = false;
    if (!SDL_CaptureMouse(true)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not capture model rotation drag: %s",
                    SDL_GetError());
    }
}

static void update_model_rotation_drag(EidolonApp *app, const EidolonAppPointerEvent *pointer) {
    if (pointer->x_relative == 0.0F && pointer->y_relative == 0.0F) {
        return;
    }
    if (!app->model_rotation_hit_test_suspended) {
        eidolon_presentation_suspend_input_region(app->presentation);
        app->model_rotation_hit_test_suspended = true;
    }
    if (app->model_rotation_roll_dragging) {
        eidolon_app_set_model_rotation(app, app->model_yaw_degrees, app->model_pitch_degrees,
                                       app->model_roll_degrees +
                                           pointer->x_relative * MODEL_ROTATION_DEGREES_PER_PIXEL);
    } else {
        eidolon_app_set_model_rotation(
            app, app->model_yaw_degrees + pointer->x_relative * MODEL_ROTATION_DEGREES_PER_PIXEL,
            app->model_pitch_degrees + pointer->y_relative * MODEL_ROTATION_DEGREES_PER_PIXEL,
            app->model_roll_degrees);
    }
}

static void handle_app_event(EidolonApp *app, const EidolonAppEvent *event) {
    switch (event->kind) {
    case EIDOLON_APP_EVENT_QUIT_REQUESTED:
        app->running = false;
        break;
    case EIDOLON_APP_EVENT_OPEN_SETTINGS:
        eidolon_settings_ui_open(app->settings_ui);
        break;
    case EIDOLON_APP_EVENT_RELOAD_CONFIGS:
        eidolon_app_reload_configs(app);
        break;
    case EIDOLON_APP_EVENT_FOCUS_LOST:
        end_primary_interaction(app);
        end_model_rotation_drag(app);
        break;
    case EIDOLON_APP_EVENT_POINTER_DOWN:
        if (event->data.pointer.button == EIDOLON_APP_POINTER_BUTTON_PRIMARY) {
            begin_primary_interaction(app, &event->data.pointer);
        } else if (event->data.pointer.button == EIDOLON_APP_POINTER_BUTTON_MIDDLE) {
            begin_model_rotation_drag(app, &event->data.pointer);
        } else if (event->data.pointer.button == EIDOLON_APP_POINTER_BUTTON_SECONDARY) {
            const SDL_FRect character = character_rect(app);
            if (point_in_rect(event->data.pointer.x, event->data.pointer.y, &character)) {
                eidolon_settings_ui_open(app->settings_ui);
            }
        }
        break;
    case EIDOLON_APP_EVENT_POINTER_UP:
        if (event->data.pointer.button == EIDOLON_APP_POINTER_BUTTON_PRIMARY) {
            if (!app->primary_moved &&
                app->primary_interaction == EIDOLON_PRIMARY_INTERACTION_SESSION_BUBBLE) {
                advance_session_bubble(app, app->primary_session_slot, SDL_GetTicks());
            } else if (!app->primary_moved &&
                       app->primary_interaction == EIDOLON_PRIMARY_INTERACTION_DIALOGUE_BUBBLE) {
                advance_fallback_dialogue(app, SDL_GetTicks());
            }
            end_primary_interaction(app);
        } else if (event->data.pointer.button == EIDOLON_APP_POINTER_BUTTON_MIDDLE) {
            end_model_rotation_drag(app);
        }
        break;
    case EIDOLON_APP_EVENT_POINTER_MOTION:
        if (app->model_rotation_dragging) {
            update_model_rotation_drag(app, &event->data.pointer);
        } else if (app->primary_interaction != EIDOLON_PRIMARY_INTERACTION_NONE) {
            update_primary_interaction(app, &event->data.pointer);
        }
        break;
    case EIDOLON_APP_EVENT_NONE:
    default:
        break;
    }
}

void eidolon_app_run(EidolonApp *app) {
    EidolonFrameClock frame_clock;
    eidolon_frame_clock_init(&frame_clock, SDL_GetTicksNS(), app->presentation_interval_ns);
    bool frame_clock_software_paced = app->presentation_software_paced;
    uint64_t next_event_pressure_log_ms = 0U;
    while (app->running) {
        EidolonAppEvent event;
        size_t event_count = 0U;
        Uint64 now_ns = SDL_GetTicksNS();
        if (frame_clock.interval_ns != app->presentation_interval_ns ||
            frame_clock_software_paced != app->presentation_software_paced) {
            eidolon_frame_clock_set_interval(&frame_clock, now_ns, app->presentation_interval_ns);
            frame_clock_software_paced = app->presentation_software_paced;
        }
        const Sint32 wait_ms =
            frame_clock_software_paced ? eidolon_frame_clock_wait_ms(&frame_clock, now_ns) : 0;
        const bool event_received = eidolon_event_pump_wait(app->event_pump, wait_ms, &event);
        const Uint64 event_batch_started_ns = SDL_GetTicksNS();
        if (event_received) {
            handle_app_event(app, &event);
            event_count = 1U;
        }
        while (event_count < EVENT_BATCH_LIMIT &&
               eidolon_event_pump_poll(app->event_pump, &event)) {
            handle_app_event(app, &event);
            ++event_count;
        }
        event_count += drain_presentation_events(
            app, event_count < EVENT_BATCH_LIMIT ? EVENT_BATCH_LIMIT - event_count : 0U);
        if (!app->running) {
            break;
        }

        now_ns = SDL_GetTicksNS();
        const Uint64 event_batch_ns = now_ns - event_batch_started_ns;
        const uint64_t event_now_ms = SDL_GetTicks();
        if (app->native_drag_completed) {
            eidolon_frame_clock_set_interval(&frame_clock, now_ns, app->presentation_interval_ns);
            frame_clock.previous_frame_ns = 0U;
            app->native_drag_completed = false;
        } else if ((event_count == EVENT_BATCH_LIMIT || event_batch_ns > frame_clock.interval_ns) &&
                   event_now_ms >= next_event_pressure_log_ms) {
            eidolon_log_write("input", "event pressure events=%zu duration_ms=%.2f capped=%s",
                              event_count, (double)event_batch_ns / (double)SDL_NS_PER_MS,
                              event_count == EVENT_BATCH_LIMIT ? "yes" : "no");
            next_event_pressure_log_ms = event_now_ms + EVENT_PRESSURE_LOG_INTERVAL_MS;
        }
        if (frame_clock_software_paced && !eidolon_frame_clock_due(&frame_clock, now_ns)) {
            continue;
        }
        commit_character_drag(app);
        now_ns = SDL_GetTicksNS();
        if (frame_clock.previous_frame_ns != 0U &&
            now_ns - frame_clock.previous_frame_ns > frame_clock.interval_ns * 2U) {
            eidolon_log_write("renderer", "presentation hitch gap_ms=%.2f",
                              (double)(now_ns - frame_clock.previous_frame_ns) /
                                  (double)SDL_NS_PER_MS);
        }
        const float delta_seconds = eidolon_frame_clock_begin(&frame_clock, now_ns);

        const uint64_t now_ms = SDL_GetTicks();
        EidolonConversationEvent conversation_event;
        bool conversation_visibility_grew = false;
        while (eidolon_conversation_sources_poll(app->conversation_sources, &conversation_event)) {
            const size_t visible_before_event =
                eidolon_session_registry_visible_count(&app->session_registry);
            const EidolonSessionPoll live = eidolon_session_registry_apply_event(
                &app->session_registry, &conversation_event, now_ms);
            conversation_visibility_grew =
                conversation_visibility_grew || eidolon_session_registry_visible_count(
                                                    &app->session_registry) > visible_before_event;
            if (live.stream_started && live.message_session != NULL) {
                eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
                const float attention = session_attention_direction(app, live.message_session);
                eidolon_portrait_set_attention(app->portrait, attention);
                const EidolonAffect responding = eidolon_affect_for_state(EIDOLON_STATE_REVIEW);
                eidolon_affect_controller_perform(&app->affect, &responding,
                                                  EIDOLON_EXPRESSION_RESPONDING, 0.65F, now_ms);
                begin_stream_performance(app, &live.message_session->dialogue, app->state,
                                         live.message_session->id, now_ms);
            }
            if (live.stream_delta && live.message_session != NULL) {
                const float attention = session_attention_direction(app, live.message_session);
                extend_stream_performance(app, &live.message_session->dialogue, app->state,
                                          attention, false, now_ms);
            }
            if (live.message_completed && live.message_session != NULL) {
                eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
                const float attention = session_attention_direction(app, live.message_session);
                eidolon_portrait_set_attention(app->portrait, attention);
                if (live.new_message) {
                    prepare_dialogue_performance(app, &live.message_session->dialogue, app->state,
                                                 attention, live.message_session->id, now_ms);
                } else {
                    extend_stream_performance(app, &live.message_session->dialogue, app->state,
                                              attention, true, now_ms);
                }
            }
        }

        EidolonState received_state;
        char received_text[EIDOLON_IPC_TEXT_CAPACITY + 1];
        const bool legacy_hooks =
            app->conversation_sources == NULL ||
            eidolon_conversation_sources_legacy_hooks(app->conversation_sources);
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
        const size_t visible_before_session_poll =
            eidolon_session_registry_visible_count(&app->session_registry);
        const EidolonSessionPoll sessions =
            eidolon_session_registry_poll(&app->session_registry, now_ms);
        const size_t visible_after_session_poll =
            eidolon_session_registry_visible_count(&app->session_registry);
        const bool session_visibility_shrank =
            visible_after_session_poll < visible_before_session_poll;
        if (sessions.new_message) {
            eidolon_app_set_state(app, EIDOLON_STATE_REVIEW);
            eidolon_portrait_set_attention(
                app->portrait, session_attention_direction(app, sessions.message_session));
            prepare_dialogue_performance(app, &sessions.message_session->dialogue, app->state,
                                         session_attention_direction(app, sessions.message_session),
                                         sessions.message_session->id, now_ms);
        }
        if (sessions.page_advanced && sessions.advanced_session != NULL) {
            eidolon_portrait_set_attention(
                app->portrait, session_attention_direction(app, sessions.advanced_session));
            activate_dialogue_performance(
                app, &sessions.advanced_session->dialogue,
                session_attention_direction(app, sessions.advanced_session), now_ms);
        }
        bool visible_session_presenting = false;
        for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
            EidolonSessionEntry *entry = &app->session_registry.entries[index];
            if (entry->visible) {
                visible_session_presenting = visible_session_presenting || entry->streaming ||
                                             eidolon_dialogue_has_unread(&entry->dialogue);
                activate_dialogue_performance(app, &entry->dialogue,
                                              session_attention_direction(app, entry), now_ms);
                activate_dialogue_delivery(app, &entry->dialogue,
                                           session_attention_direction(app, entry), now_ms);
            }
        }
        if (visible_after_session_poll > 0U && !visible_session_presenting) {
            eidolon_portrait_set_attention(app->portrait, 0.0F);
        }
        if (sessions.changed || conversation_visibility_grew) {
            if (!session_visibility_shrank || conversation_visibility_grew) {
                eidolon_app_set_model_scale(app, app->model_scale);
            } else {
                eidolon_log_write(
                    "layout",
                    "bubble removal retained canvas visible=%zu->%zu to preserve presentation",
                    visible_before_session_poll, visible_after_session_poll);
            }
            app->hit_test_initialized = false;
        }
        eidolon_animation_update(&app->animation, app->state, now_ms);
        if (eidolon_session_registry_visible_count(&app->session_registry) == 0U) {
            eidolon_dialogue_update(&app->dialogue, now_ms);
            if (eidolon_dialogue_autoplay(&app->dialogue, now_ms)) {
                activate_dialogue_performance(app, &app->dialogue, -1.0F, now_ms);
            }
            activate_dialogue_performance(app, &app->dialogue, -1.0F, now_ms);
            activate_dialogue_delivery(app, &app->dialogue, -1.0F, now_ms);
        }
        service_expression_director(app, now_ms);
        eidolon_affect_controller_update(&app->affect, delta_seconds, now_ms);
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
        if (frame_clock_software_paced) {
            eidolon_frame_clock_finish(&frame_clock, SDL_GetTicksNS());
        }
    }
}

void eidolon_app_destroy(EidolonApp *app) {
    end_model_rotation_drag(app);
    flush_user_settings(app, true);
    eidolon_event_pump_destroy(app->event_pump);
    eidolon_settings_ui_destroy(app->settings_ui);
    eidolon_affect_client_destroy(app->affect_client);
    eidolon_conversation_sources_destroy(app->conversation_sources);
    eidolon_session_registry_destroy(&app->session_registry);
    if (!app->snapshot_mode) {
        eidolon_ipc_server_destroy(&app->ipc);
    }
    eidolon_model_destroy(app->model);
    eidolon_portrait_destroy(app->portrait);
    eidolon_text_renderer_destroy(app->text_renderer);
    SDL_DestroyTexture(app->atlas);
    eidolon_presentation_destroy(app->presentation);
    SDL_Quit();
    SDL_zero(*app);
}
