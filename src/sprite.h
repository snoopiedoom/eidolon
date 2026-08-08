#ifndef EIDOLON_SPRITE_H
#define EIDOLON_SPRITE_H

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct EidolonSpriteRenderer EidolonSpriteRenderer;

EidolonSpriteRenderer *eidolon_sprite_create(SDL_Renderer *renderer, const char *atlas_path,
                                             int columns, int rows, int cell_width,
                                             int cell_height);
void eidolon_sprite_destroy(EidolonSpriteRenderer *sprite);
bool eidolon_sprite_ready(const EidolonSpriteRenderer *sprite);
bool eidolon_sprite_set_renderer(EidolonSpriteRenderer *sprite, SDL_Renderer *renderer);
bool eidolon_sprite_content_size(const EidolonSpriteRenderer *sprite, uint32_t *width,
                                 uint32_t *height);
bool eidolon_sprite_blit_content(const EidolonSpriteRenderer *sprite, const SDL_FRect *source,
                                 SDL_Surface *destination);
SDL_Texture *eidolon_sprite_texture(const EidolonSpriteRenderer *sprite);

#endif
