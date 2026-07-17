#include "bubble_layout.h"

#include <math.h>

#define LAYOUT_MARGIN 16.0F
#define LAYOUT_GAP 18.0F

void eidolon_bubble_layout_canvas(float character_width, float character_height,
                                  size_t visible_count, int *width, int *height) {
    if (width == NULL || height == NULL || visible_count == 0U) {
        return;
    }
    if (visible_count == 1U) {
        *width = (int)ceilf(EIDOLON_BUBBLE_WIDTH + LAYOUT_GAP + character_width +
                           LAYOUT_MARGIN * 2.0F);
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
    const float x = visible_count <= 1U
                        ? (float)canvas_width - LAYOUT_MARGIN - character_width
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
    const float x = column == 0 ? LAYOUT_MARGIN
                                : (float)canvas_width - LAYOUT_MARGIN - EIDOLON_BUBBLE_WIDTH;
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
