#include "affect_tokenizer.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOCAB_TABLE_CAPACITY 131071U
#define MERGE_TABLE_CAPACITY 131071U

typedef struct VocabEntry {
    char *token;
    int64_t id;
} VocabEntry;

typedef struct MergeEntry {
    char *left;
    char *right;
    int rank;
} MergeEntry;

struct EidolonAffectTokenizer {
    VocabEntry *vocab;
    MergeEntry *merges;
};

typedef struct Symbol {
    char *text;
    size_t length;
} Symbol;

static void set_error(char *error, size_t capacity, const char *format, ...) {
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static uint64_t hash_bytes(const char *text, size_t length) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t index = 0U; index < length; ++index) {
        hash ^= (unsigned char)text[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_pair(const char *left, const char *right) {
    uint64_t hash = hash_bytes(left, strlen(left));
    hash ^= UINT64_C(0xff);
    hash *= UINT64_C(1099511628211);
    for (const unsigned char *cursor = (const unsigned char *)right; *cursor != 0U; ++cursor) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static char *duplicate_range(const char *text, size_t length) {
    char *copy = malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static char *read_file(const char *path, size_t *length_out) {
    FILE *file = NULL;
#if defined(_WIN32)
    if (fopen_s(&file, path, "rb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return NULL;
    }
    const long file_length = ftell(file);
    if (file_length < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *data = malloc((size_t)file_length + 1U);
    if (data == NULL || fread(data, 1U, (size_t)file_length, file) != (size_t)file_length) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    data[file_length] = '\0';
    *length_out = (size_t)file_length;
    return data;
}

static size_t append_utf8(char *output, size_t cursor, uint32_t codepoint) {
    if (codepoint <= 0x7fU) {
        output[cursor++] = (char)codepoint;
    } else if (codepoint <= 0x7ffU) {
        output[cursor++] = (char)(0xc0U | (codepoint >> 6U));
        output[cursor++] = (char)(0x80U | (codepoint & 0x3fU));
    } else {
        output[cursor++] = (char)(0xe0U | (codepoint >> 12U));
        output[cursor++] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        output[cursor++] = (char)(0x80U | (codepoint & 0x3fU));
    }
    return cursor;
}

static int hex_value(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static char *parse_json_string(const char **cursor_in) {
    const char *cursor = *cursor_in;
    if (*cursor++ != '"') {
        return NULL;
    }
    const char *scan = cursor;
    size_t maximum = 0U;
    while (*scan != '\0' && *scan != '"') {
        maximum += (*scan == '\\' && scan[1] != '\0') ? 3U : 1U;
        scan += (*scan == '\\' && scan[1] != '\0') ? 2 : 1;
    }
    char *result = malloc(maximum + 1U);
    if (result == NULL) {
        return NULL;
    }
    size_t output = 0U;
    while (*cursor != '\0' && *cursor != '"') {
        if (*cursor != '\\') {
            result[output++] = *cursor++;
            continue;
        }
        ++cursor;
        if (*cursor == 'u') {
            uint32_t codepoint = 0U;
            ++cursor;
            for (int digit = 0; digit < 4; ++digit) {
                const int value = hex_value(*cursor++);
                if (value < 0) {
                    free(result);
                    return NULL;
                }
                codepoint = codepoint * 16U + (uint32_t)value;
            }
            output = append_utf8(result, output, codepoint);
        } else {
            const char escaped = *cursor++;
            switch (escaped) {
            case 'n':
                result[output++] = '\n';
                break;
            case 'r':
                result[output++] = '\r';
                break;
            case 't':
                result[output++] = '\t';
                break;
            case 'b':
                result[output++] = '\b';
                break;
            case 'f':
                result[output++] = '\f';
                break;
            default:
                result[output++] = escaped;
                break;
            }
        }
    }
    if (*cursor != '"') {
        free(result);
        return NULL;
    }
    result[output] = '\0';
    *cursor_in = cursor + 1;
    return result;
}

static bool vocab_insert(EidolonAffectTokenizer *tokenizer, char *token, int64_t id) {
    size_t slot = (size_t)(hash_bytes(token, strlen(token)) % VOCAB_TABLE_CAPACITY);
    while (tokenizer->vocab[slot].token != NULL) {
        slot = (slot + 1U) % VOCAB_TABLE_CAPACITY;
    }
    tokenizer->vocab[slot].token = token;
    tokenizer->vocab[slot].id = id;
    return true;
}

static bool vocab_find(const EidolonAffectTokenizer *tokenizer, const char *token,
                       int64_t *id_out) {
    size_t slot = (size_t)(hash_bytes(token, strlen(token)) % VOCAB_TABLE_CAPACITY);
    while (tokenizer->vocab[slot].token != NULL) {
        if (strcmp(tokenizer->vocab[slot].token, token) == 0) {
            *id_out = tokenizer->vocab[slot].id;
            return true;
        }
        slot = (slot + 1U) % VOCAB_TABLE_CAPACITY;
    }
    return false;
}

static bool load_vocab(EidolonAffectTokenizer *tokenizer, const char *path) {
    size_t length = 0U;
    char *data = read_file(path, &length);
    if (data == NULL) {
        return false;
    }
    const char *cursor = data;
    while ((size_t)(cursor - data) < length && *cursor != '{') {
        ++cursor;
    }
    if (*cursor++ != '{') {
        free(data);
        return false;
    }
    for (;;) {
        while (isspace((unsigned char)*cursor) || *cursor == ',') {
            ++cursor;
        }
        if (*cursor == '}') {
            break;
        }
        char *token = parse_json_string(&cursor);
        if (token == NULL) {
            free(data);
            return false;
        }
        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor++ != ':') {
            free(token);
            free(data);
            return false;
        }
        char *end = NULL;
        const long id = strtol(cursor, &end, 10);
        if (end == cursor || id < 0L) {
            free(token);
            free(data);
            return false;
        }
        cursor = end;
        (void)vocab_insert(tokenizer, token, (int64_t)id);
    }
    free(data);
    return true;
}

static bool merge_insert(EidolonAffectTokenizer *tokenizer, char *left, char *right, int rank) {
    size_t slot = (size_t)(hash_pair(left, right) % MERGE_TABLE_CAPACITY);
    while (tokenizer->merges[slot].left != NULL) {
        slot = (slot + 1U) % MERGE_TABLE_CAPACITY;
    }
    tokenizer->merges[slot] = (MergeEntry){left, right, rank};
    return true;
}

static int merge_rank(const EidolonAffectTokenizer *tokenizer, const char *left,
                      const char *right) {
    size_t slot = (size_t)(hash_pair(left, right) % MERGE_TABLE_CAPACITY);
    while (tokenizer->merges[slot].left != NULL) {
        const MergeEntry *entry = &tokenizer->merges[slot];
        if (strcmp(entry->left, left) == 0 && strcmp(entry->right, right) == 0) {
            return entry->rank;
        }
        slot = (slot + 1U) % MERGE_TABLE_CAPACITY;
    }
    return -1;
}

static bool load_merges(EidolonAffectTokenizer *tokenizer, const char *path) {
    size_t length = 0U;
    char *data = read_file(path, &length);
    if (data == NULL) {
        return false;
    }
    char *cursor = data;
    int rank = 0;
    while (*cursor != '\0') {
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }
        const size_t line_length = strlen(line);
        if (line_length > 0U && line[line_length - 1U] == '\r') {
            line[line_length - 1U] = '\0';
        }
        if (*line == '\0' || *line == '#') {
            continue;
        }
        char *space = strchr(line, ' ');
        if (space == NULL) {
            free(data);
            return false;
        }
        *space = '\0';
        char *left = duplicate_range(line, strlen(line));
        char *right = duplicate_range(space + 1, strlen(space + 1));
        if (left == NULL || right == NULL) {
            free(left);
            free(right);
            free(data);
            return false;
        }
        (void)merge_insert(tokenizer, left, right, rank++);
    }
    free(data);
    return true;
}

EidolonAffectTokenizer *eidolon_affect_tokenizer_create(const char *vocab_path,
                                                        const char *merges_path, char *error,
                                                        size_t error_capacity) {
    EidolonAffectTokenizer *tokenizer = calloc(1U, sizeof(*tokenizer));
    if (tokenizer == NULL) {
        set_error(error, error_capacity, "out of memory");
        return NULL;
    }
    tokenizer->vocab = calloc(VOCAB_TABLE_CAPACITY, sizeof(*tokenizer->vocab));
    tokenizer->merges = calloc(MERGE_TABLE_CAPACITY, sizeof(*tokenizer->merges));
    if (tokenizer->vocab == NULL || tokenizer->merges == NULL ||
        !load_vocab(tokenizer, vocab_path) || !load_merges(tokenizer, merges_path)) {
        set_error(error, error_capacity, "could not load RoBERTa vocabulary/merges");
        eidolon_affect_tokenizer_destroy(tokenizer);
        return NULL;
    }
    return tokenizer;
}

void eidolon_affect_tokenizer_destroy(EidolonAffectTokenizer *tokenizer) {
    if (tokenizer == NULL) {
        return;
    }
    if (tokenizer->vocab != NULL) {
        for (size_t index = 0U; index < VOCAB_TABLE_CAPACITY; ++index) {
            free(tokenizer->vocab[index].token);
        }
    }
    if (tokenizer->merges != NULL) {
        for (size_t index = 0U; index < MERGE_TABLE_CAPACITY; ++index) {
            free(tokenizer->merges[index].left);
            free(tokenizer->merges[index].right);
        }
    }
    free(tokenizer->vocab);
    free(tokenizer->merges);
    free(tokenizer);
}

static uint32_t byte_codepoint(unsigned byte) {
    if ((byte >= 33U && byte <= 126U) || (byte >= 161U && byte <= 172U) ||
        (byte >= 174U && byte <= 255U)) {
        return byte;
    }
    unsigned extra = 0U;
    for (unsigned candidate = 0U; candidate < byte; ++candidate) {
        if (!((candidate >= 33U && candidate <= 126U) || (candidate >= 161U && candidate <= 172U) ||
              (candidate >= 174U && candidate <= 255U))) {
            ++extra;
        }
    }
    return 256U + extra;
}

static int byte_category(unsigned char value) {
    if (value >= 0x80U || isalpha(value)) {
        return 1;
    }
    if (isdigit(value)) {
        return 2;
    }
    if (isspace(value)) {
        return 0;
    }
    return 3;
}

static size_t next_piece(const unsigned char *text, size_t length, size_t start) {
    size_t cursor = start;
    if (text[cursor] == '\'' && cursor + 1U < length) {
        static const char *suffixes[] = {"s", "t", "re", "ve", "m", "ll", "d"};
        for (size_t index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
            const size_t suffix_length = strlen(suffixes[index]);
            if (cursor + 1U + suffix_length <= length &&
                memcmp(text + cursor + 1U, suffixes[index], suffix_length) == 0) {
                return cursor + 1U + suffix_length;
            }
        }
    }
    if (isspace(text[cursor]) && text[cursor] != ' ') {
        while (cursor < length && isspace(text[cursor])) {
            ++cursor;
        }
        return cursor;
    }
    if (text[cursor] == ' ' && cursor + 1U < length && !isspace(text[cursor + 1U])) {
        ++cursor;
    }
    const int category = byte_category(text[cursor]);
    while (cursor < length && byte_category(text[cursor]) == category) {
        ++cursor;
    }
    return cursor;
}

static bool encode_piece(EidolonAffectTokenizer *tokenizer, const unsigned char *bytes,
                         size_t byte_count, int64_t *ids, size_t *count, size_t capacity,
                         char *error, size_t error_capacity) {
    const size_t symbol_capacity = byte_count * 2U + 1U;
    Symbol *symbols = calloc(symbol_capacity, sizeof(*symbols));
    char *initial = malloc(byte_count * 3U + 1U);
    if (symbols == NULL || initial == NULL) {
        free(symbols);
        free(initial);
        set_error(error, error_capacity, "out of memory while tokenizing");
        return false;
    }
    size_t initial_cursor = 0U;
    size_t symbol_count = 0U;
    for (size_t index = 0U; index < byte_count; ++index) {
        const size_t begin = initial_cursor;
        initial_cursor = append_utf8(initial, initial_cursor, byte_codepoint(bytes[index]));
        initial[initial_cursor++] = '\0';
        symbols[symbol_count++] = (Symbol){initial + begin, initial_cursor - begin - 1U};
    }
    char **owned = calloc(byte_count + 1U, sizeof(*owned));
    size_t owned_count = 0U;
    if (owned == NULL) {
        free(initial);
        free(symbols);
        set_error(error, error_capacity, "out of memory while tokenizing");
        return false;
    }
    for (;;) {
        int best_rank = -1;
        size_t best = 0U;
        for (size_t index = 0U; index + 1U < symbol_count; ++index) {
            const int rank = merge_rank(tokenizer, symbols[index].text, symbols[index + 1U].text);
            if (rank >= 0 && (best_rank < 0 || rank < best_rank)) {
                best_rank = rank;
                best = index;
            }
        }
        if (best_rank < 0) {
            break;
        }
        const size_t merged_length = symbols[best].length + symbols[best + 1U].length;
        char *merged = malloc(merged_length + 1U);
        if (merged == NULL) {
            set_error(error, error_capacity, "out of memory while applying BPE");
            goto failure;
        }
        memcpy(merged, symbols[best].text, symbols[best].length);
        memcpy(merged + symbols[best].length, symbols[best + 1U].text, symbols[best + 1U].length);
        merged[merged_length] = '\0';
        owned[owned_count++] = merged;
        symbols[best] = (Symbol){merged, merged_length};
        memmove(&symbols[best + 1U], &symbols[best + 2U],
                (symbol_count - best - 2U) * sizeof(*symbols));
        --symbol_count;
    }
    for (size_t index = 0U; index < symbol_count; ++index) {
        int64_t id = 3;
        (void)vocab_find(tokenizer, symbols[index].text, &id);
        if (*count >= capacity - 1U) {
            set_error(error, error_capacity, "text exceeds token capacity");
            goto failure;
        }
        ids[(*count)++] = id;
    }
    for (size_t index = 0U; index < owned_count; ++index) {
        free(owned[index]);
    }
    free(owned);
    free(initial);
    free(symbols);
    return true;

failure:
    for (size_t index = 0U; index < owned_count; ++index) {
        free(owned[index]);
    }
    free(owned);
    free(initial);
    free(symbols);
    return false;
}

size_t eidolon_affect_tokenizer_encode(EidolonAffectTokenizer *tokenizer, const char *text,
                                       int64_t *token_ids, int64_t *attention_mask, size_t capacity,
                                       char *error, size_t error_capacity) {
    if (tokenizer == NULL || text == NULL || token_ids == NULL || attention_mask == NULL ||
        capacity < 2U) {
        set_error(error, error_capacity, "invalid tokenizer arguments");
        return 0U;
    }
    const unsigned char *bytes = (const unsigned char *)text;
    const size_t length = strlen(text);
    size_t count = 0U;
    token_ids[count++] = 0;
    for (size_t cursor = 0U; cursor < length;) {
        const size_t end = next_piece(bytes, length, cursor);
        if (end <= cursor || !encode_piece(tokenizer, bytes + cursor, end - cursor, token_ids,
                                           &count, capacity, error, error_capacity)) {
            return 0U;
        }
        cursor = end;
    }
    token_ids[count++] = 2;
    for (size_t index = 0U; index < capacity; ++index) {
        attention_mask[index] = index < count ? 1 : 0;
        if (index >= count) {
            token_ids[index] = 1;
        }
    }
    return count;
}
