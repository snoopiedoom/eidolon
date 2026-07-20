#include "affect_client.h"

#include "affect_protocol.h"
#include "log.h"

#include <SDL3/SDL.h>

#include <string.h>

#define AFFECT_QUEUE_CAPACITY 128U

typedef struct AffectQueuedRequest {
    char text[EIDOLON_AFFECT_TEXT_CAPACITY + 1U];
    uint64_t sequence;
} AffectQueuedRequest;

struct EidolonAffectClient {
    SDL_Mutex *mutex;
    SDL_Condition *condition;
    SDL_Thread *thread;
    char worker_path[4096];
    AffectQueuedRequest requests[AFFECT_QUEUE_CAPACITY];
    EidolonAffectResponse results[AFFECT_QUEUE_CAPACITY];
    size_t request_head;
    size_t request_count;
    size_t result_head;
    size_t result_count;
    bool stopping;
    bool available;
};

static bool client_is_stopping(EidolonAffectClient *client) {
    SDL_LockMutex(client->mutex);
    const bool stopping = client->stopping;
    SDL_UnlockMutex(client->mutex);
    return stopping;
}

static bool write_exact(SDL_IOStream *stream, const void *data, size_t length) {
    const unsigned char *cursor = data;
    size_t remaining = length;
    while (remaining > 0U) {
        const size_t written = SDL_WriteIO(stream, cursor, remaining);
        if (written == 0U) {
            return false;
        }
        cursor += written;
        remaining -= written;
    }
    return true;
}

static bool read_exact(EidolonAffectClient *client, SDL_IOStream *stream, void *data,
                       size_t length) {
    unsigned char *cursor = data;
    size_t remaining = length;
    while (remaining > 0U && !client_is_stopping(client)) {
        const size_t received = SDL_ReadIO(stream, cursor, remaining);
        if (received > 0U) {
            cursor += received;
            remaining -= received;
            continue;
        }
        const SDL_IOStatus status = SDL_GetIOStatus(stream);
        if (status != SDL_IO_STATUS_READY && status != SDL_IO_STATUS_NOT_READY) {
            return false;
        }
        SDL_Delay(2U);
    }
    return remaining == 0U;
}

static int SDLCALL affect_thread(void *userdata) {
    EidolonAffectClient *client = userdata;
    const char *arguments[] = {client->worker_path, NULL};
    SDL_Process *process = SDL_CreateProcess(arguments, true);
    if (process == NULL) {
        eidolon_log_write("affect", "worker unavailable: %s", SDL_GetError());
        return 1;
    }
    SDL_IOStream *input = SDL_GetProcessInput(process);
    SDL_IOStream *output = SDL_GetProcessOutput(process);
    if (input == NULL || output == NULL) {
        eidolon_log_write("affect", "worker pipes unavailable: %s", SDL_GetError());
        SDL_DestroyProcess(process);
        return 1;
    }

    SDL_LockMutex(client->mutex);
    client->available = true;
    SDL_UnlockMutex(client->mutex);
    eidolon_log_write("affect", "native GoEmotions worker ready");

    for (;;) {
        AffectQueuedRequest queued;
        SDL_LockMutex(client->mutex);
        while (client->request_count == 0U && !client->stopping) {
            SDL_WaitCondition(client->condition, client->mutex);
        }
        if (client->stopping) {
            SDL_UnlockMutex(client->mutex);
            break;
        }
        queued = client->requests[client->request_head];
        client->request_head = (client->request_head + 1U) % AFFECT_QUEUE_CAPACITY;
        client->request_count -= 1U;
        SDL_UnlockMutex(client->mutex);

        const size_t text_length = strlen(queued.text);
        const EidolonAffectRequestHeader request = {EIDOLON_AFFECT_PROTOCOL_MAGIC,
                                                    EIDOLON_AFFECT_PROTOCOL_VERSION,
                                                    queued.sequence,
                                                    (uint32_t)text_length};
        if (!write_exact(input, &request, sizeof(request)) ||
            !write_exact(input, queued.text, text_length) || !SDL_FlushIO(input)) {
            eidolon_log_write("affect", "worker request failed: %s", SDL_GetError());
            break;
        }
        EidolonAffectResponse response;
        if (!read_exact(client, output, &response, sizeof(response))) {
            if (!client_is_stopping(client)) {
                eidolon_log_write("affect", "worker response failed: %s", SDL_GetError());
            }
            break;
        }
        if (response.magic != EIDOLON_AFFECT_PROTOCOL_MAGIC ||
            response.version != EIDOLON_AFFECT_PROTOCOL_VERSION || response.status != 0U) {
            eidolon_log_write("affect", "worker rejected sequence=%llu status=%u",
                              (unsigned long long)queued.sequence, response.status);
            continue;
        }
        SDL_LockMutex(client->mutex);
        if (client->result_count == AFFECT_QUEUE_CAPACITY) {
            client->result_head = (client->result_head + 1U) % AFFECT_QUEUE_CAPACITY;
            client->result_count -= 1U;
            eidolon_log_write("affect", "result queue full; oldest classification discarded");
        }
        const size_t result_tail =
            (client->result_head + client->result_count) % AFFECT_QUEUE_CAPACITY;
        client->results[result_tail] = response;
        client->result_count += 1U;
        SDL_UnlockMutex(client->mutex);
    }

    SDL_LockMutex(client->mutex);
    client->available = false;
    SDL_UnlockMutex(client->mutex);
    (void)SDL_KillProcess(process, true);
    SDL_DestroyProcess(process);
    return 0;
}

