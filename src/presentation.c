#include "presentation.h"

#include "presentation_internal.h"

#include <SDL3/SDL.h>

#include <math.h>

#define EIDOLON_PRESENTATION_BACKEND_NAME_CAPACITY 48U
#define EIDOLON_PRESENTATION_TARGET_SLOT_COUNT 2U

typedef struct EidolonPresentationTargetResource {
    EidolonPresentationTarget target;
    uint64_t generation;
    uint32_t width;
    uint32_t height;
    EidolonPresentationAlphaMode alpha_mode;
    bool allocated;
} EidolonPresentationTargetResource;

typedef struct EidolonPresentationLayerTarget {
    EidolonSceneLayerId layer;
    EidolonPresentationTargetResource resources[EIDOLON_PRESENTATION_TARGET_SLOT_COUNT];
    uint64_t applied_content_revision;
    uint64_t requested_content_revision;
    unsigned int active_slot;
    unsigned int staging_slot;
    bool occupied;
    bool update_in_progress;
} EidolonPresentationLayerTarget;

struct EidolonPresentation {
    char backend_name[EIDOLON_PRESENTATION_BACKEND_NAME_CAPACITY];
    uint64_t capabilities;
    void *context;
    EidolonPresentationBackendOps operations;
    EidolonPresentationHost host;
    EidolonPresentationLayerTarget layer_targets[EIDOLON_SCENE_LAYER_CAPACITY];
    uint32_t next_target_id;
    uint64_t next_target_generation;
    uint64_t committed_scene_revision;
};

static EidolonPresentationLayerTarget *find_layer_target(EidolonPresentation *presentation,
                                                         EidolonSceneLayerId layer) {
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        EidolonPresentationLayerTarget *record = &presentation->layer_targets[index];
        if (record->occupied && record->layer.value == layer.value) {
            return record;
        }
    }
    return NULL;
}

static EidolonPresentationLayerTarget *allocate_layer_target(EidolonPresentation *presentation,
                                                             EidolonSceneLayerId layer) {
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        EidolonPresentationLayerTarget *record = &presentation->layer_targets[index];
        if (!record->occupied) {
            SDL_zero(*record);
            record->occupied = true;
            record->layer = layer;
            record->active_slot = EIDOLON_PRESENTATION_TARGET_SLOT_COUNT;
            record->staging_slot = EIDOLON_PRESENTATION_TARGET_SLOT_COUNT;
            return record;
        }
    }
    return NULL;
}

static void destroy_target_resource(EidolonPresentation *presentation,
                                    EidolonPresentationTargetResource *resource) {
    if (!resource->allocated) {
        return;
    }
    presentation->operations.destroy_target(presentation->context, resource->target);
    SDL_zero(*resource);
}

static void release_layer_target(EidolonPresentation *presentation,
                                 EidolonPresentationLayerTarget *record) {
    for (size_t index = 0U; index < EIDOLON_PRESENTATION_TARGET_SLOT_COUNT; ++index) {
        destroy_target_resource(presentation, &record->resources[index]);
    }
    SDL_zero(*record);
}

static bool scene_contains_layer(const EidolonSceneSnapshot *scene, EidolonSceneLayerId layer) {
    for (size_t index = 0U; index < scene->layer_count; ++index) {
        if (scene->layers[index].id.value == layer.value) {
            return true;
        }
    }
    return false;
}

static void build_scene_commit(EidolonPresentation *presentation, const EidolonSceneSnapshot *scene,
                               EidolonPresentationSceneCommit *commit) {
    SDL_zero(*commit);
    commit->revision = scene->revision;
    commit->layer_count = scene->layer_count;
    for (size_t index = 0U; index < scene->layer_count; ++index) {
        EidolonPresentationCommittedLayer *committed = &commit->layers[index];
        committed->scene = scene->layers[index];
        EidolonPresentationLayerTarget *record =
            find_layer_target(presentation, committed->scene.id);
        if (record == NULL || record->active_slot >= EIDOLON_PRESENTATION_TARGET_SLOT_COUNT) {
            continue;
        }
        const EidolonPresentationTargetResource *active = &record->resources[record->active_slot];
        if (!active->allocated) {
            continue;
        }
        committed->target = active->target;
        committed->target_generation = active->generation;
        committed->target_content_revision = record->applied_content_revision;
        committed->target_width = active->width;
        committed->target_height = active->height;
        committed->alpha_mode = active->alpha_mode;
        committed->has_target = true;
    }
}

