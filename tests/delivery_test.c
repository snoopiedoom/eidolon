#include "delivery.h"

#include <assert.h>
#include <string.h>

static bool has_cue(const EidolonDeliveryTrack *track, EidolonDeliveryCue cue) {
    for (size_t index = 0U; index < track->count; ++index) {
        if (track->marks[index].cue == cue) {
            return true;
        }
    }
    return false;
}

int main(void) {
    static const char expressive[] =
        "well, everything looked ordinary, but then she moved! are you seeing this?";
    EidolonDeliveryTrack track;
    eidolon_delivery_track_compile(&track, expressive);
    assert(track.count >= 7U);
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_HESITATE));
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_CONTRAST));
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_EXCLAIM));
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_QUESTION));
    for (size_t index = 1U; index < track.count; ++index) {
        assert(track.marks[index - 1U].text_offset <= track.marks[index].text_offset);
    }

    static const char long_phrase[] =
        "this deliberately extended phrase contains enough ordinary words to receive several "
        "quiet delivery accents without pretending that the underlying emotion changed";
    eidolon_delivery_track_compile(&track, long_phrase);
    size_t accents = 0U;
    for (size_t index = 0U; index < track.count; ++index) {
        accents += track.marks[index].cue == EIDOLON_DELIVERY_CUE_ACCENT ? 1U : 0U;
    }
    assert(accents >= 2U);

    static const char multilingual[] =
        "這是一段足夠長的中文句子用來確認角色說話時仍然會自然移動。 한국어도 함께 움직여요!";
    eidolon_delivery_track_compile(&track, multilingual);
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_ACCENT));
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_LAND));
    assert(has_cue(&track, EIDOLON_DELIVERY_CUE_EXCLAIM));

    eidolon_delivery_track_compile(&track, expressive);
    EidolonDeliveryMark events[8];
    assert(eidolon_delivery_track_collect(&track, 0U, events, 8U) == 1U);
    const size_t collected = eidolon_delivery_track_collect(&track, strlen(expressive), events, 8U);
    assert(collected == 1U);
    assert(events[0].cue == EIDOLON_DELIVERY_CUE_EXCLAIM ||
           events[0].cue == EIDOLON_DELIVERY_CUE_QUESTION ||
           events[0].cue == EIDOLON_DELIVERY_CUE_CONTRAST);

    eidolon_delivery_track_compile(&track, expressive);
    eidolon_delivery_track_seek_after(&track, 20U);
    assert(track.next_index > 0U);
    for (size_t index = 0U; index < track.next_index; ++index) {
        assert(track.marks[index].text_offset <= 20U);
    }
    return 0;
}
