#ifndef EIDOLON_SCENE_H
#define EIDOLON_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_SCENE_LAYER_CAPACITY 8U

typedef struct EidolonSceneLayerId {
    uint32_t value;
} EidolonSceneLayerId;

typedef enum EidolonSceneLayerKind {
    EIDOLON_SCENE_LAYER_BODY,
    EIDOLON_SCENE_LAYER_DIALOGUE,
    EIDOLON_SCENE_LAYER_TRANSIENT,
} EidolonSceneLayerKind;

typedef struct EidolonSceneRect {
    float x;
    float y;
    float width;
    float height;
} EidolonSceneRect;

typedef struct EidolonSceneLayerInput {
    uint64_t stable_key;
    EidolonSceneLayerKind kind;
    uint64_t content_token;
    uint32_t content_width;
    uint32_t content_height;
    EidolonSceneRect bounds;
    float rotation_degrees;
    float opacity;
    int32_t z_order;
    bool visible;
} EidolonSceneLayerInput;

typedef struct EidolonSceneLayerSnapshot {
    EidolonSceneLayerId id;
    uint64_t stable_key;
    EidolonSceneLayerKind kind;
    uint64_t content_revision;
    uint64_t presentation_revision;
    uint32_t content_width;
    uint32_t content_height;
    EidolonSceneRect bounds;
    float rotation_degrees;
    float opacity;
    int32_t z_order;
    bool visible;
} EidolonSceneLayerSnapshot;

typedef struct EidolonSceneSnapshot {
    uint64_t revision;
    size_t layer_count;
    EidolonSceneLayerSnapshot layers[EIDOLON_SCENE_LAYER_CAPACITY];
} EidolonSceneSnapshot;

typedef struct EidolonSceneLayerRecord {
    EidolonSceneLayerSnapshot snapshot;
    uint64_t content_token;
    bool occupied;
    bool published;
} EidolonSceneLayerRecord;

typedef struct EidolonScene {
    EidolonSceneLayerRecord layers[EIDOLON_SCENE_LAYER_CAPACITY];
    uint64_t revision;
    uint32_t next_layer_id;
} EidolonScene;

void eidolon_scene_init(EidolonScene *scene);
bool eidolon_scene_publish(EidolonScene *scene, const EidolonSceneLayerInput *layers,
                           size_t layer_count, EidolonSceneSnapshot *snapshot);
const EidolonSceneLayerSnapshot *eidolon_scene_snapshot_layer(const EidolonSceneSnapshot *snapshot,
                                                              uint64_t stable_key);

#endif
