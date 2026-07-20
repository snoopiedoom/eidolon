#include "conversation.h"
#include "providers/codex_relay.h"
#include "providers/live_source.h"

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool poll_connected(EidolonConversationBus *bus) {
    EidolonConversationEvent event;
    while (eidolon_conversation_bus_poll(bus, &event)) {
        if (event.type == EIDOLON_CONVERSATION_SOURCE_CONNECTED &&
            strcmp(event.provider, "codex") == 0) {
            return true;
        }
    }
    return false;
}

int main(void) {
    static const char relay_url[] = "ws://127.0.0.1:4520";
    if (!SDL_Init(0)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    EidolonConversationBus *relay_bus = eidolon_conversation_bus_create();
    EidolonConversationBus *client_bus = eidolon_conversation_bus_create();
    EidolonCodexRelay *relay = NULL;
    EidolonLiveSource *client = NULL;
    bool relay_connected = false;
    bool client_connected = false;
    int result = 1;
    if (relay_bus == NULL || client_bus == NULL) {
        fprintf(stderr, "could not create conversation buses\n");
        goto cleanup;
    }
    relay = eidolon_codex_relay_start(relay_url, "codex", relay_bus);
    client = eidolon_codex_live_source_start(relay_url, client_bus);
    if (relay == NULL || client == NULL) {
        fprintf(stderr, "could not start relay integration probe\n");
        goto cleanup;
    }
    const uint64_t deadline = SDL_GetTicks() + 15000U;
    while ((!relay_connected || !client_connected) && SDL_GetTicks() < deadline) {
        relay_connected = relay_connected || poll_connected(relay_bus);
        client_connected = client_connected || poll_connected(client_bus);
        if (!relay_connected || !client_connected) {
            SDL_Delay(10U);
        }
    }
    if (!relay_connected || !client_connected) {
        fprintf(stderr, "relay handshake timed out relay=%s client=%s\n",
                relay_connected ? "connected" : "missing",
                client_connected ? "connected" : "missing");
        goto cleanup;
    }
    puts("codex relay forwarded a complete app-server handshake");
    result = 0;

cleanup:
    eidolon_live_source_stop(client);
    eidolon_codex_relay_stop(relay);
    eidolon_conversation_bus_destroy(client_bus);
    eidolon_conversation_bus_destroy(relay_bus);
    SDL_Quit();
    return result;
}
