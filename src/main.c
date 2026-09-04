#include "app.h"
#include "draw.h"
#include "hook_output.h"
#include "log.h"
#include "platform/ipc.h"
#include "settings_ui.h"
#include "state.h"
#include "vrma_review_harness.h"

#include <stdlib.h>
#include <string.h>

typedef struct CalibrationCameraInput {
    bool rotating;
    bool rolling;
} CalibrationCameraInput;

static bool handle_calibration_camera_event(EidolonApp *app, const SDL_Event *event,
                                            CalibrationCameraInput *input);

static bool wait_for_pose_frame(EidolonApp *app, uint64_t previous_revision) {
    const uint64_t started = SDL_GetTicks();
    for (unsigned int attempt = 0; attempt < 100U; ++attempt) {
        eidolon_model_update(app->model, started + (uint64_t)(attempt + 1U) * 34U);
        if (eidolon_model_presented_transform_revision(app->model) > previous_revision) {
            return true;
        }
        SDL_Delay(2);
    }
    SDL_SetError("timed out waiting for semantic pose frame");
    return false;
}

static bool parse_pose_index(const char *text, int *pose_index) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 ||
        (size_t)parsed >= eidolon_semantic_pose_count()) {
        return false;
    }
    *pose_index = (int)parsed;
    return true;
}

static bool parse_resolution(const char *text, int *resolution) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < EIDOLON_MODEL_RENDER_RESOLUTION_MIN ||
        parsed > EIDOLON_MODEL_RENDER_RESOLUTION_MAX) {
        return false;
    }
    *resolution = (int)parsed;
    return true;
}

static bool parse_performance_tick(const char *text, uint64_t *tick) {
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, 10);
    if (end == text || *end != '\0' || parsed > 5000ULL || parsed % 20ULL != 0ULL) {
        return false;
    }
    *tick = (uint64_t)parsed;
    return true;
}

static bool parse_portrait_motion(const char *expression_text, const char *elapsed_text,
                                  size_t expression_count, int *expression,
                                  unsigned int *elapsed_ms) {
    char *expression_end = NULL;
    char *elapsed_end = NULL;
    const long parsed_expression = strtol(expression_text, &expression_end, 10);
    const unsigned long parsed_elapsed = strtoul(elapsed_text, &elapsed_end, 10);
    if (expression_end == expression_text || *expression_end != '\0' ||
        elapsed_end == elapsed_text || *elapsed_end != '\0' || parsed_expression < 0 ||
        (size_t)parsed_expression >= expression_count || parsed_elapsed > 2000UL) {
        return false;
    }
    *expression = (int)parsed_expression;
    *elapsed_ms = (unsigned int)parsed_elapsed;
    return true;
}

static bool is_snapshot_command(int argc, char **argv) {
    if (argc < 2) {
        return false;
    }
    return strcmp(argv[1], "--snapshot") == 0 || strcmp(argv[1], "--snapshot-dialogue") == 0 ||
           strcmp(argv[1], "--snapshot-pose") == 0 ||
           strcmp(argv[1], "--snapshot-resolution") == 0 ||
           strcmp(argv[1], "--snapshot-performance") == 0 ||
           strcmp(argv[1], "--vrm-runtime-check") == 0 ||
           strcmp(argv[1], "--vrm-animation-runtime-check") == 0 ||
           strcmp(argv[1], "--snapshot-sessions") == 0 || strcmp(argv[1], "--snapshot-face") == 0 ||
           strcmp(argv[1], "--snapshot-settings") == 0 ||
           strcmp(argv[1], "--snapshot-portrait-motion") == 0;
}

static bool is_authoring_command(int argc, char **argv) {
    return argc >= 2 && (strcmp(argv[1], "--review-vrm-animation") == 0 ||
                         strcmp(argv[1], "--review-performance") == 0 ||
                         strcmp(argv[1], "--calibrate-vrm") == 0);
}

