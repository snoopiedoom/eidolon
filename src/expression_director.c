#include "expression_director.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#define MAX_BEAT_BYTES 384U

static float clamp01(float value) { return fmaxf(0.0F, fminf(1.0F, value)); }

static size_t trim_left(const char *text, size_t start, size_t end) {
    while (start < end && (text[start] == ' ' || text[start] == '\t' || text[start] == '\n' ||
                           text[start] == '\r')) {
        ++start;
    }
    return start;
}

static size_t trim_right(const char *text, size_t start, size_t end) {
    while (end > start && (text[end - 1U] == ' ' || text[end - 1U] == '\t' ||
                           text[end - 1U] == '\n' || text[end - 1U] == '\r')) {
        --end;
    }
    return end;
}

static bool starts_word_case_insensitive(const char *text, size_t start, size_t end,
                                         const char *word) {
    const size_t length = strlen(word);
    if (start + length > end) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if ((char)tolower((unsigned char)text[start + index]) != word[index]) {
            return false;
        }
    }
    return start + length == end || isalpha((unsigned char)text[start + length]) == 0;
}

static bool append_beat(EidolonExpressionTrack *track, const char *text, size_t start,
                        size_t end, EidolonBeatBoundaryReason boundary_reason) {
    start = trim_left(text, start, end);
    end = trim_right(text, start, end);
    if (start >= end) {
        return true;
    }
    if (track->count >= EIDOLON_EXPRESSION_BEAT_CAPACITY) {
        track->beats[track->count - 1U].text_end = end;
        return false;
    }
    EidolonExpressionBeat *beat = &track->beats[track->count++];
    beat->text_start = start;
    beat->text_end = end;
    beat->boundary_reason = boundary_reason;
    const bool punctuated = memchr(text + start, '?', end - start) != NULL ||
                            memchr(text + start, '!', end - start) != NULL;
    if (punctuated && (starts_word_case_insensitive(text, start, end, "oh") ||
                       starts_word_case_insensitive(text, start, end, "wait") ||
                       starts_word_case_insensitive(text, start, end, "what"))) {
        beat->cue = EIDOLON_PERFORMANCE_CUE_SURPRISE;
        beat->cue_reason = EIDOLON_CUE_REASON_INTERJECTION;
        beat->intensity = 0.78F;
    } else if (memchr(text + start, '!', end - start) != NULL) {
        beat->cue = EIDOLON_PERFORMANCE_CUE_LIFT;
        beat->cue_reason = EIDOLON_CUE_REASON_EXCLAMATION;
        beat->intensity = 0.62F;
    }
    beat->expression = eidolon_affect_expression(&beat->affect);
    return true;
}

