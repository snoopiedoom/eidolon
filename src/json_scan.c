#include "json_scan.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool parse_hex_quad(const char *text, uint32_t *value) {
    uint32_t result = 0U;
    for (size_t index = 0U; index < 4U; ++index) {
        const int digit = hex_value(text[index]);
        if (digit < 0) {
            return false;
        }
        result = (result << 4U) | (uint32_t)digit;
    }
    *value = result;
    return true;
}

static void append_byte(char *output, size_t capacity, size_t *length, unsigned char value) {
    if (*length + 1U < capacity) {
        output[*length] = (char)value;
        *length += 1U;
    }
}

static void append_codepoint(char *output, size_t capacity, size_t *length, uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        append_byte(output, capacity, length, (unsigned char)codepoint);
    } else if (codepoint <= 0x7FFU) {
        append_byte(output, capacity, length, (unsigned char)(0xC0U | (codepoint >> 6U)));
        append_byte(output, capacity, length,
                    (unsigned char)(0x80U | (codepoint & UINT32_C(0x3F))));
    } else if (codepoint <= 0xFFFFU) {
        append_byte(output, capacity, length, (unsigned char)(0xE0U | (codepoint >> 12U)));
        append_byte(output, capacity, length,
                    (unsigned char)(0x80U | ((codepoint >> 6U) & UINT32_C(0x3F))));
        append_byte(output, capacity, length,
                    (unsigned char)(0x80U | (codepoint & UINT32_C(0x3F))));
    } else {
        append_byte(output, capacity, length, (unsigned char)(0xF0U | (codepoint >> 18U)));
        append_byte(output, capacity, length,
                    (unsigned char)(0x80U | ((codepoint >> 12U) & UINT32_C(0x3F))));
        append_byte(output, capacity, length,
                    (unsigned char)(0x80U | ((codepoint >> 6U) & UINT32_C(0x3F))));
        append_byte(output, capacity, length,
                    (unsigned char)(0x80U | (codepoint & UINT32_C(0x3F))));
    }
}

static bool decode_string(const char *cursor, char *output, size_t capacity) {
    if (cursor == NULL || *cursor != '"' || output == NULL || capacity == 0U) {
        return false;
    }
    ++cursor;
    size_t length = 0U;
    while (*cursor != '\0' && *cursor != '"') {
        unsigned char character = (unsigned char)*cursor++;
        if (character != '\\') {
            append_byte(output, capacity, &length, character);
            continue;
        }
        character = (unsigned char)*cursor++;
        switch (character) {
        case '"':
        case '\\':
        case '/':
            append_byte(output, capacity, &length, character);
            break;
        case 'b':
            append_byte(output, capacity, &length, '\b');
            break;
        case 'f':
            append_byte(output, capacity, &length, '\f');
            break;
        case 'n':
            append_byte(output, capacity, &length, '\n');
            break;
        case 'r':
            append_byte(output, capacity, &length, '\r');
            break;
        case 't':
            append_byte(output, capacity, &length, '\t');
            break;
        case 'u': {
            uint32_t codepoint = 0U;
            if (!parse_hex_quad(cursor, &codepoint)) {
                return false;
            }
            cursor += 4U;
            if (codepoint >= 0xD800U && codepoint <= 0xDBFFU && cursor[0] == '\\' &&
                cursor[1] == 'u') {
                uint32_t low = 0U;
                if (!parse_hex_quad(cursor + 2U, &low) || low < 0xDC00U || low > 0xDFFFU) {
                    return false;
                }
                cursor += 6U;
                codepoint = UINT32_C(0x10000) + ((codepoint - UINT32_C(0xD800)) << 10U) +
                            (low - UINT32_C(0xDC00));
            } else if (codepoint >= 0xD800U && codepoint <= 0xDFFFU) {
                return false;
            }
            append_codepoint(output, capacity, &length, codepoint);
            break;
        }
        default:
            return false;
        }
    }
    if (*cursor != '"') {
        return false;
    }
    output[length] = '\0';
    return true;
}

static const char *find_value(const char *json, const char *key) {
    char pattern[128];
    const int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (json == NULL || key == NULL || written <= 0 || (size_t)written >= sizeof(pattern)) {
        return NULL;
    }
    const char *cursor = json;
    while ((cursor = strstr(cursor, pattern)) != NULL) {
        cursor += (size_t)written;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
            ++cursor;
        }
        if (*cursor == ':') {
            ++cursor;
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
                ++cursor;
            }
            return cursor;
        }
    }
    return NULL;
}

bool eidolon_json_get_string(const char *json, const char *key, char *output, size_t capacity) {
    return decode_string(find_value(json, key), output, capacity);
}

bool eidolon_json_get_string_after(const char *json, const char *anchor, const char *key,
                                   char *output, size_t capacity) {
    if (json == NULL || anchor == NULL) {
        return false;
    }
    const char *start = strstr(json, anchor);
    return start != NULL && eidolon_json_get_string(start + strlen(anchor), key, output, capacity);
}

bool eidolon_json_get_integer(const char *json, const char *key, int64_t *value) {
    const char *cursor = find_value(json, key);
    if (cursor == NULL || value == NULL) {
        return false;
    }
    char *end = NULL;
    const long long parsed = strtoll(cursor, &end, 10);
    if (end == cursor) {
        return false;
    }
    *value = (int64_t)parsed;
    return true;
}