static void log_usage(void) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Usage: eidolon [--snapshot <output.png>] "
                 "[--snapshot-dialogue <output.png> <text>] "
                 "[--snapshot-face <output.png>] "
                 "[--snapshot-settings <output.png>] "
                 "[--snapshot-sessions <output.png>] "
                 "[--snapshot-portrait-motion <expression> <elapsed-ms> <output.png>] "
                 "[--snapshot-pose <index> <output.png>] "
                 "[--snapshot-resolution <side> <output.png>] "
                 "[--snapshot-performance <logical-ms> <output.png>] "
                 "[--calibrate-vrm <model.vrm>] "
                 "[--review-performance <model.vrm>] "
                 "[--review-vrm-animation <model.vrm> <motion.vrma> [selection-output]] "
                 "[--vrm-runtime-check <model.vrm>] "
                 "[--vrm-animation-runtime-check <model.vrm> <motion.vrma>] [--hook <state>]");
}

static bool performance_trace_acceptance_clean(const EidolonPerformanceRuntime *runtime,
                                               uint64_t minimum_sequence, bool require_no_drops) {
    bool projection_committed = false;
    if (runtime == NULL) {
        return SDL_SetError("EPR acceptance trace is unavailable");
    }
    if (require_no_drops && runtime->trace.dropped != 0U) {
        return SDL_SetError("EPR acceptance trace dropped %llu records",
                            (unsigned long long)runtime->trace.dropped);
    }
    for (size_t index = 0U; index < runtime->trace.count; ++index) {
        const EidolonEprTraceRecord *record = eidolon_epr_trace_record(&runtime->trace, index);
        if (record == NULL || record->sequence < minimum_sequence) {
            continue;
        }
        if (record->event == EIDOLON_EPR_TRACE_PROJECTION_COMMITTED) {
            projection_committed = true;
        }
        if (record->event == EIDOLON_EPR_TRACE_REALIZER_FALLBACK ||
            record->event == EIDOLON_EPR_TRACE_REALIZER_FAILED ||
            record->event == EIDOLON_EPR_TRACE_SOLVE_REJECTED ||
            record->event == EIDOLON_EPR_TRACE_PROJECTION_REJECTED) {
            return SDL_SetError("EPR acceptance trace contains %s at tick %lld",
                                eidolon_epr_trace_event_name(record->event),
                                (long long)record->tick);
        }
    }
    if (!projection_committed) {
        return SDL_SetError("EPR acceptance trace contains no committed projection");
    }
    return true;
}

static uint32_t required_vrma_review_chains(void) {
    return EIDOLON_VRMA_CHAIN_ROOT | EIDOLON_VRMA_CHAIN_TORSO | EIDOLON_VRMA_CHAIN_HEAD |
           EIDOLON_VRMA_CHAIN_LEFT_ARM | EIDOLON_VRMA_CHAIN_RIGHT_ARM |
           EIDOLON_VRMA_CHAIN_LEFT_LEG | EIDOLON_VRMA_CHAIN_RIGHT_LEG;
}

static bool prepare_vrm_animation(EidolonApp *app, const char *motion_path, uint64_t now_ms,
                                  EidolonVrmPlaybackReport *playback) {
    EidolonVrmMeasurements measurements;
    EidolonVrmCalibration empty_calibration;
    if (app == NULL || motion_path == NULL || motion_path[0] == '\0' || playback == NULL) {
        return SDL_SetError("VRM animation preparation received invalid input");
    }
    if (!eidolon_app_set_render_mode(app, EIDOLON_RENDER_MODE_MODEL_3D) || app->model == NULL ||
        !eidolon_model_vrm_measurements(app->model, &measurements)) {
        return SDL_SetError("VRM animation could not activate the requested body");
    }
    eidolon_vrm_calibration_init(&empty_calibration, measurements.anatomy_fingerprint);
    if (!eidolon_model_set_vrm_calibration(app->model, &empty_calibration)) {
        return false;
    }
    if (!eidolon_model_vrm_animation_load(app->model, motion_path, motion_path, true, now_ms) ||
        !eidolon_model_vrm_animation_report(app->model, playback)) {
        return false;
    }
    const uint32_t required_chains = required_vrma_review_chains();
    if (playback->duration_seconds <= 0.0F ||
        (playback->coverage.varying_chain_mask & required_chains) != required_chains) {
        return SDL_SetError(
            "VRMA fixture lacks varying full-body coverage: got 0x%02x, need 0x%02x",
            (unsigned int)playback->coverage.varying_chain_mask, (unsigned int)required_chains);
    }
    return true;
}

