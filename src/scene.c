#include "scene.h"

#include <SDL3/SDL.h>

static bool rect_equal(EidolonSceneRect left, EidolonSceneRect right) {
    return left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height;
}

static bool presentation_equal(const EidolonSceneLayerSnapshot *previous,
                               const EidolonSceneLayerInput *next) {
    return rect_equal(previous->bounds, next->bounds) &&
           previous->rotation_degrees == next->rotation_degrees &&
           previous->pivot_x == next->pivot_x && previous->pivot_y == next->pivot_y &&
           previous->opacity == next->opacity && previous->z_order == next->z_order &&
           previous->visible == next->visible && previous->interaction == next->interaction;
}

static EidolonSceneLayerRecord *find_record(EidolonScene *scene, uint64_t stable_key) {
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        if (scene->layers[index].occupied &&
            scene->layers[index].snapshot.stable_key == stable_key) {
            return &scene->layers[index];
        }
    }
    return NULL;
}

static EidolonSceneLayerRecord *allocate_record(EidolonScene *scene) {
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        if (!scene->layers[index].occupied) {
            return &scene->layers[index];
        }
    }
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        if (!scene->layers[index].published && !scene->layers[index].snapshot.visible) {
            return &scene->layers[index];
        }
    }
    return NULL;
}

static bool input_contains_key(const EidolonSceneLayerInput *layers, size_t layer_count,
                               uint64_t stable_key) {
    for (size_t index = 0U; index < layer_count; ++index) {
        if (layers[index].stable_key == stable_key) {
            return true;
        }
    }
    return false;
}

void eidolon_scene_init(EidolonScene *scene) {
    if (scene == NULL) {
        return;
    }
    SDL_zero(*scene);
    scene->next_layer_id = 1U;
}

bool eidolon_scene_publish(EidolonScene *scene, const EidolonSceneLayerInput *layers,
                           size_t layer_count, EidolonSceneSnapshot *snapshot) {
    if (scene == NULL || snapshot == NULL || layer_count > EIDOLON_SCENE_LAYER_CAPACITY ||
        (layer_count > 0U && layers == NULL)) {
        SDL_SetError("invalid scene publication");
        return false;
    }
    for (size_t index = 0U; index < layer_count; ++index) {
        if (layers[index].stable_key == 0U || layers[index].content_width == 0U ||
            layers[index].content_height == 0U || layers[index].bounds.width < 0.0F ||
            layers[index].bounds.height < 0.0F || layers[index].opacity < 0.0F ||
            layers[index].opacity > 1.0F || layers[index].pivot_x < 0.0F ||
            layers[index].pivot_x > 1.0F || layers[index].pivot_y < 0.0F ||
            layers[index].pivot_y > 1.0F ||
            layers[index].interaction > EIDOLON_SCENE_INTERACTION_ROUTE_POINTER) {
            SDL_SetError("invalid scene layer");
            return false;
        }
        for (size_t duplicate = 0U; duplicate < index; ++duplicate) {
            if (layers[duplicate].stable_key == layers[index].stable_key) {
                SDL_SetError("duplicate scene layer key");
                return false;
            }
        }
    }

    bool changed = false;
    for (size_t index = 0U; index < EIDOLON_SCENE_LAYER_CAPACITY; ++index) {
        EidolonSceneLayerRecord *record = &scene->layers[index];
        if (record->occupied && record->published &&
            !input_contains_key(layers, layer_count, record->snapshot.stable_key)) {
            record->published = false;
            if (record->snapshot.visible) {
                record->snapshot.visible = false;
                ++record->snapshot.presentation_revision;
            }
            changed = true;
        }
    }

    SDL_zero(*snapshot);
    snapshot->layer_count = layer_count;
    for (size_t index = 0U; index < layer_count; ++index) {
        const EidolonSceneLayerInput *input = &layers[index];
        EidolonSceneLayerRecord *record = find_record(scene, input->stable_key);
        if (record == NULL) {
            record = allocate_record(scene);
            if (record == NULL) {
                SDL_SetError("scene layer capacity exhausted");
                return false;
            }
            SDL_zero(*record);
            record->occupied = true;
            record->snapshot.id.value = scene->next_layer_id++;
            record->snapshot.stable_key = input->stable_key;
            record->snapshot.content_revision = 1U;
            record->snapshot.presentation_revision = 1U;
            changed = true;
        } else {
            const bool content_changed = record->content_token != input->content_token ||
                                         record->snapshot.kind != input->kind ||
                                         record->snapshot.content_width != input->content_width ||
                                         record->snapshot.content_height != input->content_height;
            const bool presentation_changed =
                !record->published || !presentation_equal(&record->snapshot, input);
            if (content_changed) {
                ++record->snapshot.content_revision;
            }
            if (presentation_changed) {
                ++record->snapshot.presentation_revision;
            }
            changed = changed || content_changed || presentation_changed;
        }
        record->published = true;
        record->content_token = input->content_token;
        record->snapshot.kind = input->kind;
        record->snapshot.interaction = input->interaction;
        record->snapshot.content_width = input->content_width;
        record->snapshot.content_height = input->content_height;
        record->snapshot.bounds = input->bounds;
        record->snapshot.rotation_degrees = input->rotation_degrees;
        record->snapshot.pivot_x = input->pivot_x;
        record->snapshot.pivot_y = input->pivot_y;
        record->snapshot.opacity = input->opacity;
        record->snapshot.z_order = input->z_order;
        record->snapshot.visible = input->visible;
        snapshot->layers[index] = record->snapshot;
    }

    if (changed || scene->revision == 0U) {
        ++scene->revision;
    }
    snapshot->revision = scene->revision;
    return true;
}

const EidolonSceneLayerSnapshot *eidolon_scene_snapshot_layer(const EidolonSceneSnapshot *snapshot,
                                                              uint64_t stable_key) {
    if (snapshot == NULL || stable_key == 0U) {
        return NULL;
    }
    for (size_t index = 0U; index < snapshot->layer_count; ++index) {
        if (snapshot->layers[index].stable_key == stable_key) {
            return &snapshot->layers[index];
        }
    }
    return NULL;
}
