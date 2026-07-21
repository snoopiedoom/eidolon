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
    const uint8_t *input;
    size_t input_length;
    EidolonRelayMessage output;
    bool read;
} FakeEndpoint;

typedef struct FakeTap {
    EidolonRelayMessage message;
} FakeTap;

static bool fake_read(void *context, EidolonRelayMessage *message) {
    FakeEndpoint *endpoint = context;
    if (!endpoint->read) {
        endpoint->read = true;
        assert(eidolon_relay_message_reserve(message, endpoint->input_length));
        memcpy(message->data, endpoint->input, endpoint->input_length);
        message->length = endpoint->input_length;
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
    assert(eidolon_relay_message_reserve(&endpoint->output, length));
    memcpy(endpoint->output.data, message, length);
    endpoint->output.length = length;
    (void)SDL_AddAtomicInt(&endpoint->bridge->writes, 1);
    return true;
}

static void fake_interrupt(void *context) {
    FakeEndpoint *endpoint = context;
    (void)SDL_AddAtomicInt(&endpoint->bridge->interruptions, 1);
}

static void fake_tap(void *context, const uint8_t *message, size_t length) {
    FakeTap *tap = context;
    assert(eidolon_relay_message_reserve(&tap->message, length));
    memcpy(tap->message.data, message, length);
    tap->message.length = length;
}

int main(void) {
    assert(SDL_Init(0));
    static const char request[] = "client request";
    const size_t response_length = 2U * 1024U * 1024U;
    uint8_t *response = SDL_malloc(response_length);
    assert(response != NULL);
    for (size_t index = 0U; index < response_length; ++index) {
        response[index] = (uint8_t)(index % 251U);
    }
    FakeBridge bridge = {0};
    FakeEndpoint left = {
        .bridge = &bridge,
        .input = (const uint8_t *)request,
        .input_length = sizeof(request) - 1U,
    };
    FakeEndpoint right = {
        .bridge = &bridge,
        .input = response,
        .input_length = response_length,
    };
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
    assert(right.output.length == sizeof(request) - 1U);
    assert(memcmp(right.output.data, request, right.output.length) == 0);
    assert(left.output.length == response_length);
    assert(memcmp(left.output.data, response, response_length) == 0);
    assert(observed.message.length == response_length);
    assert(memcmp(observed.message.data, response, response_length) == 0);
    assert(SDL_GetAtomicInt(&bridge.interruptions) == 2);
    eidolon_relay_message_free(&left.output);
    eidolon_relay_message_free(&right.output);
    eidolon_relay_message_free(&observed.message);
    SDL_free(response);
    SDL_Quit();
    return 0;
}
