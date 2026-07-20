#include "relay_core.h"

#include <SDL3/SDL.h>

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

static void stop_bridge(RelayBridge *bridge) {
    if (!SDL_CompareAndSwapAtomicInt(&bridge->stopping, 0, 1)) {
        return;
    }
    bridge->left->interrupt(bridge->left->context);
    bridge->right->interrupt(bridge->right->context);
}

static int SDLCALL pump_direction(void *userdata) {
    RelayDirection *direction = userdata;
    uint8_t *message = SDL_malloc(EIDOLON_RELAY_MESSAGE_CAPACITY);
    if (message == NULL) {
        stop_bridge(direction->bridge);
        return 0;
    }
    while (SDL_GetAtomicInt(&direction->bridge->stopping) == 0) {
        size_t length = 0U;
        if (!direction->source->read(direction->source->context, message,
                                     EIDOLON_RELAY_MESSAGE_CAPACITY, &length)) {
            break;
        }
        if (direction->observation.tap != NULL) {
            direction->observation.tap(direction->observation.context, message, length);
        }
        if (!direction->destination->write(direction->destination->context, message, length)) {
            break;
        }
    }
    direction->clean = SDL_GetAtomicInt(&direction->bridge->stopping) != 0;
    SDL_free(message);
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
