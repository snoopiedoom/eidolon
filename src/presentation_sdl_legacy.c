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
    EidolonSceneLayerSnapshot input_layers[EIDOLON_SCENE_LAYER_CAPACITY];
    size_t output_count;
    size_t input_layer_count;
    uint64_t topology_revision;
    uint64_t input_scene_revision;
    uint64_t pointer_scene_revision;
    uint64_t capabilities;
    EidolonSceneLayerSnapshot pointer_layer;
    float pointer_host_x;
    float pointer_host_y;
    float pointer_layer_x;
    float pointer_layer_y;
    bool environment_valid;
    bool environment_dirty;
    bool pointer_routing;
} EidolonSdlLegacyPresentation;

static void legacy_poll_routed_pointer(EidolonSdlLegacyPresentation *legacy);

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
    legacy_poll_routed_pointer(legacy);
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
    EidolonSdlLegacyPresentation *legacy = opaque;
    legacy->input_scene_revision = commit->revision;
    legacy->input_layer_count = commit->layer_count;
    for (size_t index = 0U; index < commit->layer_count; ++index) {
        legacy->input_layers[index] = commit->layers[index].scene;
    }
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

static float legacy_coordinate_scale(const EidolonSdlLegacyPresentation *legacy) {
    if (!legacy->environment_valid ||
        (legacy->environment.valid_fields & EIDOLON_PRESENTATION_ENV_CONTENT_SCALE) == 0U ||
        (legacy->environment.valid_fields & EIDOLON_PRESENTATION_ENV_PIXEL_SCALE) == 0U ||
        legacy->environment.pixel_scale <= 0.0F) {
        return 1.0F;
    }
    return legacy->environment.content_scale / legacy->environment.pixel_scale;
}

static bool legacy_map_layer(const EidolonSceneLayerSnapshot *layer, float global_x, float global_y,
                             float *layer_x, float *layer_y) {
    if (layer == NULL || layer_x == NULL || layer_y == NULL || layer->content_width == 0U ||
        layer->content_height == 0U || layer->bounds.width <= 0.0F ||
        layer->bounds.height <= 0.0F) {
        return false;
    }
    const float source_pivot_x = (float)layer->content_width * layer->pivot_x;
    const float source_pivot_y = (float)layer->content_height * layer->pivot_y;
    const float destination_pivot_x = layer->bounds.width * layer->pivot_x;
    const float destination_pivot_y = layer->bounds.height * layer->pivot_y;
    const float scale_x = layer->bounds.width / (float)layer->content_width;
    const float scale_y = layer->bounds.height / (float)layer->content_height;
    const float radians = layer->rotation_degrees * SDL_PI_F / 180.0F;
    const float cosine = SDL_cosf(radians);
    const float sine = SDL_sinf(radians);
    const float matrix_11 = scale_x * cosine;
    const float matrix_12 = scale_x * sine;
    const float matrix_21 = -scale_y * sine;
    const float matrix_22 = scale_y * cosine;
    const float matrix_31 =
        destination_pivot_x - (source_pivot_x * scale_x * cosine - source_pivot_y * scale_y * sine);
    const float matrix_32 =
        destination_pivot_y - (source_pivot_x * scale_x * sine + source_pivot_y * scale_y * cosine);
    const float determinant = matrix_11 * matrix_22 - matrix_12 * matrix_21;
    if (SDL_fabsf(determinant) < 0.000001F) {
        return false;
    }
    const float translated_x = global_x - layer->bounds.x - matrix_31;
    const float translated_y = global_y - layer->bounds.y - matrix_32;
    *layer_x = (translated_x * matrix_22 - translated_y * matrix_21) / determinant;
    *layer_y = (translated_y * matrix_11 - translated_x * matrix_12) / determinant;
    return true;
}

static const EidolonSceneLayerSnapshot *
legacy_routed_layer(EidolonSdlLegacyPresentation *legacy, float global_x, float global_y,
                    float *layer_x, float *layer_y) {
    const EidolonSceneLayerSnapshot *result = NULL;
    int32_t result_z = INT32_MIN;
    for (size_t index = 0U; index < legacy->input_layer_count; ++index) {
        const EidolonSceneLayerSnapshot *candidate = &legacy->input_layers[index];
        if (!candidate->visible ||
            (candidate->interaction & EIDOLON_SCENE_INTERACTION_ROUTE_POINTER) == 0U ||
            candidate->z_order < result_z) {
            continue;
        }
        float candidate_x = 0.0F;
        float candidate_y = 0.0F;
        if (!legacy_map_layer(candidate, global_x, global_y, &candidate_x, &candidate_y) ||
            candidate_x < 0.0F || candidate_y < 0.0F ||
            candidate_x >= (float)candidate->content_width ||
            candidate_y >= (float)candidate->content_height) {
            continue;
        }
        result = candidate;
        result_z = candidate->z_order;
        *layer_x = candidate_x;
        *layer_y = candidate_y;
    }
    return result;
}

