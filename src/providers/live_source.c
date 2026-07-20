#include "providers/live_source.h"

#include "json_scan.h"
#include "log.h"
#include "providers/codex_stream.h"
#include "providers/opencode_stream.h"

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#define LIVE_URL_CAPACITY 512U
#define LIVE_RECEIVE_CAPACITY (64U * 1024U)
#define LIVE_CHUNK_CAPACITY 8192U

typedef enum LiveSourceKind {
    LIVE_SOURCE_CODEX,
    LIVE_SOURCE_OPENCODE,
} LiveSourceKind;

typedef bool (*LiveFrameParser)(const char *frame, EidolonConversationEvent *event);

struct EidolonLiveSource {
    LiveSourceKind kind;
    EidolonConversationBus *bus;
    SDL_Thread *thread;
    SDL_Mutex *mutex;
    SDL_AtomicInt stopping;
    HINTERNET active;
    char url[LIVE_URL_CAPACITY];
};

typedef struct LiveUrl {
    wchar_t host[256];
    wchar_t path[1024];
    INTERNET_PORT port;
    bool secure;
} LiveUrl;

static const char *source_id(const EidolonLiveSource *source) {
    return source->kind == LIVE_SOURCE_CODEX ? "codex" : "opencode";
}

static void emit_source_state(EidolonLiveSource *source, bool connected) {
    EidolonConversationEvent event = {0};
    event.type = connected ? EIDOLON_CONVERSATION_SOURCE_CONNECTED
                           : EIDOLON_CONVERSATION_SOURCE_DISCONNECTED;
    SDL_strlcpy(event.provider, source_id(source), sizeof(event.provider));
    event.source_time_ms = SDL_GetTicks();
    (void)eidolon_conversation_bus_push(source->bus, &event);
}

static bool stopping(const EidolonLiveSource *source) {
    return SDL_GetAtomicInt((SDL_AtomicInt *)&source->stopping) != 0;
}

static bool install_active(EidolonLiveSource *source, HINTERNET handle) {
    bool installed = false;
    SDL_LockMutex(source->mutex);
    if (!stopping(source)) {
        source->active = handle;
        installed = true;
    }
    SDL_UnlockMutex(source->mutex);
    return installed;
}

static bool replace_active(EidolonLiveSource *source, HINTERNET expected, HINTERNET replacement) {
    bool replaced = false;
    SDL_LockMutex(source->mutex);
    if (!stopping(source) && source->active == expected) {
        source->active = replacement;
        replaced = true;
    }
    SDL_UnlockMutex(source->mutex);
    return replaced;
}

static bool release_active(EidolonLiveSource *source, HINTERNET handle) {
    bool owned = false;
    SDL_LockMutex(source->mutex);
    if (source->active == handle) {
        source->active = NULL;
        owned = true;
    }
    SDL_UnlockMutex(source->mutex);
    return owned;
}

static bool utf8_to_wide(const char *input, wchar_t *output, size_t capacity) {
    if (input == NULL || output == NULL || capacity == 0U || capacity > (size_t)INT_MAX) {
        return false;
    }
    const int written =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, (int)capacity);
    return written > 0;
}

static bool copy_url_component(wchar_t *destination, size_t capacity, const wchar_t *source,
                               DWORD length) {
    if (source == NULL || (size_t)length >= capacity) {
        return false;
    }
    wmemcpy(destination, source, (size_t)length);
    destination[length] = L'\0';
    return true;
}

