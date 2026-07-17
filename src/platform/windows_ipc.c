#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <SDL3/SDL.h>

#include "platform/ipc.h"
#include "platform/ipc_protocol.h"

#define EIDOLON_PIPE_NAME L"\\\\.\\pipe\\eidolon-state-v1"

static HANDLE create_pipe(void) {
    return CreateNamedPipeW(EIDOLON_PIPE_NAME, PIPE_ACCESS_INBOUND,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT, 1,
                            EIDOLON_IPC_MAX_MESSAGE_SIZE, EIDOLON_IPC_MAX_MESSAGE_SIZE, 0, NULL);
}

bool eidolon_ipc_server_init(EidolonIpcServer *server) {
    server->pipe = create_pipe();
    server->connected = false;
    server->initialized = false;
    if (server->pipe == INVALID_HANDLE_VALUE) {
        server->pipe = NULL;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create Eidolon state pipe: %lu",
                     GetLastError());
        return false;
    }
    server->initialized = true;
    return true;
}

bool eidolon_ipc_server_poll(EidolonIpcServer *server, EidolonState *state, char *text,
                             size_t text_capacity) {
    HANDLE pipe = (HANDLE)server->pipe;
    if (pipe == NULL) {
        return false;
    }

    if (!server->connected) {
        if (ConnectNamedPipe(pipe, NULL)) {
            server->connected = true;
        } else {
            const DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                server->connected = true;
            } else if (error == ERROR_PIPE_LISTENING) {
                return false;
            } else if (error == ERROR_NO_DATA) {
                /* The short-lived hook may already have closed after writing. The message is
                   still readable until we disconnect this pipe instance. */
                server->connected = true;
            } else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Eidolon state pipe failed: %lu", error);
                return false;
            }
        }
    }

    uint8_t message[EIDOLON_IPC_MAX_MESSAGE_SIZE];
    DWORD bytes_read = 0;
    const BOOL read = ReadFile(pipe, message, sizeof(message), &bytes_read, NULL);
    if (!read) {
        DisconnectNamedPipe(pipe);
        server->connected = false;
        return false;
    }

    DisconnectNamedPipe(pipe);
    server->connected = false;
    return eidolon_ipc_decode(message, (size_t)bytes_read, state, text, text_capacity);
}

void eidolon_ipc_server_destroy(EidolonIpcServer *server) {
    if (server->initialized && server->pipe != NULL) {
        DisconnectNamedPipe((HANDLE)server->pipe);
        CloseHandle((HANDLE)server->pipe);
        server->pipe = NULL;
    }
    server->connected = false;
    server->initialized = false;
}

bool eidolon_ipc_send(EidolonState state, const char *text) {
    HANDLE pipe = CreateFileW(EIDOLON_PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY &&
        WaitNamedPipeW(EIDOLON_PIPE_NAME, 100)) {
        pipe = CreateFileW(EIDOLON_PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    uint8_t message[EIDOLON_IPC_MAX_MESSAGE_SIZE];
    const size_t message_size = eidolon_ipc_encode(message, state, text);
    DWORD bytes_written = 0;
    const BOOL written = WriteFile(pipe, message, (DWORD)message_size, &bytes_written, NULL);
    CloseHandle(pipe);
    return written && bytes_written == (DWORD)message_size;
}
