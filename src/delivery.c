#include "delivery.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#define DELIVERY_COALESCE_BYTES 96U
#define DELIVERY_COALESCE_MARKS 4U

static size_t utf8_next(const char *text, size_t cursor, size_t length, uint32_t *codepoint) {
    const unsigned char first = (unsigned char)text[cursor];
    if (first < 0x80U) {
        *codepoint = first;
        return cursor + 1U;
    }
    size_t bytes = 0U;
    uint32_t value = 0U;
    if (first >= 0xC2U && first <= 0xDFU) {
        bytes = 2U;
        value = first & 0x1FU;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        bytes = 3U;
        value = first & 0x0FU;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        bytes = 4U;
        value = first & 0x07U;
    } else {
        *codepoint = 0xFFFDU;
        return cursor + 1U;
    }
    if (cursor + bytes > length) {
        *codepoint = 0xFFFDU;
        return cursor + 1U;
    }
    for (size_t index = 1U; index < bytes; ++index) {
        const unsigned char continuation = (unsigned char)text[cursor + index];
        if ((continuation & 0xC0U) != 0x80U) {
            *codepoint = 0xFFFDU;
            return cursor + 1U;
        }
        value = (value << 6U) | (continuation & 0x3FU);
    }
    *codepoint = value;
    return cursor + bytes;
}

static size_t skip_space(const char *text, size_t cursor, size_t length) {
    while (cursor < length && (text[cursor] == ' ' || text[cursor] == '\t' ||
                               text[cursor] == '\r' || text[cursor] == '\n')) {
        ++cursor;
    }
    return cursor;
}

static int cue_priority(EidolonDeliveryCue cue) {
    switch (cue) {
    case EIDOLON_DELIVERY_CUE_EXCLAIM:
        return 8;
    case EIDOLON_DELIVERY_CUE_QUESTION:
        return 7;
    case EIDOLON_DELIVERY_CUE_CONTRAST:
        return 6;
    case EIDOLON_DELIVERY_CUE_HESITATE:
        return 5;
    case EIDOLON_DELIVERY_CUE_LAND:
        return 4;
    case EIDOLON_DELIVERY_CUE_ACCENT:
        return 3;
    case EIDOLON_DELIVERY_CUE_PAUSE:
        return 2;
    case EIDOLON_DELIVERY_CUE_PHRASE:
        return 1;
    }
    return 0;
}

static float mark_direction(size_t offset, EidolonDeliveryCue cue) {
    const uint32_t mixed =
        (uint32_t)offset * UINT32_C(2654435761) ^ (uint32_t)cue * UINT32_C(2246822519);
    return (mixed & 1U) != 0U ? 1.0F : -1.0F;
}

static void append_mark(EidolonDeliveryTrack *track, size_t offset, EidolonDeliveryCue cue,
                        float intensity) {
    if (track->count > 0U) {
        EidolonDeliveryMark *previous = &track->marks[track->count - 1U];
        if (previous->text_offset == offset ||
            (offset - previous->text_offset < 4U && cue_priority(previous->cue) <= 3 &&
             cue_priority(cue) <= 3)) {
            if (cue_priority(cue) > cue_priority(previous->cue) ||
                intensity > previous->intensity) {
                previous->text_offset = offset;
                previous->cue = cue;
                previous->intensity = intensity;
                previous->direction = mark_direction(offset, cue);
            }
            return;
        }
    }
    if (track->count >= EIDOLON_DELIVERY_MARK_CAPACITY) {
        track->dropped += 1U;
        return;
    }
    track->marks[track->count++] = (EidolonDeliveryMark){
        .text_offset = offset,
        .cue = cue,
        .intensity = intensity,
        .direction = mark_direction(offset, cue),
    };
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
    static const char *const words[] = {"actually", "although", "but", "except",
                                        "however",  "instead",  "no",  "yet"};
    for (size_t index = 0U; index < sizeof(words) / sizeof(words[0]); ++index) {
        if (ascii_word_equal(text, start, end, words[index])) {
            return true;
        }
    }
    return false;
}

