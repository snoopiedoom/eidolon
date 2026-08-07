#include "app.h"
#include "draw.h"
#include "hook_output.h"
#include "log.h"
#include "platform/ipc.h"
#include "settings_ui.h"
#include "state.h"

#include <stdlib.h>
#include <string.h>

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
           strcmp(argv[1], "--snapshot-sessions") == 0 || strcmp(argv[1], "--snapshot-face") == 0 ||
           strcmp(argv[1], "--snapshot-settings") == 0 ||
           strcmp(argv[1], "--snapshot-portrait-motion") == 0;
}

static bool is_authoring_command(int argc, char **argv) {
    return argc >= 2 && (strcmp(argv[1], "--review-performance") == 0 ||
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
                  "[--vrm-runtime-check <model.vrm>] [--hook <state>]");
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
    if (!eidolon_app_set_render_mode(app, EIDOLON_RENDER_MODE_MODEL_3D) ||
        !SDL_ShowWindow(app->window)) {
        return false;
    }
    SDL_Log("EPR performance review repeats until closed; press Escape after a complete pass");
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
        while (running && next_tick <= logical_elapsed && next_tick <= duration_ms) {
            if (!eidolon_app_update_performance_fixture(app, fixture_clock_ms + next_tick)) {
                return false;
            }
            next_tick += 20U;
        }
        if (!pass_complete && next_tick > duration_ms) {
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

typedef struct CalibrationCameraInput {
    bool rotating;
    bool rolling;
} CalibrationCameraInput;

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
            eidolon_app_set_model_rotation(app, app->model_yaw_degrees,
                                           app->model_pitch_degrees,
                                           app->model_roll_degrees + event->motion.xrel * 0.35F);
        } else {
            eidolon_app_set_model_rotation(app,
                                           app->model_yaw_degrees + event->motion.xrel * 0.35F,
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
        !eidolon_app_begin_vrm_calibration(app, sidecar_path) || !SDL_ShowWindow(app->window)) {
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
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
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
         strcmp(argv[1], "--review-performance") == 0 ||
         strcmp(argv[1], "--calibrate-vrm") == 0) &&
        SDL_setenv_unsafe("EIDOLON_VRM_PATH", argv[2], 1) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not configure VRM runtime path");
        return 1;
    }
    EidolonApp app;
    eidolon_log_write("renderer", "%s process starting",
                      snapshot_mode ? "snapshot"
                                    : authoring_mode ? "authoring" : "interactive");
    const EidolonAppMode app_mode = snapshot_mode   ? EIDOLON_APP_SNAPSHOT
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
        bool ready = eidolon_app_set_render_mode(&app, EIDOLON_RENDER_MODE_MODEL_3D);
        const uint64_t previous_revision =
            ready ? eidolon_model_presented_transform_revision(app.model) : 0U;
        for (uint64_t tick = 0U; ready && tick <= 5000U; tick += 20U) {
            if (!eidolon_app_update_performance_fixture(&app, tick)) {
                advanced = false;
                break;
            }
        }
        ready = ready && advanced && wait_for_pose_frame(&app, previous_revision) &&
                eidolon_model_vrm_runtime_report(app.model, &report);
        if (ready) {
            SDL_Log("vrm-runtime-check passed body=%s geometry=draws:%zu textures=%zu "
                    "skinning=joints:%zu shaders=ready projection=revision:%llu "
                    "hidden-gpu-frame=sequence:%llu",
                    eidolon_model_body_name(app.model), report.draw_count, report.texture_count,
                    report.joint_count, (unsigned long long)report.projection_revision,
                    (unsigned long long)report.frame_sequence);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vrm-runtime-check failed: %s",
                         SDL_GetError());
        }
        eidolon_app_destroy(&app);
        return ready ? 0 : 1;
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
