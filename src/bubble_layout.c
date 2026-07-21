#include "bubble_layout.h"

#include <float.h>
#include <math.h>

#define LAYOUT_MARGIN 16.0F
#define LAYOUT_GAP 18.0F
#define CANDIDATE_CAPACITY 18U

#define SCORE_CLAMP_PER_PIXEL 1000000.0
#define SCORE_BUBBLE_OVERLAP_PER_PIXEL 1000000000.0
#define SCORE_FACE_OVERLAP_PER_PIXEL 100000000.0
#define SCORE_BODY_OVERLAP_PER_PIXEL 100000.0
#define SCORE_PREVIOUS_SIDE_CHANGE 10000000.0
#define SCORE_PREVIOUS_MOVE_PER_PIXEL 100.0

typedef struct LayoutCandidate {
    SDL_FRect rect;
    EidolonBubbleSide side;
    float raw_x;
    float raw_y;
    size_t order;
} LayoutCandidate;

static bool finite_rect(SDL_FRect rect) {
    return isfinite(rect.x) && isfinite(rect.y) && isfinite(rect.w) && isfinite(rect.h) &&
           rect.w > 0.0F && rect.h > 0.0F;
}

static float clamp_float(float value, float minimum, float maximum) {
    return fminf(fmaxf(value, minimum), maximum);
}

static float overlap_area(SDL_FRect a, SDL_FRect b) {
    const float left = fmaxf(a.x, b.x);
    const float top = fmaxf(a.y, b.y);
    const float right = fminf(a.x + a.w, b.x + b.w);
    const float bottom = fminf(a.y + a.h, b.y + b.h);
    return fmaxf(0.0F, right - left) * fmaxf(0.0F, bottom - top);
}

static SDL_FRect union_rect(SDL_FRect a, SDL_FRect b) {
    const float left = fminf(a.x, b.x);
    const float top = fminf(a.y, b.y);
    const float right = fmaxf(a.x + a.w, b.x + b.w);
    const float bottom = fmaxf(a.y + a.h, b.y + b.h);
    return (SDL_FRect){left, top, right - left, bottom - top};
}

static size_t build_candidates(const EidolonBubbleLayoutInput *input,
                               const EidolonBubbleLayoutItem *item,
                               LayoutCandidate candidates[CANDIDATE_CAPACITY]) {
    const float margin = LAYOUT_MARGIN * input->spacing_scale;
    const float gap = LAYOUT_GAP * input->spacing_scale;
    const float minimum_x = input->usable_bounds.x + margin;
    const float maximum_x = input->usable_bounds.x + input->usable_bounds.w - margin - item->width;
    const float minimum_y = input->usable_bounds.y + margin;
    const float maximum_y = input->usable_bounds.y + input->usable_bounds.h - margin - item->height;
    if (maximum_x < minimum_x || maximum_y < minimum_y) {
        return 0U;
    }

    const SDL_FRect attention =
        input->has_face_bounds ? input->face_bounds : input->visible_body_bounds;
    const float anchor_y = attention.y + attention.h * 0.5F - item->height * 0.5F;
    const float step = item->height + gap;
    static const int offsets[CANDIDATE_CAPACITY / 2U] = {0, 1, -1, 2, -2, 3, -3, 4, -4};
    size_t count = 0U;
    for (size_t offset_index = 0U; offset_index < CANDIDATE_CAPACITY / 2U; ++offset_index) {
        const float raw_y = anchor_y + (float)offsets[offset_index] * step;
        for (int side_index = 0; side_index < 2; ++side_index) {
            const EidolonBubbleSide side =
                side_index == 0 ? EIDOLON_BUBBLE_SIDE_LEFT : EIDOLON_BUBBLE_SIDE_RIGHT;
            const float raw_x =
                side == EIDOLON_BUBBLE_SIDE_LEFT
                    ? input->visible_body_bounds.x - gap - item->width
                    : input->visible_body_bounds.x + input->visible_body_bounds.w + gap;
            candidates[count] = (LayoutCandidate){
                .rect = {clamp_float(raw_x, minimum_x, maximum_x),
                         clamp_float(raw_y, minimum_y, maximum_y), item->width, item->height},
                .side = side,
                .raw_x = raw_x,
                .raw_y = raw_y,
                .order = count,
            };
            ++count;
        }
    }
    return count;
}

static double score_candidate(const EidolonBubbleLayoutInput *input,
                              const EidolonBubbleLayoutItem *item,
                              const EidolonBubbleLayoutResult *result,
                              const LayoutCandidate *candidate) {
    double score = (double)fabsf(candidate->rect.x - candidate->raw_x) * SCORE_CLAMP_PER_PIXEL;
    score += (double)fabsf(candidate->rect.y - candidate->raw_y) * SCORE_CLAMP_PER_PIXEL;
    score += (double)overlap_area(candidate->rect, input->visible_body_bounds) *
             SCORE_BODY_OVERLAP_PER_PIXEL;
    if (input->has_face_bounds) {
        score += (double)overlap_area(candidate->rect, input->face_bounds) *
                 SCORE_FACE_OVERLAP_PER_PIXEL;
    }
    for (size_t index = 0U; index < result->bubble_count; ++index) {
        score += (double)overlap_area(candidate->rect, result->bubbles[index].rect) *
                 SCORE_BUBBLE_OVERLAP_PER_PIXEL;
    }
    if (item->has_previous) {
        if (item->previous_side != EIDOLON_BUBBLE_SIDE_NONE &&
            item->previous_side != candidate->side) {
            score += SCORE_PREVIOUS_SIDE_CHANGE;
        }
        score += ((double)fabsf(candidate->rect.x - item->previous_rect.x) +
                  (double)fabsf(candidate->rect.y - item->previous_rect.y)) *
                 SCORE_PREVIOUS_MOVE_PER_PIXEL;
    }
    score += (double)candidate->order;
    return score;
}

