#ifndef EIDOLON_STATE_H
#define EIDOLON_STATE_H

#include <stdbool.h>

typedef enum EidolonState {
    EIDOLON_STATE_IDLE = 0,
    EIDOLON_STATE_RUNNING,
    EIDOLON_STATE_WAITING,
    EIDOLON_STATE_REVIEW,
    EIDOLON_STATE_FAILED,
    EIDOLON_STATE_COUNT
} EidolonState;

bool eidolon_state_parse(const char *text, EidolonState *state);
const char *eidolon_state_name(EidolonState state);

#endif