static bool parse_live_url(const char *url, bool websocket, LiveUrl *parsed) {
    char normalized[LIVE_URL_CAPACITY];
    if (url == NULL || parsed == NULL) {
        return false;
    }
    if (websocket && strncmp(url, "ws://", 5U) == 0) {
        SDL_snprintf(normalized, sizeof(normalized), "http://%s", url + 5U);
    } else if (websocket && strncmp(url, "wss://", 6U) == 0) {
        SDL_snprintf(normalized, sizeof(normalized), "https://%s", url + 6U);
    } else {
        SDL_strlcpy(normalized, url, sizeof(normalized));
    }
    wchar_t wide_url[LIVE_URL_CAPACITY];
    if (!utf8_to_wide(normalized, wide_url, SDL_arraysize(wide_url))) {
        return false;
    }
    URL_COMPONENTS components = {0};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = (DWORD)-1;
    components.dwUrlPathLength = (DWORD)-1;
    components.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wide_url, 0U, 0U, &components) ||
        !copy_url_component(parsed->host, SDL_arraysize(parsed->host), components.lpszHostName,
                            components.dwHostNameLength)) {
        return false;
    }
    const size_t path_length = (size_t)components.dwUrlPathLength;
    const size_t extra_length = (size_t)components.dwExtraInfoLength;
    if (path_length + extra_length >= SDL_arraysize(parsed->path)) {
        return false;
    }
    if (path_length > 0U) {
        wmemcpy(parsed->path, components.lpszUrlPath, path_length);
    } else {
        parsed->path[0] = L'/';
    }
    const size_t actual_path_length = path_length > 0U ? path_length : 1U;
    if (extra_length > 0U) {
        wmemcpy(parsed->path + actual_path_length, components.lpszExtraInfo, extra_length);
    }
    parsed->path[actual_path_length + extra_length] = L'\0';
    parsed->port = components.nPort;
    parsed->secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

static HINTERNET open_session(void) {
    HINTERNET session = WinHttpOpen(L"Eidolon/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0U);
    if (session != NULL) {
        (void)WinHttpSetTimeouts(session, 5000, 5000, 5000, 0);
    }
    return session;
}

static bool dispatch_frame(EidolonLiveSource *source, LiveFrameParser parser, const char *frame) {
    EidolonConversationEvent event;
    if (!parser(frame, &event)) {
        return false;
    }
    event.source_time_ms = SDL_GetTicks();
    if (!eidolon_conversation_bus_push(source->bus, &event)) {
        eidolon_log_write("provider", "%s event queue full; delta rejected", source_id(source));
    }
    return true;
}

static bool websocket_send_json(HINTERNET socket, const char *json) {
    const size_t length = strlen(json);
    return length <= UINT32_MAX &&
           WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (void *)json,
                                (DWORD)length) == NO_ERROR;
}

static bool websocket_receive_text(HINTERNET socket, char *message, size_t capacity) {
    char chunk[LIVE_CHUNK_CAPACITY];
    size_t used = 0U;
    bool overflow = false;
    for (;;) {
        DWORD received = 0U;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE;
        const DWORD error =
            WinHttpWebSocketReceive(socket, chunk, (DWORD)sizeof(chunk), &received, &type);
        if (error != NO_ERROR || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            return false;
        }
        if (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE ||
            type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            used = 0U;
            overflow = false;
            continue;
        }
        if (used + (size_t)received + 1U <= capacity) {
            memcpy(message + used, chunk, (size_t)received);
            used += (size_t)received;
        } else {
            overflow = true;
        }
        if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
            if (!overflow) {
                message[used] = '\0';
                return true;
            } else {
                eidolon_log_write("provider", "codex frame exceeded %u bytes; dropped",
                                  (unsigned int)(capacity - 1U));
                return false;
            }
        }
    }
}

static bool codex_receive(EidolonLiveSource *source, HINTERNET socket) {
    char message[LIVE_RECEIVE_CAPACITY];
    while (websocket_receive_text(socket, message, sizeof(message))) {
        (void)dispatch_frame(source, eidolon_codex_stream_parse, message);
    }
    return stopping(source);
}