static bool finite_rect(const EidolonPresentationRect *rect) {
    return isfinite(rect->x) && isfinite(rect->y) && isfinite(rect->width) &&
           isfinite(rect->height) && rect->width > 0.0F && rect->height > 0.0F;
}

static bool finite_insets(const EidolonPresentationInsets *insets) {
    return isfinite(insets->top) && isfinite(insets->right) && isfinite(insets->bottom) &&
           isfinite(insets->left) && insets->top >= 0.0F && insets->right >= 0.0F &&
           insets->bottom >= 0.0F && insets->left >= 0.0F;
}

static bool valid_orientation(EidolonPresentationOrientation orientation) {
    return orientation > EIDOLON_PRESENTATION_ORIENTATION_UNKNOWN &&
           orientation <= EIDOLON_PRESENTATION_ORIENTATION_PORTRAIT_FLIPPED;
}

static bool valid_optional_environment_fields(uint64_t valid_fields,
                                              const EidolonPresentationRect *output_bounds,
                                              const EidolonPresentationRect *usable_bounds,
                                              const EidolonPresentationInsets *safe_area,
                                              float content_scale, float pixel_scale,
                                              float nominal_refresh_hz,
                                              EidolonPresentationOrientation orientation,
                                              EidolonPresentationCoordinateSpace coordinate_space) {
    const uint64_t geometry_fields =
        EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY | EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
        EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS | EIDOLON_PRESENTATION_ENV_SAFE_AREA;
    if ((valid_fields & ~EIDOLON_PRESENTATION_ENV_ALL_FIELDS) != 0U) {
        return false;
    }
    if ((valid_fields & geometry_fields) != 0U &&
        (valid_fields & EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE) == 0U) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE) != 0U &&
        (coordinate_space <= EIDOLON_PRESENTATION_COORDINATE_SPACE_UNKNOWN ||
         coordinate_space > EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL)) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS) != 0U &&
        !finite_rect(output_bounds)) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS) != 0U &&
        !finite_rect(usable_bounds)) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_SAFE_AREA) != 0U && !finite_insets(safe_area)) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_CONTENT_SCALE) != 0U &&
        (!isfinite(content_scale) || content_scale <= 0.0F)) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_PIXEL_SCALE) != 0U &&
        (!isfinite(pixel_scale) || pixel_scale <= 0.0F)) {
        return false;
    }
    if ((valid_fields & EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH) != 0U &&
        (!isfinite(nominal_refresh_hz) || nominal_refresh_hz <= 0.0F)) {
        return false;
    }
    return (valid_fields & EIDOLON_PRESENTATION_ENV_ORIENTATION) == 0U ||
           valid_orientation(orientation);
}

static bool valid_environment(const EidolonPresentation *presentation,
                              const EidolonPresentationEnvironment *environment) {
    return environment->revision > 0U && environment->host.value == presentation->host.value &&
           (environment->valid_fields & EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY) != 0U &&
           environment->host_geometry.width > 0 && environment->host_geometry.height > 0 &&
           ((environment->valid_fields & EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT) != 0U) ==
               (environment->active_output.value != 0U) &&
           ((environment->valid_fields & EIDOLON_PRESENTATION_ENV_OUTPUT_TOPOLOGY) != 0U) ==
               (environment->topology_revision != 0U) &&
           (environment->changed_fields & ~EIDOLON_PRESENTATION_ENV_ALL_FIELDS) == 0U &&
           valid_optional_environment_fields(
               environment->valid_fields, &environment->output_bounds, &environment->usable_bounds,
               &environment->safe_area, environment->content_scale, environment->pixel_scale,
               environment->nominal_refresh_hz, environment->orientation,
               environment->coordinate_space);
}

static bool valid_output_info(const EidolonPresentationOutputInfo *output) {
    const uint64_t allowed_fields =
        EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS | EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS |
        EIDOLON_PRESENTATION_ENV_SAFE_AREA | EIDOLON_PRESENTATION_ENV_CONTENT_SCALE |
        EIDOLON_PRESENTATION_ENV_PIXEL_SCALE | EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH |
        EIDOLON_PRESENTATION_ENV_ORIENTATION | EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE;
    return output->output.value != 0U &&
           (output->flags & ~(uint64_t)EIDOLON_PRESENTATION_OUTPUT_PRIMARY) == 0U &&
           (output->valid_fields & ~allowed_fields) == 0U &&
           valid_optional_environment_fields(
               output->valid_fields, &output->bounds, &output->usable_bounds, &output->safe_area,
               output->content_scale, output->pixel_scale, output->nominal_refresh_hz,
               output->orientation, output->coordinate_space);
}

