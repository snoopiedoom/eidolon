#ifndef EIDOLON_HOOK_OUTPUT_H
#define EIDOLON_HOOK_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define EIDOLON_TRANSCRIPT_TIMESTAMP_CAPACITY 40U

bool eidolon_hook_read_agent_output(FILE *input, char *output, size_t capacity);
bool eidolon_transcript_read_agent_output(const char *path, char *output, size_t capacity);
bool eidolon_transcript_read_agent_output_info(const char *path, char *output, size_t capacity,
                                               char *source_timestamp,
                                               size_t timestamp_capacity);
bool eidolon_transcript_is_primary_session(const char *path);

#endif
