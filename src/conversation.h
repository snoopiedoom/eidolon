#ifndef EIDOLON_CONVERSATION_H
#define EIDOLON_CONVERSATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_PROVIDER_ID_CAPACITY 24U
#define EIDOLON_PROVIDER_SESSION_ID_CAPACITY 96U
#define EIDOLON_PROVIDER_TURN_ID_CAPACITY 96U
#define EIDOLON_PROVIDER_MESSAGE_ID_CAPACITY 96U
#define EIDOLON_PROVIDER_TITLE_CAPACITY 128U
#define EIDOLON_PROVIDER_DELTA_CAPACITY 4096U
#define EIDOLON_CONVERSATION_EVENT_CAPACITY 256U

typedef enum EidolonConversationCapability {
    EIDOLON_CONVERSATION_CAP_LIFECYCLE = 1U << 0U,
    EIDOLON_CONVERSATION_CAP_TEXT_DELTA = 1U << 1U,
    EIDOLON_CONVERSATION_CAP_COMPLETION = 1U << 2U,
    EIDOLON_CONVERSATION_CAP_TITLES = 1U << 3U,
    EIDOLON_CONVERSATION_CAP_MULTI_SESSION = 1U << 4U,
    EIDOLON_CONVERSATION_CAP_HISTORY = 1U << 5U,
} EidolonConversationCapability;

typedef enum EidolonConversationEventType {
    EIDOLON_CONVERSATION_SOURCE_CONNECTED,
    EIDOLON_CONVERSATION_SOURCE_DISCONNECTED,
    EIDOLON_CONVERSATION_SESSION_UPDATED,
    EIDOLON_CONVERSATION_TURN_STARTED,
    EIDOLON_CONVERSATION_TEXT_DELTA,
    EIDOLON_CONVERSATION_MESSAGE_COMPLETED,
    EIDOLON_CONVERSATION_TURN_COMPLETED,
} EidolonConversationEventType;

typedef enum EidolonConversationPhase {
    EIDOLON_CONVERSATION_PHASE_UNKNOWN,
    EIDOLON_CONVERSATION_PHASE_COMMENTARY,
    EIDOLON_CONVERSATION_PHASE_FINAL,
} EidolonConversationPhase;

typedef struct EidolonConversationEvent {
    EidolonConversationEventType type;
    EidolonConversationPhase phase;
    char provider[EIDOLON_PROVIDER_ID_CAPACITY];
    char session_id[EIDOLON_PROVIDER_SESSION_ID_CAPACITY];
    char turn_id[EIDOLON_PROVIDER_TURN_ID_CAPACITY];
    char message_id[EIDOLON_PROVIDER_MESSAGE_ID_CAPACITY];
    char title[EIDOLON_PROVIDER_TITLE_CAPACITY];
    char text[EIDOLON_PROVIDER_DELTA_CAPACITY];
    uint64_t source_time_ms;
} EidolonConversationEvent;

typedef struct EidolonConversationBus EidolonConversationBus;

EidolonConversationBus *eidolon_conversation_bus_create(void);
void eidolon_conversation_bus_destroy(EidolonConversationBus *bus);
bool eidolon_conversation_bus_push(EidolonConversationBus *bus,
                                   const EidolonConversationEvent *event);
bool eidolon_conversation_bus_poll(EidolonConversationBus *bus, EidolonConversationEvent *event);
uint64_t eidolon_conversation_bus_dropped(const EidolonConversationBus *bus);
const char *eidolon_conversation_event_type_name(EidolonConversationEventType type);

#endif
