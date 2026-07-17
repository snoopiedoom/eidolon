#include "session_registry.h"

#include "hook_output.h"
#include "log.h"
#include "platform/session_files.h"

#include <SDL3/SDL.h>

#include <ctype.h>
#include <string.h>

#define SESSION_POLL_INTERVAL_MS 500U
#define SESSION_QUIET_TIMEOUT_MS (5U * 60U * 1000U)
#define SESSION_DISCOVERY_CANDIDATES 32U

struct EidolonSessionDiscovery {
    SDL_Mutex *mutex;
    SDL_Condition *condition;
    SDL_Thread *thread;
    EidolonTranscriptFile files[SESSION_DISCOVERY_CANDIDATES];
    char *session_index;
    size_t count;
    bool requested;
    bool scanning;
    bool ready;
    bool stopping;
};

static int SDLCALL discovery_thread(void *userdata) {
    EidolonSessionDiscovery *discovery = userdata;
    for (;;) {
        SDL_LockMutex(discovery->mutex);
        while (!discovery->requested && !discovery->stopping) {
            SDL_WaitCondition(discovery->condition, discovery->mutex);
        }
        if (discovery->stopping) {
            SDL_UnlockMutex(discovery->mutex);
            return 0;
        }
        discovery->requested = false;
        discovery->scanning = true;
        SDL_UnlockMutex(discovery->mutex);

        EidolonTranscriptFile files[SESSION_DISCOVERY_CANDIDATES];
        const size_t count = eidolon_platform_list_transcripts(files, SESSION_DISCOVERY_CANDIDATES);
        char index_path[EIDOLON_SESSION_PATH_CAPACITY];
        char *session_index = NULL;
        if (eidolon_platform_session_index_path(index_path, sizeof(index_path))) {
            session_index = SDL_LoadFile(index_path, NULL);
        }

        SDL_LockMutex(discovery->mutex);
        SDL_memcpy(discovery->files, files, count * sizeof(files[0]));
        discovery->count = count;
        SDL_free(discovery->session_index);
        discovery->session_index = session_index;
        discovery->ready = true;
        discovery->scanning = false;
        SDL_UnlockMutex(discovery->mutex);
    }
}

bool eidolon_session_registry_init(EidolonSessionRegistry *registry) {
    if (registry == NULL) {
        return false;
    }
    eidolon_session_registry_configure_dialogue(
        registry, EIDOLON_DIALOGUE_MOVEMENT_FOLLOW, EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS);
    EidolonSessionDiscovery *discovery = SDL_calloc(1U, sizeof(*discovery));
    if (discovery == NULL) {
        return false;
    }
    discovery->mutex = SDL_CreateMutex();
    discovery->condition = SDL_CreateCondition();
    if (discovery->mutex == NULL || discovery->condition == NULL) {
        SDL_DestroyCondition(discovery->condition);
        SDL_DestroyMutex(discovery->mutex);
        SDL_free(discovery);
        return false;
    }
    discovery->thread = SDL_CreateThread(discovery_thread, "eidolon-sessions", discovery);
    if (discovery->thread == NULL) {
        SDL_DestroyCondition(discovery->condition);
        SDL_DestroyMutex(discovery->mutex);
        SDL_free(discovery);
        return false;
    }
    registry->discovery = discovery;
    return true;
}

void eidolon_session_registry_configure_dialogue(EidolonSessionRegistry *registry,
                                                 EidolonDialogueMovement movement,
                                                 unsigned int hold_ms) {
    if (registry == NULL || movement < 0 || movement >= EIDOLON_DIALOGUE_MOVEMENT_COUNT) {
        return;
    }
    registry->dialogue_movement = movement;
    registry->dialogue_hold_ms = hold_ms;
    registry->dialogue_configured = true;
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        eidolon_dialogue_configure(&registry->entries[index].dialogue, movement, hold_ms);
    }
}

