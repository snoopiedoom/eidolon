#include "presentation_event_queue.h"

#include <string.h>

static EidolonPresentationEvent *pending_environment_event(EidolonPresentationEventQueue *queue,
                                                           EidolonPresentationHost host) {
    for (size_t offset = 0U; offset < queue->count; ++offset) {
        const size_t slot = (queue->head + offset) % EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY;
        EidolonPresentationEvent *candidate = &queue->events[slot];
        if (candidate->kind == EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED &&
            candidate->host.value == host.value) {
            return candidate;
        }
    }
    return NULL;
}

static bool retain_after_resync(EidolonPresentationEventKind kind) {
    return kind == EIDOLON_PRESENTATION_EVENT_LAYER_CONTEXT_REQUESTED ||
           kind == EIDOLON_PRESENTATION_EVENT_HOST_CLOSE_REQUESTED ||
           kind == EIDOLON_PRESENTATION_EVENT_GRAPHICS_RESET_REQUIRED;
}

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

    if (event->kind == EIDOLON_PRESENTATION_EVENT_ENVIRONMENT_CHANGED) {
        EidolonPresentationEvent *pending = pending_environment_event(queue, event->host);
        if (pending != NULL) {
            const uint64_t sequence = pending->sequence;
            *pending = *event;
            pending->sequence = sequence;
            return true;
        }
    }

    if (queue->count >= EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY) {
        queue->head = 0U;
        queue->count = 1U;
        EidolonPresentationEvent resync = *event;
        resync.kind = EIDOLON_PRESENTATION_EVENT_QUEUE_RESYNC_REQUIRED;
        resync.sequence = queue->next_sequence++;
        memset(&resync.data, 0, sizeof(resync.data));
        queue->events[0] = resync;
        if (!retain_after_resync(event->kind)) {
            return false;
        }
    }

    EidolonPresentationEvent accepted = *event;
    accepted.sequence = queue->next_sequence++;
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
