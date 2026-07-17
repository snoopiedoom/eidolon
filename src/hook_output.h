#ifndef EIDOLON_HOOK_OUTPUT_H
#define EIDOLON_HOOK_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

bool eidolon_hook_read_agent_output(FILE *input, char *output, size_t capacity);
bool eidolon_transcript_read_agent_output(const char *path, char *output, size_t capacity);
bool eidolon_transcript_is_primary_session(const char *path);

#endif