static uint64_t legacy_pointer_buttons(SDL_MouseButtonFlags state) {
    uint64_t buttons = 0U;
    if ((state & SDL_BUTTON_LMASK) != 0U) {
        buttons |= EIDOLON_PRESENTATION_POINTER_BUTTON_PRIMARY;
    }
    if ((state & SDL_BUTTON_MMASK) != 0U) {
        buttons |= EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE;
    }
    if ((state & SDL_BUTTON_RMASK) != 0U) {
        buttons |= EIDOLON_PRESENTATION_POINTER_BUTTON_SECONDARY;
    }
    return buttons;
}

static uint64_t legacy_pointer_modifiers(void) {
    return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0U
               ? EIDOLON_PRESENTATION_POINTER_MODIFIER_SHIFT
               : 0U;
}

static bool legacy_enqueue_pointer_event(EidolonSdlLegacyPresentation *legacy,
                                         EidolonPresentationEventKind kind, float host_x,
                                         float host_y, float layer_x, float layer_y,
                                         uint64_t buttons, uint32_t click_count) {
    EidolonPresentationEvent event;
    SDL_zero(event);
    event.kind = kind;
    event.monotonic_ns = SDL_GetTicksNS();
    event.host.value = 1U;
    event.data.pointer = (EidolonPresentationPointerEvent){
        .scene_revision = legacy->pointer_scene_revision,
        .pointer_id = 1U,
        .buttons = buttons,
        .modifiers = legacy_pointer_modifiers(),
        .valid_coordinates =
            EIDOLON_PRESENTATION_POINTER_COORDINATE_HOST |
            EIDOLON_PRESENTATION_POINTER_COORDINATE_LAYER |
            EIDOLON_PRESENTATION_POINTER_COORDINATE_GLOBAL,
        .layer = legacy->pointer_layer.id,
        .device_kind = EIDOLON_PRESENTATION_POINTER_DEVICE_MOUSE,
        .click_count = click_count,
        .host_x = host_x,
        .host_y = host_y,
        .layer_x = layer_x,
        .layer_y = layer_y,
        .layer_x_relative = layer_x - legacy->pointer_layer_x,
        .layer_y_relative = layer_y - legacy->pointer_layer_y,
        .global_x =
            (float)legacy->environment.host_geometry.x + host_x * legacy_coordinate_scale(legacy),
        .global_y =
            (float)legacy->environment.host_geometry.y + host_y * legacy_coordinate_scale(legacy),
    };
    const bool accepted = eidolon_presentation_event_queue_push(&legacy->event_queue, &event);
    legacy->pointer_host_x = host_x;
    legacy->pointer_host_y = host_y;
    legacy->pointer_layer_x = layer_x;
    legacy->pointer_layer_y = layer_y;
    return accepted;
}

static void legacy_stop_pointer_routing(EidolonSdlLegacyPresentation *legacy,
                                        bool release_capture) {
    legacy->pointer_routing = false;
    legacy->pointer_scene_revision = 0U;
    SDL_zero(legacy->pointer_layer);
    if (release_capture) {
        (void)SDL_CaptureMouse(false);
    }
}

static void legacy_poll_routed_pointer(EidolonSdlLegacyPresentation *legacy) {
    if (legacy == NULL || !legacy->pointer_routing) {
        return;
    }
    float global_x = 0.0F;
    float global_y = 0.0F;
    const SDL_MouseButtonFlags state = SDL_GetGlobalMouseState(&global_x, &global_y);
    const float scale = legacy_coordinate_scale(legacy);
    const float host_x =
        (global_x - (float)legacy->environment.host_geometry.x) / scale;
    const float host_y =
        (global_y - (float)legacy->environment.host_geometry.y) / scale;
    float layer_x = 0.0F;
    float layer_y = 0.0F;
    if (!legacy_map_layer(&legacy->pointer_layer, global_x, global_y, &layer_x, &layer_y)) {
        legacy_stop_pointer_routing(legacy, true);
        return;
    }
    if ((state & SDL_BUTTON_MMASK) == 0U) {
        (void)legacy_enqueue_pointer_event(
            legacy, EIDOLON_PRESENTATION_EVENT_POINTER_UP, host_x, host_y, layer_x, layer_y,
            legacy_pointer_buttons(state), 0U);
        legacy_stop_pointer_routing(legacy, true);
        return;
    }
    if (host_x == legacy->pointer_host_x && host_y == legacy->pointer_host_y) {
        return;
    }
    if (!legacy_enqueue_pointer_event(
            legacy, EIDOLON_PRESENTATION_EVENT_POINTER_MOTION, host_x, host_y, layer_x, layer_y,
            legacy_pointer_buttons(state), 0U)) {
        legacy_stop_pointer_routing(legacy, true);
    }
}