void eidolon_session_registry_destroy(EidolonSessionRegistry *registry) {
    if (registry == NULL || registry->discovery == NULL) {
        return;
    }
    EidolonSessionDiscovery *discovery = registry->discovery;
    SDL_LockMutex(discovery->mutex);
    discovery->stopping = true;
    SDL_SignalCondition(discovery->condition);
    SDL_UnlockMutex(discovery->mutex);
    SDL_WaitThread(discovery->thread, NULL);
    SDL_free(discovery->session_index);
    SDL_DestroyCondition(discovery->condition);
    SDL_DestroyMutex(discovery->mutex);
    SDL_free(discovery);
    registry->discovery = NULL;
}

static void request_discovery(EidolonSessionRegistry *registry) {
    EidolonSessionDiscovery *discovery = registry->discovery;
    if (discovery == NULL) {
        return;
    }
    SDL_LockMutex(discovery->mutex);
    if (!discovery->requested && !discovery->scanning && !discovery->ready) {
        discovery->requested = true;
        SDL_SignalCondition(discovery->condition);
    }
    SDL_UnlockMutex(discovery->mutex);
}

static size_t consume_discovery(EidolonSessionRegistry *registry,
                                EidolonTranscriptFile files[SESSION_DISCOVERY_CANDIDATES],
                                char **session_index) {
    EidolonSessionDiscovery *discovery = registry->discovery;
    if (discovery == NULL) {
        return 0U;
    }
    SDL_LockMutex(discovery->mutex);
    size_t count = 0U;
    if (discovery->ready) {
        count = discovery->count;
        SDL_memcpy(files, discovery->files, count * sizeof(files[0]));
        *session_index = discovery->session_index;
        discovery->session_index = NULL;
        discovery->ready = false;
    }
    SDL_UnlockMutex(discovery->mutex);
    return count;
}

static size_t list_known_transcripts(EidolonSessionRegistry *registry, EidolonTranscriptFile *files,
                                     size_t capacity) {
    size_t count = 0U;
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY && count < capacity; ++index) {
        EidolonSessionEntry *entry = &registry->entries[index];
        uint64_t stamp = 0U;
        if (!entry->occupied || !eidolon_platform_transcript_stamp(entry->path, &stamp)) {
            continue;
        }
        SDL_strlcpy(files[count].path, entry->path, sizeof(files[count].path));
        files[count].stamp = stamp;
        ++count;
    }
    return count;
}

static bool extract_json_string(const char *line, const char *key, char *output, size_t capacity) {
    char pattern[80];
    SDL_snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *cursor = strstr(line, pattern);
    if (cursor == NULL || capacity == 0U) {
        return false;
    }
    cursor += strlen(pattern);
    size_t length = 0U;
    while (*cursor != '\0' && *cursor != '"') {
        char value = *cursor++;
        if (value == '\\' && *cursor != '\0') {
            value = *cursor++;
            if (value == 'n') {
                value = ' ';
            }
        }
        if (length + 1U < capacity) {
            output[length++] = value;
        }
    }
    output[length] = '\0';
    return *cursor == '"';
}

static bool id_from_path(const char *path, char id[EIDOLON_SESSION_ID_CAPACITY]) {
    const char *suffix = strstr(path, ".jsonl");
    if (suffix == NULL || (size_t)(suffix - path) < 36U) {
        return false;
    }
    const char *start = suffix - 36;
    for (size_t index = 0U; index < 36U; ++index) {
        const bool hyphen = index == 8U || index == 13U || index == 18U || index == 23U;
        if ((hyphen && start[index] != '-') ||
            (!hyphen && !isxdigit((unsigned char)start[index]))) {
            return false;
        }
    }
    SDL_memcpy(id, start, 36U);
    id[36] = '\0';
    return true;
}

static EidolonSessionEntry *find_entry(EidolonSessionRegistry *registry, const char *id) {
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        if (registry->entries[index].occupied && strcmp(registry->entries[index].id, id) == 0) {
            return &registry->entries[index];
        }
    }
    return NULL;
}

