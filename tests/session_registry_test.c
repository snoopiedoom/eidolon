#include "session_registry.h"

#include "platform/session_files.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <string.h>

static uint64_t fixture_stamp = 1U;
static const char *fixture_output = "baseline";
static bool slow_discovery = false;
static SDL_AtomicInt discovery_entered;
static const char *fixture_path =
    "C:/fixture/rollout-2026-07-17T00-00-00-01234567-89ab-cdef-8123-456789abcdef.jsonl";

size_t eidolon_platform_list_transcripts(EidolonTranscriptFile *files, size_t capacity) {
    if (slow_discovery) {
        (void)SDL_SetAtomicInt(&discovery_entered, 1);
        SDL_Delay(120U);
    }
    if (capacity == 0U) {
        return 0U;
    }
    const size_t path_length = strlen(fixture_path);
    assert(path_length < sizeof(files[0].path));
    memcpy(files[0].path, fixture_path, path_length + 1U);
    files[0].stamp = fixture_stamp;
    return 1U;
}

bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp) {
    (void)path;
    (void)capacity;
    (void)stamp;
    return false;
}

bool eidolon_platform_transcript_stamp(const char *path, uint64_t *stamp) {
    assert(strcmp(path, fixture_path) == 0);
    *stamp = fixture_stamp;
    return true;
}

bool eidolon_platform_session_index_path(char *path, size_t capacity) {
    (void)path;
    (void)capacity;
    return false;
}

bool eidolon_transcript_read_agent_output(const char *path, char *output, size_t capacity) {
    assert(strcmp(path, fixture_path) == 0);
    const size_t output_length = strlen(fixture_output);
    assert(output_length + 1U <= capacity);
    memcpy(output, fixture_output, output_length + 1U);
    return true;
}

bool eidolon_transcript_read_agent_output_info(const char *path, char *output, size_t capacity,
                                               char *source_timestamp, size_t timestamp_capacity) {
    const bool found = eidolon_transcript_read_agent_output(path, output, capacity);
    if (source_timestamp != NULL && timestamp_capacity > 0U) {
        SDL_strlcpy(source_timestamp, "2026-07-20T10:00:00Z", timestamp_capacity);
    }
    return found;
}

bool eidolon_transcript_is_primary_session(const char *path) {
    assert(strcmp(path, fixture_path) == 0);
    return true;
}