static bool run_vrm_animation_runtime_check(EidolonApp *app, const char *motion_path) {
    const uint64_t duration_ms = 5000U;
    const uint64_t tick_ms = 20U;
    const uint64_t expected_samples = duration_ms / tick_ms + 1U;
    EidolonVrmPlaybackReport playback;
    EidolonVrmRuntimeReport initial_runtime;
    EidolonVrmRuntimeReport runtime;

    if (!prepare_vrm_animation(app, motion_path, 0U, &playback)) {
        return false;
    }

    (void)eidolon_model_vrm_runtime_report(app->model, &initial_runtime);
    SDL_ClearError();
    const uint64_t previous_transform = eidolon_model_presented_transform_revision(app->model);
    for (uint64_t tick = 0U; tick <= duration_ms; tick += tick_ms) {
        if (!eidolon_model_update_motion(app->model, tick)) {
            return false;
        }
        if (tick == 0U || tick == duration_ms) {
            eidolon_model_update(app->model, tick);
            if (!eidolon_draw_frame(app)) {
                return SDL_SetError("VRM animation runtime check could not draw tick %llu",
                                    (unsigned long long)tick);
            }
        }
    }

    (void)eidolon_model_vrm_runtime_report(app->model, &runtime);
    SDL_ClearError();
    if (!eidolon_model_vrm_animation_report(app->model, &playback)) {
        return SDL_SetError("VRM animation runtime check lost playback diagnostics");
    }
    if (!runtime.geometry_ready || !runtime.textures_ready || !runtime.skinning_ready ||
        !runtime.shaders_ready || !runtime.animation_ready || !runtime.hidden_frame_ready) {
        return SDL_SetError("VRM animation runtime incomplete geometry=%s textures=%s "
                            "skinning=%s shaders=%s animation=%s hidden-frame=%s",
                            runtime.geometry_ready ? "ready" : "failed",
                            runtime.textures_ready ? "ready" : "failed",
                            runtime.skinning_ready ? "ready" : "failed",
                            runtime.shaders_ready ? "ready" : "failed",
                            runtime.animation_ready ? "ready" : "failed",
                            runtime.hidden_frame_ready ? "ready" : "failed");
    }
    if (playback.state != EIDOLON_VRM_PLAYBACK_PLAYING ||
        playback.sample_revision < expected_samples ||
        playback.published_revision != playback.sample_revision ||
        runtime.playback_revision != playback.published_revision ||
        runtime.base_revision < initial_runtime.base_revision + expected_samples ||
        runtime.frame_sequence <= initial_runtime.frame_sequence ||
        eidolon_model_presented_transform_revision(app->model) <= previous_transform) {
        return SDL_SetError("VRM animation transaction did not publish every sample/frame "
                            "samples=%llu published=%llu base=%llu->%llu frame=%llu->%llu",
                            (unsigned long long)playback.sample_revision,
                            (unsigned long long)playback.published_revision,
                            (unsigned long long)initial_runtime.base_revision,
                            (unsigned long long)runtime.base_revision,
                            (unsigned long long)initial_runtime.frame_sequence,
                            (unsigned long long)runtime.frame_sequence);
    }
    SDL_Log("vrm-animation-runtime-check passed body=%s duration=%.3fs tracks=%zu "
            "varying=%zu chains=0x%02x samples=%llu base=revision:%llu "
            "hidden-gpu-frame=sequence:%llu sidecar=disabled",
            eidolon_model_body_name(app->model), playback.duration_seconds,
            playback.coverage.rotation_track_count, playback.coverage.varying_rotation_track_count,
            (unsigned int)playback.coverage.varying_chain_mask,
            (unsigned long long)playback.sample_revision, (unsigned long long)runtime.base_revision,
            (unsigned long long)runtime.frame_sequence);
    return true;
}

