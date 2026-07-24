#include "presentation_event_queue.h"

#include <assert.h>
#include <stdio.h>

static EidolonPresentationEvent activation(uint64_t tick, uint32_t layer) {
    return (EidolonPresentationEvent){
        .kind = EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED,
        .monotonic_ns = tick,
        .host = {1U},
        .data.layer =
            {
                .scene_revision = tick,
                .layer = {layer},
                .host_x = 12.0F,
                .host_y = 18.0F,
                .layer_x = 4.0F,
                .layer_y = 6.0F,
            },
    };
}

static EidolonPresentationEvent environment(uint64_t revision) {
    return (EidolonPresentationEvent){
        .kind = EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED,
        .monotonic_ns = revision,
        .host = {1U},
        .data.environment.environment =
            {
                .revision = revision,
                .topology_revision = 1U,
                .host = {1U},
                .active_output = {9U},
                .host_geometry = {10, 20, 520, 360},
                .output_bounds = {0.0F, 0.0F, 1920.0F, 1080.0F},
                .usable_bounds = {0.0F, 0.0F, 1920.0F, 1040.0F},
                .content_scale = 1.0F,
                .pixel_scale = 1.0F,
                .nominal_refresh_hz = 60.0F,
                .orientation = EIDOLON_PRESENTATION_ORIENTATION_LANDSCAPE,
                .coordinate_space = EIDOLON_PRESENTATION_COORDINATE_SPACE_GLOBAL_PIXEL,
                .valid_fields =
                    EIDOLON_PRESENTATION_ENV_HOST_GEOMETRY |
                    EIDOLON_PRESENTATION_ENV_ACTIVE_OUTPUT |
                    EIDOLON_PRESENTATION_ENV_OUTPUT_BOUNDS |
                    EIDOLON_PRESENTATION_ENV_USABLE_BOUNDS |
                    EIDOLON_PRESENTATION_ENV_CONTENT_SCALE | EIDOLON_PRESENTATION_ENV_PIXEL_SCALE |
                    EIDOLON_PRESENTATION_ENV_NOMINAL_REFRESH |
                    EIDOLON_PRESENTATION_ENV_ORIENTATION |
                    EIDOLON_PRESENTATION_ENV_COORDINATE_SPACE,
                .changed_fields = EIDOLON_PRESENTATION_ENV_ALL_FIELDS,
            },
    };
}

int main(void) {
    EidolonPresentationEventQueue queue;
    eidolon_presentation_event_queue_init(&queue);

    EidolonPresentationEvent first = activation(100U, 7U);
    assert(eidolon_presentation_event_queue_push(&queue, &first));
    EidolonPresentationEvent event;
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED);
    assert(event.sequence == 1U && event.data.layer.layer.value == 7U);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    EidolonPresentationEvent pointer_motion = {
        .kind = EIDOLON_PRESENTATION_EVENT_POINTER_MOTION,
        .monotonic_ns = 110U,
        .host = {1U},
        .data.pointer =
            {
                .scene_revision = 1U,
                .pointer_id = 1U,
                .buttons = EIDOLON_PRESENTATION_POINTER_BUTTON_MIDDLE,
                .valid_coordinates = EIDOLON_PRESENTATION_POINTER_COORDINATE_HOST |
                                     EIDOLON_PRESENTATION_POINTER_COORDINATE_LAYER,
                .layer = {7U},
                .device_kind = EIDOLON_PRESENTATION_POINTER_DEVICE_MOUSE,
                .host_x = 12.0F,
                .host_y = 18.0F,
                .layer_x = 4.0F,
                .layer_y = 6.0F,
                .layer_x_relative = 1.0F,
                .layer_y_relative = 2.0F,
            },
    };
    assert(eidolon_presentation_event_queue_push(&queue, &pointer_motion));
    pointer_motion.monotonic_ns = 120U;
    pointer_motion.data.pointer.host_x = 15.0F;
    pointer_motion.data.pointer.layer_x = 7.0F;
    pointer_motion.data.pointer.layer_x_relative = 3.0F;
    pointer_motion.data.pointer.layer_y_relative = 4.0F;
    assert(eidolon_presentation_event_queue_push(&queue, &pointer_motion));
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_POINTER_MOTION);
    assert(event.sequence == 2U && event.monotonic_ns == 120U);
    assert(event.data.pointer.layer_x == 7.0F);
    assert(event.data.pointer.layer_x_relative == 4.0F);
    assert(event.data.pointer.layer_y_relative == 6.0F);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    EidolonPresentationEvent first_environment = environment(1U);
    assert(eidolon_presentation_event_queue_push(&queue, &first_environment));
    EidolonPresentationEvent edge = activation(150U, 8U);
    assert(eidolon_presentation_event_queue_push(&queue, &edge));
    EidolonPresentationEvent latest_environment = environment(3U);
    assert(eidolon_presentation_event_queue_push(&queue, &latest_environment));
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED);
    assert(event.sequence == 3U && event.data.environment.environment.revision == 3U);
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED);
    assert(event.sequence == 4U && event.data.layer.layer.value == 8U);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    for (size_t index = 0U; index < EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY; ++index) {
        EidolonPresentationEvent queued = activation(200U + index, (uint32_t)(index + 1U));
        assert(eidolon_presentation_event_queue_push(&queue, &queued));
    }
    EidolonPresentationEvent overflow = activation(999U, 99U);
    assert(!eidolon_presentation_event_queue_push(&queue, &overflow));
    EidolonPresentationEvent completion = {
        .kind = EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED,
        .monotonic_ns = 1000U,
        .host = {1U},
        .data.move =
            {
                .scene_revision = 1U,
                .environment_revision = 3U,
                .layer = {1U},
                .geometry = {30, 40, 520, 360},
            },
    };
    assert(eidolon_presentation_event_queue_push(&queue, &completion));

    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED);
    assert(event.sequence == EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY + 5U);
    assert(event.data.layer.layer.value == 0U && event.data.layer.scene_revision == 0U);
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED);
    assert(event.sequence == EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY + 6U);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    for (size_t index = 0U; index < EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY; ++index) {
        EidolonPresentationEvent queued = activation(1100U + index, (uint32_t)(index + 1U));
        assert(eidolon_presentation_event_queue_push(&queue, &queued));
    }
    const EidolonPresentationEvent close = {
        .kind = EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED,
        .monotonic_ns = 1200U,
        .host = {1U},
    };
    assert(eidolon_presentation_event_queue_push(&queue, &close));
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED);
    const uint64_t close_resync_sequence = event.sequence;
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED);
    assert(event.sequence == close_resync_sequence + 1U);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    for (size_t index = 0U; index < EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY; ++index) {
        EidolonPresentationEvent queued = activation(1300U + index, (uint32_t)(index + 1U));
        assert(eidolon_presentation_event_queue_push(&queue, &queued));
    }
    const EidolonPresentationEvent reset = {
        .kind = EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED,
        .monotonic_ns = 1400U,
        .host = {1U},
        .data.graphics = {EIDOLON_PRESENTATION_GRAPHICS_RESET_DEVICE},
    };
    assert(eidolon_presentation_event_queue_push(&queue, &reset));
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED);
    const uint64_t reset_resync_sequence = event.sequence;
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED);
    assert(event.sequence == reset_resync_sequence + 1U);
    assert(event.data.graphics.reset_kind == EIDOLON_PRESENTATION_GRAPHICS_RESET_DEVICE);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    puts("presentation event queue tests passed");
    return 0;
}
