#ifndef EIDOLON_DELIVERY_H
#define EIDOLON_DELIVERY_H

#include <stdbool.h>
#include <stddef.h>

#define EIDOLON_DELIVERY_MARK_CAPACITY 384U

typedef enum EidolonDeliveryCue {
    EIDOLON_DELIVERY_CUE_PHRASE,
    EIDOLON_DELIVERY_CUE_ACCENT,
    EIDOLON_DELIVERY_CUE_CONTRAST,
    EIDOLON_DELIVERY_CUE_HESITATE,
    EIDOLON_DELIVERY_CUE_PAUSE,
    EIDOLON_DELIVERY_CUE_LAND,
    EIDOLON_DELIVERY_CUE_QUESTION,
    EIDOLON_DELIVERY_CUE_EXCLAIM,
} EidolonDeliveryCue;

typedef struct EidolonDeliveryMark {
    size_t text_offset;
    EidolonDeliveryCue cue;
    float intensity;
    float direction;
} EidolonDeliveryMark;

typedef struct EidolonDeliveryTrack {
    EidolonDeliveryMark marks[EIDOLON_DELIVERY_MARK_CAPACITY];
    size_t count;
    size_t next_index;
    size_t last_offset;
    size_t dropped;
    bool has_last_offset;
} EidolonDeliveryTrack;

void eidolon_delivery_track_compile(EidolonDeliveryTrack *track, const char *text);
void eidolon_delivery_track_seek_after(EidolonDeliveryTrack *track, size_t text_offset);
size_t eidolon_delivery_track_collect(EidolonDeliveryTrack *track, size_t text_offset,
                                      EidolonDeliveryMark *events, size_t capacity);
const char *eidolon_delivery_cue_name(EidolonDeliveryCue cue);

#endif
