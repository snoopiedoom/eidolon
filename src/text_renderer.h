#ifndef EIDOLON_TEXT_RENDERER_H
#define EIDOLON_TEXT_RENDERER_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>

#define EIDOLON_TEXT_SLOT_COUNT 16U

typedef struct EidolonTextRenderer EidolonTextRenderer;

EidolonTextRenderer *eidolon_text_renderer_create(SDL_Renderer *renderer, const char *font_path,
                                                  float point_size);
void eidolon_text_renderer_destroy(EidolonTextRenderer *text_renderer);
bool eidolon_text_renderer_draw(EidolonTextRenderer *text_renderer, size_t slot, const char *text,
                                size_t length, float x, float y, int wrap_width, SDL_Color color);

#endif
