#ifndef EIDOLON_SESSION_REGISTRY_H
#define EIDOLON_SESSION_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dialogue.h"

#define EIDOLON_SESSION_CAPACITY 8U
#define EIDOLON_VISIBLE_SESSION_CAPACITY 4U
#define EIDOLON_SESSION_ID_CAPACITY 37U
#define EIDOLON_SESSION_TITLE_CAPACITY 64U
#define EIDOLON_SESSION_PATH_CAPACITY 4096U

typedef struct EidolonSessionEntry {
    char id[EIDOLON_SESSION_ID_CAPACITY];
    char title[EIDOLON_SESSION_TITLE_CAPACITY];
    char path[EIDOLON_SESSION_PATH_CAPACITY];
    char last_output[EIDOLON_DIALOGUE_TEXT_CAPACITY];
    EidolonDialogue dialogue;
    uint64_t stamp;
    uint64_t last_activity_ms;
    uint64_t affect_sequence;
    int layout_slot;
    bool occupied;
    bool visible;
} EidolonSessionEntry;

typedef struct EidolonSessionDiscovery EidolonSessionDiscovery;

typedef struct EidolonSessionRegistry {
    EidolonSessionEntry entries[EIDOLON_SESSION_CAPACITY];
    uint64_t next_poll_ms;
    EidolonSessionDiscovery *discovery;
    EidolonDialogueMovement dialogue_movement;
    unsigned int dialogue_hold_ms;
    bool dialogue_configured;
} EidolonSessionRegistry;

typedef struct EidolonSessionPoll {
    bool changed;
    bool new_message;
    bool page_advanced;
    float speech_beat;
    EidolonSessionEntry *message_session;
    EidolonSessionEntry *advanced_session;
    EidolonSessionEntry *speaking_session;
} EidolonSessionPoll;

EidolonSessionPoll eidolon_session_registry_poll(EidolonSessionRegistry *registry, uint64_t now_ms);
bool eidolon_session_registry_init(EidolonSessionRegistry *registry);
void eidolon_session_registry_configure_dialogue(EidolonSessionRegistry *registry,
                                                 EidolonDialogueMovement movement,
                                                 unsigned int hold_ms);
void eidolon_session_registry_destroy(EidolonSessionRegistry *registry);
size_t eidolon_session_registry_visible_count(const EidolonSessionRegistry *registry);
EidolonSessionEntry *eidolon_session_registry_at_slot(EidolonSessionRegistry *registry, int slot);
const EidolonSessionEntry *
eidolon_session_registry_at_slot_const(const EidolonSessionRegistry *registry, int slot);
bool eidolon_session_registry_advance(EidolonSessionRegistry *registry, int slot, uint64_t now_ms);

#endif
