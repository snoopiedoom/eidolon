#include "presentation_sdl_legacy.h"

#include "platform/overlay.h"
#include "presentation_event_queue.h"
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
    EidolonPresentationEventQueue event_queue;
    EidolonPresentationEnvironment environment;
    EidolonPresentationOutputInfo *outputs;
    size_t output_count;
    uint64_t topology_revision;
    uint64_t capabilities;
    bool environment_valid;
    bool environment_dirty;
} EidolonSdlLegacyPresentation;

static EidolonPresentationRect legacy_rect(SDL_Rect rect) {
    return (EidolonPresentationRect){
        (float)rect.x,
        (float)rect.y,
        (float)rect.w,
        (float)rect.h,
    };
}

static EidolonPresentationOrientation legacy_orientation(SDL_DisplayOrientation orientation) {
    switch (orientation) {
    case SDL_ORIENTATION_LANDSCAPE:
        return EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE;
    case SDL_ORIENTATION_PORTRAIT:
        return EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE_FLIPPED;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT_FLIPPED;
    case SDL_ORIENTATION_UNKNOWN:
    default:
        return EIDOLON_PRESENTATION_ORIENTATION_UNKNOWN;
    }
}

static bool same_rect(EidolonPresentationRect left, EidolonPresentationRect right) {
    return left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height;
}

static bool same_output(const EidolonPresentationOutputInfo *left,
                        const EidolonPresentationOutputInfo *right) {
    return left->output.value == right->output.value && same_rect(left->bounds, right->bounds) &&
           same_rect(left->usable_bounds, right->usable_bounds) &&
           left->content_scale == right->content_scale && left->pixel_scale == right->pixel_scale &&
           left->nominal_refresh_hz == right->nominal_refresh_hz &&
           left->orientation == right->orientation &&
           left->coordinate_space == right->coordinate_space &&
           left->capabilities == right->capabilities && left->flags == right->flags &&
           left->valid_fields == right->valid_fields;
}

static bool same_topology(const EidolonSdlLegacyPresentation *legacy,
                          const EidolonPresentationOutputInfo *outputs, size_t count) {
    if (legacy->output_count != count) {
        return false;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!same_output(&legacy->outputs[index], &outputs[index])) {
            return false;
        }
    }
    return true;
}

static bool field_presence_changed(const EidolonPresentationEnvironment *previous,
                                   const EidolonPresentationEnvironment *candidate,
                                   uint64_t field) {
    return ((previous->valid_fields ^ candidate->valid_fields) & field) != 0U;
}

static uint64_t environment_changes(const EidolonPresentationEnvironment *previous,
                                    const EidolonPresentationEnvironment *candidate) {
    uint64_t changed = previous->valid_fields ^ candidate->valid_fields;
#define LEGACY_ENV_CHANGED(field, expression)                                                      \
    if (!field_presence_changed(previous, candidate, (field)) &&                                   \
        (candidate->valid_fields & (field)) != 0U && (expression)) {                               \
        changed |= (field);                                                                        \
    }
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY,
                       previous->host_geometry.x != candidate->host_geometry.x ||
                           previous->host_geometry.y != candidate->host_geometry.y ||
                           previous->host_geometry.width != candidate->host_geometry.width ||
                           previous->host_geometry.height != candidate->host_geometry.height)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT,
                       previous->active_output.value != candidate->active_output.value)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS,
                       !same_rect(previous->output_bounds, candidate->output_bounds))
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS,
                       !same_rect(previous->usable_bounds, candidate->usable_bounds))
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_CONTENT_SCALE,
                       previous->content_scale != candidate->content_scale)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_PIXEL_SCALE,
                       previous->pixel_scale != candidate->pixel_scale)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH,
                       previous->nominal_refresh_hz != candidate->nominal_refresh_hz)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_ORIENTATION,
                       previous->orientation != candidate->orientation)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE,
                       previous->coordinate_space != candidate->coordinate_space)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY,
                       previous->topology_revision != candidate->topology_revision)
    LEGACY_ENV_CHANGED(EIDOLON_PRESENTATION_ENV_CAPABILITIES,
                       previous->capabilities != candidate->capabilities)
#undef LEGACY_ENV_CHANGED
    return changed;
}

