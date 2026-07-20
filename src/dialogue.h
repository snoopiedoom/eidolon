#ifndef EIDOLON_DIALOGUE_H
#define EIDOLON_DIALOGUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "expression_director.h"

#define EIDOLON_DIALOGUE_TEXT_CAPACITY 4096
#define EIDOLON_DIALOGUE_PAGE_CAPACITY 256
#define EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS 3000U

typedef enum EidolonDialogueMovement {
    EIDOLON_DIALOGUE_MOVEMENT_MANUAL,
    EIDOLON_DIALOGUE_MOVEMENT_PAGED,
    EIDOLON_DIALOGUE_MOVEMENT_FOLLOW,
    EIDOLON_DIALOGUE_MOVEMENT_COUNT,
} EidolonDialogueMovement;

typedef struct EidolonDialogue {
    char text[EIDOLON_DIALOGUE_TEXT_CAPACITY];
    char page[EIDOLON_DIALOGUE_PAGE_CAPACITY];
    size_t page_text_offsets[EIDOLON_DIALOGUE_PAGE_CAPACITY + 1U];
    size_t cursor;
    size_t scroll_cursor;
    size_t next_cursor;
    size_t newest_line_offset;
    size_t revealed;
    uint64_t reveal_tick_ms;
    uint64_t page_complete_tick_ms;
    EidolonDialogueMovement movement;
    unsigned int hold_ms;
    EidolonExpressionTrack expression_track;
} EidolonDialogue;

void eidolon_dialogue_set(EidolonDialogue *dialogue, const char *text, uint64_t now_ms);
bool eidolon_dialogue_sync(EidolonDialogue *dialogue, const char *text, uint64_t now_ms);
void eidolon_dialogue_configure(EidolonDialogue *dialogue, EidolonDialogueMovement movement,
                                unsigned int hold_ms);
void eidolon_dialogue_update(EidolonDialogue *dialogue, uint64_t now_ms);
void eidolon_dialogue_resume(EidolonDialogue *dialogue, uint64_t now_ms);
void eidolon_dialogue_advance(EidolonDialogue *dialogue, uint64_t now_ms);
bool eidolon_dialogue_autoplay(EidolonDialogue *dialogue, uint64_t now_ms);
float eidolon_dialogue_indicator_alpha(const EidolonDialogue *dialogue, uint64_t now_ms);
float eidolon_dialogue_reveal_emphasis(const EidolonDialogue *dialogue,
                                       size_t previous_revealed);
bool eidolon_dialogue_is_active(const EidolonDialogue *dialogue);
bool eidolon_dialogue_has_next_page(const EidolonDialogue *dialogue);
bool eidolon_dialogue_has_unread(const EidolonDialogue *dialogue);
size_t eidolon_dialogue_revealed_text_offset(const EidolonDialogue *dialogue);
const char *eidolon_dialogue_movement_name(EidolonDialogueMovement movement);

#endif
