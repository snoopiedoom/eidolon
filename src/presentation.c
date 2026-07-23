#include "presentation.h"

#include "presentation_internal.h"

#include <SDL3/SDL.h>

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
