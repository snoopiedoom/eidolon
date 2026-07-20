#ifndef EIDOLON_JSON_SCAN_H
#define EIDOLON_JSON_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool eidolon_json_get_string(const char *json, const char *key, char *output, size_t capacity);
bool eidolon_json_get_string_after(const char *json, const char *anchor, const char *key,
                                   char *output, size_t capacity);
bool eidolon_json_get_integer(const char *json, const char *key, int64_t *value);

#endif
