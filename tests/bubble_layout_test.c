#include "bubble_layout.h"

#include <assert.h>
#include <math.h>

static bool nearly_equal(float a, float b) { return fabsf(a - b) < 0.001F; }

static bool same_rect(SDL_FRect a, SDL_FRect b) {
    return nearly_equal(a.x, b.x) && nearly_equal(a.y, b.y) && nearly_equal(a.w, b.w) &&
           nearly_equal(a.h, b.h);
}

static bool rect_inside(SDL_FRect inner, SDL_FRect outer) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

static bool rects_overlap(SDL_FRect a, SDL_FRect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static EidolonBubbleLayoutInput desktop_input(SDL_FRect usable, SDL_FRect body, SDL_FRect face,
                                              size_t bubble_count) {
    EidolonBubbleLayoutInput input = {
        .usable_bounds = usable,
        .body_render_bounds = body,
        .visible_body_bounds = body,
        .spacing_scale = 1.0F,
        .has_face_bounds = true,
        .face_bounds = face,
        .bubble_count = bubble_count,
    };
    for (size_t index = 0U; index < bubble_count; ++index) {
        input.bubbles[index].width = EIDOLON_BUBBLE_WIDTH;
        input.bubbles[index].height = EIDOLON_BUBBLE_HEIGHT;
    }
    return input;
}

static void test_monitor_bound_layout(void) {
    const SDL_FRect usable = {0.0F, 0.0F, 1920.0F, 1040.0F};
    const SDL_FRect body = {1600.0F, 600.0F, 300.0F, 400.0F};
    const SDL_FRect face = {1690.0F, 620.0F, 120.0F, 120.0F};
    EidolonBubbleLayoutInput input = desktop_input(usable, body, face, 1U);
    EidolonBubbleLayoutResult first;
    assert(eidolon_bubble_layout_solve(&input, &first));
    assert(first.bubble_count == 1U);
    assert(first.bubbles[0].side == EIDOLON_BUBBLE_SIDE_LEFT);
    assert(rect_inside(first.bubbles[0].rect, usable));
    assert(!rects_overlap(first.bubbles[0].rect, face));

    input.bubble_count = 2U;
    input.bubbles[0].has_previous = true;
    input.bubbles[0].previous_rect = first.bubbles[0].rect;
    input.bubbles[0].previous_side = first.bubbles[0].side;
    input.bubbles[1].width = EIDOLON_BUBBLE_WIDTH;
    input.bubbles[1].height = EIDOLON_BUBBLE_HEIGHT;
    EidolonBubbleLayoutResult second;
    assert(eidolon_bubble_layout_solve(&input, &second));
    assert(second.bubble_count == 2U);
    assert(same_rect(second.bubbles[0].rect, first.bubbles[0].rect));
    assert(second.bubbles[0].side == EIDOLON_BUBBLE_SIDE_LEFT);
    assert(second.bubbles[1].side == EIDOLON_BUBBLE_SIDE_LEFT);
    assert(rect_inside(second.bubbles[0].rect, usable));
    assert(rect_inside(second.bubbles[1].rect, usable));
    assert(!rects_overlap(second.bubbles[0].rect, second.bubbles[1].rect));
    assert(rect_inside(body, second.canvas_bounds));
    assert(rect_inside(second.bubbles[0].rect, second.canvas_bounds));
    assert(rect_inside(second.bubbles[1].rect, second.canvas_bounds));
}

static void test_left_edge_chooses_right(void) {
    const SDL_FRect usable = {0.0F, 0.0F, 1920.0F, 1040.0F};
    const SDL_FRect body = {20.0F, 600.0F, 300.0F, 400.0F};
    const SDL_FRect face = {80.0F, 620.0F, 120.0F, 120.0F};
    const EidolonBubbleLayoutInput input = desktop_input(usable, body, face, 1U);
    EidolonBubbleLayoutResult result;
    assert(eidolon_bubble_layout_solve(&input, &result));
    assert(result.bubbles[0].side == EIDOLON_BUBBLE_SIDE_RIGHT);
    assert(rect_inside(result.bubbles[0].rect, usable));
    assert(!rects_overlap(result.bubbles[0].rect, face));
}

static void test_negative_monitor_origin(void) {
    const SDL_FRect usable = {-1920.0F, 0.0F, 1920.0F, 1040.0F};
    const SDL_FRect body = {-340.0F, 600.0F, 300.0F, 400.0F};
    const SDL_FRect face = {-280.0F, 620.0F, 120.0F, 120.0F};
    const EidolonBubbleLayoutInput input = desktop_input(usable, body, face, 2U);
    EidolonBubbleLayoutResult result;
    assert(eidolon_bubble_layout_solve(&input, &result));
    for (size_t index = 0U; index < result.bubble_count; ++index) {
        assert(rect_inside(result.bubbles[index].rect, usable));
    }
    assert(!rects_overlap(result.bubbles[0].rect, result.bubbles[1].rect));
}

static void test_deterministic_result(void) {
    const EidolonBubbleLayoutInput input = desktop_input(
        (SDL_FRect){0.0F, 0.0F, 1600.0F, 900.0F}, (SDL_FRect){650.0F, 420.0F, 300.0F, 400.0F},
        (SDL_FRect){740.0F, 440.0F, 120.0F, 120.0F}, 4U);
    EidolonBubbleLayoutResult first;
    EidolonBubbleLayoutResult second;
    assert(eidolon_bubble_layout_solve(&input, &first));
    assert(eidolon_bubble_layout_solve(&input, &second));
    assert(first.bubble_count == second.bubble_count);
    for (size_t index = 0U; index < first.bubble_count; ++index) {
        assert(first.bubbles[index].side == second.bubbles[index].side);
        assert(same_rect(first.bubbles[index].rect, second.bubbles[index].rect));
        assert(rect_inside(first.bubbles[index].rect, input.usable_bounds));
        assert(!rects_overlap(first.bubbles[index].rect, input.face_bounds));
    }
}

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
    const SDL_FRect character = eidolon_bubble_layout_character(width, height, 406.0F, 560.0F, 4U);
    assert(left.x + left.w < character.x);
    assert(right.x > character.x + character.w);
    assert(lower_left.y > left.y + left.h);
    assert(eidolon_bubble_layout_hit_test(width, height, 4U, left.x + 4.0F, left.y + 4.0F) == 0);

    test_monitor_bound_layout();
    test_left_edge_chooses_right();
    test_negative_monitor_origin();
    test_deterministic_result();
    return 0;
}
