#include "dialogue.h"

#include <string.h>

#define DIALOGUE_COLUMNS 39
#define DIALOGUE_LINES 5
#define REVEAL_INTERVAL_MS 24U

static void normalize_text(char *output, size_t capacity, const char *input) {
    size_t length = 0;
    bool previous_space = false;
    bool line_start = true;

    while (*input != '\0' && length + 1 < capacity) {
        const unsigned char character = (unsigned char)*input++;
        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
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
            if (!previous_space && !line_start) {
                output[length++] = ' ';
                previous_space = true;
            }
            continue;
        }
        if (character >= 128) {
            if ((character & 0xC0U) == 0x80U) {
                continue;
            }
            output[length++] = '?';
        } else if (character >= 32) {
            output[length++] = (char)character;
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

    while (dialogue->text[source] != '\0' && line < DIALOGUE_LINES &&
           output + 1 < sizeof(dialogue->page)) {
        if (dialogue->text[source] == '\n') {
            ++source;
            if (line + 1 >= DIALOGUE_LINES) {
                break;
            }
            dialogue->page[output++] = '\n';
            column = 0;
            ++line;
            continue;
        }

        if (dialogue->text[source] == ' ') {
            ++source;
            if (column > 0 && column < DIALOGUE_COLUMNS) {
                dialogue->page[output++] = ' ';
                ++column;
            }
            continue;
        }

        size_t word_length = 0;
        while (dialogue->text[source + word_length] != '\0' &&
               dialogue->text[source + word_length] != ' ' &&
               dialogue->text[source + word_length] != '\n') {
            ++word_length;
        }

        if (column > 0 && column + (int)word_length > DIALOGUE_COLUMNS) {
            if (line + 1 >= DIALOGUE_LINES) {
                break;
            }
            while (output > 0 && dialogue->page[output - 1] == ' ') {
                --output;
            }
            dialogue->page[output++] = '\n';
            column = 0;
            ++line;
        }

        while (word_length > 0 && line < DIALOGUE_LINES &&
               output + 1 < sizeof(dialogue->page)) {
            if (column == DIALOGUE_COLUMNS) {
                if (line + 1 >= DIALOGUE_LINES) {
                    break;
                }
                dialogue->page[output++] = '\n';
                column = 0;
                ++line;
            }
            dialogue->page[output++] = dialogue->text[source++];
            ++column;
            --word_length;
        }
        if (word_length > 0) {
            break;
        }
    }

    while (output > 0 && (dialogue->page[output - 1] == ' ' ||
                          dialogue->page[output - 1] == '\n')) {
        --output;
    }
    dialogue->page[output] = '\0';
    dialogue->next_cursor = source;
}

void eidolon_dialogue_set(EidolonDialogue *dialogue, const char *text, uint64_t now_ms) {
    memset(dialogue, 0, sizeof(*dialogue));
    if (text == NULL) {
        return;
    }
    normalize_text(dialogue->text, sizeof(dialogue->text), text);
    build_page(dialogue);
    dialogue->reveal_tick_ms = now_ms;
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
    dialogue->revealed += characters;
    if (dialogue->revealed > page_length) {
        dialogue->revealed = page_length;
    }
    dialogue->reveal_tick_ms += (uint64_t)characters * REVEAL_INTERVAL_MS;
}

void eidolon_dialogue_advance(EidolonDialogue *dialogue, uint64_t now_ms) {
    const size_t page_length = strlen(dialogue->page);
    if (dialogue->revealed < page_length) {
        dialogue->revealed = page_length;
        return;
    }
    if (dialogue->text[dialogue->next_cursor] == '\0') {
        return;
    }

    dialogue->cursor = dialogue->next_cursor;
    dialogue->revealed = 0;
    build_page(dialogue);
    dialogue->reveal_tick_ms = now_ms;
}

bool eidolon_dialogue_is_active(const EidolonDialogue *dialogue) {
    return dialogue->page[0] != '\0';
}

bool eidolon_dialogue_has_next_page(const EidolonDialogue *dialogue) {
    return dialogue->revealed >= strlen(dialogue->page) &&
           dialogue->text[dialogue->next_cursor] != '\0';
}
