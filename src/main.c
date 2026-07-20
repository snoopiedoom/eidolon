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
           strcmp(argv[1], "--snapshot-sessions") == 0 || strcmp(argv[1], "--snapshot-face") == 0 ||
           strcmp(argv[1], "--snapshot-settings") == 0 ||
           strcmp(argv[1], "--snapshot-portrait-motion") == 0;
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