static bool modifier_fragment(const char *text, const EidolonExpressionBeat *beat) {
    char word[16];
    size_t count = 0U;
    bool word_finished = false;
    for (size_t cursor = beat->text_start; cursor < beat->text_end; ++cursor) {
        const unsigned char byte = (unsigned char)text[cursor];
        if (isalpha(byte) != 0) {
            if (word_finished || count + 1U >= sizeof(word)) {
                return false;
            }
            word[count++] = (char)tolower(byte);
        } else if (count > 0U && byte < 0x80U && byte != '\'' && byte != '"') {
            word_finished = true;
        }
    }
    word[count] = '\0';
    static const char *const modifiers[] = {"actually", "but", "fine", "hmm", "mm", "no",
                                            "oh", "okay", "ugh", "wait", "well", "yet"};
    for (size_t index = 0U; index < sizeof(modifiers) / sizeof(modifiers[0]); ++index) {
        if (strcmp(word, modifiers[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool heart_fragment(const char *text, const EidolonExpressionBeat *beat) {
    const size_t length = beat->text_end - beat->text_start;
    static const char heart[] = "\xE2\x9D\xA4\xEF\xB8\x8F";
    return length == sizeof(heart) - 1U &&
           memcmp(text + beat->text_start, heart, sizeof(heart) - 1U) == 0;
}

static void merge_fragments(EidolonExpressionTrack *track, const char *text) {
    EidolonExpressionBeat original[EIDOLON_EXPRESSION_BEAT_CAPACITY];
    const size_t original_count = track->count;
    memcpy(original, track->beats, original_count * sizeof(original[0]));
    memset(track->beats, 0, sizeof(track->beats));
    track->count = 0U;
    for (size_t index = 0U; index < original_count; ++index) {
        EidolonExpressionBeat beat = original[index];
        if (heart_fragment(text, &beat) && track->count > 0U) {
            EidolonExpressionBeat *previous = &track->beats[track->count - 1U];
            previous->text_end = beat.text_end;
            previous->boundary_reason = beat.boundary_reason;
            continue;
        }
        while (modifier_fragment(text, &original[index]) && index + 1U < original_count) {
            const EidolonPerformanceCue hint = beat.cue;
            const EidolonCueReason hint_reason = beat.cue_reason;
            const float hint_intensity = beat.intensity;
            ++index;
            beat.text_end = original[index].text_end;
            beat.boundary_reason = original[index].boundary_reason;
            beat.cue = hint != EIDOLON_PERFORMANCE_CUE_NONE ? hint : original[index].cue;
            beat.cue_reason = hint != EIDOLON_PERFORMANCE_CUE_NONE ? hint_reason
                                                                  : original[index].cue_reason;
            beat.intensity = fmaxf(hint_intensity, original[index].intensity);
        }
        track->beats[track->count++] = beat;
    }
}

static bool ascii_word_equal(const char *text, size_t start, size_t end, const char *word) {
    const size_t length = strlen(word);
    if (end - start != length) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if ((char)tolower((unsigned char)text[start + index]) != word[index]) {
            return false;
        }
    }
    return true;
}

static bool contrast_word(const char *text, size_t start, size_t end) {
    static const char *const words[] = {"actually", "but", "except", "however", "instead",
                                        "no", "oh", "wait", "yet"};
    for (size_t index = 0U; index < sizeof(words) / sizeof(words[0]); ++index) {
        if (ascii_word_equal(text, start, end, words[index])) {
            return true;
        }
    }
    return false;
}

static size_t previous_nonspace(const char *text, size_t start, size_t cursor) {
    while (cursor > start) {
        --cursor;
        if (text[cursor] != ' ' && text[cursor] != '\t') {
            return cursor;
        }
    }
    return start;
}

static bool contrast_starts_beat(const char *text, size_t beat_start, size_t word_start,
                                 size_t word_end) {
    if (word_start <= beat_start || !contrast_word(text, word_start, word_end)) {
        return false;
    }
    const size_t previous = previous_nonspace(text, beat_start, word_start);
    const unsigned char byte = (unsigned char)text[previous];
    return byte == ',' || byte == ';' || byte == ':' || byte == '-' ||
           (previous >= 2U && (unsigned char)text[previous - 2U] == 0xE2U &&
            (unsigned char)text[previous - 1U] == 0x80U && byte == 0x94U);
}

static size_t punctuation_end(const char *text, size_t cursor, size_t length) {
    while (cursor < length) {
        const unsigned char byte = (unsigned char)text[cursor];
        if (byte == '.' || byte == '!' || byte == '?' || byte == '\'' || byte == '"' ||
            byte == ')' || byte == ']') {
            ++cursor;
            continue;
        }
        if (cursor + 2U < length && byte == 0xE2U &&
            (unsigned char)text[cursor + 1U] == 0x80U &&
            (unsigned char)text[cursor + 2U] == 0xA6U) {
            cursor += 3U;
            continue;
        }
        break;
    }
    return cursor;
}

static bool decimal_point(const char *text, size_t cursor, size_t length) {
    return text[cursor] == '.' && cursor > 0U && cursor + 1U < length &&
           isdigit((unsigned char)text[cursor - 1U]) && isdigit((unsigned char)text[cursor + 1U]);
}

static size_t forced_boundary(const char *text, size_t start, size_t limit) {
    size_t cursor = limit;
    while (cursor > start && text[cursor] != ' ' && text[cursor] != '\t') {
        --cursor;
    }
    return cursor > start ? cursor : limit;
}

void eidolon_expression_track_compile(EidolonExpressionTrack *track, const char *text,
                                      EidolonState state) {
    if (track == NULL) {
        return;
    }
    memset(track, 0, sizeof(*track));
    track->active_index = SIZE_MAX;
    track->state = state >= 0 && state < EIDOLON_STATE_COUNT ? state : EIDOLON_STATE_IDLE;
    if (text == NULL || text[0] == '\0') {
        track->complete = true;
        return;
    }

    const size_t length = strlen(text);
    size_t start = trim_left(text, 0U, length);
    size_t cursor = start;
    while (cursor < length && track->count < EIDOLON_EXPRESSION_BEAT_CAPACITY) {
        if (cursor - start >= MAX_BEAT_BYTES) {
            const size_t boundary = forced_boundary(text, start, cursor);
            if (!append_beat(track, text, start, boundary, EIDOLON_BEAT_BOUNDARY_LENGTH)) {
                break;
            }
            start = trim_left(text, boundary, length);
            cursor = start;
            continue;
        }
        if (text[cursor] == '\n') {
            if (!append_beat(track, text, start, cursor, EIDOLON_BEAT_BOUNDARY_LINE)) {
                break;
            }
            start = trim_left(text, cursor + 1U, length);
            cursor = start;
            continue;
        }

        const unsigned char byte = (unsigned char)text[cursor];
        if ((isalpha(byte) != 0) && (cursor == 0U || isalpha((unsigned char)text[cursor - 1U]) == 0)) {
            size_t word_end = cursor + 1U;
            while (word_end < length && isalpha((unsigned char)text[word_end]) != 0) {
                ++word_end;
            }
            if (contrast_starts_beat(text, start, cursor, word_end)) {
                const size_t previous = previous_nonspace(text, start, cursor);
                if (!append_beat(track, text, start, previous + 1U,
                                 EIDOLON_BEAT_BOUNDARY_CONTRAST)) {
                    break;
                }
                start = cursor;
            }
            cursor = word_end;
            continue;
        }

        const bool unicode_ellipsis = cursor + 2U < length && byte == 0xE2U &&
                                      (unsigned char)text[cursor + 1U] == 0x80U &&
                                      (unsigned char)text[cursor + 2U] == 0xA6U;
        if ((byte == '.' || byte == '!' || byte == '?' || unicode_ellipsis) &&
            !decimal_point(text, cursor, length)) {
            const size_t after = punctuation_end(text, cursor, length);
            if (!append_beat(track, text, start, after, EIDOLON_BEAT_BOUNDARY_SENTENCE)) {
                break;
            }
            start = trim_left(text, after, length);
            cursor = start;
            continue;
        }
        ++cursor;
    }
    if (start < length && track->count < EIDOLON_EXPRESSION_BEAT_CAPACITY) {
        (void)append_beat(track, text, start, length, EIDOLON_BEAT_BOUNDARY_END);
    } else if (start < length && track->count > 0U) {
        track->beats[track->count - 1U].text_end = trim_right(text, start, length);
    }
    merge_fragments(track, text);
    track->waiting = track->count > 0U;
    track->complete = track->count == 0U;
}

bool eidolon_expression_track_set_sequence(EidolonExpressionTrack *track, size_t beat_index,
                                           uint64_t sequence) {
    if (track == NULL || beat_index >= track->count || sequence == 0U) {
        return false;
    }
    track->beats[beat_index].sequence = sequence;
    return true;
}

bool eidolon_expression_track_copy_text(const EidolonExpressionTrack *track, size_t beat_index,
                                        const char *text, char *output, size_t capacity) {
    if (track == NULL || text == NULL || output == NULL || capacity == 0U ||
        beat_index >= track->count) {
        return false;
    }
    const EidolonExpressionBeat *beat = &track->beats[beat_index];
    const size_t length = beat->text_end - beat->text_start;
    if (length + 1U > capacity) {
        return false;
    }
    memcpy(output, text + beat->text_start, length);
    output[length] = '\0';
    return true;
}

static float affect_change(const EidolonAffect *left, const EidolonAffect *right) {
    return fabsf(left->valence - right->valence) + fabsf(left->arousal - right->arousal) +
           fabsf(left->warmth - right->warmth) + fabsf(left->surprise - right->surprise);
}

static void stabilize_expressions(EidolonExpressionTrack *track) {
    for (size_t index = 0U; index < track->count; ++index) {
        EidolonExpressionBeat *beat = &track->beats[index];
        beat->raw_expression = beat->expression;
        if (index == 0U) {
            continue;
        }
        const EidolonExpressionIntent previous = track->beats[index - 1U].expression;
        if (beat->expression == previous) {
            continue;
        }
        const float winning_distance =
            eidolon_affect_expression_distance(&beat->affect, beat->expression);
        const float previous_distance =
            eidolon_affect_expression_distance(&beat->affect, previous);
        beat->previous_expression_advantage = previous_distance - winning_distance;
        if (beat->previous_expression_advantage < 0.12F &&
            affect_change(&beat->affect, &track->beats[index - 1U].affect) < 0.72F) {
            beat->expression = previous;
            beat->expression_held = true;
        }
    }
}

static void derive_cues(EidolonExpressionTrack *track) {
    const EidolonAffect state = eidolon_affect_for_state(track->state);
    stabilize_expressions(track);
    for (size_t index = 0U; index < track->count; ++index) {
        EidolonExpressionBeat *beat = &track->beats[index];
        const EidolonAffect previous = index > 0U ? track->beats[index - 1U].affect : state;
        const float surprise_delta = beat->affect.surprise - previous.surprise;
        const float arousal_delta = beat->affect.arousal - previous.arousal;
        const float valence_delta = beat->affect.valence - previous.valence;
        const float warmth_delta = beat->affect.warmth - previous.warmth;
        const EidolonPerformanceCue semantic_hint = beat->cue;
        const EidolonCueReason semantic_hint_reason = beat->cue_reason;
        const float hint_intensity = beat->intensity;
        beat->intensity = fmaxf(hint_intensity,
                                clamp01(0.35F + beat->evidence * 0.50F +
                                        fabsf(arousal_delta) * 0.25F));
        if (index == 0U && semantic_hint == EIDOLON_PERFORMANCE_CUE_NONE) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_NONE;
            beat->cue_reason = EIDOLON_CUE_REASON_TRACK_START;
            beat->intensity = 0.0F;
        } else if (semantic_hint == EIDOLON_PERFORMANCE_CUE_SURPRISE ||
            beat->affect.surprise > 0.52F || surprise_delta > 0.30F) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_SURPRISE;
            beat->cue_reason = semantic_hint == EIDOLON_PERFORMANCE_CUE_SURPRISE
                                   ? semantic_hint_reason
                                   : EIDOLON_CUE_REASON_SURPRISE_AFFECT;
            beat->intensity = fmaxf(beat->intensity, clamp01(beat->affect.surprise));
        } else if (semantic_hint == EIDOLON_PERFORMANCE_CUE_LIFT) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_LIFT;
            beat->cue_reason = semantic_hint_reason;
        } else if (valence_delta < -0.35F) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_RECOIL;
            beat->cue_reason = EIDOLON_CUE_REASON_VALENCE_DROP;
        } else if (valence_delta > 0.30F || arousal_delta > 0.38F) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_LIFT;
            beat->cue_reason = EIDOLON_CUE_REASON_VALENCE_LIFT;
        } else if (beat->affect.warmth > 0.48F && warmth_delta > 0.15F) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_LEAN;
            beat->cue_reason = EIDOLON_CUE_REASON_WARMTH_LIFT;
        } else {
            beat->cue = EIDOLON_PERFORMANCE_CUE_ACCENT;
            beat->cue_reason = EIDOLON_CUE_REASON_DEFAULT_ACCENT;
        }
        const float affect_delta = affect_change(&beat->affect, &previous);
        if (index > 0U && semantic_hint == EIDOLON_PERFORMANCE_CUE_NONE &&
            beat->expression == track->beats[index - 1U].expression && affect_delta < 0.32F) {
            beat->cue = EIDOLON_PERFORMANCE_CUE_NONE;
            beat->cue_reason = EIDOLON_CUE_REASON_SIMILAR_SUPPRESSED;
            beat->intensity = 0.0F;
        }
    }
}

