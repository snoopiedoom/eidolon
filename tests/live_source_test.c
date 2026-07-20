#include "conversation.h"
#include "providers/live_source.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "codex") != 0 && strcmp(argv[1], "opencode") != 0)) {
        fprintf(stderr, "usage: live_source_test <codex|opencode> <url>\n");
        return 2;
    }
    if (!SDL_Init(0)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    EidolonConversationBus *bus = eidolon_conversation_bus_create();
    EidolonLiveSource *source = strcmp(argv[1], "codex") == 0
                                    ? eidolon_codex_live_source_start(argv[2], bus)
                                    : eidolon_opencode_live_source_start(argv[2], bus);
    if (bus == NULL || source == NULL) {
        fprintf(stderr, "could not start %s source\n", argv[1]);
        eidolon_live_source_stop(source);
        eidolon_conversation_bus_destroy(bus);
        SDL_Quit();
        return 1;
    }
    bool connected = false;
    const uint64_t deadline = SDL_GetTicks() + 15000U;
    while (!connected && SDL_GetTicks() < deadline) {
        EidolonConversationEvent event;
        while (eidolon_conversation_bus_poll(bus, &event)) {
            if (event.type == EIDOLON_CONVERSATION_SOURCE_CONNECTED &&
                strcmp(event.provider, argv[1]) == 0) {
                connected = true;
            }
        }
        if (!connected) {
            SDL_Delay(10U);
        }
    }
    eidolon_live_source_stop(source);
    eidolon_conversation_bus_destroy(bus);
    SDL_Quit();
    if (!connected) {
        fprintf(stderr, "%s source did not connect\n", argv[1]);
        return 1;
    }
    printf("%s source connected\n", argv[1]);
    return 0;
}
