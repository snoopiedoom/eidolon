#ifndef EIDOLON_PLATFORM_SESSION_FILES_H
#define EIDOLON_PLATFORM_SESSION_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_TRANSCRIPT_PATH_CAPACITY 4096

typedef struct EidolonTranscriptFile {
    char path[EIDOLON_TRANSCRIPT_PATH_CAPACITY];
    uint64_t stamp;
} EidolonTranscriptFile;

size_t eidolon_platform_list_transcripts(EidolonTranscriptFile *files, size_t capacity);
bool eidolon_platform_latest_transcript(char *path, size_t capacity, uint64_t *stamp);
bool eidolon_platform_transcript_stamp(const char *path, uint64_t *stamp);
bool eidolon_platform_session_index_path(char *path, size_t capacity);

#endif