static bool query_outputs(EidolonSdlLegacyPresentation *legacy,
                          EidolonPresentationOutputInfo **result, size_t *result_count) {
    int count = 0;
    SDL_DisplayID *ids = SDL_GetDisplays(&count);
    if (ids == NULL || count <= 0) {
        SDL_free(ids);
        SDL_SetError("SDL legacy output topology unavailable");
        return false;
    }
    EidolonPresentationOutputInfo *outputs = SDL_calloc((size_t)count, sizeof(*outputs));
    if (outputs == NULL) {
        SDL_free(ids);
        return false;
    }
    const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    for (int index = 0; index < count; ++index) {
        SDL_Rect bounds;
        SDL_Rect usable;
        if (!SDL_GetDisplayBounds(ids[index], &bounds) ||
            !SDL_GetDisplayUsableBounds(ids[index], &usable)) {
            SDL_free(outputs);
            SDL_free(ids);
            return false;
        }
        EidolonPresentationOutputInfo *output = &outputs[index];
        output->output.value = ids[index];
        output->bounds = legacy_rect(bounds);
        output->usable_bounds = legacy_rect(usable);
        output->coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_LOGICAL;
        output->capabilities = legacy->capabilities;
        output->flags = ids[index] == primary ? EIDOLON_PRESENTATION_OUTPUT_PRIMARY : 0U;
        output->valid_fields = EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                               EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS |
                               EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE;

        const float content_scale = SDL_GetDisplayContentScale(ids[index]);
        if (content_scale > 0.0F) {
            output->content_scale = content_scale;
            output->valid_fields |= EIDOLON_PRESENTATION_ENV_CONTENT_SCALE;
        } else {
            SDL_ClearError();
        }
        const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(ids[index]);
        if (mode != NULL) {
            if (mode->pixel_density > 0.0F) {
                output->pixel_scale = mode->pixel_density;
                output->valid_fields |= EIDOLON_PRESENTATION_ENV_PIXEL_SCALE;
            }
            if (mode->refresh_rate > 0.0F) {
                output->nominal_refresh_hz = mode->refresh_rate;
                output->valid_fields |= EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH;
            }
        } else {
            SDL_ClearError();
        }
        output->orientation = legacy_orientation(SDL_GetCurrentDisplayOrientation(ids[index]));
        if (output->orientation != EIDOLON_PRESENTATION_ORIENTATION_UNKNOWN) {
            output->valid_fields |= EIDOLON_PRESENTATION_ENV_ORIENTATION;
        }
    }
    SDL_free(ids);
    *result = outputs;
    *result_count = (size_t)count;
    return true;
}