static bool run_vrm_animation_review(EidolonApp *app, const char *motion_path) {
    EidolonVrmPlaybackReport playback;
    CalibrationCameraInput camera_input = {0};
    const uint64_t started_ms = SDL_GetTicks();
    bool completed_loop = false;
    bool running = true;
    if (!prepare_vrm_animation(app, motion_path, started_ms, &playback) ||
        (app->window != NULL && !SDL_ShowWindow(app->window))) {
        return false;
    }
    const uint64_t loop_duration_ms = (uint64_t)SDL_ceilf(playback.duration_seconds * 1000.0F);
    SDL_Log("VRMA review active body=%s duration=%.3fs tracks=%zu varying=%zu chains=0x%02x; "
            "middle-drag rotates, Shift+middle rolls, wheel resizes, double-middle resets; "
            "close or press Escape after at least one full loop",
            eidolon_model_body_name(app->model), playback.duration_seconds,
            playback.coverage.rotation_track_count, playback.coverage.varying_rotation_track_count,
            (unsigned int)playback.coverage.varying_chain_mask);
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (handle_calibration_camera_event(app, &event, &camera_input)) {
                continue;
            }
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }
        eidolon_app_pump_presentation_events(app);
        running = running && app->running;
        const uint64_t now_ms = SDL_GetTicks();
        eidolon_model_update(app->model, now_ms);
        if (!eidolon_model_vrm_animation_report(app->model, &playback) ||
            playback.state == EIDOLON_VRM_PLAYBACK_FAILED) {
            return SDL_SetError("VRMA review playback failed: %s", playback.failure);
        }
        if (running && !eidolon_draw_frame(app)) {
            return SDL_SetError("VRMA review could not present a native frame");
        }
        if (!completed_loop && now_ms - started_ms >= loop_duration_ms) {
            completed_loop = true;
            SDL_Log("VRMA review completed one full loop; close when inspection is finished");
        }
        SDL_Delay(8U);
    }
    SDL_CaptureMouse(false);
    if (!completed_loop) {
        return SDL_SetError("VRMA review closed before one complete animation loop");
    }
    return true;
}

static bool run_performance_review(EidolonApp *app) {
    const uint64_t pre_roll_ms = 1000U;
    const uint64_t duration_ms = 5000U;
    const uint64_t hold_ms = 1000U;
    uint64_t fixture_clock_ms = 0U;
    uint64_t next_tick = 0U;
    uint64_t cycle_started;
    unsigned int completed_passes = 0U;
    bool pass_complete = false;
    bool running = true;
    if (!eidolon_app_vrm_performance_acceptance_ready(app) ||
        !eidolon_app_set_render_mode(app, EIDOLON_RENDER_MODE_MODEL_3D) ||
        (app->window != NULL && !SDL_ShowWindow(app->window))) {
        return false;
    }
    uint64_t cycle_trace_sequence = app->performance_runtime.trace.next_sequence;
    SDL_Log("EPR performance review verified all calibrated anchors and repeats until closed; "
            "press Escape after a complete pass");
    cycle_started = SDL_GetTicks();
    while (running) {
        const uint64_t now = SDL_GetTicks();
        const uint64_t elapsed = now - cycle_started;
        const uint64_t logical_elapsed =
            elapsed <= pre_roll_ms ? 0U : SDL_min(elapsed - pre_roll_ms, duration_ms);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }
        eidolon_app_pump_presentation_events(app);
        running = running && app->running;
        while (running && next_tick <= logical_elapsed && next_tick <= duration_ms) {
            if (!eidolon_app_update_performance_fixture(app, fixture_clock_ms + next_tick)) {
                return false;
            }
            next_tick += 20U;
        }
        if (!pass_complete && next_tick > duration_ms) {
            if (!performance_trace_acceptance_clean(&app->performance_runtime, cycle_trace_sequence,
                                                    false)) {
                return false;
            }
            pass_complete = true;
            completed_passes += 1U;
            SDL_Log("EPR performance review completed pass %u", completed_passes);
        }
        eidolon_model_update(app->model, now);
        if (running && !eidolon_draw_frame(app)) {
            return false;
        }
        if (running && elapsed >= pre_roll_ms + duration_ms + hold_ms) {
            fixture_clock_ms += duration_ms + 20U;
            if (!eidolon_app_restart_performance_fixture(app, fixture_clock_ms)) {
                return false;
            }
            cycle_trace_sequence = app->performance_runtime.trace.next_sequence;
            next_tick = 0U;
            pass_complete = false;
            cycle_started = SDL_GetTicks();
        }
        SDL_Delay(8U);
    }
    if (completed_passes == 0U) {
        SDL_SetError("performance review closed before one complete five-second pass");
        return false;
    }
    return true;
}