static EidolonPresentationTopologyResult topology_error(const char *message) {
    SDL_SetError("%s", message);
    return (EidolonPresentationTopologyResult){
        .status = EIDOLON_PRESENTATION_TOPOLOGY_ERROR,
    };
}

EidolonPresentation *
eidolon_presentation_create_backend(const char *backend_name, uint64_t capabilities, void *context,
                                    const EidolonPresentationBackendOps *operations) {
    if (backend_name == NULL || backend_name[0] == '\0' || context == NULL || operations == NULL ||
        operations->destroy == NULL || operations->present == NULL) {
        SDL_SetError("invalid presentation backend contract");
        return NULL;
    }
    EidolonPresentation *presentation = SDL_calloc(1U, sizeof(*presentation));
    if (presentation == NULL) {
        return NULL;
    }
    SDL_strlcpy(presentation->backend_name, backend_name, sizeof(presentation->backend_name));
    presentation->capabilities = capabilities;
    presentation->context = context;
    presentation->operations = *operations;
    presentation->host.value = 1U;
    presentation->next_target_id = 1U;
    presentation->next_target_generation = 1U;
    return presentation;
}

void eidolon_presentation_destroy(EidolonPresentation *presentation) {
    if (presentation == NULL) {
        return;
    }
    if (presentation->operations.destroy_target != NULL) {
        for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
            if (presentation->layer_targets[index].occupied) {
                release_layer_target(presentation, &presentation->layer_targets[index]);
            }
        }
    }
    presentation->operations.destroy(presentation->context);
    SDL_free(presentation);
}

const char *eidolon_presentation_backend_name(const EidolonPresentation *presentation) {
    return presentation != NULL ? presentation->backend_name : "none";
}

uint64_t eidolon_presentation_capabilities(const EidolonPresentation *presentation) {
    return presentation != NULL ? presentation->capabilities : 0U;
}

bool eidolon_presentation_supports(const EidolonPresentation *presentation,
                                   EidolonPresentationCapability capability) {
    return presentation != NULL &&
           (presentation->capabilities & (uint64_t)capability) == (uint64_t)capability;
}

EidolonPresentationHost eidolon_presentation_host(const EidolonPresentation *presentation) {
    return presentation != NULL ? presentation->host : (EidolonPresentationHost){0U};
}

bool eidolon_presentation_configure_host(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.configure_host != NULL &&
           presentation->operations.configure_host(presentation->context);
}

bool eidolon_presentation_get_geometry(EidolonPresentation *presentation,
                                       EidolonPresentationGeometry *geometry) {
    return presentation != NULL && geometry != NULL &&
           presentation->operations.get_geometry != NULL &&
           presentation->operations.get_geometry(presentation->context, geometry);
}

bool eidolon_presentation_set_geometry(EidolonPresentation *presentation,
                                       const EidolonPresentationGeometry *geometry) {
    return presentation != NULL && geometry != NULL && geometry->width > 0 &&
           geometry->height > 0 && presentation->operations.set_geometry != NULL &&
           presentation->operations.set_geometry(presentation->context, geometry);
}

bool eidolon_presentation_sync_host(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.sync_host != NULL &&
           presentation->operations.sync_host(presentation->context);
}

float eidolon_presentation_display_scale(EidolonPresentation *presentation) {
    if (presentation == NULL || presentation->operations.display_scale == NULL) {
        return 1.0F;
    }
    return presentation->operations.display_scale(presentation->context);
}

bool eidolon_presentation_set_vsync(EidolonPresentation *presentation, int interval) {
    return presentation != NULL && presentation->operations.set_vsync != NULL &&
           presentation->operations.set_vsync(presentation->context, interval);
}

bool eidolon_presentation_begin_interactive_move(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.begin_interactive_move != NULL &&
           presentation->operations.begin_interactive_move(presentation->context);
}

void eidolon_presentation_suspend_input_region(EidolonPresentation *presentation) {
    if (presentation != NULL && presentation->operations.suspend_input_region != NULL) {
        presentation->operations.suspend_input_region(presentation->context);
    }
}

