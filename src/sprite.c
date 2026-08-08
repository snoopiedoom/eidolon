#include "sprite.h"

#include <limits.h>
#include <math.h>

struct EidolonSpriteRenderer {
    SDL_Renderer *renderer;
    SDL_Surface *atlas_surface;
    SDL_Texture *atlas_texture;
    int columns;
    int rows;
    int cell_width;
    int cell_height;
};

static SDL_Texture *create_atlas_texture(SDL_Renderer *renderer, SDL_Surface *surface) {
    if (renderer == NULL) {
        return NULL;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        return NULL;
    }
    if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)) {
        SDL_DestroyTexture(texture);
        return NULL;
    }
    return texture;
}

EidolonSpriteRenderer *eidolon_sprite_create(SDL_Renderer *renderer, const char *atlas_path,
                                             int columns, int rows, int cell_width,
                                             int cell_height) {
    if (atlas_path == NULL || columns <= 0 || rows <= 0 || cell_width <= 0 || cell_height <= 0 ||
        columns > INT_MAX / cell_width || rows > INT_MAX / cell_height) {
        SDL_SetError("invalid sprite atlas configuration");
        return NULL;
    }
    SDL_Surface *loaded = SDL_LoadPNG(atlas_path);
    if (loaded == NULL) {
        return NULL;
    }
    const int expected_width = columns * cell_width;
    const int expected_height = rows * cell_height;
    if (loaded->w != expected_width || loaded->h != expected_height) {
        SDL_SetError("invalid sprite atlas size %dx%d; expected %dx%d", loaded->w, loaded->h,
                     expected_width, expected_height);
        SDL_DestroySurface(loaded);
        return NULL;
    }
    SDL_Surface *surface = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_BGRA32);
    SDL_DestroySurface(loaded);
    if (surface == NULL || !SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE)) {
        SDL_DestroySurface(surface);
        return NULL;
    }
    EidolonSpriteRenderer *sprite = SDL_calloc(1, sizeof(*sprite));
    if (sprite == NULL) {
        SDL_SetError("out of memory while creating sprite renderer");
        SDL_DestroySurface(surface);
        return NULL;
    }
    sprite->atlas_surface = surface;
    sprite->columns = columns;
    sprite->rows = rows;
    sprite->cell_width = cell_width;
    sprite->cell_height = cell_height;
    if (!eidolon_sprite_set_renderer(sprite, renderer)) {
        eidolon_sprite_destroy(sprite);
        return NULL;
    }
    return sprite;
}

void eidolon_sprite_destroy(EidolonSpriteRenderer *sprite) {
    if (sprite == NULL) {
        return;
    }
    SDL_DestroyTexture(sprite->atlas_texture);
    SDL_DestroySurface(sprite->atlas_surface);
    SDL_free(sprite);
}

bool eidolon_sprite_ready(const EidolonSpriteRenderer *sprite) {
    return sprite != NULL && sprite->atlas_surface != NULL;
}

bool eidolon_sprite_set_renderer(EidolonSpriteRenderer *sprite, SDL_Renderer *renderer) {
    if (!eidolon_sprite_ready(sprite)) {
        SDL_SetError("sprite atlas is unavailable");
        return false;
    }
    if (sprite->renderer == renderer && (renderer == NULL || sprite->atlas_texture != NULL)) {
        return true;
    }
    SDL_Texture *texture = create_atlas_texture(renderer, sprite->atlas_surface);
    if (renderer != NULL && texture == NULL) {
        return false;
    }
    SDL_DestroyTexture(sprite->atlas_texture);
    sprite->atlas_texture = texture;
    sprite->renderer = renderer;
    return true;
}

bool eidolon_sprite_content_size(const EidolonSpriteRenderer *sprite, uint32_t *width,
                                 uint32_t *height) {
    if (!eidolon_sprite_ready(sprite) || width == NULL || height == NULL) {
        SDL_SetError("invalid sprite content-size query");
        return false;
    }
    *width = (uint32_t)sprite->cell_width;
    *height = (uint32_t)sprite->cell_height;
    return true;
}

bool eidolon_sprite_blit_content(const EidolonSpriteRenderer *sprite, const SDL_FRect *source,
                                 SDL_Surface *destination) {
    if (!eidolon_sprite_ready(sprite) || source == NULL || destination == NULL ||
        !isfinite(source->x) || !isfinite(source->y) || !isfinite(source->w) ||
        !isfinite(source->h) || source->x < 0.0F || source->y < 0.0F || source->w <= 0.0F ||
        source->h <= 0.0F || source->x > (float)INT_MAX || source->y > (float)INT_MAX ||
        source->w > (float)INT_MAX || source->h > (float)INT_MAX) {
        SDL_SetError("invalid sprite raster input");
        return false;
    }
    const SDL_Rect source_rect = {
        (int)SDL_roundf(source->x),
        (int)SDL_roundf(source->y),
        (int)SDL_roundf(source->w),
        (int)SDL_roundf(source->h),
    };
    if (source_rect.w <= 0 || source_rect.h <= 0 ||
        source_rect.x > sprite->atlas_surface->w - source_rect.w ||
        source_rect.y > sprite->atlas_surface->h - source_rect.h) {
        SDL_SetError("sprite source rectangle exceeds atlas bounds");
        return false;
    }
    const SDL_Rect destination_rect = {0, 0, destination->w, destination->h};
    return SDL_FillSurfaceRect(destination, NULL,
                               SDL_MapSurfaceRGBA(destination, 0U, 0U, 0U, 0U)) &&
           SDL_BlitSurfaceScaled(sprite->atlas_surface, &source_rect, destination,
                                 &destination_rect, SDL_SCALEMODE_NEAREST);
}

SDL_Texture *eidolon_sprite_texture(const EidolonSpriteRenderer *sprite) {
    return sprite != NULL ? sprite->atlas_texture : NULL;
}
