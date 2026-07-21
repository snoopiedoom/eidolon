#include "dialogue.h"

#include <assert.h>
#include <string.h>

int main(void) {
    static const char long_message[] =
        "This is a deliberately long message that should wrap across several lines and then "
        "continue onto another JRPG dialogue page without losing words along the way. The pet "
        "has quite a lot to say today, apparently.";
    EidolonDialogue dialogue;
    eidolon_dialogue_set(&dialogue, long_message, 1000U);

    assert(eidolon_dialogue_is_active(&dialogue));
    assert(eidolon_dialogue_has_unread(&dialogue));
    assert(strlen(dialogue.page) < EIDOLON_DIALOGUE_PAGE_CAPACITY);
    assert(dialogue.revealed == 0);

    eidolon_dialogue_update(&dialogue, 1023);
    assert(dialogue.revealed == 0);
    eidolon_dialogue_update(&dialogue, 1024);
    assert(dialogue.revealed == 1);

    eidolon_dialogue_advance(&dialogue, 1100);
    assert(dialogue.revealed == strlen(dialogue.page));
    assert(eidolon_dialogue_has_next_page(&dialogue));

    char retained_lines[EIDOLON_DIALOGUE_PAGE_CAPACITY];
    const char *second_line = strchr(dialogue.page, '\n');
    assert(second_line != NULL);
    ++second_line;
    memcpy(retained_lines, second_line, strlen(second_line) + 1U);
    const size_t first_scroll_cursor = dialogue.scroll_cursor;
    assert(first_scroll_cursor > dialogue.cursor);
    assert(first_scroll_cursor < dialogue.next_cursor);
    assert(eidolon_dialogue_indicator_alpha(&dialogue, 1100U) == 0.0F);
    assert(eidolon_dialogue_autoplay(&dialogue, 1100U));
    assert(dialogue.cursor == first_scroll_cursor);
    assert(dialogue.revealed == dialogue.newest_line_offset);
    assert(dialogue.revealed > 0U);
    assert(eidolon_dialogue_has_unread(&dialogue));
    assert(strncmp(dialogue.page, retained_lines, strlen(retained_lines)) == 0);
    assert(dialogue.page[strlen(retained_lines)] == '\n');

    eidolon_dialogue_set(&dialogue, long_message, 1000U);
    eidolon_dialogue_configure(&dialogue, EIDOLON_DIALOGUE_MOVEMENT_PAGED, 3000U);
    eidolon_dialogue_advance(&dialogue, 1100U);
    const size_t next_page_cursor = dialogue.next_cursor;
    assert(eidolon_dialogue_indicator_alpha(&dialogue, 1100U) > 0.9F);
    assert(!eidolon_dialogue_autoplay(&dialogue, 4099U));
    assert(eidolon_dialogue_autoplay(&dialogue, 4100U));
    assert(dialogue.cursor == next_page_cursor);
    assert(dialogue.revealed == 0U);

    eidolon_dialogue_set(&dialogue, long_message, 1000U);
    eidolon_dialogue_configure(&dialogue, EIDOLON_DIALOGUE_MOVEMENT_MANUAL, 3000U);
    eidolon_dialogue_advance(&dialogue, 1100U);
    const size_t manual_page_cursor = dialogue.next_cursor;
    assert(eidolon_dialogue_indicator_alpha(&dialogue, 5000U) == 1.0F);
    assert(!eidolon_dialogue_autoplay(&dialogue, 100000U));
    eidolon_dialogue_advance(&dialogue, 5000U);
    assert(dialogue.cursor == manual_page_cursor);
    assert(dialogue.revealed == 0U);

    eidolon_dialogue_set(&dialogue, "Živjo, čudovita! 中文 한국어", 2000U);
    assert(strstr(dialogue.page, "Živjo") != NULL);
    assert(strstr(dialogue.page, "中文") != NULL);
    assert(strstr(dialogue.page, "한국어") != NULL);
    eidolon_dialogue_update(&dialogue, 2024U);
    assert(dialogue.revealed == strlen("Ž"));
    assert(((unsigned char)dialogue.page[dialogue.revealed] & 0xC0U) != 0x80U);

    eidolon_dialogue_set(&dialogue, "e\xCC\x81lan", 3000U);
    eidolon_dialogue_update(&dialogue, 3024U);
    assert(dialogue.revealed == 3U);

    eidolon_dialogue_set(&dialogue, "oh, really?!", 4000U);
    eidolon_dialogue_update(&dialogue, 5000U);

    eidolon_dialogue_set(&dialogue, "ready, set, go", 6000U);
    eidolon_expression_track_compile(&dialogue.expression_track, dialogue.text,
                                     EIDOLON_STATE_REVIEW);
    eidolon_dialogue_update(&dialogue, 7000U);
    assert(dialogue.revealed == 0U);
    eidolon_expression_track_fallback(&dialogue.expression_track, dialogue.text);
    eidolon_dialogue_resume(&dialogue, 7000U);
    eidolon_dialogue_update(&dialogue, 7024U);
    assert(dialogue.revealed == 1U);
    assert(eidolon_dialogue_revealed_text_offset(&dialogue) == 1U);

    eidolon_dialogue_set(&dialogue, "hello", 8000U);
    eidolon_dialogue_update(&dialogue, 9000U);
    assert(dialogue.revealed == strlen("hello"));
    assert(eidolon_dialogue_sync(&dialogue, "hello world 💖", 9100U));
    assert(strcmp(dialogue.text, "hello world 💖") == 0);
    assert(dialogue.revealed == strlen("hello"));
    eidolon_dialogue_update(&dialogue, 9123U);
    assert(dialogue.revealed == strlen("hello"));
    eidolon_dialogue_update(&dialogue, 9124U);
    assert(dialogue.revealed == strlen("hello "));
    assert(!eidolon_dialogue_sync(&dialogue, "hello world 💖", 9200U));
    return 0;
}
