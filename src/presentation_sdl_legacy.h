#ifndef EIDOLON_PRESENTATION_SDL_LEGACY_H
#define EIDOLON_PRESENTATION_SDL_LEGACY_H

#include "presentation.h"

#include <SDL3/SDL.h>

typedef struct EidolonSdlLegacyConfig {
    const char *title;
    int width;
    int height;
    SDL_WindowFlags window_flags;
} EidolonSdlLegacyConfig;

EidolonPresentation *eidolon_sdl_legacy_presentation_create(const EidolonSdlLegacyConfig *config);
SDL_Window *eidolon_sdl_legacy_window(EidolonPresentation *presentation);
SDL_Renderer *eidolon_sdl_legacy_renderer(EidolonPresentation *presentation);
SDL_Texture *eidolon_sdl_legacy_target_texture(EidolonPresentation *presentation,
                                               EidolonPresentationTarget target);
SDL_Surface *eidolon_sdl_legacy_read_pixels(EidolonPresentation *presentation);

#endif