static bool run_codex_once(EidolonLiveSource *source) {
    LiveUrl url;
    if (!parse_live_url(source->url, true, &url)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    HINTERNET session = open_session();
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    HINTERNET socket = NULL;
    bool connected = false;
    bool request_installed = false;
    bool socket_installed = false;
    DWORD failure_error = ERROR_SUCCESS;
    if (session == NULL) {
        goto cleanup;
    }
    connection = WinHttpConnect(session, url.host, url.port, 0U);
    if (connection == NULL) {
        goto cleanup;
    }
    const DWORD flags = url.secure ? WINHTTP_FLAG_SECURE : 0U;
    request = WinHttpOpenRequest(connection, L"GET", url.path, NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (request == NULL ||
        !WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0U)) {
        goto cleanup;
    }
    request_installed = install_active(source, request);
    if (!request_installed) {
        goto cleanup;
    }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0U, WINHTTP_NO_REQUEST_DATA, 0U,
                            0U, 0U) ||
        !WinHttpReceiveResponse(request, NULL)) {
        goto cleanup;
    }
    socket = WinHttpWebSocketCompleteUpgrade(request, 0U);
    if (socket == NULL) {
        goto cleanup;
    }
    socket_installed = replace_active(source, request, socket);
    if (!socket_installed) {
        goto cleanup;
    }
    WinHttpCloseHandle(request);
    request = NULL;
    static const char initialize[] =
        "{\"method\":\"initialize\",\"id\":0,\"params\":{\"clientInfo\":{"
        "\"name\":\"eidolon\",\"title\":\"Eidolon\",\"version\":\"0.1.0\"}}}";
    static const char initialized[] = "{\"method\":\"initialized\",\"params\":{}}";
    if (!websocket_send_json(socket, initialize)) {
        goto cleanup;
    }
    char initialization_response[LIVE_RECEIVE_CAPACITY];
    int64_t response_id = -1;
    do {
        if (!websocket_receive_text(socket, initialization_response,
                                    sizeof(initialization_response))) {
            goto cleanup;
        }
    } while (!eidolon_json_get_integer(initialization_response, "id", &response_id) ||
             response_id != 0);
    if (!websocket_send_json(socket, initialized)) {
        goto cleanup;
    }
    connected = true;
    emit_source_state(source, true);
    eidolon_log_write("provider", "codex live stream connected url=%s", source->url);
    (void)codex_receive(source, socket);
    failure_error = GetLastError();

cleanup:
    if (!connected && failure_error == ERROR_SUCCESS) {
        failure_error = GetLastError();
    }
    if (connected && !stopping(source)) {
        emit_source_state(source, false);
    }
    if (socket != NULL) {
        if (!socket_installed || release_active(source, socket)) {
            WinHttpCloseHandle(socket);
        }
    }
    if (request != NULL) {
        if (!request_installed || release_active(source, request)) {
            WinHttpCloseHandle(request);
        }
    }
    if (connection != NULL) {
        WinHttpCloseHandle(connection);
    }
    if (session != NULL) {
        WinHttpCloseHandle(session);
    }
    SetLastError(failure_error);
    return connected;
}

static void sse_dispatch(EidolonLiveSource *source, char *event_data, size_t *event_length) {
    if (*event_length == 0U) {
        return;
    }
    if (event_data[*event_length - 1U] == '\n') {
        --*event_length;
    }
    event_data[*event_length] = '\0';
    (void)dispatch_frame(source, eidolon_opencode_stream_parse, event_data);
    *event_length = 0U;
}

