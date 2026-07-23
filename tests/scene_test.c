#include "scene.h"

#include <assert.h>
#include <stdio.h>

static EidolonSceneLayerInput body(uint64_t token) {
    return (EidolonSceneLayerInput){
        .stable_key = 1U,
        .kind = EIDOLON_SCENE_LAYER_BODY,
        .content_token = token,
        .content_width = 256U,
        .content_height = 256U,
        .bounds = {100.0F, 200.0F, 256.0F, 256.0F},
        .opacity = 1.0F,
        .z_order = 10,
        .visible = true,
    };
}

int main(void) {
    EidolonScene scene;
    EidolonSceneSnapshot snapshot;
    eidolon_scene_init(&scene);

    EidolonSceneLayerInput layers[2] = {
        body(10U),
        {
            .stable_key = 2U,
            .kind = EIDOLON_SCENE_LAYER_DIALOGUE,
            .content_token = 20U,
            .content_width = 420U,
            .content_height = 160U,
            .bounds = {360.0F, 40.0F, 420.0F, 160.0F},
            .opacity = 1.0F,
            .z_order = 20,
            .visible = true,
        },
    };
    assert(eidolon_scene_publish(&scene, layers, 2U, &snapshot));
    assert(snapshot.revision == 1U && snapshot.layer_count == 2U);
    const EidolonSceneLayerSnapshot *first_body = eidolon_scene_snapshot_layer(&snapshot, 1U);
    const EidolonSceneLayerSnapshot *first_bubble = eidolon_scene_snapshot_layer(&snapshot, 2U);
    assert(first_body != NULL && first_body->id.value != 0U);
    assert(first_body->content_revision == 1U && first_body->presentation_revision == 1U);
    assert(first_bubble != NULL && first_bubble->id.value != first_body->id.value);
    const uint32_t bubble_id = first_bubble->id.value;

    assert(eidolon_scene_publish(&scene, layers, 2U, &snapshot));
    assert(snapshot.revision == 1U);
    first_bubble = eidolon_scene_snapshot_layer(&snapshot, 2U);
    assert(first_bubble->content_revision == 1U && first_bubble->presentation_revision == 1U);

    layers[1].opacity = 0.5F;
    assert(eidolon_scene_publish(&scene, layers, 2U, &snapshot));
    first_bubble = eidolon_scene_snapshot_layer(&snapshot, 2U);
    assert(snapshot.revision == 2U);
    assert(first_bubble->content_revision == 1U && first_bubble->presentation_revision == 2U);

    layers[1].content_token = 21U;
    assert(eidolon_scene_publish(&scene, layers, 2U, &snapshot));
    first_bubble = eidolon_scene_snapshot_layer(&snapshot, 2U);
    assert(snapshot.revision == 3U);
    assert(first_bubble->content_revision == 2U && first_bubble->presentation_revision == 2U);

    assert(eidolon_scene_publish(&scene, layers, 1U, &snapshot));
    assert(snapshot.revision == 4U && snapshot.layer_count == 1U);
    assert(eidolon_scene_publish(&scene, layers, 2U, &snapshot));
    first_bubble = eidolon_scene_snapshot_layer(&snapshot, 2U);
    assert(snapshot.revision == 5U && first_bubble->id.value == bubble_id);
    assert(first_bubble->content_revision == 2U && first_bubble->presentation_revision == 4U);

    layers[1].stable_key = 1U;
    assert(!eidolon_scene_publish(&scene, layers, 2U, &snapshot));
    assert(!eidolon_scene_publish(&scene, layers, EIDOLON_SCENE_LAYER_CAPACITY + 1U, &snapshot));

    EidolonScene churn;
    eidolon_scene_init(&churn);
    for (uint64_t generation = 0U; generation < EIDOLON_SCENE_LAYER_CAPACITY * 3U; ++generation) {
        EidolonSceneLayerInput replacement = body(generation + 100U);
        replacement.stable_key = generation + 100U;
        assert(eidolon_scene_publish(&churn, &replacement, 1U, &snapshot));
        assert(snapshot.layer_count == 1U && snapshot.layers[0].id.value != 0U);
    }

    puts("scene tests passed");
    return 0;
}
