#ifndef EIDOLON_RELAY_CORE_H
#define EIDOLON_RELAY_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_RELAY_MESSAGE_CAPACITY (1024U * 1024U)

typedef bool (*EidolonRelayRead)(void *context, uint8_t *message, size_t capacity, size_t *length);
typedef bool (*EidolonRelayWrite)(void *context, const uint8_t *message, size_t length);
typedef void (*EidolonRelayInterrupt)(void *context);
typedef void (*EidolonRelayTap)(void *context, const uint8_t *message, size_t length);

typedef struct EidolonRelayEndpoint {
    void *context;
    EidolonRelayRead read;
    EidolonRelayWrite write;
    EidolonRelayInterrupt interrupt;
} EidolonRelayEndpoint;

typedef struct EidolonRelayObservation {
    void *context;
    EidolonRelayTap tap;
} EidolonRelayObservation;

bool eidolon_relay_bridge_run(const EidolonRelayEndpoint *left, const EidolonRelayEndpoint *right,
                              EidolonRelayObservation left_to_right,
                              EidolonRelayObservation right_to_left);

#endif