static bool reconcile_environment(EidolonSdlLegacyPresentation *legacy, bool publish_event) {
    if (!legacy->environment_dirty && legacy->environment_valid) {
        return true;
    }

    EidolonPresentationOutputInfo *outputs = NULL;
    size_t output_count = 0U;
    if (!query_outputs(legacy, &outputs, &output_count)) {
        return false;
    }
    if (!same_topology(legacy, outputs, output_count)) {
        if (legacy->topology_revision == UINT64_MAX) {
            SDL_free(outputs);
            SDL_SetError("SDL legacy topology revision exhausted");
            return false;
        }
        SDL_free(legacy->outputs);
        legacy->outputs = outputs;
        legacy->output_count = output_count;
        ++legacy->topology_revision;
    } else {
        SDL_free(outputs);
    }

    EidolonPresentationEnvironment candidate;
    SDL_zero(candidate);
    candidate.host.value = 1U;
    candidate.topology_revision = legacy->topology_revision;
    candidate.capabilities = legacy->capabilities;
    candidate.coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_LOGICAL;
    candidate.valid_fields =
        EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY | EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE |
        EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY | EIDOLON_PRESENTATION_ENV_CAPABILITIES;
    if (!SDL_GetWindowPosition(legacy->window, &candidate.host_geometry.x,
                               &candidate.host_geometry.y) ||
        !SDL_GetWindowSize(legacy->window, &candidate.host_geometry.width,
                           &candidate.host_geometry.height)) {
        return false;
    }

    const float content_scale = SDL_GetWindowDisplayScale(legacy->window);
    if (content_scale > 0.0F) {
        candidate.content_scale = content_scale;
        candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_CONTENT_SCALE;
    } else {
        SDL_ClearError();
    }
    const float pixel_scale = SDL_GetWindowPixelDensity(legacy->window);
    if (pixel_scale > 0.0F) {
        candidate.pixel_scale = pixel_scale;
        candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_PIXEL_SCALE;
    } else {
        SDL_ClearError();
    }

    const SDL_DisplayID active = SDL_GetDisplayForWindow(legacy->window);
    if (active != 0U) {
        for (size_t index = 0U; index < legacy->output_count; ++index) {
            const EidolonPresentationOutputInfo *output = &legacy->outputs[index];
            if (output->output.value != active) {
                continue;
            }
            candidate.active_output = output->output;
            candidate.output_bounds = output->bounds;
            candidate.usable_bounds = output->usable_bounds;
            candidate.nominal_refresh_hz = output->nominal_refresh_hz;
            candidate.orientation = output->orientation;
            candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT |
                                      EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                                      EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS;
            if ((output->valid_fields & EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH) != 0U) {
                candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH;
            }
            if ((output->valid_fields & EIDOLON_PRESENTATION_ENV_ORIENTATION) != 0U) {
                candidate.valid_fields |= EIDOLON_PRESENTATION_ENV_ORIENTATION;
            }
            break;
        }
    } else {
        SDL_ClearError();
    }

    const uint64_t changed = legacy->environment_valid
                                 ? environment_changes(&legacy->environment, &candidate)
                                 : candidate.valid_fields;
    if (!legacy->environment_valid || changed != 0U) {
        if (legacy->environment_valid && legacy->environment.revision == UINT64_MAX) {
            SDL_SetError("SDL legacy environment revision exhausted");
            return false;
        }
        candidate.revision = legacy->environment_valid ? legacy->environment.revision + 1U : 1U;
        candidate.changed_fields = changed;
        legacy->environment = candidate;
        legacy->environment_valid = true;
        if (publish_event) {
            EidolonPresentationEvent event;
            SDL_zero(event);
            event.kind = EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED;
            event.monotonic_ns = SDL_GetTicksNS();
            event.host = candidate.host;
            event.data.environment.environment = candidate;
            (void)eidolon_presentation_event_queue_push(&legacy->event_queue, &event);
        }
    }
    legacy->environment_dirty = false;
    return true;
}

static void legacy_destroy(void *opaque) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    for (size_t index = 0U; index < SDL_arraysize(legacy->targets); ++index) {
        SDL_DestroyTexture(legacy->targets[index].texture);
    }
    eidolon_platform_destroy_overlay(legacy->window);
    SDL_DestroyRenderer(legacy->renderer);
    SDL_DestroyWindow(legacy->window);
    SDL_free(legacy->outputs);
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

static bool legacy_poll_event(void *opaque, EidolonPresentationEvent *event) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    if (!reconcile_environment(legacy, true)) {
        return false;
    }
    return eidolon_presentation_event_queue_poll(&legacy->event_queue, event);
}

static bool legacy_get_environment(void *opaque, EidolonPresentationEnvironment *environment) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    if (!reconcile_environment(legacy, legacy->environment_valid)) {
        return false;
    }
    *environment = legacy->environment;
    return true;
}

static EidolonPresentationTopologyResult
legacy_copy_outputs(void *opaque, EidolonPresentationOutputInfo *outputs, size_t capacity) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    if (!reconcile_environment(legacy, legacy->environment_valid)) {
        return (EidolonPresentationTopologyResult){
            .status = EIDOLON_PRESENTATION_TOPOLOGY_ERROR,
        };
    }
    if (outputs == NULL && capacity != 0U) {
        SDL_SetError("SDL legacy output storage is null");
        return (EidolonPresentationTopologyResult){
            .revision = legacy->topology_revision,
            .required_count = legacy->output_count,
            .status = EIDOLON_PRESENTATION_TOPOLOGY_ERROR,
        };
    }
    const size_t copied = SDL_min(capacity, legacy->output_count);
    for (size_t index = 0U; index < copied; ++index) {
        outputs[index] = legacy->outputs[index];
    }
    return (EidolonPresentationTopologyResult){
        .revision = legacy->topology_revision,
        .required_count = legacy->output_count,
        .copied_count = copied,
        .status = copied < legacy->output_count
                      ? EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY
                      : EIDOLON_PRESENTATION_TOPOLOGY_OK,
    };
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

