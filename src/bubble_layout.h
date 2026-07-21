#ifndef EIDOLON_BUBBLE_LAYOUT_H
#define EIDOLON_BUBBLE_LAYOUT_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>

#define EIDOLON_BUBBLE_WIDTH 365.0F
#define EIDOLON_BUBBLE_HEIGHT 132.0F
#define EIDOLON_BUBBLE_LAYOUT_CAPACITY 4U

typedef enum EidolonBubbleSide {
    EIDOLON_BUBBLE_SIDE_NONE = 0,
    EIDOLON_BUBBLE_SIDE_LEFT,
    EIDOLON_BUBBLE_SIDE_RIGHT,
} EidolonBubbleSide;

typedef struct EidolonBubbleLayoutItem {
    float width;
    float height;
    bool has_previous;
    SDL_FRect previous_rect;
    EidolonBubbleSide previous_side;
} EidolonBubbleLayoutItem;

typedef struct EidolonBubbleLayoutInput {
    SDL_FRect usable_bounds;
    SDL_FRect body_render_bounds;
    SDL_FRect visible_body_bounds;
    float spacing_scale;
    bool has_face_bounds;
    SDL_FRect face_bounds;
    size_t bubble_count;
    EidolonBubbleLayoutItem bubbles[EIDOLON_BUBBLE_LAYOUT_CAPACITY];
} EidolonBubbleLayoutInput;

typedef struct EidolonBubblePlacement {
    SDL_FRect rect;
    EidolonBubbleSide side;
} EidolonBubblePlacement;

typedef struct EidolonBubbleLayoutResult {
    SDL_FRect canvas_bounds;
    size_t bubble_count;
    EidolonBubblePlacement bubbles[EIDOLON_BUBBLE_LAYOUT_CAPACITY];
} EidolonBubbleLayoutResult;

bool eidolon_bubble_layout_solve(const EidolonBubbleLayoutInput *input,
                                 EidolonBubbleLayoutResult *result);

/* Temporary local-canvas compatibility API. Remove when app/draw own layout results. */
void eidolon_bubble_layout_canvas(float character_width, float character_height,
                                  size_t visible_count, int *width, int *height);
SDL_FRect eidolon_bubble_layout_character(int canvas_width, int canvas_height,
                                          float character_width, float character_height,
                                          size_t visible_count);
SDL_FRect eidolon_bubble_layout_rect(int canvas_width, int canvas_height, int slot,
                                     size_t visible_count);
int eidolon_bubble_layout_hit_test(int canvas_width, int canvas_height, size_t visible_count,
                                   float x, float y);

#endif
