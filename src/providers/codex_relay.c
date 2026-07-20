#include "providers/codex_relay.h"

#include "log.h"
#include "providers/codex_stream.h"
#include "relay_core.h"

#include <SDL3/SDL.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <bcrypt.h>

#define CODEX_RELAY_URL_CAPACITY 512U
#define CODEX_RELAY_EXECUTABLE_CAPACITY 512U
#define CODEX_RELAY_HTTP_CAPACITY 8192U
#define CODEX_RELAY_PIPE_BUFFER_CAPACITY 8192U

struct EidolonCodexRelay {
    EidolonConversationBus *bus;
    SDL_Thread *thread;
    SDL_Mutex *mutex;
    SDL_AtomicInt stopping;
    SOCKET listener;
    SOCKET client;
    HANDLE child_process;
    char listen_url[CODEX_RELAY_URL_CAPACITY];
    char executable[CODEX_RELAY_EXECUTABLE_CAPACITY];
};

typedef struct WebSocketEndpoint {
    EidolonCodexRelay *relay;
    SDL_Mutex *write_mutex;
} WebSocketEndpoint;

typedef struct StdioEndpoint {
    EidolonCodexRelay *relay;
    HANDLE read_handle;
    HANDLE write_handle;
    SDL_Mutex *write_mutex;
    uint8_t buffered[CODEX_RELAY_PIPE_BUFFER_CAPACITY];
    size_t buffered_start;
    size_t buffered_length;
} StdioEndpoint;

typedef struct ChildProcess {
    HANDLE process;
    HANDLE read_handle;
    HANDLE write_handle;
} ChildProcess;

static bool relay_stopping(const EidolonCodexRelay *relay) {
    return SDL_GetAtomicInt((SDL_AtomicInt *)&relay->stopping) != 0;
}

static SOCKET relay_client(EidolonCodexRelay *relay) {
    SDL_LockMutex(relay->mutex);
    const SOCKET client = relay->client;
    SDL_UnlockMutex(relay->mutex);
    return client;
}

static void close_relay_client(EidolonCodexRelay *relay) {
    SDL_LockMutex(relay->mutex);
    const SOCKET client = relay->client;
    relay->client = INVALID_SOCKET;
    SDL_UnlockMutex(relay->mutex);
    if (client != INVALID_SOCKET) {
        (void)shutdown(client, SD_BOTH);
        closesocket(client);
    }
}

static bool release_relay_listener(EidolonCodexRelay *relay, SOCKET listener) {
    bool owned = false;
    SDL_LockMutex(relay->mutex);
    if (relay->listener == listener) {
        relay->listener = INVALID_SOCKET;
        owned = true;
    }
    SDL_UnlockMutex(relay->mutex);
    return owned;
}

static void terminate_relay_child(EidolonCodexRelay *relay) {
    SDL_LockMutex(relay->mutex);
    const HANDLE process = relay->child_process;
    SDL_UnlockMutex(relay->mutex);
    if (process != NULL && WaitForSingleObject(process, 0U) == WAIT_TIMEOUT) {
        (void)TerminateProcess(process, 1U);
    }
}

static bool socket_send_all(SOCKET socket_handle, const uint8_t *data, size_t length) {
    size_t sent = 0U;
    while (sent < length) {
        const size_t remaining = length - sent;
        const int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
        const int result = send(socket_handle, (const char *)data + sent, chunk, 0);
        if (result <= 0) {
            return false;
        }
        sent += (size_t)result;
    }
    return true;
}

static bool socket_receive_exact(SOCKET socket_handle, uint8_t *data, size_t length) {
    size_t received = 0U;
    while (received < length) {
        const size_t remaining = length - received;
        const int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
        const int result = recv(socket_handle, (char *)data + received, chunk, 0);
        if (result <= 0) {
            return false;
        }
        received += (size_t)result;
    }
    return true;
}

