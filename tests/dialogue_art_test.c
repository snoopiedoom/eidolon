#include "dialogue_art.h"
#include "text_renderer.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <string.h>

static Uint8 surface_alpha(SDL_Surface *surface, int x, int y) {
    Uint8 red = 0U;
    Uint8 green = 0U;
    Uint8 blue = 0U;
    Uint8 alpha = 0U;
    assert(SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha));
    return alpha;
}

int main(void) {
    SDL_Surface *surface = SDL_CreateSurface(260, 150, SDL_PIXELFORMAT_BGRA32);
    EidolonTextRenderer *text_renderer =
        eidolon_text_renderer_create(NULL, EIDOLON_FONT_PATH, 12.0F);
    assert(surface != NULL);
    assert(text_renderer != NULL);

    EidolonDialogue dialogue;
    eidolon_dialogue_set(&dialogue, "hello world", 0U);
    dialogue.revealed = strlen(dialogue.page);
    const SDL_FRect bubble = {32.0F, 32.0F, 196.0F, 86.0F};
    assert(eidolon_dialogue_art_draw_surface(
        surface, text_renderer, EIDOLON_DIALOGUE_THEME_ACADEMY_HEART, &bubble, &dialogue,
        "EIDOLON", 0U, 1U, false));

    /*
     * The old three-rectangle corner raster left this join transparent above and
     * shadow-black below in SDL's software renderer.
     */
    const int former_join_x = (int)(bubble.x + 19.0F) - 1;
    assert(surface_alpha(surface, former_join_x, (int)bubble.y + 1) > 200U);
    assert(surface_alpha(surface, former_join_x,
                         (int)(bubble.y + bubble.h) - 3) > 200U);

    eidolon_text_renderer_destroy(text_renderer);
    SDL_DestroySurface(surface);
    return 0;
}