bool eidolon_presentation_update_input_region(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.update_input_region != NULL &&
           presentation->operations.update_input_region(presentation->context);
}

bool eidolon_presentation_poll_event(EidolonPresentation *presentation,
                                     EidolonPresentationEvent *event) {
    if (presentation == NULL || event == NULL || presentation->operations.poll_event == NULL) {
        return false;
    }
    EidolonPresentationEvent next;
    SDL_zero(next);
    if (!presentation->operations.poll_event(presentation->context, &next)) {
        return false;
    }
    if (next.kind <= EIDOLON_PRESENTATION_EVENT_NONE ||
        next.kind > EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED || next.sequence == 0U ||
        next.host.value != presentation->host.value) {
        SDL_SetError("invalid presentation event");
        return false;
    }
    switch (next.kind) {
    case EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED:
    case EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED:
        if (next.data.layer.layer.value == 0U) {
            SDL_SetError("invalid presentation layer event");
            return false;
        }
        break;
    case EIDOLON_PRESENTATION_EVENT_MOVE_STARTED:
    case EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED:
    case EIDOLON_PRESENTATION_EVENT_MOVE_CANCELED:
        if (next.data.move.layer.value == 0U) {
            SDL_SetError("invalid presentation move event");
            return false;
        }
        break;
    case EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED:
        if (!valid_environment(presentation, &next.data.environment.environment)) {
            SDL_SetError("invalid presentation environment event");
            return false;
        }
        break;
    case EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED:
    case EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED:
    case EIDOLON_PRESENTATION_EVENT_NONE:
        break;
    }
    *event = next;
    return true;
}

bool eidolon_presentation_get_environment(EidolonPresentation *presentation,
                                          EidolonPresentationEnvironment *environment) {
    if (presentation == NULL || environment == NULL ||
        presentation->operations.get_environment == NULL) {
        return false;
    }
    EidolonPresentationEnvironment current;
    SDL_zero(current);
    if (!presentation->operations.get_environment(presentation->context, &current)) {
        return false;
    }
    if (!valid_environment(presentation, &current)) {
        SDL_SetError("invalid presentation environment");
        return false;
    }
    *environment = current;
    return true;
}

EidolonPresentationTopologyResult
eidolon_presentation_copy_outputs(EidolonPresentation *presentation,
                                  EidolonPresentationOutputInfo *outputs, size_t capacity) {
    if (presentation == NULL || (capacity > 0U && outputs == NULL)) {
        return topology_error("invalid presentation topology destination");
    }
    if (presentation->operations.copy_outputs == NULL) {
        return (EidolonPresentationTopologyResult){
            .status = EIDOLON_PRESENTATION_TOPOLOGY_UNAVAILABLE,
        };
    }
    EidolonPresentationTopologyResult result =
        presentation->operations.copy_outputs(presentation->context, outputs, capacity);
    if (result.status < EIDOLON_PRESENTATION_TOPOLOGY_OK ||
        result.status > EIDOLON_PRESENTATION_TOPOLOGY_ERROR || result.copied_count > capacity ||
        result.required_count < result.copied_count) {
        return topology_error("invalid presentation topology result");
    }
    if (result.status == EIDOLON_PRESENTATION_TOPOLOGY_UNAVAILABLE ||
        result.status == EIDOLON_PRESENTATION_TOPOLOGY_ERROR) {
        return result;
    }
    if (result.revision == 0U ||
        (result.status == EIDOLON_PRESENTATION_TOPOLOGY_OK &&
         result.required_count != result.copied_count) ||
        (result.status == EIDOLON_PRESENTATION_TOPOLOGY_INSUFFICIENT_CAPACITY &&
         result.required_count <= capacity) ||
        (result.status == EIDOLON_PRESENTATION_TOPOLOGY_CHANGED && result.copied_count != 0U)) {
        return topology_error("incoherent presentation topology result");
    }
    for (size_t index = 0U; index < result.copied_count; ++index) {
        if (!valid_output_info(&outputs[index])) {
            return topology_error("invalid presentation output");
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (outputs[previous].output.value == outputs[index].output.value) {
                return topology_error("duplicate presentation output");
            }
        }
    }
    return result;
}