static void rank_emotions(EidolonExpressionBeat *beat,
                          const float probabilities[EIDOLON_GOEMOTIONS_COUNT]) {
    for (size_t rank = 0U; rank < 3U; ++rank) {
        beat->top_emotions[rank] = 0U;
        beat->top_probabilities[rank] = -1.0F;
    }
    for (size_t emotion = 0U; emotion < EIDOLON_GOEMOTIONS_COUNT; ++emotion) {
        const float probability = probabilities[emotion];
        for (size_t rank = 0U; rank < 3U; ++rank) {
            if (probability > beat->top_probabilities[rank]) {
                for (size_t move = 2U; move > rank; --move) {
                    beat->top_emotions[move] = beat->top_emotions[move - 1U];
                    beat->top_probabilities[move] = beat->top_probabilities[move - 1U];
                }
                beat->top_emotions[rank] = emotion;
                beat->top_probabilities[rank] = probability;
                break;
            }
        }
    }
}

static void rank_expressions(EidolonExpressionBeat *beat) {
    const float winning_distance =
        eidolon_affect_expression_distance(&beat->affect, beat->expression);
    float runner_distance = INFINITY;
    beat->runner_up_expression = beat->expression;
    for (int candidate = 0; candidate < (int)EIDOLON_EXPRESSION_COUNT; ++candidate) {
        if (candidate == (int)beat->expression) {
            continue;
        }
        const float distance = eidolon_affect_expression_distance(
            &beat->affect, (EidolonExpressionIntent)candidate);
        if (distance < runner_distance) {
            runner_distance = distance;
            beat->runner_up_expression = (EidolonExpressionIntent)candidate;
        }
    }
    beat->expression_margin = runner_distance - winning_distance;
}