EidolonAffectClient *eidolon_affect_client_create(const char *worker_path) {
    SDL_PathInfo path_info;
    if (worker_path == NULL || !SDL_GetPathInfo(worker_path, &path_info) ||
        path_info.type != SDL_PATHTYPE_FILE) {
        eidolon_log_write("affect", "worker not installed; state fallback active");
        return NULL;
    }
    EidolonAffectClient *client = SDL_calloc(1U, sizeof(*client));
    if (client == NULL) {
        return NULL;
    }
    SDL_strlcpy(client->worker_path, worker_path, sizeof(client->worker_path));
    client->mutex = SDL_CreateMutex();
    client->condition = SDL_CreateCondition();
    if (client->mutex == NULL || client->condition == NULL) {
        eidolon_affect_client_destroy(client);
        return NULL;
    }
    client->thread = SDL_CreateThread(affect_thread, "eidolon-affect", client);
    if (client->thread == NULL) {
        eidolon_affect_client_destroy(client);
        return NULL;
    }
    return client;
}

void eidolon_affect_client_destroy(EidolonAffectClient *client) {
    if (client == NULL) {
        return;
    }
    if (client->mutex != NULL) {
        SDL_LockMutex(client->mutex);
        client->stopping = true;
        if (client->condition != NULL) {
            SDL_SignalCondition(client->condition);
        }
        SDL_UnlockMutex(client->mutex);
    }
    if (client->thread != NULL) {
        SDL_WaitThread(client->thread, NULL);
    }
    SDL_DestroyCondition(client->condition);
    SDL_DestroyMutex(client->mutex);
    SDL_free(client);
}

bool eidolon_affect_client_submit(EidolonAffectClient *client, uint64_t sequence,
                                  const char *text) {
    if (client == NULL || text == NULL || text[0] == '\0') {
        return false;
    }
    SDL_LockMutex(client->mutex);
    if (client->request_count == AFFECT_QUEUE_CAPACITY) {
        SDL_UnlockMutex(client->mutex);
        return false;
    }
    const size_t tail = (client->request_head + client->request_count) % AFFECT_QUEUE_CAPACITY;
    client->requests[tail].sequence = sequence;
    SDL_strlcpy(client->requests[tail].text, text, sizeof(client->requests[tail].text));
    client->request_count += 1U;
    SDL_SignalCondition(client->condition);
    SDL_UnlockMutex(client->mutex);
    return true;
}

bool eidolon_affect_client_poll(EidolonAffectClient *client, uint64_t *sequence,
                                float probabilities[EIDOLON_GOEMOTIONS_COUNT]) {
    if (client == NULL) {
        return false;
    }
    SDL_LockMutex(client->mutex);
    const bool pending = client->result_count > 0U;
    if (pending) {
        const EidolonAffectResponse *result = &client->results[client->result_head];
        *sequence = result->sequence;
        memcpy(probabilities, result->probabilities, sizeof(result->probabilities));
        client->result_head = (client->result_head + 1U) % AFFECT_QUEUE_CAPACITY;
        client->result_count -= 1U;
    }
    SDL_UnlockMutex(client->mutex);
    return pending;
}

bool eidolon_affect_client_available(EidolonAffectClient *client) {
    if (client == NULL) {
        return false;
    }
    SDL_LockMutex(client->mutex);
    const bool available = client->available;
    SDL_UnlockMutex(client->mutex);
    return available;
}