static bool websocket_send_frame(WebSocketEndpoint *endpoint, uint8_t opcode,
                                 const uint8_t *message, size_t length) {
    uint8_t header[10];
    size_t header_length = 0U;
    header[header_length++] = (uint8_t)(UINT8_C(0x80) | opcode);
    if (length <= 125U) {
        header[header_length++] = (uint8_t)length;
    } else if (length <= UINT16_MAX) {
        header[header_length++] = 126U;
        header[header_length++] = (uint8_t)(length >> 8U);
        header[header_length++] = (uint8_t)length;
    } else {
        header[header_length++] = 127U;
        const uint64_t wide_length = (uint64_t)length;
        for (int shift = 56; shift >= 0; shift -= 8) {
            header[header_length++] = (uint8_t)(wide_length >> (unsigned int)shift);
        }
    }
    SDL_LockMutex(endpoint->write_mutex);
    const SOCKET client = relay_client(endpoint->relay);
    const bool sent = client != INVALID_SOCKET && socket_send_all(client, header, header_length) &&
                      socket_send_all(client, message, length);
    SDL_UnlockMutex(endpoint->write_mutex);
    return sent;
}

static bool websocket_read(void *context, uint8_t *message, size_t capacity, size_t *length) {
    WebSocketEndpoint *endpoint = context;
    size_t used = 0U;
    bool fragmented = false;
    for (;;) {
        const SOCKET client = relay_client(endpoint->relay);
        uint8_t header[2];
        if (client == INVALID_SOCKET || !socket_receive_exact(client, header, sizeof(header))) {
            return false;
        }
        const bool final = (header[0] & UINT8_C(0x80)) != 0U;
        const bool masked = (header[1] & UINT8_C(0x80)) != 0U;
        const uint8_t opcode = header[0] & UINT8_C(0x0F);
        uint64_t payload_length = header[1] & UINT8_C(0x7F);
        if ((header[0] & UINT8_C(0x70)) != 0U || !masked) {
            return false;
        }
        if (payload_length == 126U) {
            uint8_t extended[2];
            if (!socket_receive_exact(client, extended, sizeof(extended))) {
                return false;
            }
            payload_length = ((uint64_t)extended[0] << 8U) | extended[1];
        } else if (payload_length == 127U) {
            uint8_t extended[8];
            if (!socket_receive_exact(client, extended, sizeof(extended))) {
                return false;
            }
            payload_length = 0U;
            for (size_t index = 0U; index < sizeof(extended); ++index) {
                payload_length = (payload_length << 8U) | extended[index];
            }
        }
        uint8_t mask[4];
        if (!socket_receive_exact(client, mask, sizeof(mask))) {
            return false;
        }
        if (opcode >= 8U) {
            if (!final || payload_length > 125U) {
                return false;
            }
            uint8_t control[125];
            if (!socket_receive_exact(client, control, (size_t)payload_length)) {
                return false;
            }
            for (size_t index = 0U; index < (size_t)payload_length; ++index) {
                control[index] ^= mask[index % 4U];
            }
            if (opcode == 8U) {
                (void)websocket_send_frame(endpoint, 8U, control, (size_t)payload_length);
                return false;
            }
            if (opcode == 9U &&
                !websocket_send_frame(endpoint, 10U, control, (size_t)payload_length)) {
                return false;
            }
            continue;
        }
        if ((opcode == 0U && !fragmented) || (opcode != 0U && opcode != 1U) ||
            (opcode == 1U && fragmented) || payload_length > (uint64_t)(capacity - used)) {
            return false;
        }
        if (!socket_receive_exact(client, message + used, (size_t)payload_length)) {
            return false;
        }
        for (size_t index = 0U; index < (size_t)payload_length; ++index) {
            message[used + index] ^= mask[index % 4U];
        }
        used += (size_t)payload_length;
        if (final) {
            *length = used;
            return true;
        }
        fragmented = true;
    }
}

static bool websocket_write(void *context, const uint8_t *message, size_t length) {
    return websocket_send_frame(context, 1U, message, length);
}

static void websocket_interrupt(void *context) {
    WebSocketEndpoint *endpoint = context;
    close_relay_client(endpoint->relay);
}

static bool write_handle_all(HANDLE handle, const uint8_t *data, size_t length) {
    size_t written = 0U;
    while (written < length) {
        const size_t remaining = length - written;
        const DWORD chunk = remaining > UINT32_MAX ? UINT32_MAX : (DWORD)remaining;
        DWORD result = 0U;
        if (!WriteFile(handle, data + written, chunk, &result, NULL) || result == 0U) {
            return false;
        }
        written += result;
    }
    return true;
}

