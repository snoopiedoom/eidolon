#include "raster_sdl_legacy.h"

#include "dialogue_art.h"
#include "presentation_sdl_legacy.h"

static SDL_Renderer *legacy_renderer(EidolonPresentation *presentation) {
    return eidolon_sdl_legacy_renderer(presentation);
}

bool eidolon_sdl_legacy_clear_host(EidolonPresentation *presentation) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) &&
           SDL_RenderClear(renderer);
}

EidolonSdlLegacyRasterResult
eidolon_sdl_legacy_raster_portrait(EidolonPresentation *presentation,
                                   EidolonPortraitRenderer *portrait,
                                   const EidolonPresentationTargetUpdate *update) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *target =
        update != NULL ? eidolon_sdl_legacy_target_texture(presentation, update->target) : NULL;
    if (renderer == NULL || portrait == NULL || update == NULL || target == NULL) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (!SDL_SetRenderTarget(renderer, target)) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    const bool content_valid =
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) && SDL_RenderClear(renderer) &&
        eidolon_portrait_draw_content(portrait, renderer, update->width, update->height);
    if (!SDL_SetRenderTarget(renderer, previous_target)) {
        return EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED;
    }
    return content_valid ? EIDOLON_SDL_LEGACY_RASTER_VALID
                         : EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
}

bool eidolon_sdl_legacy_composite_portrait(EidolonPresentation *presentation,
                                           EidolonPresentationTarget target,
                                           const EidolonPortraitTransform *transform) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *texture = eidolon_sdl_legacy_target_texture(presentation, target);
    if (renderer == NULL || texture == NULL || transform == NULL) {
        return false;
    }
    const SDL_FRect destination = {
        transform->x,
        transform->y,
        transform->width,
        transform->height,
    };
    const SDL_FPoint pivot = {
        transform->width * transform->pivot_x,
        transform->height * transform->pivot_y,
    };
    return SDL_RenderTextureRotated(renderer, texture, NULL, &destination,
                                    (double)transform->rotation_degrees, &pivot, SDL_FLIP_NONE);
}

EidolonSdlLegacyRasterResult eidolon_sdl_legacy_raster_dialogue(
    EidolonPresentation *presentation, EidolonTextRenderer *text_renderer,
    EidolonDialogueTheme theme, const EidolonPresentationTargetUpdate *update,
    const SDL_FRect *local_bubble, const EidolonDialogue *dialogue, const char *title,
    size_t title_slot, size_t body_slot, bool points_right) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *target =
        update != NULL ? eidolon_sdl_legacy_target_texture(presentation, update->target) : NULL;
    if (renderer == NULL || update == NULL || target == NULL || local_bubble == NULL ||
        dialogue == NULL) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (!SDL_SetRenderTarget(renderer, target)) {
        return EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
    }
    const bool content_valid =
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) && SDL_RenderClear(renderer);
    if (content_valid) {
        eidolon_dialogue_art_draw_renderer(renderer, text_renderer, theme, local_bubble, dialogue,
                                           title, title_slot, body_slot, 1.0F, points_right);
    }
    if (!SDL_SetRenderTarget(renderer, previous_target)) {
        return EIDOLON_SDL_LEGACY_RASTER_HOST_RESTORE_FAILED;
    }
    return content_valid ? EIDOLON_SDL_LEGACY_RASTER_VALID
                         : EIDOLON_SDL_LEGACY_RASTER_CONTENT_FAILED;
}

bool eidolon_sdl_legacy_composite_dialogue(EidolonPresentation *presentation,
                                           EidolonPresentationTarget target,
                                           const SDL_FRect *destination, float opacity) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    SDL_Texture *texture = eidolon_sdl_legacy_target_texture(presentation, target);
    return renderer != NULL && texture != NULL && destination != NULL &&
           SDL_SetTextureColorModFloat(texture, opacity, opacity, opacity) &&
           SDL_SetTextureAlphaModFloat(texture, opacity) &&
           SDL_RenderTexture(renderer, texture, NULL, destination);
}

bool eidolon_sdl_legacy_draw_dialogue(EidolonPresentation *presentation,
                                      EidolonTextRenderer *text_renderer,
                                      EidolonDialogueTheme theme, const SDL_FRect *bubble,
                                      const EidolonDialogue *dialogue, const char *title,
                                      size_t title_slot, size_t body_slot, float opacity,
                                      bool points_right) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    if (renderer == NULL || bubble == NULL || dialogue == NULL) {
        return false;
    }
    eidolon_dialogue_art_draw_renderer(renderer, text_renderer, theme, bubble, dialogue, title,
                                       title_slot, body_slot, opacity, points_right);
    return true;
}

bool eidolon_sdl_legacy_draw_portrait(EidolonPresentation *presentation,
                                      EidolonPortraitRenderer *portrait,
                                      const EidolonPortraitTransform *transform) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && portrait != NULL && transform != NULL &&
           eidolon_portrait_draw_transform(portrait, renderer, transform);
}

bool eidolon_sdl_legacy_draw_model(EidolonPresentation *presentation, SDL_Texture *texture,
                                   const SDL_FRect *destination) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && texture != NULL && destination != NULL &&
           SDL_RenderTexture(renderer, texture, NULL, destination);
}

bool eidolon_sdl_legacy_draw_sprite(EidolonPresentation *presentation, SDL_Texture *atlas,
                                    const SDL_FRect *source, const SDL_FRect *destination) {
    SDL_Renderer *renderer = legacy_renderer(presentation);
    return renderer != NULL && atlas != NULL && source != NULL && destination != NULL &&
           SDL_RenderTexture(renderer, atlas, source, destination);
}
