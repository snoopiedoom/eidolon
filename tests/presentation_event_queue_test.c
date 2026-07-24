#include "presentation_event_queue.h"

#include <assert.h>
#include <stdio.h>

static EidolonPresentationEvent activation(uint64_t tick, uint32_t layer) {
    return (EidolonPresentationEvent){
        .kind = EIDOLON_PRESENTATION_EVENT_LAYER_ACTIVATED,
        .monotonic_ns = tick,
        .scene_revision = tick,
        .host = {1U},
        .layer = {layer},
        .geometry = {10, 20, 520, 360},
        .host_x = 12.0F,
        .host_y = 18.0F,
        .layer_x = 4.0F,
        .layer_y = 6.0F,
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
    assert(event.sequence == 1U && event.layer.value == 7U);
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
        .layer = {1U},
        .geometry = {30, 40, 520, 360},
    };
    assert(eidolon_presentation_event_queue_push(&queue, &completion));

    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED);
    assert(event.sequence == EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY + 2U);
    assert(event.layer.value == 0U && event.scene_revision == 0U);
    assert(eidolon_presentation_event_queue_poll(&queue, &event));
    assert(event.kind == EIDOLON_PRESENTATION_EVENT_MOVE_COMPLETED);
    assert(event.sequence == EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY + 3U);
    assert(!eidolon_presentation_event_queue_poll(&queue, &event));

    puts("presentation event queue tests passed");
    return 0;
}
