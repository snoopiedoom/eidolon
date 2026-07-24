#include "presentation_event_queue.h"

#include <string.h>

void eidolon_presentation_event_queue_init(EidolonPresentationEventQueue *queue) {
    if (queue == NULL) {
        return;
    }
    memset(queue, 0, sizeof(*queue));
    queue->next_sequence = 1U;
}

bool eidolon_presentation_event_queue_push(EidolonPresentationEventQueue *queue,
                                           const EidolonPresentationEvent *event) {
    if (queue == NULL || event == NULL || event->kind == EIDOLON_PRESENTATION_EVENT_NONE) {
        return false;
    }

    EidolonPresentationEvent accepted = *event;
    accepted.sequence = queue->next_sequence++;
    if (queue->count >= EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY) {
        queue->head = 0U;
        queue->count = 1U;
        accepted.kind = EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED;
        accepted.layer = (EidolonSceneLayerId){0U};
        accepted.scene_revision = 0U;
        accepted.host_x = 0.0F;
        accepted.host_y = 0.0F;
        accepted.layer_x = 0.0F;
        accepted.layer_y = 0.0F;
        queue->events[0] = accepted;
        return false;
    }

    const size_t slot = (queue->head + queue->count) % EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY;
    queue->events[slot] = accepted;
    ++queue->count;
    return true;
}

bool eidolon_presentation_event_queue_poll(EidolonPresentationEventQueue *queue,
                                           EidolonPresentationEvent *event) {
    if (queue == NULL || event == NULL || queue->count == 0U) {
        return false;
    }
    *event = queue->events[queue->head];
    queue->head = (queue->head + 1U) % EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY;
    --queue->count;
    return true;
}
