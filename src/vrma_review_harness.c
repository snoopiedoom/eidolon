#include "vrma_review_harness.h"

#include "draw.h"
#include "vrma_review_selection.h"

#include <stdio.h>

typedef struct ReviewCameraInput {
    bool rotating;
    bool rolling;
} ReviewCameraInput;

static bool handle_camera_event(EidolonApp *app, const SDL_Event *event, ReviewCameraInput *input) {
    if (app == NULL || event == NULL || input == NULL ||
        SDL_GetWindowFromEvent(event) != app->window) {
        return false;
    }
    switch (event->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button != SDL_BUTTON_MIDDLE) {
            return false;
        }
        if (event->button.clicks >= 2U) {
            input->rotating = false;
            input->rolling = false;
            SDL_CaptureMouse(false);
            eidolon_app_set_model_rotation(app, 0.0F, 0.0F, 0.0F);
            eidolon_app_set_model_scale(app, 1.0F);
        } else {
            input->rotating = true;
            input->rolling = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            if (!SDL_CaptureMouse(true)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Could not capture VRMA review camera drag: %s", SDL_GetError());
            }
        }
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button != SDL_BUTTON_MIDDLE) {
            return false;
        }
        input->rotating = false;
        input->rolling = false;
        SDL_CaptureMouse(false);
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        if (!input->rotating) {
            return false;
        }
        if (input->rolling) {
            eidolon_app_set_model_rotation(app, app->model_yaw_degrees, app->model_pitch_degrees,
                                           app->model_roll_degrees + event->motion.xrel * 0.35F);
        } else {
            eidolon_app_set_model_rotation(app, app->model_yaw_degrees + event->motion.xrel * 0.35F,
                                           app->model_pitch_degrees + event->motion.yrel * 0.35F,
                                           app->model_roll_degrees);
        }
        return true;
    case SDL_EVENT_MOUSE_WHEEL: {
        const float steps =
            event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event->wheel.y : event->wheel.y;
        eidolon_app_adjust_model_scale(app, steps);
        return true;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        input->rotating = false;
        input->rolling = false;
        SDL_CaptureMouse(false);
        return true;
    default:
        return false;
    }
}

static bool pause_and_report(EidolonApp *app, uint64_t now_ms, EidolonVrmPlaybackReport *playback) {
    if (playback->state == EIDOLON_VRM_PLAYBACK_PLAYING &&
        !eidolon_model_vrm_animation_pause(app->model, now_ms)) {
        return false;
    }
    eidolon_model_update(app->model, now_ms);
    return eidolon_model_vrm_animation_report(app->model, playback) ||
           SDL_SetError("VRMA review lost playback diagnostics after pausing");
}

static bool seek_and_report(EidolonApp *app, const EidolonVrmaReviewSelection *selection,
                            EidolonVrmPlaybackReport *playback, float delta_seconds,
                            uint64_t now_ms) {
    if (!pause_and_report(app, now_ms, playback)) {
        return false;
    }
    const float target =
        eidolon_vrma_review_selection_seek(selection, playback->position_seconds, delta_seconds);
    if (!eidolon_model_vrm_animation_seek(app->model, target, now_ms)) {
        return false;
    }
    eidolon_model_update(app->model, now_ms);
    return eidolon_model_vrm_animation_report(app->model, playback) ||
           SDL_SetError("VRMA review lost playback diagnostics after seeking");
}

static bool write_selection(const char *path, const EidolonVrmaReviewSelection *selection) {
    float start_seconds = 0.0F;
    float end_seconds = 0.0F;
    if (path == NULL || path[0] == '\0' ||
        !eidolon_vrma_review_selection_range(selection, &start_seconds, &end_seconds)) {
        return SDL_SetError("VRMA review cannot publish an incomplete selection");
    }
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        return SDL_SetError("VRMA review could not open selection output '%s'", path);
    }
    const uint64_t duration_ms =
        eidolon_vrma_review_selection_milliseconds(selection->duration_seconds);
    const uint64_t start_ms = eidolon_vrma_review_selection_milliseconds(start_seconds);
    const uint64_t end_ms = eidolon_vrma_review_selection_milliseconds(end_seconds);
    const int written = fprintf(output,
                                "eidolon_vrma_review_selection=1\n"
                                "clip_duration_milliseconds=%llu\n"
                                "start_milliseconds=%llu\n"
                                "end_milliseconds=%llu\n",
                                (unsigned long long)duration_ms, (unsigned long long)start_ms,
                                (unsigned long long)end_ms);
    const int closed = fclose(output);
    if (written < 0 || closed != 0) {
        return SDL_SetError("VRMA review could not write selection output '%s'", path);
    }
    SDL_Log("VRMA review selection saved path=%s start=%llums end=%llums duration=%llums", path,
            (unsigned long long)start_ms, (unsigned long long)end_ms,
            (unsigned long long)(end_ms - start_ms));
    return true;
}

