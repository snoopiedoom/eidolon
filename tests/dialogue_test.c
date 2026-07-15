#include "dialogue.h"

#include <assert.h>
#include <string.h>

int main(void) {
    EidolonDialogue dialogue;
    eidolon_dialogue_set(
        &dialogue,
        "This is a deliberately long message that should wrap across several lines and then "
        "continue onto another JRPG dialogue page without losing words along the way. The pet "
        "has quite a lot to say today, apparently.",
        1000);

    assert(eidolon_dialogue_is_active(&dialogue));
    assert(strlen(dialogue.page) < EIDOLON_DIALOGUE_PAGE_CAPACITY);
    assert(dialogue.revealed == 0);

    eidolon_dialogue_update(&dialogue, 1023);
    assert(dialogue.revealed == 0);
    eidolon_dialogue_update(&dialogue, 1024);
    assert(dialogue.revealed == 1);

    eidolon_dialogue_advance(&dialogue, 1100);
    assert(dialogue.revealed == strlen(dialogue.page));
    assert(eidolon_dialogue_has_next_page(&dialogue));

    const size_t first_cursor = dialogue.next_cursor;
    eidolon_dialogue_advance(&dialogue, 1200);
    assert(dialogue.cursor == first_cursor);
    assert(dialogue.revealed == 0);
    return 0;
}
