#ifndef EIDOLON_DIALOGUE_H
#define EIDOLON_DIALOGUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_DIALOGUE_TEXT_CAPACITY 4096
#define EIDOLON_DIALOGUE_PAGE_CAPACITY 256

typedef struct EidolonDialogue {
    char text[EIDOLON_DIALOGUE_TEXT_CAPACITY];
    char page[EIDOLON_DIALOGUE_PAGE_CAPACITY];
    size_t cursor;
    size_t next_cursor;
    size_t revealed;
    uint64_t reveal_tick_ms;
} EidolonDialogue;

void eidolon_dialogue_set(EidolonDialogue *dialogue, const char *text, uint64_t now_ms);
void eidolon_dialogue_update(EidolonDialogue *dialogue, uint64_t now_ms);
void eidolon_dialogue_advance(EidolonDialogue *dialogue, uint64_t now_ms);
bool eidolon_dialogue_is_active(const EidolonDialogue *dialogue);
bool eidolon_dialogue_has_next_page(const EidolonDialogue *dialogue);

#endif
