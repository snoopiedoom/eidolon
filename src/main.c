#include "app.h"
#include "draw.h"
#include "hook_output.h"
#include "log.h"
#include "platform/ipc.h"
#include "state.h"

#include <string.h>

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

    EidolonApp app;
    eidolon_log_write("renderer", "process starting");
    if (!eidolon_app_init(&app)) {
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

    if (argc == 4 && strcmp(argv[1], "--snapshot-dialogue") == 0) {
        eidolon_app_set_state(&app, EIDOLON_STATE_REVIEW);
        eidolon_dialogue_set(&app.dialogue, argv[3], SDL_GetTicks());
        eidolon_dialogue_advance(&app.dialogue, SDL_GetTicks());
        const bool saved = eidolon_draw_snapshot(&app, argv[2]);
        eidolon_app_destroy(&app);
        return saved ? 0 : 1;
    }

    if (argc != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Usage: eidolon [--snapshot <output.png>] [--snapshot-dialogue "
                     "<output.png> <text>] [--hook <state>]");
        eidolon_app_destroy(&app);
        return 2;
    }

    eidolon_app_run(&app);
    eidolon_log_write("renderer", "event loop stopped");
    eidolon_app_destroy(&app);
    return 0;
}
