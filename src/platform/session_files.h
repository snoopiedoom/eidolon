#ifndef EIDOLON_PLATFORM_SESSION_FILES_H
#define EIDOLON_PLATFORM_SESSION_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp);

#endif
