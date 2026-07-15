#include "state.h"

#include <string.h>

bool eidolon_state_parse(const char *text, EidolonState *state) {
    static const struct {
        const char *name;
        EidolonState state;
    } STATES[] = {
        {"idle", EIDOLON_STATE_IDLE},       {"running", EIDOLON_STATE_RUNNING},
        {"waiting", EIDOLON_STATE_WAITING}, {"review", EIDOLON_STATE_REVIEW},
        {"failed", EIDOLON_STATE_FAILED},
    };

    if (text == NULL || state == NULL) {
        return false;
    }

    for (size_t index = 0; index < sizeof(STATES) / sizeof(STATES[0]); ++index) {
        if (strcmp(text, STATES[index].name) == 0) {
            *state = STATES[index].state;
            return true;
        }
    }
    return false;
}

const char *eidolon_state_name(EidolonState state) {
    static const char *const NAMES[EIDOLON_STATE_COUNT] = {
        [EIDOLON_STATE_IDLE] = "idle",       [EIDOLON_STATE_RUNNING] = "running",
        [EIDOLON_STATE_WAITING] = "waiting", [EIDOLON_STATE_REVIEW] = "review",
        [EIDOLON_STATE_FAILED] = "failed",
    };
    return state >= 0 && state < EIDOLON_STATE_COUNT ? NAMES[state] : "unknown";
}
