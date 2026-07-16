#include "app.h"
#include "draw.h"
#include "hook_output.h"
#include "log.h"
#include "platform/ipc.h"
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

static bool is_snapshot_command(int argc, char **argv) {
    if (argc < 2) {
        return false;
    }
    return strcmp(argv[1], "--snapshot") == 0 || strcmp(argv[1], "--snapshot-debug") == 0 ||
           strcmp(argv[1], "--snapshot-debug-resolution") == 0 ||
           strcmp(argv[1], "--snapshot-dialogue") == 0 || strcmp(argv[1], "--snapshot-pose") == 0 ||
           strcmp(argv[1], "--snapshot-debug-pose") == 0 ||
           strcmp(argv[1], "--snapshot-resolution") == 0;
}

static void log_usage(void) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Usage: eidolon [--snapshot <output.png>] [--snapshot-debug <output.png>] "
                 "[--snapshot-debug-resolution <output.png>] "
                 "[--snapshot-dialogue <output.png> <text>] "
                 "[--snapshot-pose <index> <output.png>] "
                 "[--snapshot-debug-pose <index> <output.png>] "
                 "[--snapshot-resolution <side> <output.png>] [--hook <state>]");
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
    if (argc != 1 && !snapshot_mode) {
        log_usage();
        return 2;
    }
    EidolonApp app;
    eidolon_log_write("renderer", "%s process starting",
                      snapshot_mode ? "snapshot" : "interactive");
    if (!eidolon_app_init(&app, snapshot_mode ? EIDOLON_APP_SNAPSHOT : EIDOLON_APP_INTERACTIVE)) {
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

    if (argc == 3 && strcmp(argv[1], "--snapshot-debug") == 0) {
        app.debug_visible = true;
        app.debug_pose_dropdown_open = true;
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--snapshot-debug-resolution") == 0) {
        app.debug_visible = true;
        app.debug_resolution_dropdown_open = true;
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--snapshot-dialogue") == 0) {
        eidolon_app_set_state(&app, EIDOLON_STATE_REVIEW);
        eidolon_dialogue_set(&app.dialogue, argv[3], SDL_GetTicks());
        eidolon_dialogue_advance(&app.dialogue, SDL_GetTicks());
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
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
        const uint64_t previous_revision = eidolon_model_presented_transform_revision(app.model);
        const bool already_selected = eidolon_model_render_resolution(app.model) == resolution;
        const bool changed = eidolon_app_set_model_render_resolution(&app, resolution);
        const bool settled =
            changed && (already_selected || wait_for_pose_frame(&app, previous_revision));
        const bool saved = settled && eidolon_draw_snapshot(&app, argv[3]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--snapshot-debug-pose") == 0) {
        int pose_index = 0;
        if (!parse_pose_index(argv[2], &pose_index)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid semantic pose index: %s", argv[2]);
            eidolon_app_destroy(&app);
            return 2;
        }
        const uint64_t previous_revision = eidolon_model_presented_transform_revision(app.model);
        eidolon_app_select_semantic_pose(&app, pose_index);
        app.debug_visible = true;
        const bool settled = wait_for_pose_frame(&app, previous_revision);
        const bool saved = settled && eidolon_draw_snapshot(&app, argv[3]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
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