bool eidolon_expression_track_apply(EidolonExpressionTrack *track, uint64_t sequence,
                                    const float probabilities[EIDOLON_GOEMOTIONS_COUNT],
                                    uint64_t now_ms) {
    if (track == NULL || probabilities == NULL || sequence == 0U) {
        return false;
    }
    for (size_t index = 0U; index < track->count; ++index) {
        EidolonExpressionBeat *beat = &track->beats[index];
        if (beat->sequence != sequence || beat->ready) {
            continue;
        }
        beat->affect = eidolon_affect_from_goemotions(
            probabilities, eidolon_affect_for_state(track->state), &beat->evidence);
        beat->expression = eidolon_affect_expression(&beat->affect);
        beat->classified_ms = now_ms;
        rank_emotions(beat, probabilities);
        rank_expressions(beat);
        beat->ready = true;
        track->ready_count += 1U;
        if (track->ready_count == track->count) {
            derive_cues(track);
            track->waiting = false;
            track->complete = true;
        }
        return true;
    }
    return false;
}

static bool contains_case_insensitive(const char *text, size_t start, size_t end,
                                      const char *needle) {
    const size_t length = strlen(needle);
    for (size_t cursor = start; cursor + length <= end; ++cursor) {
        size_t index = 0U;
        while (index < length &&
               (char)tolower((unsigned char)text[cursor + index]) == needle[index]) {
            ++index;
        }
        if (index == length) {
            return true;
        }
    }
    return false;
}

