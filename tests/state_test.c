#include "state.h"

#include <assert.h>

int main(void) {
    EidolonState state = EIDOLON_STATE_COUNT;
    assert(eidolon_state_parse("idle", &state));
    assert(state == EIDOLON_STATE_IDLE);
    assert(eidolon_state_parse("running", &state));
    assert(state == EIDOLON_STATE_RUNNING);
    assert(eidolon_state_parse("waiting", &state));
    assert(state == EIDOLON_STATE_WAITING);
    assert(eidolon_state_parse("review", &state));
    assert(state == EIDOLON_STATE_REVIEW);
    assert(eidolon_state_parse("failed", &state));
    assert(state == EIDOLON_STATE_FAILED);
    assert(!eidolon_state_parse("unknown", &state));
    assert(!eidolon_state_parse(NULL, &state));
    assert(!eidolon_state_parse("idle", NULL));
    return 0;
}
