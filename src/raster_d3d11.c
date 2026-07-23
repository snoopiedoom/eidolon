#define COBJMACROS

#include "raster_d3d11.h"

#if defined(_WIN32)

#include "dialogue_art.h"
#include "platform/windows_dcomp.h"

static SDL_Surface *create_target_surface(const EidolonPresentationTargetUpdate *update) {
    if (update == NULL || update->width == 0U || update->height == 0U || update->width > INT_MAX ||
        update->height > INT_MAX) {
        SDL_SetError("invalid D3D11 raster target extent");
        return NULL;
    }
    return SDL_CreateSurface((int)update->width, (int)update->height, SDL_PIXELFORMAT_BGRA32);
}

bool eidolon_d3d11_upload_straight_alpha(EidolonPresentation *presentation,
                                         const EidolonPresentationTargetUpdate *update,
                                         SDL_Surface *surface) {
    if (presentation == NULL || update == NULL || surface == NULL ||
        update->alpha_mode != EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED ||
        surface->w != (int)update->width || surface->h != (int)update->height) {
        SDL_SetError("invalid straight-alpha D3D11 upload");
        return false;
    }
    SDL_Surface *premultiplied = create_target_surface(update);
    if (premultiplied == NULL) {
        return false;
    }
    if (!eidolon_presentation_set_target_alpha_mask(presentation, update, surface->pixels,
                                                    (size_t)surface->pitch, 4U, 3U)) {
        SDL_DestroySurface(premultiplied);
        return false;
    }
    const bool converted = SDL_PremultiplyAlpha(
        surface->w, surface->h, surface->format, surface->pixels, surface->pitch,
        premultiplied->format, premultiplied->pixels, premultiplied->pitch, false);
    if (!converted) {
        SDL_DestroySurface(premultiplied);
        return false;
    }
    ID3D11DeviceContext *context = eidolon_win32_dcomp_device_context(presentation);
    ID3D11Texture2D *texture =
        eidolon_win32_dcomp_target_texture(presentation, update->target, update->generation);
    if (context == NULL || texture == NULL) {
        SDL_DestroySurface(premultiplied);
        return false;
    }
    ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)texture, 0U, NULL,
                                          premultiplied->pixels, (UINT)premultiplied->pitch, 0U);
    SDL_DestroySurface(premultiplied);
    return true;
}

bool eidolon_d3d11_raster_portrait(EidolonPresentation *presentation,
                                   EidolonPortraitRenderer *portrait,
                                   const EidolonPresentationTargetUpdate *update) {
    if (portrait == NULL) {
        SDL_SetError("missing portrait for D3D11 raster");
        return false;
    }
    SDL_Surface *surface = create_target_surface(update);
    if (surface == NULL) {
        return false;
    }
    const bool rendered =
        SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGBA(surface, 0U, 0U, 0U, 0U)) &&
        eidolon_portrait_blit_content(portrait, surface) &&
        eidolon_d3d11_upload_straight_alpha(presentation, update, surface);
    SDL_DestroySurface(surface);
    return rendered;
}

bool eidolon_d3d11_raster_dialogue(EidolonPresentation *presentation,
                                   EidolonTextRenderer *text_renderer, EidolonDialogueTheme theme,
                                   const EidolonPresentationTargetUpdate *update,
                                   const SDL_FRect *local_bubble, const EidolonDialogue *dialogue,
                                   const char *title, size_t title_slot, size_t body_slot,
                                   bool points_right) {
    if (text_renderer == NULL || local_bubble == NULL || dialogue == NULL) {
        SDL_SetError("missing dialogue input for D3D11 raster");
        return false;
    }
    SDL_Surface *surface = create_target_surface(update);
    if (surface == NULL) {
        return false;
    }
    const bool rendered =
        eidolon_dialogue_art_draw_surface(surface, text_renderer, theme, local_bubble, dialogue,
                                          title, title_slot, body_slot, points_right) &&
        eidolon_d3d11_upload_straight_alpha(presentation, update, surface);
    SDL_DestroySurface(surface);
    return rendered;
}

#endif