bool eidolon_sdl_legacy_handle_event(EidolonPresentation *presentation, const SDL_Event *event) {
    EidolonSdlLegacyPresentation *legacy = legacy_context(presentation);
    if (legacy == NULL || event == NULL) {
        return false;
    }
    switch (event->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (SDL_GetWindowFromEvent(event) != legacy->window ||
            event->button.button != SDL_BUTTON_MIDDLE || legacy->pointer_routing) {
            return false;
        } else {
            SDL_Event converted = *event;
            if (!SDL_ConvertEventToRenderCoordinates(legacy->renderer, &converted)) {
                return false;
            }
            const float scale = legacy_coordinate_scale(legacy);
            const float global_x =
                (float)legacy->environment.host_geometry.x + converted.button.x * scale;
            const float global_y =
                (float)legacy->environment.host_geometry.y + converted.button.y * scale;
            float layer_x = 0.0F;
            float layer_y = 0.0F;
            const EidolonSceneLayerSnapshot *layer =
                legacy_routed_layer(legacy, global_x, global_y, &layer_x, &layer_y);
            if (layer == NULL) {
                return false;
            }
            legacy->pointer_layer = *layer;
            legacy->pointer_scene_revision = legacy->input_scene_revision;
            legacy->pointer_routing = true;
            legacy->pointer_host_x = converted.button.x;
            legacy->pointer_host_y = converted.button.y;
            legacy->pointer_layer_x = layer_x;
            legacy->pointer_layer_y = layer_y;
            if (!SDL_CaptureMouse(true)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Could not capture routed SDL pointer: %s", SDL_GetError());
                legacy_stop_pointer_routing(legacy, false);
                return true;
            }
            if (!legacy_enqueue_pointer_event(
                    legacy, EIDOLON_PRESENTATION_EVENT_POINTER_DOWN, converted.button.x,
                    converted.button.y, layer_x, layer_y,
                    legacy_pointer_buttons(SDL_GetMouseState(NULL, NULL)) |
                        EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE,
                    converted.button.clicks)) {
                legacy_stop_pointer_routing(legacy, true);
            }
            return true;
        }
    case SDL_EVENT_MOUSE_MOTION:
        if (!legacy->pointer_routing) {
            return false;
        } else {
            SDL_Event converted = *event;
            if (!SDL_ConvertEventToRenderCoordinates(legacy->renderer, &converted)) {
                legacy_stop_pointer_routing(legacy, true);
                return true;
            }
            const float scale = legacy_coordinate_scale(legacy);
            const float global_x =
                (float)legacy->environment.host_geometry.x + converted.motion.x * scale;
            const float global_y =
                (float)legacy->environment.host_geometry.y + converted.motion.y * scale;
            float layer_x = 0.0F;
            float layer_y = 0.0F;
            if (!legacy_map_layer(&legacy->pointer_layer, global_x, global_y, &layer_x, &layer_y) ||
                !legacy_enqueue_pointer_event(
                    legacy, EIDOLON_PRESENTATION_EVENT_POINTER_MOTION, converted.motion.x,
                    converted.motion.y, layer_x, layer_y,
                    legacy_pointer_buttons(converted.motion.state), 0U)) {
                legacy_stop_pointer_routing(legacy, true);
            }
            return true;
        }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button != SDL_BUTTON_MIDDLE || !legacy->pointer_routing) {
            return false;
        } else {
            SDL_Event converted = *event;
            if (SDL_ConvertEventToRenderCoordinates(legacy->renderer, &converted)) {
                const float scale = legacy_coordinate_scale(legacy);
                const float global_x =
                    (float)legacy->environment.host_geometry.x + converted.button.x * scale;
                const float global_y =
                    (float)legacy->environment.host_geometry.y + converted.button.y * scale;
                float layer_x = 0.0F;
                float layer_y = 0.0F;
                if (legacy_map_layer(&legacy->pointer_layer, global_x, global_y, &layer_x,
                                     &layer_y)) {
                    (void)legacy_enqueue_pointer_event(
                        legacy, EIDOLON_PRESENTATION_EVENT_POINTER_UP, converted.button.x,
                        converted.button.y, layer_x, layer_y,
                        legacy_pointer_buttons(SDL_GetMouseState(NULL, NULL)) &
                            ~(uint64_t)EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE,
                        0U);
                }
            }
            legacy_stop_pointer_routing(legacy, true);
            return true;
        }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (SDL_GetWindowFromEvent(event) == legacy->window && legacy->pointer_routing) {
            (void)legacy_enqueue_pointer_event(
                legacy, EIDOLON_PRESENTATION_EVENT_POINTER_CANCELED, legacy->pointer_host_x,
                legacy->pointer_host_y, legacy->pointer_layer_x, legacy->pointer_layer_y, 0U, 0U);
            legacy_stop_pointer_routing(legacy, true);
        }
        return false;
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
