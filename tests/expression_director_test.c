#include "expression_director.h"

#include <assert.h>
#include <string.h>

int main(void) {
    static const char example[] =
        "and of course, i would never... Oh, what is that?";
    EidolonExpressionTrack track;
    eidolon_expression_track_compile(&track, example, EIDOLON_STATE_REVIEW);
    assert(track.count == 2U);
    assert(track.beats[0].boundary_reason == EIDOLON_BEAT_BOUNDARY_SENTENCE);
    assert(track.beats[1].boundary_reason == EIDOLON_BEAT_BOUNDARY_SENTENCE);
    assert(track.beats[1].cue_reason == EIDOLON_CUE_REASON_INTERJECTION);
    assert(strncmp(example + track.beats[0].text_start, "and of course", 13U) == 0);
    assert(strncmp(example + track.beats[1].text_start, "Oh, what", 8U) == 0);
    assert(track.waiting);

    char beat_text[EIDOLON_EXPRESSION_BEAT_TEXT_CAPACITY];
    assert(eidolon_expression_track_copy_text(&track, 1U, example, beat_text,
                                              sizeof(beat_text)));
    assert(strcmp(beat_text, "Oh, what is that?") == 0);

    assert(eidolon_expression_track_set_sequence(&track, 0U, 101U));
    assert(eidolon_expression_track_set_sequence(&track, 1U, 102U));
    float neutral[EIDOLON_GOEMOTIONS_COUNT] = {0};
    neutral[EIDOLON_EMOTION_NEUTRAL] = 0.9F;
    float surprise[EIDOLON_GOEMOTIONS_COUNT] = {0};
    surprise[EIDOLON_EMOTION_SURPRISE] = 1.0F;
    surprise[EIDOLON_EMOTION_CURIOSITY] = 0.7F;
    assert(eidolon_expression_track_apply(&track, 101U, neutral, 1000U));
    assert(!eidolon_expression_track_ready(&track));
    assert(eidolon_expression_track_apply(&track, 102U, surprise, 1020U));
    assert(eidolon_expression_track_ready(&track));
    assert(track.beats[1].cue == EIDOLON_PERFORMANCE_CUE_SURPRISE);
    assert(track.beats[1].cue_reason == EIDOLON_CUE_REASON_INTERJECTION);
    assert(track.beats[1].top_emotions[0] == EIDOLON_EMOTION_SURPRISE);
    assert(track.beats[1].classified_ms == 1020U);
    assert(track.beats[1].expression_margin >= 0.0F);
    assert(track.beats[0].cue == EIDOLON_PERFORMANCE_CUE_NONE);
    assert(track.beats[0].cue_reason == EIDOLON_CUE_REASON_TRACK_START);

    EidolonPerformanceEvent event;
    assert(eidolon_expression_track_event(&track, 0U, &event));
    assert(event.beat_index == 0U);
    assert(!eidolon_expression_track_event(&track, 4U, &event));
    assert(eidolon_expression_track_event(&track, track.beats[1].text_start, &event));
    assert(event.beat_index == 1U);
    assert(event.cue == EIDOLON_PERFORMANCE_CUE_SURPRISE);

    static const char contrast[] = "I adore it, but this part makes me furious.";
    eidolon_expression_track_compile(&track, contrast, EIDOLON_STATE_RUNNING);
    assert(track.count == 2U);
    assert(strncmp(contrast + track.beats[1].text_start, "but", 3U) == 0);
    assert(track.beats[0].boundary_reason == EIDOLON_BEAT_BOUNDARY_CONTRAST);

    static const char paragraphs[] = "curious\nangry\nsoft";
    eidolon_expression_track_compile(&track, paragraphs, EIDOLON_STATE_RUNNING);
    assert(track.count == 3U);
    eidolon_expression_track_fallback(&track, paragraphs);
    assert(eidolon_expression_track_ready(&track));

    static const char rollercoaster[] =
        "you changed the default renderer without breaking inheritance?\n"
        "suspicious. that was far too easy.\n"
        "oh, shit--the user config disappeared. did reset delete the whole--\n"
        "no. version remains, override removed, system default restored. exactly right.\n"
        "ugh. fine. you win. this is clean, elegant, and disgustingly satisfying.\n"
        "now stop looking so smug before i climb into your lap and give you a much better reason.";
    eidolon_expression_track_compile(&track, rollercoaster, EIDOLON_STATE_RUNNING);
    assert(track.count == 10U);

    static const char fragments[] =
        "mm… everything is quiet. wait. something moved. but… look. oh! there she is. "
        "ugh… now embarrassed. fine. come here. ❤️";
    eidolon_expression_track_compile(&track, fragments, EIDOLON_STATE_RUNNING);
    assert(track.count == 6U);
    assert(eidolon_expression_track_copy_text(&track, 0U, fragments, beat_text,
                                              sizeof(beat_text)));
    assert(strcmp(beat_text, "mm… everything is quiet.") == 0);
    assert(eidolon_expression_track_copy_text(&track, 1U, fragments, beat_text,
                                              sizeof(beat_text)));
    assert(strcmp(beat_text, "wait. something moved.") == 0);
    assert(eidolon_expression_track_copy_text(&track, 5U, fragments, beat_text,
                                              sizeof(beat_text)));
    assert(strcmp(beat_text, "fine. come here. ❤️") == 0);
    return 0;
}