static bool handle_calibration_camera_event(EidolonApp *app, const SDL_Event *event,
                                            CalibrationCameraInput *input) {
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
                            "Could not capture calibration camera drag: %s", SDL_GetError());
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

static bool run_vrm_calibration(EidolonApp *app, const char *sidecar_path) {
    if (!eidolon_app_set_render_mode(app, EIDOLON_RENDER_MODE_MODEL_3D) ||
        !eidolon_app_begin_vrm_calibration(app, sidecar_path) ||
        (app->window != NULL && !SDL_ShowWindow(app->window))) {
        return false;
    }
    app->settings_ui = eidolon_settings_ui_create(EIDOLON_FONT_PATH);
    if (app->settings_ui == NULL) {
        return SDL_SetError("could not create VRM calibration settings window");
    }
    eidolon_settings_ui_open(app->settings_ui);
    SDL_Log("VRM calibration active; adjust the frozen anchor in Settings, accept it, and save. "
            "Middle-drag rotates, Shift+middle rolls, the wheel zooms, double-middle resets, F1 "
            "reopens Settings, and Escape in the body window exits.");
    bool running = true;
    CalibrationCameraInput camera_input = {0};
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (eidolon_settings_ui_handle_event(app->settings_ui, &event)) {
                continue;
            }
            if (handle_calibration_camera_event(app, &event, &camera_input)) {
                continue;
            }
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                 SDL_GetWindowFromEvent(&event) == app->window)) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       SDL_GetWindowFromEvent(&event) == app->window) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_F1) {
                    eidolon_settings_ui_open(app->settings_ui);
                }
            }
        }
        eidolon_app_pump_presentation_events(app);
        running = running && app->running;
        const uint64_t now = SDL_GetTicks();
        eidolon_model_update(app->model, now);
        if (running && !eidolon_draw_frame(app)) {
            return false;
        }
        eidolon_settings_ui_draw(app->settings_ui, app);
        SDL_Delay(8U);
    }
    SDL_CaptureMouse(false);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--hook") == 0) {
        EidolonState state;
        if (!eidolon_state_parse(argv[2], &state)) {
            eidolon_log_write("hook", "rejected unknown state: %s", argv[2]);
            return 2;
        }
        eidolon_log_write("hook", "invoked state=%s", eidolon_state_name(state));
        char agent_output[EIDOLON_IPC_TEXT_CAPACITY + 1] = {0};
        if (state == EIDOLON_STATE_REVIEW) {
            (void)eidolon_hook_read_agent_output(stdin, agent_output, sizeof(agent_output));
        }
        const bool sent = eidolon_ipc_send(state, agent_output);
        eidolon_log_write("hook", "ipc send state=%s bytes=%zu success=%s",
                          eidolon_state_name(state), strlen(agent_output), sent ? "yes" : "no");
        return 0;
    }

    const bool snapshot_mode = is_snapshot_command(argc, argv);
    const bool authoring_mode = is_authoring_command(argc, argv);
    if (argc != 1 && !snapshot_mode && !authoring_mode) {
        log_usage();
        return 2;
    }
    char calibration_sidecar[EIDOLON_VRM_CALIBRATION_PATH_CAPACITY] = {0};
    if (argc == 3 && strcmp(argv[1], "--calibrate-vrm") == 0) {
        const char *configured = SDL_getenv("EIDOLON_VRM_CALIBRATION_PATH");
        if (configured != NULL && configured[0] != '\0') {
            SDL_strlcpy(calibration_sidecar, configured, sizeof(calibration_sidecar));
        } else {
            const int written = SDL_snprintf(calibration_sidecar, sizeof(calibration_sidecar),
                                             "%s.epr-calibration", argv[2]);
            if (written <= 0 || (size_t)written >= sizeof(calibration_sidecar) ||
                SDL_setenv_unsafe("EIDOLON_VRM_CALIBRATION_PATH", calibration_sidecar, 1) != 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Could not configure VRM calibration sidecar path");
                return 1;
            }
        }
    }
    if (argc == 3 &&
        (strcmp(argv[1], "--vrm-runtime-check") == 0 ||
         strcmp(argv[1], "--review-performance") == 0 || strcmp(argv[1], "--calibrate-vrm") == 0) &&
        SDL_setenv_unsafe("EIDOLON_VRM_PATH", argv[2], 1) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not configure VRM runtime path");
        return 1;
    }
    const bool vrm_animation_review =
        (argc == 4 || argc == 5) && strcmp(argv[1], "--review-vrm-animation") == 0;
    if (((argc == 4 && strcmp(argv[1], "--vrm-animation-runtime-check") == 0) ||
         vrm_animation_review) &&
        SDL_setenv_unsafe("EIDOLON_VRM_PATH", argv[2], 1) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not configure VRM animation body path");
        return 1;
    }
    EidolonApp app;
    eidolon_log_write("renderer", "%s process starting",
                      snapshot_mode    ? "snapshot"
                      : authoring_mode ? "authoring"
                                       : "interactive");
    const EidolonAppMode app_mode = snapshot_mode    ? EIDOLON_APP_SNAPSHOT
                                    : authoring_mode ? EIDOLON_APP_AUTHORING
                                                     : EIDOLON_APP_INTERACTIVE;
    if (!eidolon_app_init(&app, app_mode)) {
        eidolon_log_write("renderer", "initialization failed: %s", SDL_GetError());
        eidolon_app_destroy(&app);
        return 1;
    }

    if (argc == 3 && strcmp(argv[1], "--snapshot") == 0) {
        eidolon_app_set_state(&app, EIDOLON_STATE_WAITING);
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--snapshot-face") == 0) {
        eidolon_portrait_set_face_mode(app.portrait, true);
        eidolon_app_set_model_scale(&app, app.model_scale);
        eidolon_app_set_state(&app, EIDOLON_STATE_WAITING);
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--snapshot-settings") == 0) {
        app.settings_ui = eidolon_settings_ui_create(EIDOLON_FONT_PATH);
        const bool saved =
            app.settings_ui != NULL && eidolon_settings_ui_snapshot(app.settings_ui, &app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--snapshot-dialogue") == 0) {
        eidolon_app_set_state(&app, EIDOLON_STATE_REVIEW);
        eidolon_dialogue_set(&app.dialogue, argv[3], SDL_GetTicks());
        eidolon_dialogue_configure(&app.dialogue, app.dialogue_movement, app.dialogue_hold_ms);
        eidolon_dialogue_advance(&app.dialogue, SDL_GetTicks());
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--snapshot-sessions") == 0) {
        static const char *const titles[4] = {"eidolon", "Fix authentication tests",
                                              "Review shader pipeline", "Plan release notes"};
        static const char *const messages[4] = {
            "multiple session registry is alive. each bubble owns its own dialogue state.",
            "three tests remain. the transaction boundary is the suspicious part.",
            "the texture coordinates are correct now; material alpha still needs review.",
            "release notes drafted. this bubble scrolls its own dialogue independently.",
        };
        for (int slot = 0; slot < 4; ++slot) {
            EidolonSessionEntry *entry = &app.session_registry.entries[slot];
            entry->occupied = true;
            entry->visible = true;
            entry->layout_slot = slot;
            SDL_strlcpy(entry->title, titles[slot], sizeof(entry->title));
            eidolon_dialogue_set(&entry->dialogue, messages[slot], SDL_GetTicks());
            eidolon_dialogue_configure(&entry->dialogue, app.dialogue_movement,
                                       app.dialogue_hold_ms);
            eidolon_dialogue_advance(&entry->dialogue, SDL_GetTicks());
        }
        eidolon_app_set_model_scale(&app, app.model_scale);
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 5 && strcmp(argv[1], "--snapshot-portrait-motion") == 0) {
        int expression = 0;
        unsigned int elapsed_ms = 0U;
        if (!parse_portrait_motion(argv[2], argv[3],
                                   eidolon_portrait_expression_count(app.portrait), &expression,
                                   &elapsed_ms)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid portrait motion sample");
            eidolon_app_destroy(&app);
            return 2;
        }
        eidolon_app_select_portrait(&app, expression);
        SDL_Delay(elapsed_ms);
        const bool saved = eidolon_draw_snapshot(&app, argv[4]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--snapshot-pose") == 0) {
        int pose_index = 0;
        if (!parse_pose_index(argv[2], &pose_index)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid semantic pose index: %s", argv[2]);
            eidolon_app_destroy(&app);
            return 2;
        }
        if (!eidolon_app_set_render_mode(&app, EIDOLON_RENDER_MODE_MODEL_3D)) {
            eidolon_app_destroy(&app);
            return 1;
        }
        const uint64_t previous_revision = eidolon_model_presented_transform_revision(app.model);
        eidolon_app_select_semantic_pose(&app, pose_index);
        const bool settled = wait_for_pose_frame(&app, previous_revision);
        const bool saved = settled && eidolon_draw_snapshot(&app, argv[3]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--snapshot-resolution") == 0) {
        int resolution = 0;
        if (!parse_resolution(argv[2], &resolution)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid render resolution: %s", argv[2]);
            eidolon_app_destroy(&app);
            return 2;
        }
        if (!eidolon_app_set_render_mode(&app, EIDOLON_RENDER_MODE_MODEL_3D)) {
            eidolon_app_destroy(&app);
            return 1;
        }
        const uint64_t previous_revision = eidolon_model_presented_transform_revision(app.model);
        const bool already_selected = eidolon_model_render_resolution(app.model) == resolution;
        const bool changed = eidolon_app_set_model_render_resolution(&app, resolution);
        const bool settled =
            changed && (already_selected || wait_for_pose_frame(&app, previous_revision));
        const bool saved = settled && eidolon_draw_snapshot(&app, argv[3]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--snapshot-performance") == 0) {
        uint64_t target_tick = 0U;
        if (!parse_performance_tick(argv[2], &target_tick)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Performance tick must be a multiple of 20 from 0 through 5000");
            eidolon_app_destroy(&app);
            return 2;
        }
        if (!eidolon_app_set_render_mode(&app, EIDOLON_RENDER_MODE_MODEL_3D)) {
            eidolon_app_destroy(&app);
            return 1;
        }
        const uint64_t previous_revision = eidolon_model_presented_transform_revision(app.model);
        bool advanced = true;
        for (uint64_t tick = 0U; tick <= target_tick; tick += 20U) {
            if (!eidolon_app_update_performance_fixture(&app, tick)) {
                advanced = false;
                break;
            }
        }
        const bool settled = advanced && wait_for_pose_frame(&app, previous_revision);
        const bool saved = settled && eidolon_draw_snapshot(&app, argv[3]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--vrm-runtime-check") == 0) {
        EidolonVrmRuntimeReport report;
        bool advanced = true;
        bool ready = eidolon_app_vrm_performance_acceptance_ready(&app) &&
                     eidolon_app_set_render_mode(&app, EIDOLON_RENDER_MODE_MODEL_3D);
        const uint64_t previous_revision =
            ready ? eidolon_model_presented_transform_revision(app.model) : 0U;
        for (uint64_t tick = 0U; ready && tick <= 5000U; tick += 20U) {
            if (!eidolon_app_update_performance_fixture(&app, tick)) {
                advanced = false;
                break;
            }
        }
        ready = ready && advanced && wait_for_pose_frame(&app, previous_revision) &&
                eidolon_model_vrm_runtime_report(app.model, &report) &&
                performance_trace_acceptance_clean(&app.performance_runtime, 1U, true);
        if (ready &&
            (!app.performance_runtime.has_tick || app.performance_runtime.last_tick != 5000)) {
            ready = SDL_SetError("EPR runtime check did not reach the five-second endpoint");
        }
        if (ready && report.projection_revision != app.performance_runtime.control.revision) {
            ready = SDL_SetError("EPR runtime check ended with an unprojected control revision");
        }
        if (ready) {
            SDL_Log("vrm-runtime-check passed body=%s geometry=draws:%zu textures=%zu "
                    "skinning=joints:%zu shaders=ready projection=revision:%llu "
                    "hidden-gpu-frame=sequence:%llu calibration=anchors:0x%02x "
                    "epr-trace=records:%zu/hash:%016llx control=%016llx",
                    eidolon_model_body_name(app.model), report.draw_count, report.texture_count,
                    report.joint_count, (unsigned long long)report.projection_revision,
                    (unsigned long long)report.frame_sequence,
                    EIDOLON_VRM_CALIBRATION_COMPLETE_ANCHOR_MASK,
                    app.performance_runtime.trace.count,
                    (unsigned long long)eidolon_epr_trace_hash(&app.performance_runtime.trace),
                    (unsigned long long)app.performance_runtime.control.hash);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vrm-runtime-check failed: %s",
                         SDL_GetError());
        }
        eidolon_app_destroy(&app);
        return ready ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--vrm-animation-runtime-check") == 0) {
        const bool ready = run_vrm_animation_runtime_check(&app, argv[3]);
        if (!ready) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vrm-animation-runtime-check failed: %s",
                         SDL_GetError());
        }
        eidolon_app_destroy(&app);
        return ready ? 0 : 1;
    }

    if (vrm_animation_review) {
        bool reviewed;
        if (argc == 5) {
            EidolonVrmPlaybackReport playback;
            reviewed = prepare_vrm_animation(&app, argv[3], SDL_GetTicks(), &playback) &&
                       eidolon_vrma_review_harness_run(&app, &playback, argv[4]);
        } else {
            reviewed = run_vrm_animation_review(&app, argv[3]);
        }
        if (!reviewed) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VRMA review failed: %s", SDL_GetError());
        }
        eidolon_app_destroy(&app);
        return reviewed ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--review-performance") == 0) {
        const bool reviewed = run_performance_review(&app);
        if (!reviewed) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "performance review failed: %s",
                         SDL_GetError());
        }
        eidolon_app_destroy(&app);
        return reviewed ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--calibrate-vrm") == 0) {
        const bool calibrated = run_vrm_calibration(&app, calibration_sidecar);
        if (!calibrated) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VRM calibration failed: %s",
                         SDL_GetError());
        }
        eidolon_app_destroy(&app);
        return calibrated ? 0 : 1;
    }

    if (argc != 1) {
        log_usage();
        eidolon_app_destroy(&app);
        return 2;
    }

    eidolon_app_run(&app);
    eidolon_log_write("renderer", "event loop stopped");
    eidolon_app_destroy(&app);
    return 0;
}