static EidolonSessionEntry *allocate_entry(EidolonSessionRegistry *registry) {
    EidolonSessionEntry *oldest = &registry->entries[0];
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        if (!registry->entries[index].occupied) {
            return &registry->entries[index];
        }
        if (!registry->entries[index].visible &&
            (oldest->visible || registry->entries[index].stamp < oldest->stamp)) {
            oldest = &registry->entries[index];
        }
    }
    return oldest->visible ? NULL : oldest;
}

static int allocate_slot(EidolonSessionRegistry *registry, uint64_t now_ms) {
    bool used[EIDOLON_VISIBLE_SESSION_CAPACITY] = {false};
    EidolonSessionEntry *oldest = NULL;
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        EidolonSessionEntry *entry = &registry->entries[index];
        if (!entry->visible) {
            continue;
        }
        if (entry->layout_slot >= 0 && entry->layout_slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY) {
            used[entry->layout_slot] = true;
        }
        if (oldest == NULL || entry->last_activity_ms < oldest->last_activity_ms) {
            oldest = entry;
        }
    }
    for (int slot = 0; slot < (int)EIDOLON_VISIBLE_SESSION_CAPACITY; ++slot) {
        if (!used[slot]) {
            return slot;
        }
    }
    if (oldest != NULL) {
        const int slot = oldest->layout_slot;
        oldest->visible = false;
        eidolon_log_write("session", "visible capacity reached; retiring %s age_ms=%llu",
                          oldest->id, (unsigned long long)(now_ms - oldest->last_activity_ms));
        return slot;
    }
    return 0;
}

static void refresh_titles(EidolonSessionRegistry *registry, char *text) {
    if (text == NULL) {
        return;
    }
    char *cursor = text;
    while (*cursor != '\0') {
        char *end = strchr(cursor, '\n');
        if (end != NULL) {
            *end = '\0';
        }
        char id[EIDOLON_SESSION_ID_CAPACITY];
        char title[EIDOLON_SESSION_TITLE_CAPACITY];
        if (extract_json_string(cursor, "id", id, sizeof(id)) &&
            extract_json_string(cursor, "thread_name", title, sizeof(title))) {
            EidolonSessionEntry *entry = find_entry(registry, id);
            if (entry != NULL) {
                SDL_strlcpy(entry->title, title, sizeof(entry->title));
            }
        }
        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }
    SDL_free(text);
}