bool eidolon_bubble_layout_solve(const EidolonBubbleLayoutInput *input,
                                 EidolonBubbleLayoutResult *result) {
    if (input == NULL || result == NULL || input->bubble_count == 0U ||
        input->bubble_count > EIDOLON_BUBBLE_LAYOUT_CAPACITY ||
        !finite_rect(input->usable_bounds) || !finite_rect(input->body_render_bounds) ||
        !finite_rect(input->visible_body_bounds) || !isfinite(input->spacing_scale) ||
        input->spacing_scale <= 0.0F ||
        (input->has_face_bounds && !finite_rect(input->face_bounds))) {
        return false;
    }

    *result = (EidolonBubbleLayoutResult){0};
    result->canvas_bounds = input->body_render_bounds;
    for (size_t bubble_index = 0U; bubble_index < input->bubble_count; ++bubble_index) {
        const EidolonBubbleLayoutItem *item = &input->bubbles[bubble_index];
        if (!isfinite(item->width) || !isfinite(item->height) || item->width <= 0.0F ||
            item->height <= 0.0F || (item->has_previous && !finite_rect(item->previous_rect))) {
            return false;
        }
        LayoutCandidate candidates[CANDIDATE_CAPACITY];
        const size_t candidate_count = build_candidates(input, item, candidates);
        if (candidate_count == 0U) {
            return false;
        }

        size_t best_index = 0U;
        double best_score = DBL_MAX;
        for (size_t candidate_index = 0U; candidate_index < candidate_count; ++candidate_index) {
            const double score = score_candidate(input, item, result, &candidates[candidate_index]);
            if (score < best_score) {
                best_score = score;
                best_index = candidate_index;
            }
        }
        result->bubbles[bubble_index] = (EidolonBubblePlacement){
            .rect = candidates[best_index].rect,
            .side = candidates[best_index].side,
        };
        result->canvas_bounds =
            union_rect(result->canvas_bounds, result->bubbles[bubble_index].rect);
        ++result->bubble_count;
    }
    const float padding = LAYOUT_MARGIN * input->spacing_scale;
    result->canvas_bounds.x -= padding;
    result->canvas_bounds.y -= padding;
    result->canvas_bounds.w += padding * 2.0F;
    result->canvas_bounds.h += padding * 2.0F;
    return true;
}

void eidolon_bubble_layout_canvas(float character_width, float character_height,
                                  size_t visible_count, int *width, int *height) {
    if (width == NULL || height == NULL || visible_count == 0U) {
        return;
    }
    if (visible_count == 1U) {
        *width =
            (int)ceilf(EIDOLON_BUBBLE_WIDTH + LAYOUT_GAP + character_width + LAYOUT_MARGIN * 2.0F);
        *height = (int)ceilf(fmaxf(character_height + LAYOUT_MARGIN * 2.0F,
                                   EIDOLON_BUBBLE_HEIGHT + LAYOUT_MARGIN * 2.0F));
        return;
    }
    const size_t rows = (visible_count + 1U) / 2U;
    *width = (int)ceilf(EIDOLON_BUBBLE_WIDTH * 2.0F + character_width + LAYOUT_GAP * 2.0F +
                        LAYOUT_MARGIN * 2.0F);
    *height = (int)ceilf(fmaxf(character_height + LAYOUT_MARGIN * 2.0F,
                               (float)rows * EIDOLON_BUBBLE_HEIGHT +
                                   (float)(rows - 1U) * LAYOUT_GAP + LAYOUT_MARGIN * 2.0F));
}

SDL_FRect eidolon_bubble_layout_character(int canvas_width, int canvas_height,
                                          float character_width, float character_height,
                                          size_t visible_count) {
    const float x = visible_count <= 1U ? (float)canvas_width - LAYOUT_MARGIN - character_width
                                        : ((float)canvas_width - character_width) * 0.5F;
    return (SDL_FRect){x, (float)canvas_height - LAYOUT_MARGIN - character_height, character_width,
                       character_height};
}

SDL_FRect eidolon_bubble_layout_rect(int canvas_width, int canvas_height, int slot,
                                     size_t visible_count) {
    (void)canvas_height;
    if (visible_count <= 1U) {
        return (SDL_FRect){LAYOUT_MARGIN, LAYOUT_MARGIN, EIDOLON_BUBBLE_WIDTH,
                           EIDOLON_BUBBLE_HEIGHT};
    }
    const int column = slot & 1;
    const int row = slot / 2;
    const float x =
        column == 0 ? LAYOUT_MARGIN : (float)canvas_width - LAYOUT_MARGIN - EIDOLON_BUBBLE_WIDTH;
    const float y = LAYOUT_MARGIN + (float)row * (EIDOLON_BUBBLE_HEIGHT + LAYOUT_GAP);
    return (SDL_FRect){x, y, EIDOLON_BUBBLE_WIDTH, EIDOLON_BUBBLE_HEIGHT};
}

int eidolon_bubble_layout_hit_test(int canvas_width, int canvas_height, size_t visible_count,
                                   float x, float y) {
    for (int slot = 0; slot < 4; ++slot) {
        const SDL_FRect rect =
            eidolon_bubble_layout_rect(canvas_width, canvas_height, slot, visible_count);
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            return slot;
        }
    }
    return -1;
}
