#ifndef EIDOLON_BUBBLE_LAYOUT_H
#define EIDOLON_BUBBLE_LAYOUT_H

#include <SDL3/SDL.h>

#include <stddef.h>

#define EIDOLON_BUBBLE_WIDTH 365.0F
#define EIDOLON_BUBBLE_HEIGHT 132.0F

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