EidolonSessionPoll eidolon_session_registry_poll(EidolonSessionRegistry *registry,
                                                 uint64_t now_ms) {
    EidolonSessionPoll result = {0};
    if (registry == NULL) {
        return result;
    }
    if (!registry->dialogue_configured) {
        eidolon_session_registry_configure_dialogue(
            registry, EIDOLON_DIALOGUE_MOVEMENT_FOLLOW, EIDOLON_DIALOGUE_AUTOPLAY_HOLD_MS);
    }
    const bool discovery_due = now_ms >= registry->next_poll_ms;
    EidolonTranscriptFile files[SESSION_DISCOVERY_CANDIDATES];
    size_t count = 0U;
    char *session_index = NULL;
    if (discovery_due) {
        registry->next_poll_ms = now_ms + SESSION_POLL_INTERVAL_MS;
        request_discovery(registry);
    }
    count = consume_discovery(registry, files, &session_index);
    if (count == 0U) {
        count = list_known_transcripts(registry, files, SESSION_DISCOVERY_CANDIDATES);
    }
    for (size_t index = 0U; index < count; ++index) {
        char id[EIDOLON_SESSION_ID_CAPACITY];
        if (!id_from_path(files[index].path, id) ||
            !eidolon_transcript_is_primary_session(files[index].path)) {
            continue;
        }
        EidolonSessionEntry *entry = find_entry(registry, id);
        if (entry == NULL) {
            entry = allocate_entry(registry);
            if (entry == NULL) {
                continue;
            }
            SDL_zero(*entry);
            entry->occupied = true;
            entry->layout_slot = -1;
            SDL_strlcpy(entry->id, id, sizeof(entry->id));
            SDL_strlcpy(entry->path, files[index].path, sizeof(entry->path));
            SDL_snprintf(entry->title, sizeof(entry->title), "SESSION %.8s", id);
            (void)eidolon_transcript_read_agent_output(entry->path, entry->last_output,
                                                       sizeof(entry->last_output));
            entry->stamp = files[index].stamp;
            continue;
        }
        if (entry->stamp == files[index].stamp) {
            continue;
        }
        entry->stamp = files[index].stamp;
        char output[EIDOLON_DIALOGUE_TEXT_CAPACITY];
        if (!eidolon_transcript_read_agent_output(entry->path, output, sizeof(output)) ||
            output[0] == '\0' || strcmp(output, entry->last_output) == 0) {
            continue;
        }
        SDL_strlcpy(entry->last_output, output, sizeof(entry->last_output));
        eidolon_dialogue_set(&entry->dialogue, output, now_ms);
        eidolon_dialogue_configure(&entry->dialogue, registry->dialogue_movement,
                                   registry->dialogue_hold_ms);
        entry->last_activity_ms = now_ms;
        if (!entry->visible) {
            entry->layout_slot = allocate_slot(registry, now_ms);
            entry->visible = true;
            result.changed = true;
        }
        result.new_message = true;
        result.message_session = entry;
        eidolon_log_write("session", "agent output session=%s title=%s bytes=%zu", entry->id,
                          entry->title, strlen(output));
    }
    if (session_index != NULL) {
        refresh_titles(registry, session_index);
    }
    for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
        EidolonSessionEntry *entry = &registry->entries[index];
        if (entry->visible && now_ms - entry->last_activity_ms >= SESSION_QUIET_TIMEOUT_MS &&
            !eidolon_dialogue_has_unread(&entry->dialogue)) {
            entry->visible = false;
            entry->layout_slot = -1;
            result.changed = true;
        }
        if (entry->visible) {
            const size_t previous_revealed = entry->dialogue.revealed;
            eidolon_dialogue_update(&entry->dialogue, now_ms);
            const float speech_beat =
                eidolon_dialogue_reveal_emphasis(&entry->dialogue, previous_revealed);
            if (speech_beat > result.speech_beat) {
                result.speech_beat = speech_beat;
                result.speaking_session = entry;
            }
            if (eidolon_dialogue_autoplay(&entry->dialogue, now_ms)) {
                result.page_advanced = true;
                result.advanced_session = entry;
            }
        }
    }
    return result;
}

size_t eidolon_session_registry_visible_count(const EidolonSessionRegistry *registry) {
    size_t count = 0U;
    if (registry != NULL) {
        for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
            count += registry->entries[index].visible ? 1U : 0U;
        }
    }
    return count;
}

EidolonSessionEntry *eidolon_session_registry_at_slot(EidolonSessionRegistry *registry, int slot) {
    if (registry != NULL) {
        for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
            if (registry->entries[index].visible && registry->entries[index].layout_slot == slot) {
                return &registry->entries[index];
            }
        }
    }
    return NULL;
}

const EidolonSessionEntry *
eidolon_session_registry_at_slot_const(const EidolonSessionRegistry *registry, int slot) {
    if (registry != NULL) {
        for (size_t index = 0U; index < EIDOLON_SESSION_CAPACITY; ++index) {
            if (registry->entries[index].visible && registry->entries[index].layout_slot == slot) {
                return &registry->entries[index];
            }
        }
    }
    return NULL;
}

bool eidolon_session_registry_advance(EidolonSessionRegistry *registry, int slot, uint64_t now_ms) {
    EidolonSessionEntry *entry = eidolon_session_registry_at_slot(registry, slot);
    if (entry == NULL) {
        return false;
    }
    const size_t previous_cursor = entry->dialogue.cursor;
    const size_t previous_revealed = entry->dialogue.revealed;
    eidolon_dialogue_advance(&entry->dialogue, now_ms);
    return previous_cursor != entry->dialogue.cursor ||
           previous_revealed != entry->dialogue.revealed;
}