static bool legacy_create_target(void *opaque, EidolonSceneLayerId layer,
                                 EidolonPresentationTarget target, uint64_t generation,
                                 uint32_t width, uint32_t height,
                                 EidolonPresentationAlphaMode alpha_mode) {
    EidolonSdlLegacyPresentation *legacy = opaque;
    (void)layer;
    (void)generation;
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

static bool legacy_commit_scene(void *opaque, const EidolonPresentationSceneCommit *commit) {
    (void)opaque;
    (void)commit;
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
    eidolon_presentation_event_queue_init(&legacy->event_queue);
    legacy->environment_dirty = true;

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
        .poll_event = legacy_poll_event,
        .create_target = legacy_create_target,
        .destroy_target = legacy_destroy_target,
        .commit_scene = legacy_commit_scene,
        .present = legacy_present,
        .get_environment = legacy_get_environment,
        .copy_outputs = legacy_copy_outputs,
    };
    uint64_t capabilities =
        EIDOLON_PRESENTATION_CAP_MULTIPLE_OUTPUTS | EIDOLON_PRESENTATION_CAP_BACKGROUND_VISIBILITY;
#if defined(_WIN32)
    capabilities |= EIDOLON_PRESENTATION_CAP_PERSISTENT_OVER_OTHER_APPS |
                    EIDOLON_PRESENTATION_CAP_GLOBAL_PLACEMENT |
                    EIDOLON_PRESENTATION_CAP_PER_PIXEL_INPUT |
                    EIDOLON_PRESENTATION_CAP_NATIVE_INTERACTIVE_MOVE;
#endif
    legacy->capabilities = capabilities;
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

static bool legacy_enqueue_structural_event(
    EidolonPresentation *presentation, EidolonSdlLegacyPresentation *legacy,
    EidolonPresentationEventKind kind, EidolonPresentationGraphicsResetKind reset_kind) {
    EidolonPresentationEvent event;
    SDL_zero(event);
    event.kind = kind;
    event.monotonic_ns = SDL_GetTicksNS();
    event.host = eidolon_presentation_host(presentation);
    event.data.graphics.reset_kind = reset_kind;
    if (eidolon_presentation_event_queue_push(&legacy->event_queue, &event)) {
        return true;
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not retain SDL presentation event kind=%d",
                 (int)kind);
    return false;
}

bool eidolon_sdl_legacy_handle_event(EidolonPresentation *presentation, const SDL_Event *event) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    if (legacy == NULL || event == NULL) {
        return false;
    }
    switch (event->type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (SDL_GetWindowFromEvent(event) != legacy->window) {
            return false;
        }
        (void)legacy_enqueue_structural_event(
            presentation, legacy, EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED,
            EIDOLON_PRESENTATION_GRAPHICS_RESET_NONE);
        return true;
    case SDL_EVENT_RENDER_TARGETS_RESET:
        if (SDL_GetWindowFromEvent(event) != legacy->window) {
            return false;
        }
        (void)legacy_enqueue_structural_event(
            presentation, legacy, EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED,
            EIDOLON_PRESENTATION_GRAPHICS_RESET_TARGETS);
        return true;
    case SDL_EVENT_RENDER_DEVICE_RESET:
    case SDL_EVENT_RENDER_DEVICE_LOST:
        if (SDL_GetWindowFromEvent(event) != legacy->window) {
            return false;
        }
        (void)legacy_enqueue_structural_event(
            presentation, legacy, EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED,
            EIDOLON_PRESENTATION_GRAPHICS_RESET_DEVICE);
        return true;
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        if (SDL_GetWindowFromEvent(event) != legacy->window) {
            return false;
        }
        legacy->environment_dirty = true;
        return true;
    case SDL_EVENT_DISPLAY_ADDED:
    case SDL_EVENT_DISPLAY_REMOVED:
    case SDL_EVENT_DISPLAY_MOVED:
    case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
    case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
    case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
        legacy->environment_dirty = true;
        return true;
    default:
        return false;
    }
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
