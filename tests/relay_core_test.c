#include "relay_core.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <string.h>

typedef struct FakeBridge {
    SDL_AtomicInt writes;
    SDL_AtomicInt interruptions;
} FakeBridge;

typedef struct FakeEndpoint {
    FakeBridge *bridge;
    const char *input;
    char output[64];
    bool read;
} FakeEndpoint;

typedef struct FakeTap {
    char message[64];
} FakeTap;

static bool fake_read(void *context, uint8_t *message, size_t capacity, size_t *length) {
    FakeEndpoint *endpoint = context;
    if (!endpoint->read) {
        endpoint->read = true;
        *length = strlen(endpoint->input);
        assert(*length <= capacity);
        memcpy(message, endpoint->input, *length);
        return true;
    }
    while (SDL_GetAtomicInt(&endpoint->bridge->writes) < 2 &&
           SDL_GetAtomicInt(&endpoint->bridge->interruptions) == 0) {
        SDL_Delay(1U);
    }
    return false;
}

static bool fake_write(void *context, const uint8_t *message, size_t length) {
    FakeEndpoint *endpoint = context;
    assert(length + 1U <= sizeof(endpoint->output));
    memcpy(endpoint->output, message, length);
    endpoint->output[length] = '\0';
    (void)SDL_AddAtomicInt(&endpoint->bridge->writes, 1);
    return true;
}

static void fake_interrupt(void *context) {
    FakeEndpoint *endpoint = context;
    (void)SDL_AddAtomicInt(&endpoint->bridge->interruptions, 1);
}

static void fake_tap(void *context, const uint8_t *message, size_t length) {
    FakeTap *tap = context;
    assert(length + 1U <= sizeof(tap->message));
    memcpy(tap->message, message, length);
    tap->message[length] = '\0';
}

int main(void) {
    assert(SDL_Init(0));
    FakeBridge bridge = {0};
    FakeEndpoint left = {.bridge = &bridge, .input = "client request"};
    FakeEndpoint right = {.bridge = &bridge, .input = "server response"};
    FakeTap observed = {0};
    const EidolonRelayEndpoint left_endpoint = {
        .context = &left,
        .read = fake_read,
        .write = fake_write,
        .interrupt = fake_interrupt,
    };
    const EidolonRelayEndpoint right_endpoint = {
        .context = &right,
        .read = fake_read,
        .write = fake_write,
        .interrupt = fake_interrupt,
    };
    (void)eidolon_relay_bridge_run(
        &left_endpoint, &right_endpoint, (EidolonRelayObservation){0},
        (EidolonRelayObservation){.context = &observed, .tap = fake_tap});
    assert(strcmp(right.output, "client request") == 0);
    assert(strcmp(left.output, "server response") == 0);
    assert(strcmp(observed.message, "server response") == 0);
    assert(SDL_GetAtomicInt(&bridge.interruptions) == 2);
    SDL_Quit();
    return 0;
}
