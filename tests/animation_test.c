#include "animation.h"

#include <assert.h>

static void verify_state(EidolonState state, int expected_row, int expected_frames) {
    EidolonAnimation animation;
    eidolon_animation_set_state(&animation, state, 1000);
    assert(animation.row == expected_row);
    assert(animation.frame_count == expected_frames);
    assert(animation.frame == 0);

    const SDL_FRect source = eidolon_animation_source_rect(&animation);
    assert(source.x == 0.0F);
    assert(source.y == (float)(expected_row * EIDOLON_CELL_HEIGHT));
    assert(source.w == EIDOLON_CELL_WIDTH);
    assert(source.h == EIDOLON_CELL_HEIGHT);
}

int main(void) {
    verify_state(EIDOLON_STATE_IDLE, 0, 6);
    verify_state(EIDOLON_STATE_RUNNING, 7, 6);
    verify_state(EIDOLON_STATE_WAITING, 6, 6);
    verify_state(EIDOLON_STATE_REVIEW, 8, 6);
    verify_state(EIDOLON_STATE_FAILED, 5, 8);

    EidolonAnimation animation;
    eidolon_animation_set_state(&animation, EIDOLON_STATE_RUNNING, 500);
    eidolon_animation_update(&animation, EIDOLON_STATE_RUNNING, 619);
    assert(animation.frame == 0);
    eidolon_animation_update(&animation, EIDOLON_STATE_RUNNING, 620);
    assert(animation.frame == 1);

    return 0;
}