bool eidolon_presentation_begin_target_update(EidolonPresentation *presentation,
                                              EidolonSceneLayerId layer, uint32_t width,
                                              uint32_t height,
                                              EidolonPresentationAlphaMode alpha_mode,
                                              uint64_t content_revision,
                                              EidolonPresentationTargetUpdate *update) {
    if (presentation == NULL || layer.value == 0U || width == 0U || height == 0U ||
        (alpha_mode != EIDOLON_PRESENTATION_ALPHA_STRAIGHT &&
         alpha_mode != EIDOLON_PRESENTATION_ALPHA_PREMULTIPLIED) ||
        content_revision == 0U || update == NULL ||
        presentation->operations.create_target == NULL ||
        presentation->operations.destroy_target == NULL) {
        SDL_SetError("invalid presentation target update");
        return false;
    }
    EidolonPresentationLayerTarget *record = find_layer_target(presentation, layer);
    if (record == NULL) {
        record = allocate_layer_target(presentation, layer);
    }
    if (record == NULL || record->update_in_progress ||
        content_revision < record->applied_content_revision) {
        SDL_SetError("stale or concurrent presentation target update");
        return false;
    }
    if (record->active_slot < EIDOLON_PRESENTATION_TARGET_SLOT_COUNT) {
        const EidolonPresentationTargetResource *active = &record->resources[record->active_slot];
        if (active->allocated && active->width == width && active->height == height &&
            active->alpha_mode == alpha_mode &&
            record->applied_content_revision == content_revision) {
            *update = (EidolonPresentationTargetUpdate){
                .target = active->target,
                .generation = active->generation,
                .content_revision = content_revision,
                .width = width,
                .height = height,
                .alpha_mode = alpha_mode,
                .redraw_required = false,
            };
            return true;
        }
    }

    const unsigned int staging_slot = record->active_slot == 0U ? 1U : 0U;
    EidolonPresentationTargetResource *staging = &record->resources[staging_slot];
    if (staging->allocated && (staging->width != width || staging->height != height ||
                               staging->alpha_mode != alpha_mode)) {
        destroy_target_resource(presentation, staging);
    }
    if (!staging->allocated) {
        const EidolonPresentationTarget target = {presentation->next_target_id++};
        const uint64_t generation = presentation->next_target_generation++;
        if (!presentation->operations.create_target(presentation->context, layer, target,
                                                    generation, width, height, alpha_mode)) {
            return false;
        }
        *staging = (EidolonPresentationTargetResource){
            .target = target,
            .generation = generation,
            .width = width,
            .height = height,
            .alpha_mode = alpha_mode,
            .allocated = true,
        };
    }
    record->staging_slot = staging_slot;
    record->requested_content_revision = content_revision;
    record->update_in_progress = true;
    *update = (EidolonPresentationTargetUpdate){
        .target = staging->target,
        .generation = staging->generation,
        .content_revision = content_revision,
        .width = width,
        .height = height,
        .alpha_mode = alpha_mode,
        .redraw_required = true,
    };
    return true;
}

bool eidolon_presentation_finish_target_update(EidolonPresentation *presentation,
                                               const EidolonPresentationTargetUpdate *update,
                                               bool content_valid) {
    if (presentation == NULL || update == NULL || !update->redraw_required) {
        SDL_SetError("invalid completed presentation target update");
        return false;
    }
    EidolonPresentationLayerTarget *record = NULL;
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        EidolonPresentationLayerTarget *candidate = &presentation->layer_targets[index];
        if (candidate->occupied && candidate->update_in_progress &&
            candidate->staging_slot < EIDOLON_PRESENTATION_TARGET_SLOT_COUNT) {
            const EidolonPresentationTargetResource *staging =
                &candidate->resources[candidate->staging_slot];
            if (staging->target.value == update->target.value &&
                staging->generation == update->generation) {
                record = candidate;
                break;
            }
        }
    }
    if (record == NULL || record->requested_content_revision != update->content_revision) {
        SDL_SetError("presentation target update does not match staging state");
        return false;
    }
    if (content_valid && presentation->operations.submit_target != NULL &&
        !presentation->operations.submit_target(presentation->context, update->target,
                                                update->generation)) {
        record->staging_slot = EIDOLON_PRESENTATION_TARGET_SLOT_COUNT;
        record->requested_content_revision = 0U;
        record->update_in_progress = false;
        return false;
    }
    if (content_valid) {
        record->active_slot = record->staging_slot;
        record->applied_content_revision = record->requested_content_revision;
    }
    record->staging_slot = EIDOLON_PRESENTATION_TARGET_SLOT_COUNT;
    record->requested_content_revision = 0U;
    record->update_in_progress = false;
    return true;
}

