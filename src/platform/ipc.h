#ifndef EIDOLON_PLATFORM_IPC_H
#define EIDOLON_PLATFORM_IPC_H

#include "state.h"

#include <stdbool.h>
#include <stddef.h>

#define EIDOLON_IPC_TEXT_CAPACITY 4096

#ifdef _WIN32
typedef struct EidolonIpcServer {
    void *pipe;
    bool connected;
    bool initialized;
} EidolonIpcServer;
#else
typedef struct EidolonIpcServer {
    int socket_fd;
    char path[108];
    bool initialized;
} EidolonIpcServer;
#endif

bool eidolon_ipc_server_init(EidolonIpcServer *server);
bool eidolon_ipc_server_poll(EidolonIpcServer *server, EidolonState *state, char *text,
                             size_t text_capacity);
void eidolon_ipc_server_destroy(EidolonIpcServer *server);

/* Best-effort by design: hooks must never make a Codex turn fail. */
bool eidolon_ipc_send(EidolonState state, const char *text);

#endif
