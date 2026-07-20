#include "conversation.h"

#include <SDL3/SDL.h>

struct EidolonConversationBus {
    SDL_Mutex *mutex;
    EidolonConversationEvent events[EIDOLON_CONVERSATION_EVENT_CAPACITY];
    size_t read_index;
    size_t write_index;
    size_t count;
    uint64_t dropped;
};

EidolonConversationBus *eidolon_conversation_bus_create(void) {
    EidolonConversationBus *bus = SDL_calloc(1U, sizeof(*bus));
    if (bus == NULL) {
        return NULL;
    }
    bus->mutex = SDL_CreateMutex();
    if (bus->mutex == NULL) {
        SDL_free(bus);
        return NULL;
    }
    return bus;
}

void eidolon_conversation_bus_destroy(EidolonConversationBus *bus) {
    if (bus != NULL) {
        SDL_DestroyMutex(bus->mutex);
        SDL_free(bus);
    }
}

bool eidolon_conversation_bus_push(EidolonConversationBus *bus,
                                   const EidolonConversationEvent *event) {
    if (bus == NULL || event == NULL) {
        return false;
    }
    SDL_LockMutex(bus->mutex);
    if (bus->count == EIDOLON_CONVERSATION_EVENT_CAPACITY) {
        bus->dropped += 1U;
        SDL_UnlockMutex(bus->mutex);
        return false;
    }
    bus->events[bus->write_index] = *event;
    bus->write_index = (bus->write_index + 1U) % EIDOLON_CONVERSATION_EVENT_CAPACITY;
    bus->count += 1U;
    SDL_UnlockMutex(bus->mutex);
    return true;
}

bool eidolon_conversation_bus_poll(EidolonConversationBus *bus, EidolonConversationEvent *event) {
    if (bus == NULL || event == NULL) {
        return false;
    }
    SDL_LockMutex(bus->mutex);
    if (bus->count == 0U) {
        SDL_UnlockMutex(bus->mutex);
        return false;
    }
    *event = bus->events[bus->read_index];
    bus->read_index = (bus->read_index + 1U) % EIDOLON_CONVERSATION_EVENT_CAPACITY;
    bus->count -= 1U;
    SDL_UnlockMutex(bus->mutex);
    return true;
}

uint64_t eidolon_conversation_bus_dropped(const EidolonConversationBus *bus) {
    if (bus == NULL) {
        return 0U;
    }
    SDL_LockMutex(bus->mutex);
    const uint64_t dropped = bus->dropped;
    SDL_UnlockMutex(bus->mutex);
    return dropped;
}

const char *eidolon_conversation_event_type_name(EidolonConversationEventType type) {
    switch (type) {
    case EIDOLON_CONVERSATION_SOURCE_CONNECTED:
        return "source-connected";
    case EIDOLON_CONVERSATION_SOURCE_DISCONNECTED:
        return "source-disconnected";
    case EIDOLON_CONVERSATION_SESSION_UPDATED:
        return "session-updated";
    case EIDOLON_CONVERSATION_TURN_STARTED:
        return "turn-started";
    case EIDOLON_CONVERSATION_TEXT_DELTA:
        return "text-delta";
    case EIDOLON_CONVERSATION_MESSAGE_COMPLETED:
        return "message-completed";
    case EIDOLON_CONVERSATION_TURN_COMPLETED:
        return "turn-completed";
    }
    return "unknown";
}
