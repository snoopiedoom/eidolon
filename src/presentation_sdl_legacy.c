#include "presentation_sdl_legacy.h"

#include "platform/overlay.h"
#include "presentation_internal.h"

#define SDL_LEGACY_TARGET_CAPACITY (EIDOLON_SCENE_LAYER_CAPACITY * 2U)

typedef struct EidolonSdlLegacyTarget {
    EidolonPresentationTarget id;
    SDL_Texture *texture;
} EidolonSdlLegacyTarget;

typedef struct EidolonSdlLegacyPresentation {
    SDL_Window *window;
    SDL_Renderer *renderer;
    EidolonSdlLegacyTarget targets[SDL_LEGACY_TARGET_CAPACITY];
} EidolonSdlLegacyPresentation;

static void legacy_destroy(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    for (size_t index = 0U; index < SDL_arraysize(legacy->targets); ++index) {
        SDL_DestroyTexture(legacy->targets[index].texture);
    }
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

static EidolonSdlLegacyTarget *legacy_find_target(EidolonSdlLegacyPresentation *legacy,
                                                  EidolonPresentationTarget target) {
    for (size_t index = 0U; index < SDL_arraysize(legacy->targets); ++index) {
        if (legacy->targets[index].texture != NULL &&
            legacy->targets[index].id.value == target.value) {
            return &legacy->targets[index];
        }
    }
    return NULL;
}

static bool legacy_create_target(void *opaque, EidolonPresentationTarget target, uint32_t width,
                                 uint32_t height, EidolonPresentationAlphaMode alpha_mode) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    EidolonSdlLegacyTarget *slot = NULL;
    for (size_t index = 0U; index < SDL_arraysize(legacy->targets); ++index) {
        if (legacy->targets[index].texture == NULL) {
            slot = &legacy->targets[index];
            break;
        }
    }
    if (slot == NULL) {
        SDL_SetError("SDL legacy target capacity exhausted");
        return false;
    }
    SDL_Texture *texture = SDL_CreateTexture(legacy->renderer, SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_TARGET, (int)width, (int)height);
    const SDL_BlendMode blend =
        alpha_mode == EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED
            ? SDL_ComposeCustomBlendMode(
                  SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                  SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD)
            : SDL_BLENDMODE_BLEND;
    if (texture == NULL || !SDL_SetTextureBlendMode(texture, blend) ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
        SDL_DestroyTexture(texture);
        return false;
    }
    slot->id = target;
    slot->texture = texture;
    return true;
}

static void legacy_destroy_target(void *opaque, EidolonPresentationTarget target) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    EidolonSdlLegacyTarget *slot = legacy_find_target(legacy, target);
    if (slot != NULL) {
        SDL_DestroyTexture(slot->texture);
        SDL_zero(*slot);
    }
}

static bool legacy_commit_scene(void *opaque, const EidolonSceneSnapshot *scene) {
    (void)opaque;
    (void)scene;
    return true;
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
        .create_target = legacy_create_target,
        .destroy_target = legacy_destroy_target,
        .commit_scene = legacy_commit_scene,
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

SDL_Texture *eidolon_sdl_legacy_target_texture(EidolonPresentation *presentation,
                                               EidolonPresentationTarget target) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    EidolonSdlLegacyTarget *slot = legacy != NULL ? legacy_find_target(legacy, target) : NULL;
    return slot != NULL ? slot->texture : NULL;
}

SDL_Surface *eidolon_sdl_legacy_read_pixels(EidolonPresentation *presentation) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    return legacy != NULL ? eidolon_platform_read_pixels(legacy->renderer) : NULL;
}
