#ifndef EIDOLON_PRESENTATION_EVENT_QUEUE_H
#define EIDOLON_PRESENTATION_EVENT_QUEUE_H

#include "presentation.h"

#define EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY 64U

typedef struct EidolonPresentationEventQueue {
    EidolonPresentationEvent events[EIDOLON_PRESENTATION_EVENT_QUEUE_CAPACITY];
    size_t head;
    size_t count;
    uint64_t next_sequence;
} EidolonPresentationEventQueue;

#ifdef __cplusplus
extern "C" {
#endif

void eidolon_presentation_event_queue_init(EidolonPresentationEventQueue *queue);
bool eidolon_presentation_event_queue_push(EidolonPresentationEventQueue *queue,
                                           const EidolonPresentationEvent *event);
bool eidolon_presentation_event_queue_poll(EidolonPresentationEventQueue *queue,
                                           EidolonPresentationEvent *event);

#ifdef __cplusplus
}
#endif

#endif