void eidolon_expression_track_fallback(EidolonExpressionTrack *track, const char *text) {
    if (track == NULL) {
        return;
    }
    for (size_t index = 0U; index < track->count; ++index) {
        EidolonExpressionBeat *beat = &track->beats[index];
        beat->affect = eidolon_affect_for_state(track->state);
        beat->evidence = 0.25F;
        if (text != NULL &&
            (contains_case_insensitive(text, beat->text_start, beat->text_end, "oh") ||
             memchr(text + beat->text_start, '!', beat->text_end - beat->text_start) != NULL)) {
            beat->affect.surprise = 0.82F;
            beat->affect.arousal = 0.72F;
        } else if (text != NULL &&
                   memchr(text + beat->text_start, '?', beat->text_end - beat->text_start) !=
                       NULL) {
            beat->affect.arousal = 0.42F;
            beat->affect.certainty = -0.25F;
        }
        beat->expression = eidolon_affect_expression(&beat->affect);
        beat->runner_up_expression = beat->expression;
        beat->expression_margin = 0.0F;
        beat->top_emotions[0] = EIDOLON_EMOTION_NEUTRAL;
        beat->top_probabilities[0] = 1.0F;
        beat->ready = true;
    }
    track->ready_count = track->count;
    derive_cues(track);
    track->waiting = false;
    track->complete = true;
}

