#include "session_watch.h"

#include "hook_output.h"
#include "log.h"
#include "platform/session_files.h"

#include <SDL3/SDL.h>

#include <string.h>

#define SESSION_POLL_INTERVAL_MS 500U

bool eidolon_session_watch_poll(EidolonSessionWatch *watch, uint64_t now_ms, char *output,
                                size_t capacity) {
    if (watch == NULL || output == NULL || capacity == 0 || now_ms < watch->next_poll_ms) {
        return false;
    }
    watch->next_poll_ms = now_ms + SESSION_POLL_INTERVAL_MS;

    char path[EIDOLON_SESSION_PATH_CAPACITY];
    uint64_t stamp = 0;
    if (!eidolon_platform_latest_transcript(path, sizeof(path), &stamp)) {
        return false;
    }
    if (stamp == watch->stamp && strcmp(path, watch->path) == 0) {
        return false;
    }

    const bool path_changed = strcmp(path, watch->path) != 0;
    char latest[sizeof(watch->last_output)];
    const bool found = eidolon_transcript_read_agent_output(path, latest, sizeof(latest));
    watch->stamp = stamp;
    SDL_strlcpy(watch->path, path, sizeof(watch->path));

    if (!watch->initialized || path_changed) {
        watch->initialized = true;
        watch->last_output[0] = '\0';
        if (found) {
            SDL_strlcpy(watch->last_output, latest, sizeof(watch->last_output));
        }
        eidolon_log_write("session", "watching transcript: %s", path);
        return false;
    }
    if (!found || latest[0] == '\0' || strcmp(latest, watch->last_output) == 0) {
        return false;
    }

    SDL_strlcpy(watch->last_output, latest, sizeof(watch->last_output));
    SDL_strlcpy(output, latest, capacity);
    eidolon_log_write("session", "agent output detected bytes=%zu transcript=%s", strlen(output),
                      path);
    return true;
}
