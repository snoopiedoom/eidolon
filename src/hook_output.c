#include "hook_output.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <share.h>
#endif

#define HOOK_INPUT_CAPACITY (64U * 1024U)
#define TRANSCRIPT_TAIL_CAPACITY (1024U * 1024U)

static int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static bool decode_json_string(const char *cursor, char *output, size_t capacity) {
    if (*cursor != '"' || capacity == 0) {
        return false;
    }

    ++cursor;
    size_t length = 0;
    while (*cursor != '\0' && *cursor != '"') {
        unsigned char character = (unsigned char)*cursor++;
        if (character == '\\') {
            character = (unsigned char)*cursor++;
            switch (character) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                character = '\b';
                break;
            case 'f':
                character = '\f';
                break;
            case 'n':
                character = '\n';
                break;
            case 'r':
                character = '\r';
                break;
            case 't':
                character = '\t';
                break;
            case 'u': {
                int codepoint = 0;
                for (int digit = 0; digit < 4; ++digit) {
                    const int value = hex_value(cursor[digit]);
                    if (value < 0) {
                        return false;
                    }
                    codepoint = (codepoint << 4) | value;
                }
                cursor += 4;
                character = codepoint >= 32 && codepoint < 127 ? (unsigned char)codepoint : '?';
                break;
            }
            default:
                return false;
            }
        }

        if (length + 1 < capacity) {
            output[length++] = (char)character;
        }
    }

    if (*cursor != '"') {
        return false;
    }
    output[length] = '\0';
    return true;
}

static bool find_string_value(const char *json, const char *key, char *output, size_t capacity) {
    char pattern[96];
    const int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(pattern)) {
        return false;
    }

    const char *cursor = strstr(json, pattern);
    if (cursor == NULL) {
        return false;
    }
    cursor += (size_t)written;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    if (*cursor++ != ':') {
        return false;
    }
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    return decode_json_string(cursor, output, capacity);
}

static char *read_stream(FILE *stream, size_t limit, size_t *size) {
    char *buffer = malloc(limit + 1);
    if (buffer == NULL) {
        return NULL;
    }

    *size = fread(buffer, 1, limit, stream);
    buffer[*size] = '\0';
    return buffer;
}

static FILE *open_binary_file(const char *path) {
#ifdef _WIN32
    return _fsopen(path, "rb", _SH_DENYNO);
#else
    return fopen(path, "rb");
#endif
}

static char *read_transcript_tail(const char *path, size_t *size) {
    FILE *file = open_binary_file(path);
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long end = ftell(file);
    if (end < 0) {
        fclose(file);
        return NULL;
    }

    const long tail = end > (long)TRANSCRIPT_TAIL_CAPACITY ? (long)TRANSCRIPT_TAIL_CAPACITY : end;
    if (fseek(file, end - tail, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = read_stream(file, (size_t)tail, size);
    fclose(file);
    if (buffer != NULL && end > tail) {
        char *first_line = strchr(buffer, '\n');
        if (first_line != NULL) {
            ++first_line;
            *size -= (size_t)(first_line - buffer);
            memmove(buffer, first_line, *size + 1);
        }
    }
    return buffer;
}

static bool extract_latest_message(char *transcript, char *output, size_t capacity) {
    bool found = false;
    char *line = transcript;
    while (*line != '\0') {
        char *end = strchr(line, '\n');
        if (end != NULL) {
            *end = '\0';
        }

        if (strstr(line, "\"type\":\"task_complete\"") != NULL) {
            found = find_string_value(line, "last_agent_message", output, capacity) || found;
        } else if (strstr(line, "\"type\":\"response_item\"") != NULL &&
                   strstr(line, "\"role\":\"assistant\"") != NULL &&
                   strstr(line, "\"phase\":\"final\"") != NULL &&
                   strstr(line, "\"type\":\"output_text\"") != NULL) {
            const char *content = strstr(line, "\"type\":\"output_text\"");
            found = find_string_value(content, "text", output, capacity) || found;
        }

        if (end == NULL) {
            break;
        }
        line = end + 1;
    }
    return found;
}

bool eidolon_hook_read_agent_output(FILE *input, char *output, size_t capacity) {
    if (input == NULL || output == NULL || capacity == 0) {
        return false;
    }
    output[0] = '\0';

    size_t input_size = 0;
    char *hook_input = read_stream(input, HOOK_INPUT_CAPACITY, &input_size);
    (void)input_size;
    if (hook_input == NULL) {
        eidolon_log_write("hook", "could not read hook stdin");
        return false;
    }

    char transcript_path[4096];
    const bool has_path =
        find_string_value(hook_input, "transcript_path", transcript_path, sizeof(transcript_path));
    free(hook_input);
    if (!has_path) {
        eidolon_log_write("hook", "stdin did not contain a usable transcript_path");
        return false;
    }

    const bool found = eidolon_transcript_read_agent_output(transcript_path, output, capacity);
    eidolon_log_write("hook", "transcript parsed found_output=%s bytes=%zu", found ? "yes" : "no",
                      found ? strlen(output) : 0U);
    return found;
}

bool eidolon_transcript_read_agent_output(const char *path, char *output, size_t capacity) {
    if (path == NULL || output == NULL || capacity == 0) {
        return false;
    }
    output[0] = '\0';

    size_t transcript_size = 0;
    char *transcript = read_transcript_tail(path, &transcript_size);
    (void)transcript_size;
    if (transcript == NULL) {
        eidolon_log_write("session", "could not read transcript: %s", path);
        return false;
    }

    const bool found = extract_latest_message(transcript, output, capacity);
    free(transcript);
    return found;
}
