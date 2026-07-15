#ifndef EIDOLON_SESSION_WATCH_H
#define EIDOLON_SESSION_WATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_SESSION_PATH_CAPACITY 4096

typedef struct EidolonSessionWatch {
    char path[EIDOLON_SESSION_PATH_CAPACITY];
    char last_output[4097];
    uint64_t stamp;
    uint64_t next_poll_ms;
    bool initialized;
} EidolonSessionWatch;

bool eidolon_session_watch_poll(EidolonSessionWatch *watch, uint64_t now_ms, char *output,
                                size_t capacity);

#endif
