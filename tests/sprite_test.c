#include <SDL3/SDL.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "sprite.h"

#ifndef EIDOLON_TEST_SPRITE_PATH
#error EIDOLON_TEST_SPRITE_PATH is required
#endif

static void put_pixel(SDL_Surface *surface, int x, int y, Uint8 red, Uint8 green, Uint8 blue,
                      Uint8 alpha) {
    Uint8 *pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
    const Uint32 value = SDL_MapSurfaceRGBA(surface, red, green, blue, alpha);
    SDL_memcpy(pixel, &value, sizeof(value));
}

static void get_pixel(SDL_Surface *surface, int x, int y, Uint8 *red, Uint8 *green, Uint8 *blue,
                      Uint8 *alpha) {
    const Uint8 *pixel = (const Uint8 *)surface->pixels + y * surface->pitch + x * 4;
    Uint32 value = 0U;
    SDL_memcpy(&value, pixel, sizeof(value));
    SDL_GetRGBA(value, SDL_GetPixelFormatDetails(surface->format), NULL, red, green, blue, alpha);
}

int main(void) {
    assert(SDL_Init(0));
    SDL_Surface *atlas = SDL_CreateSurface(4, 2, SDL_PIXELFORMAT_BGRA32);
    assert(atlas != NULL);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            put_pixel(atlas, x, y, 220U, 15U, 25U, 255U);
            put_pixel(atlas, x + 2, y, 10U, 80U, 230U, (Uint8)(80 + x * 60 + y * 30));
        }
    }
    assert(SDL_SavePNG(atlas, EIDOLON_TEST_SPRITE_PATH));
    SDL_DestroySurface(atlas);

    EidolonSpriteRenderer *invalid =
        eidolon_sprite_create(NULL, EIDOLON_TEST_SPRITE_PATH, 3, 1, 2, 2);
    assert(invalid == NULL);
    SDL_ClearError();

    EidolonSpriteRenderer *sprite =
        eidolon_sprite_create(NULL, EIDOLON_TEST_SPRITE_PATH, 2, 1, 2, 2);
    assert(eidolon_sprite_ready(sprite));
    assert(eidolon_sprite_texture(sprite) == NULL);
    assert(eidolon_sprite_set_renderer(sprite, NULL));
    uint32_t width = 0U;
    uint32_t height = 0U;
    assert(eidolon_sprite_content_size(sprite, &width, &height));
    assert(width == 2U && height == 2U);

    SDL_Surface *target = SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_BGRA32);
    assert(target != NULL);
    const SDL_FRect source = {2.0F, 0.0F, 2.0F, 2.0F};
    assert(eidolon_sprite_blit_content(sprite, &source, target));
    Uint8 red = 0U;
    Uint8 green = 0U;
    Uint8 blue = 0U;
    Uint8 alpha = 0U;
    get_pixel(target, 0, 0, &red, &green, &blue, &alpha);
    assert(red == 10U && green == 80U && blue == 230U && alpha == 80U);
    get_pixel(target, 3, 3, &red, &green, &blue, &alpha);
    assert(red == 10U && green == 80U && blue == 230U && alpha == 170U);

    SDL_DestroySurface(target);
    eidolon_sprite_destroy(sprite);
    assert(SDL_RemovePath(EIDOLON_TEST_SPRITE_PATH));
    SDL_Quit();
    puts("sprite renderer tests passed");
    return 0;
}