bool eidolon_presentation_set_target_alpha_mask(EidolonPresentation *presentation,
                                                const EidolonPresentationTargetUpdate *update,
                                                const uint8_t *pixels, size_t pitch,
                                                uint8_t pixel_stride, uint8_t alpha_offset) {
    if (presentation == NULL || update == NULL || !update->redraw_required || pixels == NULL ||
        pixel_stride == 0U || alpha_offset >= pixel_stride ||
        pitch < (size_t)update->width * (size_t)pixel_stride) {
        SDL_SetError("invalid presentation target alpha mask");
        return false;
    }
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        EidolonPresentationLayerTarget *record = &presentation->layer_targets[index];
        if (!record->occupied || !record->update_in_progress ||
            record->staging_slot >= EIDOLON_PRESENTATION_TARGET_SLOT_COUNT) {
            continue;
        }
        const EidolonPresentationTargetResource *staging = &record->resources[record->staging_slot];
        if (staging->target.value != update->target.value ||
            staging->generation != update->generation || staging->width != update->width ||
            staging->height != update->height) {
            continue;
        }
        return presentation->operations.set_target_alpha_mask == NULL ||
               presentation->operations.set_target_alpha_mask(presentation->context, update->target,
                                                              update->generation, pixels, pitch,
                                                              pixel_stride, alpha_offset);
    }
    SDL_SetError("presentation target alpha mask does not match staging state");
    return false;
}

bool eidolon_presentation_target_for_layer(EidolonPresentation *presentation,
                                           EidolonSceneLayerId layer,
                                           EidolonPresentationTargetUpdate *target) {
    EidolonPresentationLayerTarget *record =
        presentation != NULL ? find_layer_target(presentation, layer) : NULL;
    if (record == NULL || target == NULL ||
        record->active_slot >= EIDOLON_PRESENTATION_TARGET_SLOT_COUNT) {
        return false;
    }
    const EidolonPresentationTargetResource *active = &record->resources[record->active_slot];
    if (!active->allocated) {
        return false;
    }
    *target = (EidolonPresentationTargetUpdate){
        .target = active->target,
        .generation = active->generation,
        .content_revision = record->applied_content_revision,
        .width = active->width,
        .height = active->height,
        .alpha_mode = active->alpha_mode,
        .redraw_required = false,
    };
    return true;
}

void eidolon_presentation_release_target(EidolonPresentation *presentation,
                                         EidolonSceneLayerId layer) {
    EidolonPresentationLayerTarget *record =
        presentation != NULL ? find_layer_target(presentation, layer) : NULL;
    if (record != NULL && presentation->operations.destroy_target != NULL) {
        release_layer_target(presentation, record);
    }
}

bool eidolon_presentation_commit_scene(EidolonPresentation *presentation,
                                       const EidolonSceneSnapshot *scene) {
    if (presentation == NULL || scene == NULL || scene->revision == 0U ||
        scene->revision < presentation->committed_scene_revision) {
        SDL_SetError("stale or invalid presentation scene");
        return false;
    }
    if (scene->revision == presentation->committed_scene_revision) {
        return true;
    }
    if (presentation->operations.commit_scene != NULL) {
        EidolonPresentationSceneCommit commit;
        build_scene_commit(presentation, scene, &commit);
        if (!presentation->operations.commit_scene(presentation->context, &commit)) {
            return false;
        }
    }
    if (presentation->operations.destroy_target != NULL) {
        for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
            EidolonPresentationLayerTarget *record = &presentation->layer_targets[index];
            if (record->occupied && !scene_contains_layer(scene, record->layer)) {
                release_layer_target(presentation, record);
            }
        }
    }
    presentation->committed_scene_revision = scene->revision;
    return true;
}

bool eidolon_presentation_present(EidolonPresentation *presentation) {
    return presentation != NULL && presentation->operations.present(presentation->context);
}

void *eidolon_presentation_backend_context(EidolonPresentation *presentation,
                                           const char *backend_name) {
    if (presentation == NULL || backend_name == NULL ||
        SDL_strcmp(presentation->backend_name, backend_name) != 0) {
        SDL_SetError("presentation backend mismatch");
        return NULL;
    }
    return presentation->context;
}
