#ifndef EIDOLON_SESSION_REGISTRY_H
#define EIDOLON_SESSION_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "conversation.h"
#include "dialogue.h"
#include "hook_output.h"

#define EIDOLON_SESSION_CAPACITY 8U
#define EIDOLON_VISIBLE_SESSION_CAPACITY 4U
#define EIDOLON_SESSION_BUBBLE_TIMEOUT_MS 5000U
#define EIDOLON_SESSION_BUBBLE_FADE_MS 3000U
#define EIDOLON_SESSION_ID_CAPACITY EIDOLON_PROVIDER_SESSION_ID_CAPACITY
#define EIDOLON_SESSION_TITLE_CAPACITY EIDOLON_PROVIDER_TITLE_CAPACITY
#define EIDOLON_SESSION_PATH_CAPACITY 4096U

typedef struct EidolonSessionEntry {
    char provider[EIDOLON_PROVIDER_ID_CAPACITY];
    char id[EIDOLON_SESSION_ID_CAPACITY];
    char turn_id[EIDOLON_PROVIDER_TURN_ID_CAPACITY];
    char message_id[EIDOLON_PROVIDER_MESSAGE_ID_CAPACITY];
    char title[EIDOLON_SESSION_TITLE_CAPACITY];
    char path[EIDOLON_SESSION_PATH_CAPACITY];
    char last_output[EIDOLON_DIALOGUE_TEXT_CAPACITY];
    char source_timestamp[EIDOLON_TRANSCRIPT_TIMESTAMP_CAPACITY];
    EidolonDialogue dialogue;
    uint64_t stamp;
    uint64_t last_activity_ms;
    uint64_t dismissal_started_ms;
    uint64_t detected_ms;
    uint64_t first_glyph_ms;
    int layout_slot;
    bool occupied;
    bool visible;
    bool first_glyph_logged;
    bool streaming;
    bool live_owned;
} EidolonSessionEntry;

typedef struct EidolonSessionDiscovery EidolonSessionDiscovery;

typedef struct EidolonSessionRegistry {
    EidolonSessionEntry entries[EIDOLON_SESSION_CAPACITY];
    uint64_t next_poll_ms;
    EidolonSessionDiscovery *discovery;
    EidolonDialogueMovement dialogue_movement;
    unsigned int dialogue_hold_ms;
    bool dialogue_configured;
    bool legacy_transcripts_enabled;
    bool legacy_transcripts_configured;
} EidolonSessionRegistry;

typedef struct EidolonSessionPoll {
    bool changed;
    bool new_message;
    bool page_advanced;
    bool stream_started;
    bool stream_delta;
    bool message_completed;
    EidolonSessionEntry *message_session;
    EidolonSessionEntry *advanced_session;
} EidolonSessionPoll;

EidolonSessionPoll eidolon_session_registry_poll(EidolonSessionRegistry *registry, uint64_t now_ms);
bool eidolon_session_registry_init(EidolonSessionRegistry *registry);
void eidolon_session_registry_set_legacy_transcripts(EidolonSessionRegistry *registry,
                                                     bool enabled);
EidolonSessionPoll eidolon_session_registry_apply_event(EidolonSessionRegistry *registry,
                                                        const EidolonConversationEvent *event,
                                                        uint64_t now_ms);
void eidolon_session_registry_configure_dialogue(EidolonSessionRegistry *registry,
                                                 EidolonDialogueMovement movement,
                                                 unsigned int hold_ms);
void eidolon_session_registry_destroy(EidolonSessionRegistry *registry);
size_t eidolon_session_registry_visible_count(const EidolonSessionRegistry *registry);
float eidolon_session_entry_opacity(const EidolonSessionEntry *entry, uint64_t now_ms);
EidolonSessionEntry *eidolon_session_registry_at_slot(EidolonSessionRegistry *registry, int slot);
const EidolonSessionEntry *
eidolon_session_registry_at_slot_const(const EidolonSessionRegistry *registry, int slot);
bool eidolon_session_registry_advance(EidolonSessionRegistry *registry, int slot, uint64_t now_ms);

#endif
