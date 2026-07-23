#include "presentation_sdl_legacy.h"

#include "platform/overlay.h"
#include "presentation_internal.h"

typedef struct EidolonSdlLegacyPresentation {
    SDL_Window *window;
    SDL_Renderer *renderer;
} EidolonSdlLegacyPresentation;

static void legacy_destroy(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    eidolon_platform_destroy_overlay(legacy->window);
    SDL_DestroyRenderer(legacy->renderer);
    SDL_DestroyWindow(legacy->window);
    SDL_free(legacy);
}

static bool legacy_configure_host(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    return eidolon_platform_configure_overlay(legacy->window);
}

static bool legacy_get_geometry(void *opaque, EidolonPresentationGeometry *geometry) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    return SDL_GetWindowPosition(legacy->window, &geometry->x, &geometry->y) &&
           SDL_GetWindowSize(legacy->window, &geometry->width, &geometry->height);
}

static bool legacy_set_geometry(void *opaque, const EidolonPresentationGeometry *geometry) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    int current_x = 0;
    int current_y = 0;
    int current_width = 0;
    int current_height = 0;
    if (!SDL_GetWindowPosition(legacy->window, &current_x, &current_y) ||
        !SDL_GetWindowSize(legacy->window, &current_width, &current_height)) {
        return false;
    }
    if ((current_width != geometry->width || current_height != geometry->height) &&
        !SDL_SetWindowSize(legacy->window, geometry->width, geometry->height)) {
        return false;
    }
    return (current_x == geometry->x && current_y == geometry->y) ||
           SDL_SetWindowPosition(legacy->window, geometry->x, geometry->y);
}

static bool legacy_sync_host(void *opaque) {
    const EidolonSdlLegacyPresentation *legacy = opaque;
    return SDL_SyncWindow(legacy->window);
}

static float legacy_display_scale(void *opaque) {
    const EidolonSdlLegacyPresentation *legacy = opaque;
    return SDL_GetWindowDisplayScale(legacy->window);
}

static bool legacy_set_vsync(void *opaque, int interval) {
    const EidolonSdlLegacyPresentation *legacy = opaque;
    return SDL_SetRenderVSync(legacy->renderer, interval);
}

static bool legacy_begin_interactive_move(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    return eidolon_platform_begin_window_drag(legacy->window);
}

static void legacy_suspend_input_region(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    eidolon_platform_suspend_hit_test(legacy->window);
}

static bool legacy_update_input_region(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    return eidolon_platform_update_hit_test(legacy->window, legacy->renderer);
}

static bool legacy_present(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    return SDL_RenderPresent(legacy->renderer);
}

EidolonPresentation *eidolon_sdl_legacy_presentation_create(const EidolonSdlLegacyConfig *config) {
    if (config == NULL || config->title == NULL || config->width <= 0 || config->height <= 0) {
        SDL_SetError("invalid SDL legacy presentation configuration");
        return NULL;
    }
    EidolonSdlLegacyPresentation *legacy = SDL_calloc(1U, sizeof(*legacy));
    if (legacy == NULL) {
        return NULL;
    }
    legacy->window =
        SDL_CreateWindow(config->title, config->width, config->height, config->window_flags);
    if (legacy->window == NULL) {
        SDL_free(legacy);
        return NULL;
    }
#if defined(_WIN32)
    legacy->renderer = SDL_CreateRenderer(legacy->window, "direct3d11");
    if (legacy->renderer == NULL) {
        SDL_ClearError();
        legacy->renderer = SDL_CreateRenderer(legacy->window, NULL);
    }
#else
    legacy->renderer = SDL_CreateRenderer(legacy->window, NULL);
#endif
    if (legacy->renderer == NULL) {
        SDL_DestroyWindow(legacy->window);
        SDL_free(legacy);
        return NULL;
    }

    const EidolonPresentationBackendOps operations = {
        .destroy = legacy_destroy,
        .configure_host = legacy_configure_host,
        .get_geometry = legacy_get_geometry,
        .set_geometry = legacy_set_geometry,
        .sync_host = legacy_sync_host,
        .display_scale = legacy_display_scale,
        .set_vsync = legacy_set_vsync,
        .begin_interactive_move = legacy_begin_interactive_move,
        .suspend_input_region = legacy_suspend_input_region,
        .update_input_region = legacy_update_input_region,
        .present = legacy_present,
    };
    uint64_t capabilities =
        EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS | EIDOLON_PRESENTATION_CAP_BACKGROUND_VISIBILITY;
#if defined(_WIN32)
    capabilities |= EIDOLON_PRESENTATION_CAP_PERSISTENT_OVER_OTHER_APPS |
                    EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT |
                    EIDOLON_PRESENTATION_CAP_PER_PIXEL_INPUT |
                    EIDOLON_PRESENTATION_CAP_NATIVE_INTERACTIVE_MOVE;
#endif
    EidolonPresentation *presentation =
        eidolon_presentation_create_backend("sdl_window_legacy", capabilities, legacy, &operations);
    if (presentation == NULL) {
        legacy_destroy(legacy);
    }
    return presentation;
}

static EidolonSdlLegacyPresentation *legacy_context(EidolonPresentation *presentation) {
    return eidolon_presentation_backend_context(presentation, "sdl_window_legacy");
}

SDL_Window *eidolon_sdl_legacy_window(EidolonPresentation *presentation) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    return legacy != NULL ? legacy->window : NULL;
}

SDL_Renderer *eidolon_sdl_legacy_renderer(EidolonPresentation *presentation) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    return legacy != NULL ? legacy->renderer : NULL;
}

SDL_Surface *eidolon_sdl_legacy_read_pixels(EidolonPresentation *presentation) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    return legacy != NULL ? eidolon_platform_read_pixels(legacy->renderer) : NULL;
}
