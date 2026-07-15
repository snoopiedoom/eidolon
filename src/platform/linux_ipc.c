#include "platform/ipc.h"
#include "platform/ipc_protocol.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static bool socket_path(char *path, size_t capacity) {
    const int length = snprintf(path, capacity, "/tmp/eidolon-%lu.sock", (unsigned long)getuid());
    return length > 0 && (size_t)length < capacity;
}

static bool set_nonblocking(int socket_fd) {
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    return flags >= 0 && fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool eidolon_ipc_server_init(EidolonIpcServer *server) {
    server->socket_fd = -1;
    server->path[0] = '\0';
    server->initialized = false;
    if (!socket_path(server->path, sizeof(server->path))) {
        return false;
    }

    const int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0 || !set_nonblocking(socket_fd)) {
        if (socket_fd >= 0) {
            close(socket_fd);
        }
        return false;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    SDL_strlcpy(address.sun_path, server->path, sizeof(address.sun_path));

    if (bind(socket_fd, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        const int bind_error = errno;
        uint8_t probe = 0;
        const ssize_t probe_result =
            sendto(socket_fd, &probe, sizeof(probe), 0, (const struct sockaddr *)&address,
                   sizeof(address));
        if (bind_error != EADDRINUSE || probe_result >= 0 || errno != ECONNREFUSED ||
            unlink(server->path) != 0 ||
            bind(socket_fd, (const struct sockaddr *)&address, sizeof(address)) != 0) {
            close(socket_fd);
            return false;
        }
    }
    if (chmod(server->path, S_IRUSR | S_IWUSR) != 0) {
        close(socket_fd);
        unlink(server->path);
        return false;
    }

    server->socket_fd = socket_fd;
    server->initialized = true;
    return true;
}

bool eidolon_ipc_server_poll(EidolonIpcServer *server, EidolonState *state, char *text,
                             size_t text_capacity) {
    uint8_t message[EIDOLON_IPC_MAX_MESSAGE_SIZE];
    const ssize_t bytes_read = recv(server->socket_fd, message, sizeof(message), 0);
    if (bytes_read < 0) {
        return false;
    }
    return eidolon_ipc_decode(message, (size_t)bytes_read, state, text, text_capacity);
}

void eidolon_ipc_server_destroy(EidolonIpcServer *server) {
    if (server->initialized) {
        if (server->socket_fd >= 0) {
            close(server->socket_fd);
            server->socket_fd = -1;
        }
        if (server->path[0] != '\0') {
            unlink(server->path);
        }
    }
    server->path[0] = '\0';
    server->initialized = false;
}

bool eidolon_ipc_send(EidolonState state, const char *text) {
    char path[108];
    if (!socket_path(path, sizeof(path))) {
        return false;
    }

    const int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        return false;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    SDL_strlcpy(address.sun_path, path, sizeof(address.sun_path));

    uint8_t message[EIDOLON_IPC_MAX_MESSAGE_SIZE];
    const size_t message_size = eidolon_ipc_encode(message, state, text);
    const ssize_t bytes_written =
        sendto(socket_fd, message, message_size, 0, (const struct sockaddr *)&address,
               sizeof(address));
    close(socket_fd);
    return bytes_written == (ssize_t)message_size;
}