static bool hesitation_word(const char *text, size_t start, size_t end) {
    static const char *const words[] = {"hmm", "maybe", "mm", "oh", "uh", "um", "wait", "well"};
    for (size_t index = 0U; index < sizeof(words) / sizeof(words[0]); ++index) {
        if (ascii_word_equal(text, start, end, words[index])) {
            return true;
        }
    }
    return false;
}

static void append_next_phrase(EidolonDeliveryTrack *track, const char *text, size_t cursor,
                               size_t length) {
    const size_t next = skip_space(text, cursor, length);
    if (next < length) {
        append_mark(track, next, EIDOLON_DELIVERY_CUE_PHRASE, 0.30F);
    }
}

void eidolon_delivery_track_compile(EidolonDeliveryTrack *track, const char *text) {
    if (track == NULL) {
        return;
    }
    memset(track, 0, sizeof(*track));
    if (text == NULL || text[0] == '\0') {
        return;
    }
    const size_t length = strlen(text);
    const size_t first = skip_space(text, 0U, length);
    if (first < length) {
        append_mark(track, first, EIDOLON_DELIVERY_CUE_PHRASE, 0.34F);
    }

    size_t cursor = first;
    size_t words_since_accent = 0U;
    size_t visible_since_accent = 0U;
    while (cursor < length) {
        const unsigned char byte = (unsigned char)text[cursor];
        if (isalpha(byte) != 0) {
            const size_t word_start = cursor;
            while (cursor < length && isalpha((unsigned char)text[cursor]) != 0) {
                ++cursor;
            }
            if (contrast_word(text, word_start, cursor)) {
                append_mark(track, word_start, EIDOLON_DELIVERY_CUE_CONTRAST, 0.72F);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            } else if (hesitation_word(text, word_start, cursor)) {
                append_mark(track, word_start, EIDOLON_DELIVERY_CUE_HESITATE, 0.58F);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            } else if (words_since_accent >= 5U || visible_since_accent >= 24U) {
                append_mark(track, word_start, EIDOLON_DELIVERY_CUE_ACCENT, 0.34F);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            }
            words_since_accent += 1U;
            visible_since_accent += cursor - word_start;
            continue;
        }
        if (byte >= 0x80U) {
            uint32_t codepoint = 0U;
            const size_t next = utf8_next(text, cursor, length, &codepoint);
            if (codepoint == 0x2026U) {
                append_mark(track, cursor, EIDOLON_DELIVERY_CUE_HESITATE, 0.55F);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            } else if (codepoint == 0x2014U) {
                append_mark(track, cursor, EIDOLON_DELIVERY_CUE_PAUSE, 0.50F);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            } else if (codepoint == 0x3002U || codepoint == 0xFF01U || codepoint == 0xFF1FU) {
                const EidolonDeliveryCue cue = codepoint == 0xFF01U ? EIDOLON_DELIVERY_CUE_EXCLAIM
                                               : codepoint == 0xFF1FU
                                                   ? EIDOLON_DELIVERY_CUE_QUESTION
                                                   : EIDOLON_DELIVERY_CUE_LAND;
                const float intensity = codepoint == 0xFF01U   ? 0.88F
                                        : codepoint == 0xFF1FU ? 0.68F
                                                               : 0.46F;
                append_mark(track, cursor, cue, intensity);
                append_next_phrase(track, text, next, length);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            } else if (codepoint == 0xFF0CU || codepoint == 0xFF1BU || codepoint == 0xFF1AU) {
                append_mark(track, cursor, EIDOLON_DELIVERY_CUE_PAUSE,
                            codepoint == 0xFF0CU ? 0.32F : 0.42F);
                words_since_accent = 0U;
                visible_since_accent = 0U;
            } else {
                visible_since_accent += 1U;
                if (visible_since_accent >= 18U) {
                    append_mark(track, cursor, EIDOLON_DELIVERY_CUE_ACCENT, 0.32F);
                    visible_since_accent = 0U;
                }
            }
            cursor = next;
            continue;
        }
        if (byte == '.' && cursor + 2U < length && text[cursor + 1U] == '.' &&
            text[cursor + 2U] == '.') {
            append_mark(track, cursor, EIDOLON_DELIVERY_CUE_HESITATE, 0.56F);
            cursor += 3U;
            append_next_phrase(track, text, cursor, length);
            words_since_accent = 0U;
            visible_since_accent = 0U;
            continue;
        }
        if (byte == '!' || byte == '?' || byte == '.' || byte == '\n') {
            const EidolonDeliveryCue cue = byte == '!'   ? EIDOLON_DELIVERY_CUE_EXCLAIM
                                           : byte == '?' ? EIDOLON_DELIVERY_CUE_QUESTION
                                                         : EIDOLON_DELIVERY_CUE_LAND;
            const float intensity = byte == '!' ? 0.88F : byte == '?' ? 0.68F : 0.46F;
            append_mark(track, cursor, cue, intensity);
            ++cursor;
            append_next_phrase(track, text, cursor, length);
            words_since_accent = 0U;
            visible_since_accent = 0U;
            continue;
        }
        if (byte == ',' || byte == ';' || byte == ':') {
            append_mark(track, cursor, EIDOLON_DELIVERY_CUE_PAUSE, byte == ',' ? 0.32F : 0.42F);
            words_since_accent = 0U;
            visible_since_accent = 0U;
        } else if (byte != ' ' && byte != '\t' && byte != '\r') {
            visible_since_accent += 1U;
        }
        ++cursor;
    }
}