static bool opencode_receive(EidolonLiveSource *source, HINTERNET request) {
    char line[LIVE_RECEIVE_CAPACITY];
    char event_data[LIVE_RECEIVE_CAPACITY];
    size_t line_length = 0U;
    size_t event_length = 0U;
    char chunk[LIVE_CHUNK_CAPACITY];
    for (;;) {
        DWORD available = 0U;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            return stopping(source);
        }
        if (available == 0U) {
            return false;
        }
        while (available > 0U) {
            DWORD received = 0U;
            const DWORD wanted = available < sizeof(chunk) ? available : (DWORD)sizeof(chunk);
            if (!WinHttpReadData(request, chunk, wanted, &received) || received == 0U) {
                return stopping(source);
            }
            available -= received;
            for (DWORD index = 0U; index < received; ++index) {
                const char character = chunk[index];
                if (character != '\n') {
                    if (line_length + 1U < sizeof(line)) {
                        line[line_length++] = character;
                    }
                    continue;
                }
                if (line_length > 0U && line[line_length - 1U] == '\r') {
                    --line_length;
                }
                line[line_length] = '\0';
                if (line_length == 0U) {
                    sse_dispatch(source, event_data, &event_length);
                } else if (strncmp(line, "data:", 5U) == 0) {
                    const char *data = line + 5U;
                    if (*data == ' ') {
                        ++data;
                    }
                    const size_t data_length = strlen(data);
                    if (event_length + data_length + 1U < sizeof(event_data)) {
                        memcpy(event_data + event_length, data, data_length);
                        event_length += data_length;
                        event_data[event_length++] = '\n';
                    } else {
                        event_length = 0U;
                        eidolon_log_write("provider",
                                          "opencode SSE event exceeded %u bytes; dropped",
                                          (unsigned int)(sizeof(event_data) - 1U));
                    }
                }
                line_length = 0U;
            }
        }
    }
}

static bool base64_encode(const unsigned char *input, size_t length, char *output,
                          size_t capacity) {
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

static bool add_opencode_authorization(HINTERNET request) {
    const char *password = SDL_getenv("OPENCODE_SERVER_PASSWORD");
    if (password == NULL || password[0] == '\0') {
        return true;
    }
    const char *username = SDL_getenv("OPENCODE_SERVER_USERNAME");
    if (username == NULL || username[0] == '\0') {
        username = "opencode";
    }
    char credentials[512];
    const int credentials_length =
        SDL_snprintf(credentials, sizeof(credentials), "%s:%s", username, password);
    char encoded[768];
    char header[832];
    wchar_t wide_header[832];
    if (credentials_length < 0 || (size_t)credentials_length >= sizeof(credentials) ||
        !base64_encode((const unsigned char *)credentials, (size_t)credentials_length, encoded,
                       sizeof(encoded))) {
        return false;
    }
    const int header_length =
        SDL_snprintf(header, sizeof(header), "Authorization: Basic %s\r\n", encoded);
    return header_length > 0 && (size_t)header_length < sizeof(header) &&
           utf8_to_wide(header, wide_header, SDL_arraysize(wide_header)) &&
           WinHttpAddRequestHeaders(request, wide_header, (DWORD)-1L,
                                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
}

static bool run_opencode_once(EidolonLiveSource *source) {
    LiveUrl url;
    if (!parse_live_url(source->url, false, &url)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    HINTERNET session = open_session();
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    bool connected = false;
    bool request_installed = false;
    DWORD failure_error = ERROR_SUCCESS;
    if (session == NULL) {
        goto cleanup;
    }
    connection = WinHttpConnect(session, url.host, url.port, 0U);
    if (connection == NULL) {
        goto cleanup;
    }
    const wchar_t *accept_types[] = {L"text/event-stream", NULL};
    const DWORD flags = url.secure ? WINHTTP_FLAG_SECURE : 0U;
    request = WinHttpOpenRequest(connection, L"GET", url.path, NULL, WINHTTP_NO_REFERER,
                                 accept_types, flags);
    if (request == NULL || !add_opencode_authorization(request)) {
        goto cleanup;
    }
    request_installed = install_active(source, request);
    if (!request_installed) {
        goto cleanup;
    }
    static const wchar_t headers[] = L"Accept: text/event-stream\r\nCache-Control: no-cache\r\n";
    if (!WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0U, 0U, 0U) ||
        !WinHttpReceiveResponse(request, NULL)) {
        goto cleanup;
    }
    DWORD status = 0U;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX) ||
        status < 200U || status >= 300U) {
        SetLastError(ERROR_WINHTTP_INVALID_SERVER_RESPONSE);
        goto cleanup;
    }
    connected = true;
    emit_source_state(source, true);
    eidolon_log_write("provider", "opencode live stream connected url=%s", source->url);
    (void)opencode_receive(source, request);
    failure_error = GetLastError();

cleanup:
    if (!connected && failure_error == ERROR_SUCCESS) {
        failure_error = GetLastError();
    }
    if (connected && !stopping(source)) {
        emit_source_state(source, false);
    }
    if (request != NULL) {
        if (!request_installed || release_active(source, request)) {
            WinHttpCloseHandle(request);
        }
    }
    if (connection != NULL) {
        WinHttpCloseHandle(connection);
    }
    if (session != NULL) {
        WinHttpCloseHandle(session);
    }
    SetLastError(failure_error);
    return connected;
}

