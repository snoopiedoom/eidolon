#include "bubble_layout.h"

#include <assert.h>

int main(void) {
    int width = 520;
    int height = 360;
    eidolon_bubble_layout_canvas(406.0F, 560.0F, 1U, &width, &height);
    assert(width >= 819);
    assert(height >= 592);
    const SDL_FRect single_character =
        eidolon_bubble_layout_character(width, height, 406.0F, 560.0F, 1U);
    assert(single_character.x > EIDOLON_BUBBLE_WIDTH);

    eidolon_bubble_layout_canvas(406.0F, 560.0F, 4U, &width, &height);
    const SDL_FRect left = eidolon_bubble_layout_rect(width, height, 0, 4U);
    const SDL_FRect right = eidolon_bubble_layout_rect(width, height, 1, 4U);
    const SDL_FRect lower_left = eidolon_bubble_layout_rect(width, height, 2, 4U);
    const SDL_FRect character =
        eidolon_bubble_layout_character(width, height, 406.0F, 560.0F, 4U);
    assert(left.x + left.w < character.x);
    assert(right.x > character.x + character.w);
    assert(lower_left.y > left.y + left.h);
    assert(eidolon_bubble_layout_hit_test(width, height, 4U, left.x + 4.0F, left.y + 4.0F) ==
           0);
    return 0;
}