static bool stdio_read(void *context, uint8_t *message, size_t capacity, size_t *length) {
    StdioEndpoint *endpoint = context;
    size_t used = 0U;
    for (;;) {
        for (size_t index = endpoint->buffered_start; index < endpoint->buffered_length; ++index) {
            if (endpoint->buffered[index] != '\n') {
                continue;
            }
            const size_t available = index - endpoint->buffered_start;
            if (used + available > capacity) {
                return false;
            }
            memcpy(message + used, endpoint->buffered + endpoint->buffered_start, available);
            used += available;
            endpoint->buffered_start = index + 1U;
            if (used > 0U && message[used - 1U] == '\r') {
                --used;
            }
            *length = used;
            return true;
        }
        const size_t available = endpoint->buffered_length - endpoint->buffered_start;
        if (used + available > capacity) {
            return false;
        }
        if (available > 0U) {
            memcpy(message + used, endpoint->buffered + endpoint->buffered_start, available);
            used += available;
        }
        endpoint->buffered_start = 0U;
        endpoint->buffered_length = 0U;
        DWORD received = 0U;
        if (!ReadFile(endpoint->read_handle, endpoint->buffered, (DWORD)sizeof(endpoint->buffered),
                      &received, NULL) ||
            received == 0U) {
            return false;
        }
        endpoint->buffered_length = received;
    }
}

static bool stdio_write(void *context, const uint8_t *message, size_t length) {
    StdioEndpoint *endpoint = context;
    static const uint8_t newline = '\n';
    SDL_LockMutex(endpoint->write_mutex);
    const bool written = write_handle_all(endpoint->write_handle, message, length) &&
                         write_handle_all(endpoint->write_handle, &newline, 1U);
    SDL_UnlockMutex(endpoint->write_mutex);
    return written;
}

static void stdio_interrupt(void *context) {
    StdioEndpoint *endpoint = context;
    terminate_relay_child(endpoint->relay);
}

static bool sha1_digest(const uint8_t *data, size_t length, uint8_t output[20]) {
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    const NTSTATUS opened =
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, NULL, 0U);
    NTSTATUS status = opened;
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptCreateHash(algorithm, &hash, NULL, 0U, NULL, 0U, 0U);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptHashData(hash, (PUCHAR)data, (ULONG)length, 0U);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hash, output, 20U, 0U);
    }
    if (hash != NULL) {
        BCryptDestroyHash(hash);
    }
    if (algorithm != NULL) {
        BCryptCloseAlgorithmProvider(algorithm, 0U);
    }
    return BCRYPT_SUCCESS(status);
}

static bool base64_encode(const uint8_t *input, size_t length, char *output, size_t capacity) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t required = ((length + 2U) / 3U) * 4U;
    if (required + 1U > capacity) {
        return false;
    }
    size_t source = 0U;
    size_t destination = 0U;
    while (source < length) {
        const size_t remaining = length - source;
        const uint32_t first = input[source++];
        const uint32_t second = remaining > 1U ? input[source++] : 0U;
        const uint32_t third = remaining > 2U ? input[source++] : 0U;
        const uint32_t value = (first << 16U) | (second << 8U) | third;
        output[destination++] = alphabet[(value >> 18U) & UINT32_C(0x3F)];
        output[destination++] = alphabet[(value >> 12U) & UINT32_C(0x3F)];
        output[destination++] = remaining > 1U ? alphabet[(value >> 6U) & UINT32_C(0x3F)] : '=';
        output[destination++] = remaining > 2U ? alphabet[value & UINT32_C(0x3F)] : '=';
    }
    output[destination] = '\0';
    return true;
}

static bool extract_websocket_key(char *request, char *key, size_t capacity) {
    char *line = request;
    while (line != NULL && *line != '\0') {
        char *next = strstr(line, "\r\n");
        if (next != NULL) {
            *next = '\0';
            next += 2;
        }
        static const char name[] = "Sec-WebSocket-Key:";
        if (_strnicmp(line, name, sizeof(name) - 1U) == 0) {
            const char *value = line + sizeof(name) - 1U;
            while (*value == ' ' || *value == '\t') {
                ++value;
            }
            if (*value == '\0' || strlen(value) >= capacity) {
                return false;
            }
            SDL_strlcpy(key, value, capacity);
            return true;
        }
        line = next;
    }
    return false;
}