bool eidolon_expression_track_ready(const EidolonExpressionTrack *track) {
    return track != NULL && track->complete && !track->waiting;
}

bool eidolon_expression_track_event(EidolonExpressionTrack *track, size_t text_offset,
                                    EidolonPerformanceEvent *event) {
    if (!eidolon_expression_track_ready(track) || track->count == 0U || event == NULL) {
        return false;
    }
    size_t selected = 0U;
    for (size_t index = 0U; index < track->count; ++index) {
        if (text_offset < track->beats[index].text_start) {
            break;
        }
        selected = index;
    }
    if (selected == track->active_index) {
        return false;
    }
    track->active_index = selected;
    const EidolonExpressionBeat *beat = &track->beats[selected];
    *event = (EidolonPerformanceEvent){beat->affect, beat->expression, beat->cue,
                                      beat->evidence, beat->intensity, selected};
    return true;
}

const char *eidolon_performance_cue_name(EidolonPerformanceCue cue) {
    switch (cue) {
    case EIDOLON_PERFORMANCE_CUE_NONE:
        return "none";
    case EIDOLON_PERFORMANCE_CUE_ACCENT:
        return "accent";
    case EIDOLON_PERFORMANCE_CUE_LIFT:
        return "lift";
    case EIDOLON_PERFORMANCE_CUE_RECOIL:
        return "recoil";
    case EIDOLON_PERFORMANCE_CUE_LEAN:
        return "lean";
    case EIDOLON_PERFORMANCE_CUE_SURPRISE:
        return "surprise";
    }
    return "unknown";
}

const char *eidolon_beat_boundary_reason_name(EidolonBeatBoundaryReason reason) {
    switch (reason) {
    case EIDOLON_BEAT_BOUNDARY_END:
        return "end";
    case EIDOLON_BEAT_BOUNDARY_LINE:
        return "line";
    case EIDOLON_BEAT_BOUNDARY_SENTENCE:
        return "sentence";
    case EIDOLON_BEAT_BOUNDARY_CONTRAST:
        return "contrast";
    case EIDOLON_BEAT_BOUNDARY_LENGTH:
        return "length";
    }
    return "unknown";
}

const char *eidolon_cue_reason_name(EidolonCueReason reason) {
    switch (reason) {
    case EIDOLON_CUE_REASON_NONE:
        return "none";
    case EIDOLON_CUE_REASON_INTERJECTION:
        return "interjection";
    case EIDOLON_CUE_REASON_EXCLAMATION:
        return "exclamation";
    case EIDOLON_CUE_REASON_SURPRISE_AFFECT:
        return "surprise-affect";
    case EIDOLON_CUE_REASON_VALENCE_DROP:
        return "valence-drop";
    case EIDOLON_CUE_REASON_VALENCE_LIFT:
        return "valence-lift";
    case EIDOLON_CUE_REASON_WARMTH_LIFT:
        return "warmth-lift";
    case EIDOLON_CUE_REASON_DEFAULT_ACCENT:
        return "default-accent";
    case EIDOLON_CUE_REASON_SIMILAR_SUPPRESSED:
        return "similar-suppressed";
    case EIDOLON_CUE_REASON_TRACK_START:
        return "track-start";
    }
    return "unknown";
}
