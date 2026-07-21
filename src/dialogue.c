#include "dialogue.h"

#include <math.h>
#include <string.h>

#define DIALOGUE_COLUMNS 39
#define DIALOGUE_LINES 5
#define REVEAL_INTERVAL_MS 24U

static size_t utf8_next(const char *text, size_t cursor, uint32_t *codepoint) {
    const unsigned char first = (unsigned char)text[cursor];
    if (first < 0x80U) {
        *codepoint = first;
        return cursor + 1U;
    }
    size_t length = 0U;
    uint32_t value = 0U;
    uint32_t minimum = 0U;
    if (first >= 0xC2U && first <= 0xDFU) {
        length = 2U;
        value = first & 0x1FU;
        minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        length = 3U;
        value = first & 0x0FU;
        minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        length = 4U;
        value = first & 0x07U;
        minimum = 0x10000U;
    } else {
        *codepoint = 0xFFFDU;
        return cursor + 1U;
    }
    for (size_t index = 1U; index < length; ++index) {
        const unsigned char continuation = (unsigned char)text[cursor + index];
        if (continuation == 0U || (continuation & 0xC0U) != 0x80U) {
            *codepoint = 0xFFFDU;
            return cursor + 1U;
        }
        value = (value << 6U) | (continuation & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
        *codepoint = 0xFFFDU;
        return cursor + 1U;
    }
    *codepoint = value;
    return cursor + length;
}

static bool grapheme_extend(uint32_t codepoint) {
    return (codepoint >= 0x0300U && codepoint <= 0x036FU) ||
           (codepoint >= 0x1AB0U && codepoint <= 0x1AFFU) ||
           (codepoint >= 0x1DC0U && codepoint <= 0x1DFFU) ||
           (codepoint >= 0x20D0U && codepoint <= 0x20FFU) ||
           (codepoint >= 0xFE00U && codepoint <= 0xFE0FU) ||
           (codepoint >= 0xFE20U && codepoint <= 0xFE2FU) ||
           (codepoint >= 0x1F3FBU && codepoint <= 0x1F3FFU);
}

static size_t utf8_next_grapheme(const char *text, size_t cursor) {
    uint32_t first = 0U;
    size_t next = utf8_next(text, cursor, &first);
    const bool regional = first >= 0x1F1E6U && first <= 0x1F1FFU;
    bool regional_pair_consumed = false;
    while (text[next] != '\0') {
        uint32_t codepoint = 0U;
        const size_t after = utf8_next(text, next, &codepoint);
        if (grapheme_extend(codepoint)) {
            next = after;
            continue;
        }
        if (regional && !regional_pair_consumed && codepoint >= 0x1F1E6U && codepoint <= 0x1F1FFU) {
            regional_pair_consumed = true;
            next = after;
            continue;
        }
        if (codepoint == 0x200DU && text[after] != '\0') {
            next = utf8_next(text, after, &codepoint);
            continue;
        }
        break;
    }
    return next;
}

static int codepoint_columns(uint32_t codepoint) {
    if (grapheme_extend(codepoint) || codepoint == 0x200DU) {
        return 0;
    }
    if ((codepoint >= 0x1100U && codepoint <= 0x115FU) ||
        (codepoint >= 0x2E80U && codepoint <= 0xA4CFU) ||
        (codepoint >= 0xAC00U && codepoint <= 0xD7A3U) ||
        (codepoint >= 0xF900U && codepoint <= 0xFAFFU) ||
        (codepoint >= 0xFE10U && codepoint <= 0xFE6FU) ||
        (codepoint >= 0xFF00U && codepoint <= 0xFF60U) ||
        (codepoint >= 0x1F300U && codepoint <= 0x1FAFFU)) {
        return 2;
    }
    return 1;
}

static void normalize_text(char *output, size_t capacity, const char *input) {
    size_t length = 0U;
    size_t cursor = 0U;
    bool previous_space = false;
    bool line_start = true;

    while (input[cursor] != '\0' && length + 1U < capacity) {
        const unsigned char character = (unsigned char)input[cursor];
        if (character == '\r') {
            ++cursor;
            continue;
        }
        if (character == '\n') {
            ++cursor;
            while (length > 0 && output[length - 1] == ' ') {
                --length;
            }
            if (length > 0 && output[length - 1] != '\n') {
                output[length++] = '\n';
            }
            previous_space = false;
            line_start = true;
            continue;
        }
        if (character == '\t' || character == ' ') {
            ++cursor;
            if (!previous_space && !line_start) {
                output[length++] = ' ';
                previous_space = true;
            }
            continue;
        }
        if (character >= 0x80U) {
            uint32_t codepoint = 0U;
            const size_t next = utf8_next(input, cursor, &codepoint);
            if (codepoint == 0xFFFDU && next == cursor + 1U) {
                static const char replacement[] = "\xEF\xBF\xBD";
                if (length + sizeof(replacement) > capacity) {
                    break;
                }
                memcpy(output + length, replacement, sizeof(replacement) - 1U);
                length += sizeof(replacement) - 1U;
            } else {
                const size_t bytes = next - cursor;
                if (length + bytes + 1U > capacity) {
                    break;
                }
                memcpy(output + length, input + cursor, bytes);
                length += bytes;
            }
            cursor = next;
        } else if (character >= 32U) {
            output[length++] = (char)character;
            ++cursor;
        } else {
            ++cursor;
        }
        previous_space = false;
        line_start = false;
    }

    while (length > 0 && (output[length - 1] == ' ' || output[length - 1] == '\n')) {
        --length;
    }
    output[length] = '\0';
}

static void build_page(EidolonDialogue *dialogue) {
    size_t source = dialogue->cursor;
    size_t output = 0;
    int column = 0;
    int line = 0;
    dialogue->scroll_cursor = dialogue->cursor;
    dialogue->newest_line_offset = 0U;
    dialogue->page_text_offsets[0] = source;

    while (dialogue->text[source] != '\0' && line < DIALOGUE_LINES &&
           output + 1 < sizeof(dialogue->page)) {
        if (dialogue->text[source] == '\n') {
            const size_t newline_source = source;
            ++source;
            if (line + 1 >= DIALOGUE_LINES) {
                break;
            }
            dialogue->page_text_offsets[output] = newline_source;
            dialogue->page[output++] = '\n';
            dialogue->page_text_offsets[output] = source;
            column = 0;
            ++line;
            if (line == 1) {
                dialogue->scroll_cursor = source;
            }
            if (line == DIALOGUE_LINES - 1) {
                dialogue->newest_line_offset = output;
            }
            continue;
        }

        if (dialogue->text[source] == ' ') {
            const size_t space_source = source;
            ++source;
            if (column > 0 && column < DIALOGUE_COLUMNS) {
                dialogue->page_text_offsets[output] = space_source;
                dialogue->page[output++] = ' ';
                dialogue->page_text_offsets[output] = source;
                ++column;
            } else {
                dialogue->page_text_offsets[output] = source;
            }
            continue;
        }

        size_t word_end = source;
        int word_columns = 0;
        while (dialogue->text[word_end] != '\0' && dialogue->text[word_end] != ' ' &&
               dialogue->text[word_end] != '\n') {
            uint32_t codepoint = 0U;
            word_end = utf8_next(dialogue->text, word_end, &codepoint);
            word_columns += codepoint_columns(codepoint);
        }

        if (column > 0 && column + word_columns > DIALOGUE_COLUMNS) {
            if (line + 1 >= DIALOGUE_LINES) {
                break;
            }
            while (output > 0 && dialogue->page[output - 1] == ' ') {
                --output;
            }
            dialogue->page_text_offsets[output] = source;
            dialogue->page[output++] = '\n';
            dialogue->page_text_offsets[output] = source;
            column = 0;
            ++line;
            if (line == 1) {
                dialogue->scroll_cursor = source;
            }
            if (line == DIALOGUE_LINES - 1) {
                dialogue->newest_line_offset = output;
            }
        }

        while (source < word_end && line < DIALOGUE_LINES) {
            uint32_t codepoint = 0U;
            const size_t next = utf8_next(dialogue->text, source, &codepoint);
            const size_t bytes = next - source;
            const int width = codepoint_columns(codepoint);
            if (column > 0 && column + width > DIALOGUE_COLUMNS) {
                if (line + 1 >= DIALOGUE_LINES) {
                    break;
                }
                dialogue->page_text_offsets[output] = source;
                dialogue->page[output++] = '\n';
                dialogue->page_text_offsets[output] = source;
                column = 0;
                ++line;
                if (line == 1) {
                    dialogue->scroll_cursor = source;
                }
                if (line == DIALOGUE_LINES - 1) {
                    dialogue->newest_line_offset = output;
                }
            }
            if (output + bytes + 1U > sizeof(dialogue->page)) {
                break;
            }
            memcpy(dialogue->page + output, dialogue->text + source, bytes);
            for (size_t byte = 0U; byte < bytes; ++byte) {
                dialogue->page_text_offsets[output + byte] = source + byte;
            }
            output += bytes;
            source = next;
            dialogue->page_text_offsets[output] = source;
            column += width;
        }
        if (source < word_end) {
            break;
        }
    }

    while (output > 0 &&
           (dialogue->page[output - 1] == ' ' || dialogue->page[output - 1] == '\n')) {
        --output;
    }
    dialogue->page[output] = '\0';
    dialogue->page_text_offsets[output] = source;
    dialogue->next_cursor = source;
    if (dialogue->scroll_cursor == dialogue->cursor) {
        dialogue->scroll_cursor = source;
    }
}

void eidolon_dialogue_set(EidolonDialogue *dialogue, const char *text, uint64_t now_ms) {
    memset(dialogue, 0, sizeof(*dialogue));
    dialogue->movement = EIDOLON_DIALOGUE_MOVEMENT_FOLLOW;
    dialogue->hold_ms = EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS;
    if (text == NULL) {
        return;
    }
    normalize_text(dialogue->text, sizeof(dialogue->text), text);
    eidolon_delivery_track_compile(&dialogue->delivery_track, dialogue->text);
    build_page(dialogue);
    dialogue->reveal_tick_ms = now_ms;
}

bool eidolon_dialogue_sync(EidolonDialogue *dialogue, const char *text, uint64_t now_ms) {
    if (dialogue == NULL || text == NULL) {
        return false;
    }
    char normalized[EIDOLON_DIALOGUE_TEXT_CAPACITY];
    normalize_text(normalized, sizeof(normalized), text);
    if (strcmp(dialogue->text, normalized) == 0) {
        return false;
    }

    const size_t previous_text_length = strlen(dialogue->text);
    size_t common = 0U;
    while (dialogue->text[common] != '\0' && normalized[common] != '\0' &&
           dialogue->text[common] == normalized[common]) {
        ++common;
    }
    const size_t page_length = strlen(dialogue->page);
    const size_t revealed = dialogue->revealed < page_length ? dialogue->revealed : page_length;
    const bool was_page_complete = dialogue->revealed >= page_length;
    size_t revealed_text_offset = dialogue->page_text_offsets[revealed];
    if (revealed_text_offset > common) {
        revealed_text_offset = common;
    }
    if (dialogue->cursor > common) {
        dialogue->cursor = common;
    }
    memcpy(dialogue->text, normalized, strlen(normalized) + 1U);
    eidolon_delivery_track_compile(&dialogue->delivery_track, dialogue->text);
    if (revealed_text_offset > 0U) {
        eidolon_delivery_track_seek_after(&dialogue->delivery_track, revealed_text_offset);
    }
    build_page(dialogue);

    dialogue->revealed = 0U;
    const size_t rebuilt_length = strlen(dialogue->page);
    while (dialogue->revealed < rebuilt_length &&
           dialogue->page_text_offsets[dialogue->revealed + 1U] <= revealed_text_offset) {
        ++dialogue->revealed;
    }
    if (strlen(normalized) > previous_text_length || dialogue->revealed < rebuilt_length) {
        dialogue->page_complete_tick_ms = 0U;
        if (was_page_complete || dialogue->reveal_tick_ms == 0U ||
            now_ms > dialogue->reveal_tick_ms + 1000U) {
            dialogue->reveal_tick_ms = now_ms;
        }
    }
    return true;
}

void eidolon_dialogue_configure(EidolonDialogue *dialogue, EidolonDialogueMovement movement,
                                unsigned int hold_ms) {
    if (dialogue == NULL || movement < 0 || movement >= EIDOLON_DIALOGUE_MOVEMENT_COUNT) {
        return;
    }
    dialogue->movement = movement;
    dialogue->hold_ms = hold_ms;
}

void eidolon_dialogue_update(EidolonDialogue *dialogue, uint64_t now_ms) {
    const size_t page_length = strlen(dialogue->page);
    if (dialogue->revealed >= page_length || now_ms < dialogue->reveal_tick_ms) {
        return;
    }

    const uint64_t elapsed = now_ms - dialogue->reveal_tick_ms;
    const size_t characters = (size_t)(elapsed / REVEAL_INTERVAL_MS);
    if (characters == 0) {
        return;
    }
    size_t advanced = 0U;
    for (size_t index = 0U; index < characters && dialogue->revealed < page_length; ++index) {
        const size_t next = utf8_next_grapheme(dialogue->page, dialogue->revealed);
        if (eidolon_expression_track_blocks_reveal(&dialogue->expression_track,
                                                   dialogue->page_text_offsets[next])) {
            break;
        }
        dialogue->revealed = next;
        advanced += 1U;
    }
    dialogue->reveal_tick_ms += (uint64_t)advanced * REVEAL_INTERVAL_MS;
    if (dialogue->revealed >= page_length && dialogue->page_complete_tick_ms == 0U) {
        dialogue->page_complete_tick_ms = now_ms;
    }
}

void eidolon_dialogue_resume(EidolonDialogue *dialogue, uint64_t now_ms) {
    if (dialogue != NULL && dialogue->revealed < strlen(dialogue->page)) {
        dialogue->reveal_tick_ms = now_ms;
    }
}

void eidolon_dialogue_advance(EidolonDialogue *dialogue, uint64_t now_ms) {
    const size_t page_length = strlen(dialogue->page);
    if (dialogue->revealed < page_length) {
        dialogue->revealed = page_length;
        dialogue->page_complete_tick_ms = now_ms;
        return;
    }
    if (dialogue->text[dialogue->next_cursor] == '\0') {
        return;
    }

    const bool rolling = dialogue->movement == EIDOLON_DIALOGUE_MOVEMENT_FOLLOW;
    dialogue->cursor = rolling ? dialogue->scroll_cursor : dialogue->next_cursor;
    build_page(dialogue);
    dialogue->revealed = rolling ? dialogue->newest_line_offset : 0U;
    dialogue->reveal_tick_ms = now_ms;
    dialogue->page_complete_tick_ms = 0U;
}

bool eidolon_dialogue_autoplay(EidolonDialogue *dialogue, uint64_t now_ms) {
    if (!eidolon_dialogue_has_next_page(dialogue) ||
        dialogue->movement == EIDOLON_DIALOGUE_MOVEMENT_MANUAL) {
        return false;
    }
    if (dialogue->movement == EIDOLON_DIALOGUE_MOVEMENT_FOLLOW) {
        dialogue->cursor = dialogue->scroll_cursor;
        build_page(dialogue);
        dialogue->revealed = dialogue->newest_line_offset;
        dialogue->reveal_tick_ms = now_ms;
        dialogue->page_complete_tick_ms = 0U;
        return true;
    }
    if (dialogue->page_complete_tick_ms == 0U) {
        dialogue->page_complete_tick_ms = now_ms;
        return false;
    }
    if (now_ms < dialogue->page_complete_tick_ms ||
        now_ms - dialogue->page_complete_tick_ms < dialogue->hold_ms) {
        return false;
    }
    dialogue->cursor = dialogue->next_cursor;
    build_page(dialogue);
    dialogue->revealed = 0U;
    dialogue->reveal_tick_ms = now_ms;
    dialogue->page_complete_tick_ms = 0U;
    return true;
}

float eidolon_dialogue_indicator_alpha(const EidolonDialogue *dialogue, uint64_t now_ms) {
    if (!eidolon_dialogue_has_next_page(dialogue) || dialogue->page_complete_tick_ms == 0U ||
        now_ms < dialogue->page_complete_tick_ms ||
        dialogue->movement == EIDOLON_DIALOGUE_MOVEMENT_FOLLOW) {
        return 0.0F;
    }
    if (dialogue->movement == EIDOLON_DIALOGUE_MOVEMENT_MANUAL) {
        return 1.0F;
    }
    const float phase = (float)((now_ms - dialogue->page_complete_tick_ms) % 1000U) / 1000.0F;
    return 0.28F + 0.72F * (0.5F + 0.5F * cosf(phase * 2.0F * 3.14159265F));
}

bool eidolon_dialogue_is_active(const EidolonDialogue *dialogue) {
    return dialogue->page[0] != '\0';
}

bool eidolon_dialogue_has_next_page(const EidolonDialogue *dialogue) {
    return dialogue->revealed >= strlen(dialogue->page) &&
           dialogue->text[dialogue->next_cursor] != '\0';
}

bool eidolon_dialogue_has_unread(const EidolonDialogue *dialogue) {
    return dialogue->revealed < strlen(dialogue->page) ||
           dialogue->text[dialogue->next_cursor] != '\0';
}

size_t eidolon_dialogue_revealed_text_offset(const EidolonDialogue *dialogue) {
    if (dialogue == NULL) {
        return 0U;
    }
    const size_t page_length = strlen(dialogue->page);
    const size_t revealed = dialogue->revealed < page_length ? dialogue->revealed : page_length;
    return dialogue->page_text_offsets[revealed];
}

const char *eidolon_dialogue_movement_name(EidolonDialogueMovement movement) {
    switch (movement) {
    case EIDOLON_DIALOGUE_MOVEMENT_MANUAL:
        return "manual";
    case EIDOLON_DIALOGUE_MOVEMENT_PAGED:
        return "paged";
    case EIDOLON_DIALOGUE_MOVEMENT_FOLLOW:
        return "follow";
    case EIDOLON_DIALOGUE_MOVEMENT_COUNT:
        break;
    }
    return "unknown";
}
