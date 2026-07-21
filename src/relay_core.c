#include "relay_core.h"

#include <SDL3/SDL.h>

#include <string.h>

typedef struct RelayBridge RelayBridge;

typedef struct RelayDirection {
    RelayBridge *bridge;
    const EidolonRelayEndpoint *source;
    const EidolonRelayEndpoint *destination;
    EidolonRelayObservation observation;
    bool clean;
} RelayDirection;

struct RelayBridge {
    const EidolonRelayEndpoint *left;
    const EidolonRelayEndpoint *right;
    SDL_AtomicInt stopping;
};

bool eidolon_relay_message_reserve(EidolonRelayMessage *message, size_t capacity) {
    if (message == NULL || capacity > EIDOLON_RELAY_MESSAGE_LIMIT) {
        return false;
    }
    if (capacity <= message->capacity) {
        return true;
    }
    size_t grown =
        message->capacity > 0U ? message->capacity : EIDOLON_RELAY_MESSAGE_INITIAL_CAPACITY;
    while (grown < capacity) {
        const size_t increment = grown < EIDOLON_RELAY_MESSAGE_RETAIN_LIMIT
                                     ? grown
                                     : SDL_min(grown / 2U, (size_t)(16U * 1024U * 1024U));
        if (increment > EIDOLON_RELAY_MESSAGE_LIMIT - grown) {
            grown = EIDOLON_RELAY_MESSAGE_LIMIT;
            break;
        }
        grown += increment;
    }
    uint8_t *data = SDL_realloc(message->data, grown);
    if (data == NULL) {
        return false;
    }
    message->data = data;
    message->capacity = grown;
    return true;
}

void eidolon_relay_message_free(EidolonRelayMessage *message) {
    if (message == NULL) {
        return;
    }
    SDL_free(message->data);
    memset(message, 0, sizeof(*message));
}

static void stop_bridge(RelayBridge *bridge) {
    if (!SDL_CompareAndSwapAtomicInt(&bridge->stopping, 0, 1)) {
        return;
    }
    bridge->left->interrupt(bridge->left->context);
    bridge->right->interrupt(bridge->right->context);
}

static int SDLCALL pump_direction(void *userdata) {
    RelayDirection *direction = userdata;
    EidolonRelayMessage message = {0};
    while (SDL_GetAtomicInt(&direction->bridge->stopping) == 0) {
        message.length = 0U;
        if (!direction->source->read(direction->source->context, &message)) {
            break;
        }
        if (direction->observation.tap != NULL) {
            direction->observation.tap(direction->observation.context, message.data,
                                       message.length);
        }
        if (!direction->destination->write(direction->destination->context, message.data,
                                           message.length)) {
            break;
        }
        if (message.capacity > EIDOLON_RELAY_MESSAGE_RETAIN_LIMIT) {
            eidolon_relay_message_free(&message);
        }
    }
    direction->clean = SDL_GetAtomicInt(&direction->bridge->stopping) != 0;
    eidolon_relay_message_free(&message);
    stop_bridge(direction->bridge);
    return 0;
}

bool eidolon_relay_bridge_run(const EidolonRelayEndpoint *left, const EidolonRelayEndpoint *right,
                              EidolonRelayObservation left_to_right,
                              EidolonRelayObservation right_to_left) {
    if (left == NULL || right == NULL || left->read == NULL || left->write == NULL ||
        left->interrupt == NULL || right->read == NULL || right->write == NULL ||
        right->interrupt == NULL) {
        return false;
    }
    RelayBridge bridge = {.left = left, .right = right};
    RelayDirection outbound = {
        .bridge = &bridge,
        .source = left,
        .destination = right,
        .observation = left_to_right,
    };
    RelayDirection inbound = {
        .bridge = &bridge,
        .source = right,
        .destination = left,
        .observation = right_to_left,
    };
    SDL_Thread *thread = SDL_CreateThread(pump_direction, "eidolon-relay-out", &outbound);
    if (thread == NULL) {
        stop_bridge(&bridge);
        return false;
    }
    (void)pump_direction(&inbound);
    SDL_WaitThread(thread, NULL);
    return outbound.clean && inbound.clean;
}
