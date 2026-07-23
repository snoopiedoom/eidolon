#ifndef EIDOLON_RASTER_SDL_LEGACY_H
#define EIDOLON_RASTER_SDL_LEGACY_H

#include <SDL3/SDL.h>

#include "dialogue.h"
#include "portrait.h"
#include "presentation.h"
#include "text_renderer.h"

typedef enum EidolonSdlLegacyRasterResult {
    EIDOLON_SDL_LEGACY_RASTER_VALID,
    EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED,
    EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED,
} EidolonSdlLegacyRasterResult;

bool eidolon_sdl_legacy_clear_host(EidolonPresentation *presentation);
EidolonSdlLegacyRasterResult
eidolon_sdl_legacy_raster_portrait(EidolonPresentation *presentation,
                                   EidolonPortraitRenderer *portrait,
                                   const EidolonPresentationTargetUpdate *update);
bool eidolon_sdl_legacy_composite_portrait(EidolonPresentation *presentation,
                                           EidolonPresentationTarget target,
                                           const EidolonPortraitTransform *transform);
EidolonSdlLegacyRasterResult eidolon_sdl_legacy_raster_dialogue(
    EidolonPresentation *presentation, EidolonTextRenderer *text_renderer,
    EidolonDialogueTheme theme, const EidolonPresentationTargetUpdate *update,
    const SDL_FRect *local_bubble, const EidolonDialogue *dialogue, const char *title,
    size_t title_slot, size_t body_slot, bool points_right);
bool eidolon_sdl_legacy_composite_dialogue(EidolonPresentation *presentation,
                                           EidolonPresentationTarget target,
                                           const SDL_FRect *destination, float opacity);
bool eidolon_sdl_legacy_draw_dialogue(EidolonPresentation *presentation,
                                      EidolonTextRenderer *text_renderer,
                                      EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                      const EidolonDialogue *dialogue, const char *title,
                                      size_t title_slot, size_t body_slot, float opacity,
                                      bool points_right);
bool eidolon_sdl_legacy_draw_portrait(EidolonPresentation *presentation,
                                      EidolonPortraitRenderer *portrait,
                                      const EidolonPortraitTransform *transform);
bool eidolon_sdl_legacy_draw_model(EidolonPresentation *presentation, SDL_Texture *texture,
                                   const SDL_FRect *destination);
bool eidolon_sdl_legacy_draw_sprite(EidolonPresentation *presentation, SDL_Texture *atlas,
                                    const SDL_FRect *source, const SDL_FRect *destination);

#endif