int main(void) {
    assert(SDL_Init(0));
    EidolonSessionRegistry registry = {0};
    EidolonSessionEntry *entry = &registry.entries[0];
    entry->occupied = true;
    memcpy(entry->provider, "codex", sizeof("codex"));
    entry->stamp = fixture_stamp;
    memcpy(entry->id, "01234567-89ab-cdef-8123-456789abcdef", EIDOLON_SESSION_ID_CAPACITY);
    memcpy(entry->path, fixture_path, strlen(fixture_path) + 1U);
    memcpy(entry->last_output, fixture_output, strlen(fixture_output) + 1U);

    fixture_stamp = 2U;
    fixture_output = "smooth dialogue!";
    EidolonSessionPoll poll = eidolon_session_registry_poll(&registry, 10U);
    assert(poll.new_message);
    assert(poll.message_session != NULL);
    assert(strcmp(poll.message_session->dialogue.page, fixture_output) == 0);
    assert(poll.message_session->dialogue.revealed == 0U);
    assert(poll.message_session->detected_ms == 10U);
    assert(!poll.message_session->first_glyph_logged);

    poll = eidolon_session_registry_poll(&registry, 44U);
    assert(!poll.new_message);
    assert(registry.entries[0].dialogue.revealed > 0U);
    assert(registry.entries[0].first_glyph_logged);
    assert(registry.entries[0].first_glyph_ms == 44U);

    poll = eidolon_session_registry_poll(&registry, 500U);
    assert(!poll.new_message);

    EidolonConversationEvent delta = {.type = EIDOLON_CONVERSATION_TEXT_DELTA};
    SDL_strlcpy(delta.provider, "opencode", sizeof(delta.provider));
    SDL_strlcpy(delta.session_id, "session-live", sizeof(delta.session_id));
    SDL_strlcpy(delta.message_id, "message-live", sizeof(delta.message_id));
    SDL_strlcpy(delta.text, "hello ", sizeof(delta.text));
    poll = eidolon_session_registry_apply_event(&registry, &delta, 600U);
    assert(poll.stream_started);
    assert(poll.stream_delta);
    assert(strcmp(poll.message_session->provider, "opencode") == 0);
    delta.source_time_ms = 601U;
    SDL_strlcpy(delta.text, "world", sizeof(delta.text));
    poll = eidolon_session_registry_apply_event(&registry, &delta, 601U);
    assert(!poll.stream_started);
    assert(strcmp(poll.message_session->dialogue.text, "hello world") == 0);

    EidolonConversationEvent completed = {.type = EIDOLON_CONVERSATION_MESSAGE_COMPLETED};
    SDL_strlcpy(completed.provider, "opencode", sizeof(completed.provider));
    SDL_strlcpy(completed.session_id, "session-live", sizeof(completed.session_id));
    SDL_strlcpy(completed.message_id, "message-live", sizeof(completed.message_id));
    SDL_strlcpy(completed.text, "hello world!", sizeof(completed.text));
    poll = eidolon_session_registry_apply_event(&registry, &completed, 602U);
    assert(poll.message_completed);
    assert(!poll.new_message);
    assert(strcmp(poll.message_session->dialogue.text, "hello world!") == 0);

    EidolonSessionRegistry *timeouts = SDL_calloc(1U, sizeof(*timeouts));
    assert(timeouts != NULL);
    eidolon_session_registry_set_legacy_transcripts(timeouts, false);
    EidolonConversationEvent first = {.type = EIDOLON_CONVERSATION_MESSAGE_COMPLETED};
    SDL_strlcpy(first.provider, "opencode", sizeof(first.provider));
    SDL_strlcpy(first.session_id, "quiet-first", sizeof(first.session_id));
    SDL_strlcpy(first.text, "first remains unread", sizeof(first.text));
    poll = eidolon_session_registry_apply_event(timeouts, &first, 1000U);
    EidolonSessionEntry *first_entry = poll.message_session;
    assert(first_entry != NULL && first_entry->visible);

    EidolonConversationEvent second = {.type = EIDOLON_CONVERSATION_MESSAGE_COMPLETED};
    SDL_strlcpy(second.provider, "opencode", sizeof(second.provider));
    SDL_strlcpy(second.session_id, "active-second", sizeof(second.session_id));
    SDL_strlcpy(second.text, "second", sizeof(second.text));
    poll = eidolon_session_registry_apply_event(timeouts, &second, 4000U);
    EidolonSessionEntry *second_entry = poll.message_session;
    assert(second_entry != NULL && second_entry->visible);

    poll = eidolon_session_registry_poll(timeouts, 1000U + EIDOLON_SESSION_BUBBLE_TIMEOUT_MS - 1U);
    assert(!poll.changed);
    assert(first_entry->visible);
    assert(second_entry->visible);

    poll = eidolon_session_registry_poll(timeouts, 1000U + EIDOLON_SESSION_BUBBLE_TIMEOUT_MS);
    assert(!poll.changed);
    assert(first_entry->visible);
    assert(eidolon_session_entry_opacity(first_entry, 1000U + EIDOLON_SESSION_BUBBLE_TIMEOUT_MS) ==
           1.0F);
    assert(second_entry->visible);

    const uint64_t half_fade =
        1000U + EIDOLON_SESSION_BUBBLE_TIMEOUT_MS + EIDOLON_SESSION_BUBBLE_FADE_MS / 2U;
    poll = eidolon_session_registry_poll(timeouts, half_fade);
    assert(!poll.changed);
    assert(first_entry->visible);
    assert(eidolon_session_entry_opacity(first_entry, half_fade) == 0.5F);
    const int first_slot = first_entry->layout_slot;

    EidolonConversationEvent renewed = {.type = EIDOLON_CONVERSATION_TEXT_DELTA};
    SDL_strlcpy(renewed.provider, "opencode", sizeof(renewed.provider));
    SDL_strlcpy(renewed.session_id, "quiet-first", sizeof(renewed.session_id));
    SDL_strlcpy(renewed.message_id, "renewed-message", sizeof(renewed.message_id));
    SDL_strlcpy(renewed.text, "back", sizeof(renewed.text));
    poll = eidolon_session_registry_apply_event(timeouts, &renewed, half_fade + 1U);
    assert(!poll.changed);
    assert(first_entry->visible);
    assert(first_entry->layout_slot == first_slot);
    assert(first_entry->last_activity_ms == half_fade + 1U);
    assert(eidolon_session_entry_opacity(first_entry, half_fade + 1U) == 1.0F);

    const uint64_t renewed_hide_ms = first_entry->last_activity_ms +
                                     EIDOLON_SESSION_BUBBLE_TIMEOUT_MS +
                                     EIDOLON_SESSION_BUBBLE_FADE_MS;
    (void)eidolon_session_registry_poll(timeouts, renewed_hide_ms - 1U);
    assert(first_entry->visible);
    assert(eidolon_session_entry_opacity(first_entry, renewed_hide_ms - 1U) > 0.0F);
    poll = eidolon_session_registry_poll(timeouts, renewed_hide_ms);
    assert(poll.changed);
    assert(!first_entry->visible);
    assert(first_entry->layout_slot == -1);
    assert(eidolon_session_entry_opacity(first_entry, renewed_hide_ms) == 0.0F);
    eidolon_session_registry_destroy(timeouts);
    SDL_free(timeouts);

    EidolonSessionRegistry asynchronous = {0};
    slow_discovery = true;
    assert(eidolon_session_registry_init(&asynchronous));
    (void)eidolon_session_registry_poll(&asynchronous, SDL_GetTicks());
    const uint64_t discovery_deadline = SDL_GetTicks() + 1000U;
    while (SDL_GetAtomicInt(&discovery_entered) == 0 && SDL_GetTicks() < discovery_deadline) {
        SDL_Delay(1U);
    }
    assert(SDL_GetAtomicInt(&discovery_entered) != 0);
    const uint64_t presentation_started = SDL_GetTicks();
    (void)eidolon_session_registry_poll(&asynchronous, presentation_started);
    assert(SDL_GetTicks() - presentation_started < 20U);
    eidolon_session_registry_destroy(&asynchronous);
    SDL_Quit();
    return 0;
}