static bool websocket_handshake(SOCKET client) {
    char request[CODEX_RELAY_HTTP_CAPACITY];
    size_t used = 0U;
    while (used + 1U < sizeof(request)) {
        const int result = recv(client, request + used, (int)(sizeof(request) - 1U - used), 0);
        if (result <= 0) {
            return false;
        }
        used += (size_t)result;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n") != NULL) {
            break;
        }
    }
    if (strncmp(request, "GET ", 4U) != 0 || strstr(request, "\r\n\r\n") == NULL) {
        return false;
    }
    char key[128];
    if (!extract_websocket_key(request, key, sizeof(key))) {
        return false;
    }
    static const char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char challenge[sizeof(key) + sizeof(guid)];
    const int challenge_length = SDL_snprintf(challenge, sizeof(challenge), "%s%s", key, guid);
    uint8_t digest[20];
    char accept[64];
    if (challenge_length <= 0 || (size_t)challenge_length >= sizeof(challenge) ||
        !sha1_digest((const uint8_t *)challenge, (size_t)challenge_length, digest) ||
        !base64_encode(digest, sizeof(digest), accept, sizeof(accept))) {
        return false;
    }
    char response[512];
    const int response_length = SDL_snprintf(response, sizeof(response),
                                             "HTTP/1.1 101 Switching Protocols\r\n"
                                             "Upgrade: websocket\r\n"
                                             "Connection: Upgrade\r\n"
                                             "Sec-WebSocket-Accept: %s\r\n\r\n",
                                             accept);
    return response_length > 0 && (size_t)response_length < sizeof(response) &&
           socket_send_all(client, (const uint8_t *)response, (size_t)response_length);
}

static bool parse_listen_port(const char *url, uint16_t *port) {
    static const char prefix[] = "ws://";
    if (url == NULL || strncmp(url, prefix, sizeof(prefix) - 1U) != 0) {
        return false;
    }
    const char *authority = url + sizeof(prefix) - 1U;
    const char *separator = strrchr(authority, ':');
    if (separator == NULL) {
        return false;
    }
    const size_t host_length = (size_t)(separator - authority);
    if (!((host_length == strlen("127.0.0.1") &&
           strncmp(authority, "127.0.0.1", host_length) == 0) ||
          (host_length == strlen("localhost") &&
           strncmp(authority, "localhost", host_length) == 0))) {
        return false;
    }
    char *end = NULL;
    const unsigned long parsed = strtoul(separator + 1, &end, 10);
    if (end == separator + 1 || (*end != '\0' && strcmp(end, "/") != 0) || parsed == 0UL ||
        parsed > UINT16_MAX) {
        return false;
    }
    *port = (uint16_t)parsed;
    return true;
}

static SOCKET create_listener(uint16_t port) {
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    const BOOL reuse = TRUE;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(listener, 1) == SOCKET_ERROR) {
        closesocket(listener);
        return INVALID_SOCKET;
    }
    return listener;
}

static bool wide_from_utf8(const char *input, wchar_t *output, size_t capacity) {
    return input != NULL && output != NULL && capacity > 0U && capacity <= (size_t)INT_MAX &&
           MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, (int)capacity) > 0;
}

static bool resolve_executable(const char *configured, wchar_t *resolved, size_t capacity) {
    wchar_t input[CODEX_RELAY_EXECUTABLE_CAPACITY];
    if (!wide_from_utf8(configured, input, SDL_arraysize(input))) {
        return false;
    }
    const DWORD result = SearchPathW(NULL, input, L".exe", (DWORD)capacity, resolved, NULL);
    return result > 0U && (size_t)result < capacity;
}

static bool spawn_codex(EidolonCodexRelay *relay, ChildProcess *child) {
    SECURITY_ATTRIBUTES security = {
        .nLength = sizeof(security),
        .bInheritHandle = TRUE,
    };
    HANDLE stdout_read = NULL;
    HANDLE stdout_write = NULL;
    HANDLE stdin_read = NULL;
    HANDLE stdin_write = NULL;
    HANDLE null_output = INVALID_HANDLE_VALUE;
    wchar_t executable[CODEX_RELAY_EXECUTABLE_CAPACITY];
    wchar_t command_line[CODEX_RELAY_EXECUTABLE_CAPACITY + 64U];
    PROCESS_INFORMATION process = {0};
    bool created = false;
    if (!resolve_executable(relay->executable, executable, SDL_arraysize(executable)) ||
        !CreatePipe(&stdout_read, &stdout_write, &security, 0U) ||
        !SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0U) ||
        !CreatePipe(&stdin_read, &stdin_write, &security, 0U) ||
        !SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0U)) {
        goto cleanup;
    }
    null_output = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (null_output == INVALID_HANDLE_VALUE) {
        goto cleanup;
    }
    STARTUPINFOW startup = {0};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = stdin_read;
    startup.hStdOutput = stdout_write;
    startup.hStdError = null_output;
    const int command_length = swprintf(command_line, SDL_arraysize(command_line),
                                        L"\"%ls\" app-server --listen stdio://", executable);
    if (command_length <= 0 || (size_t)command_length >= SDL_arraysize(command_line)) {
        goto cleanup;
    }
    created = CreateProcessW(executable, command_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                             NULL, &startup, &process) != FALSE;
    if (!created) {
        goto cleanup;
    }
    CloseHandle(process.hThread);
    process.hThread = NULL;
    child->process = process.hProcess;
    child->read_handle = stdout_read;
    child->write_handle = stdin_write;
    stdout_read = NULL;
    stdin_write = NULL;
    SDL_LockMutex(relay->mutex);
    relay->child_process = process.hProcess;
    SDL_UnlockMutex(relay->mutex);