static int SDLCALL live_source_thread(void *userdata) {
    EidolonLiveSource *source = userdata;
    unsigned int retry_ms = 1000U;
    while (!stopping(source)) {
        SetLastError(ERROR_SUCCESS);
        const bool reached_stream =
            source->kind == LIVE_SOURCE_CODEX ? run_codex_once(source) : run_opencode_once(source);
        if (stopping(source)) {
            break;
        }
        const DWORD error = GetLastError();
        eidolon_log_write("provider", "%s live stream disconnected error=%lu retry_ms=%u",
                          source_id(source), (unsigned long)error, retry_ms);
        retry_ms = reached_stream ? 1000U : SDL_min(retry_ms * 2U, 8000U);
        const uint64_t deadline = SDL_GetTicks() + retry_ms;
        while (!stopping(source) && SDL_GetTicks() < deadline) {
            SDL_Delay(50U);
        }
    }
    return 0;
}

static EidolonLiveSource *live_source_start(LiveSourceKind kind, const char *url,
                                            EidolonConversationBus *bus) {
    if (url == NULL || url[0] == '\0' || bus == NULL) {
        return NULL;
    }
    EidolonLiveSource *source = SDL_calloc(1U, sizeof(*source));
    if (source == NULL) {
        return NULL;
    }
    source->kind = kind;
    source->bus = bus;
    SDL_strlcpy(source->url, url, sizeof(source->url));
    source->mutex = SDL_CreateMutex();
    if (source->mutex == NULL) {
        SDL_free(source);
        return NULL;
    }
    source->thread = SDL_CreateThread(live_source_thread, source_id(source), source);
    if (source->thread == NULL) {
        SDL_DestroyMutex(source->mutex);
        SDL_free(source);
        return NULL;
    }
    return source;
}

EidolonLiveSource *eidolon_codex_live_source_start(const char *url, EidolonConversationBus *bus) {
    return live_source_start(LIVE_SOURCE_CODEX, url, bus);
}

EidolonLiveSource *eidolon_opencode_live_source_start(const char *url,
                                                      EidolonConversationBus *bus) {
    return live_source_start(LIVE_SOURCE_OPENCODE, url, bus);
}

void eidolon_live_source_stop(EidolonLiveSource *source) {
    if (source == NULL) {
        return;
    }
    (void)SDL_SetAtomicInt(&source->stopping, 1);
    SDL_LockMutex(source->mutex);
    HINTERNET active = source->active;
    source->active = NULL;
    SDL_UnlockMutex(source->mutex);
    if (active != NULL) {
        WinHttpCloseHandle(active);
    }
    SDL_WaitThread(source->thread, NULL);
    SDL_DestroyMutex(source->mutex);
    SDL_free(source);
}

#else

struct EidolonLiveSource {
    int unused;
};

EidolonLiveSource *eidolon_codex_live_source_start(const char *url, EidolonConversationBus *bus) {
    (void)url;
    (void)bus;
    return NULL;
}

EidolonLiveSource *eidolon_opencode_live_source_start(const char *url,
                                                      EidolonConversationBus *bus) {
    (void)url;
    (void)bus;
    return NULL;
}

void eidolon_live_source_stop(EidolonLiveSource *source) { (void)source; }

#endif