void eidolon_delivery_track_seek_after(EidolonDeliveryTrack *track, size_t text_offset) {
    if (track == NULL) {
        return;
    }
    track->next_index = 0U;
    while (track->next_index < track->count &&
           track->marks[track->next_index].text_offset <= text_offset) {
        track->next_index += 1U;
    }
    track->last_offset = text_offset;
    track->has_last_offset = true;
}

size_t eidolon_delivery_track_collect(EidolonDeliveryTrack *track, size_t text_offset,
                                      EidolonDeliveryMark *events, size_t capacity) {
    if (track == NULL || events == NULL || capacity == 0U) {
        return 0U;
    }
    const size_t begin = track->next_index;
    size_t end = begin;
    while (end < track->count && track->marks[end].text_offset <= text_offset) {
        ++end;
    }
    if (begin == end) {
        track->last_offset = text_offset;
        track->has_last_offset = true;
        return 0U;
    }

    const size_t crossed = end - begin;
    const bool jumped =
        (track->has_last_offset && text_offset > track->last_offset + DELIVERY_COALESCE_BYTES) ||
        crossed > DELIVERY_COALESCE_MARKS;
    size_t count = 0U;
    if (jumped) {
        size_t strongest = begin;
        for (size_t index = begin + 1U; index < end; ++index) {
            const EidolonDeliveryMark *candidate = &track->marks[index];
            const EidolonDeliveryMark *selected = &track->marks[strongest];
            if (cue_priority(candidate->cue) > cue_priority(selected->cue) ||
                (cue_priority(candidate->cue) == cue_priority(selected->cue) &&
                 candidate->intensity > selected->intensity)) {
                strongest = index;
            }
        }
        events[count++] = track->marks[strongest];
    } else {
        for (size_t index = begin; index < end && count < capacity; ++index) {
            events[count++] = track->marks[index];
        }
    }
    track->next_index = end;
    track->last_offset = text_offset;
    track->has_last_offset = true;
    return count;
}

const char *eidolon_delivery_cue_name(EidolonDeliveryCue cue) {
    switch (cue) {
    case EIDOLON_DELIVERY_CUE_PHRASE:
        return "phrase";
    case EIDOLON_DELIVERY_CUE_ACCENT:
        return "accent";
    case EIDOLON_DELIVERY_CUE_CONTRAST:
        return "contrast";
    case EIDOLON_DELIVERY_CUE_HESITATE:
        return "hesitate";
    case EIDOLON_DELIVERY_CUE_PAUSE:
        return "pause";
    case EIDOLON_DELIVERY_CUE_LAND:
        return "land";
    case EIDOLON_DELIVERY_CUE_QUESTION:
        return "question";
    case EIDOLON_DELIVERY_CUE_EXCLAIM:
        return "exclaim";
    }
    return "unknown";
}