cleanup:
    if (stdout_write != NULL) {
        CloseHandle(stdout_write);
    }
    if (stdin_read != NULL) {
        CloseHandle(stdin_read);
    }
    if (null_output != INVALID_HANDLE_VALUE) {
        CloseHandle(null_output);
    }
    if (stdout_read != NULL) {
        CloseHandle(stdout_read);
    }
    if (stdin_write != NULL) {
        CloseHandle(stdin_write);
    }
    return created;
}

static void destroy_child(EidolonCodexRelay *relay, ChildProcess *child) {
    if (child->process != NULL && WaitForSingleObject(child->process, 1000U) == WAIT_TIMEOUT) {
        (void)TerminateProcess(child->process, 1U);
        (void)WaitForSingleObject(child->process, 1000U);
    }
    SDL_LockMutex(relay->mutex);
    if (relay->child_process == child->process) {
        relay->child_process = NULL;
    }
    SDL_UnlockMutex(relay->mutex);
    if (child->read_handle != NULL) {
        CloseHandle(child->read_handle);
    }
    if (child->write_handle != NULL) {
        CloseHandle(child->write_handle);
    }
    if (child->process != NULL) {
        CloseHandle(child->process);
    }
    SDL_zero(*child);
}

static void emit_source_state(EidolonCodexRelay *relay, bool connected) {
    EidolonConversationEvent event = {0};
    event.type = connected ? EIDOLON_CONVERSATION_SOURCE_CONNECTED
                           : EIDOLON_CONVERSATION_SOURCE_DISCONNECTED;
    SDL_strlcpy(event.provider, "codex", sizeof(event.provider));
    event.source_time_ms = SDL_GetTicks();
    (void)eidolon_conversation_bus_push(relay->bus, &event);
}

static void observe_codex(void *context, const uint8_t *message, size_t length) {
    EidolonCodexRelay *relay = context;
    char *frame = SDL_malloc(length + 1U);
    if (frame == NULL) {
        return;
    }
    memcpy(frame, message, length);
    frame[length] = '\0';
    EidolonConversationEvent event;
    if (eidolon_codex_stream_parse(frame, &event)) {
        event.source_time_ms = SDL_GetTicks();
        if (!eidolon_conversation_bus_push(relay->bus, &event)) {
            eidolon_log_write("provider", "codex relay event queue full; frame rejected");
        }
    }
    SDL_free(frame);
}

static void run_client(EidolonCodexRelay *relay) {
    ChildProcess child = {0};
    if (!spawn_codex(relay, &child)) {
        eidolon_log_write("provider", "codex relay could not launch %s error=%lu",
                          relay->executable, (unsigned long)GetLastError());
        close_relay_client(relay);
        return;
    }
    WebSocketEndpoint websocket = {
        .relay = relay,
        .write_mutex = SDL_CreateMutex(),
    };
    StdioEndpoint stdio = {
        .relay = relay,
        .read_handle = child.read_handle,
        .write_handle = child.write_handle,
        .write_mutex = SDL_CreateMutex(),
    };
    if (websocket.write_mutex == NULL || stdio.write_mutex == NULL) {
        SDL_DestroyMutex(websocket.write_mutex);
        SDL_DestroyMutex(stdio.write_mutex);
        terminate_relay_child(relay);
        destroy_child(relay, &child);
        close_relay_client(relay);
        return;
    }
    const EidolonRelayEndpoint client_endpoint = {
        .context = &websocket,
        .read = websocket_read,
        .write = websocket_write,
        .interrupt = websocket_interrupt,
    };
    const EidolonRelayEndpoint server_endpoint = {
        .context = &stdio,
        .read = stdio_read,
        .write = stdio_write,
        .interrupt = stdio_interrupt,
    };
    emit_source_state(relay, true);
    eidolon_log_write("provider", "codex relay client attached");
    (void)eidolon_relay_bridge_run(
        &client_endpoint, &server_endpoint, (EidolonRelayObservation){0},
        (EidolonRelayObservation){.context = relay, .tap = observe_codex});
    if (!relay_stopping(relay)) {
        emit_source_state(relay, false);
        eidolon_log_write("provider", "codex relay client disconnected");
    }
    SDL_DestroyMutex(websocket.write_mutex);
    SDL_DestroyMutex(stdio.write_mutex);
    destroy_child(relay, &child);
    close_relay_client(relay);
}