static bool handle_key(EidolonApp *app, const SDL_KeyboardEvent *key,
                       EidolonVrmaReviewSelection *selection, EidolonVrmPlaybackReport *playback,
                       bool *selection_loop, bool *accepted, bool *running,
                       const char *selection_output_path) {
    const uint64_t now_ms = SDL_GetTicks();
    const bool coarse = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    if (key->key == SDLK_ESCAPE) {
        *running = false;
        return true;
    }
    if (key->key == SDLK_SPACE) {
        if (playback->state == EIDOLON_VRM_PLAYBACK_PLAYING) {
            if (!pause_and_report(app, now_ms, playback)) {
                return false;
            }
            SDL_Log("VRMA review paused at %llums",
                    (unsigned long long)eidolon_vrma_review_selection_milliseconds(
                        playback->position_seconds));
        } else if (!eidolon_model_vrm_animation_play(app->model, now_ms)) {
            return false;
        }
        return true;
    }
    if (key->key == SDLK_LEFT || key->key == SDLK_RIGHT) {
        const float direction = key->key == SDLK_LEFT ? -1.0F : 1.0F;
        *selection_loop = false;
        if (!eidolon_model_vrm_animation_set_loop(app->model, true) ||
            !seek_and_report(app, selection, playback, direction * (coarse ? 1.0F : 0.25F),
                             now_ms)) {
            return false;
        }
        SDL_Log("VRMA review position %llums",
                (unsigned long long)eidolon_vrma_review_selection_milliseconds(
                    playback->position_seconds));
        return true;
    }
    if (key->key == SDLK_I || key->key == SDLK_O) {
        *selection_loop = false;
        if (!eidolon_model_vrm_animation_set_loop(app->model, true) ||
            !pause_and_report(app, now_ms, playback)) {
            return false;
        }
        if (key->key == SDLK_I) {
            (void)eidolon_vrma_review_selection_mark_start(selection, playback->position_seconds);
            SDL_Log("VRMA review in mark %llums",
                    (unsigned long long)eidolon_vrma_review_selection_milliseconds(
                        selection->start_seconds));
        } else if (eidolon_vrma_review_selection_mark_end(selection, playback->position_seconds)) {
            SDL_Log("VRMA review out mark %llums",
                    (unsigned long long)eidolon_vrma_review_selection_milliseconds(
                        selection->end_seconds));
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Set I before O, with O later than I");
        }
        return true;
    }
    if (key->key == SDLK_R) {
        float start_seconds = 0.0F;
        float end_seconds = 0.0F;
        if (!eidolon_vrma_review_selection_range(selection, &start_seconds, &end_seconds)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Set valid I and O marks before replaying a selection");
            return true;
        }
        if (!eidolon_model_vrm_animation_set_loop(app->model, false) ||
            !eidolon_model_vrm_animation_seek(app->model, start_seconds, now_ms) ||
            !eidolon_model_vrm_animation_play(app->model, now_ms)) {
            return false;
        }
        *selection_loop = true;
        SDL_Log("VRMA review looping selection %llums..%llums",
                (unsigned long long)eidolon_vrma_review_selection_milliseconds(start_seconds),
                (unsigned long long)eidolon_vrma_review_selection_milliseconds(end_seconds));
        return true;
    }
    if (key->key == SDLK_RETURN) {
        if (!pause_and_report(app, now_ms, playback) ||
            !write_selection(selection_output_path, selection)) {
            return false;
        }
        *accepted = true;
        *running = false;
    }
    return true;
}

bool eidolon_vrma_review_harness_run(EidolonApp *app,
                                     const EidolonVrmPlaybackReport *initial_playback,
                                     const char *selection_output_path) {
    if (app == NULL || app->model == NULL || initial_playback == NULL ||
        initial_playback->duration_seconds <= 0.0F || selection_output_path == NULL ||
        selection_output_path[0] == '\0') {
        return SDL_SetError("VRMA selection review received invalid input");
    }
    EidolonVrmPlaybackReport playback = *initial_playback;
    EidolonVrmaReviewSelection selection;
    ReviewCameraInput camera = {0};
    bool selection_loop = false;
    bool accepted = false;
    bool running = true;
    if (!eidolon_vrma_review_selection_init(&selection, playback.duration_seconds) ||
        (app->window != NULL && !SDL_ShowWindow(app->window))) {
        return false;
    }
    SDL_Log("VRMA slice review active body=%s duration=%.3fs; Space pauses, arrows scrub "
            "0.25s (Shift: 1s), I/O mark, R loops the marked range, Enter accepts, Escape "
            "cancels; middle-drag rotates and the wheel zooms",
            eidolon_model_body_name(app->model), playback.duration_seconds);
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (handle_camera_event(app, &event, &camera)) {
                continue;
            }
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       SDL_GetWindowFromEvent(&event) == app->window &&
                       !handle_key(app, &event.key, &selection, &playback, &selection_loop,
                                   &accepted, &running, selection_output_path)) {
                SDL_CaptureMouse(false);
                return false;
            }
        }
        eidolon_app_pump_presentation_events(app);
        running = running && app->running;
        const uint64_t now_ms = SDL_GetTicks();
        eidolon_model_update(app->model, now_ms);
        if (!eidolon_model_vrm_animation_report(app->model, &playback) ||
            playback.state == EIDOLON_VRM_PLAYBACK_FAILED) {
            SDL_CaptureMouse(false);
            return SDL_SetError("VRMA review playback failed: %s", playback.failure);
        }
        float selection_start = 0.0F;
        float selection_end = 0.0F;
        if (selection_loop &&
            eidolon_vrma_review_selection_range(&selection, &selection_start, &selection_end) &&
            playback.position_seconds >= selection_end) {
            if (!eidolon_model_vrm_animation_seek(app->model, selection_start, now_ms) ||
                !eidolon_model_vrm_animation_play(app->model, now_ms)) {
                SDL_CaptureMouse(false);
                return false;
            }
        }
        if (running && !eidolon_draw_frame(app)) {
            SDL_CaptureMouse(false);
            return SDL_SetError("VRMA review could not present a native frame");
        }
        SDL_Delay(8U);
    }
    SDL_CaptureMouse(false);
    if (!accepted) {
        return SDL_SetError("VRMA review closed without accepting an I/O selection");
    }
    return true;
}