static int SDLCALL codex_relay_thread(void *userdata) {
    EidolonCodexRelay *relay = userdata;
    uint16_t port = 0U;
    WSADATA winsock;
    if (!parse_listen_port(relay->listen_url, &port) || WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        eidolon_log_write("provider", "invalid codex relay URL: %s", relay->listen_url);
        return 0;
    }
    const SOCKET listener = create_listener(port);
    if (listener == INVALID_SOCKET) {
        eidolon_log_write("provider", "codex relay listen failed url=%s error=%d",
                          relay->listen_url, WSAGetLastError());
        WSACleanup();
        return 0;
    }
    SDL_LockMutex(relay->mutex);
    relay->listener = listener;
    SDL_UnlockMutex(relay->mutex);
    eidolon_log_write("provider", "codex relay ready url=%s", relay->listen_url);
    while (!relay_stopping(relay)) {
        const SOCKET client = accept(listener, NULL, NULL);
        if (client == INVALID_SOCKET) {
            break;
        }
        SDL_LockMutex(relay->mutex);
        relay->client = client;
        SDL_UnlockMutex(relay->mutex);
        if (!websocket_handshake(client)) {
            close_relay_client(relay);
            continue;
        }
        run_client(relay);
    }
    if (release_relay_listener(relay, listener)) {
        closesocket(listener);
    }
    WSACleanup();
    return 0;
}

EidolonCodexRelay *eidolon_codex_relay_start(const char *listen_url, const char *executable,
                                             EidolonConversationBus *bus) {
    if (listen_url == NULL || executable == NULL || bus == NULL) {
        return NULL;
    }
    EidolonCodexRelay *relay = SDL_calloc(1U, sizeof(*relay));
    if (relay == NULL) {
        return NULL;
    }
    relay->bus = bus;
    relay->listener = INVALID_SOCKET;
    relay->client = INVALID_SOCKET;
    SDL_strlcpy(relay->listen_url, listen_url, sizeof(relay->listen_url));
    SDL_strlcpy(relay->executable, executable, sizeof(relay->executable));
    relay->mutex = SDL_CreateMutex();
    if (relay->mutex == NULL) {
        SDL_free(relay);
        return NULL;
    }
    relay->thread = SDL_CreateThread(codex_relay_thread, "eidolon-codex-relay", relay);
    if (relay->thread == NULL) {
        SDL_DestroyMutex(relay->mutex);
        SDL_free(relay);
        return NULL;
    }
    return relay;
}

void eidolon_codex_relay_stop(EidolonCodexRelay *relay) {
    if (relay == NULL) {
        return;
    }
    (void)SDL_SetAtomicInt(&relay->stopping, 1);
    SDL_LockMutex(relay->mutex);
    const SOCKET listener = relay->listener;
    relay->listener = INVALID_SOCKET;
    SDL_UnlockMutex(relay->mutex);
    close_relay_client(relay);
    terminate_relay_child(relay);
    if (listener != INVALID_SOCKET) {
        closesocket(listener);
    }
    SDL_WaitThread(relay->thread, NULL);
    SDL_DestroyMutex(relay->mutex);
    SDL_free(relay);
}

#else

struct EidolonCodexRelay {
    int unused;
};

EidolonCodexRelay *eidolon_codex_relay_start(const char *listen_url, const char *executable,
                                             EidolonConversationBus *bus) {
    (void)listen_url;
    (void)executable;
    (void)bus;
    return NULL;
}

void eidolon_codex_relay_stop(EidolonCodexRelay *relay) { (void)relay; }

#endif
