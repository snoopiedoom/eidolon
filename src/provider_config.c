#include "provider_config.h"

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define EIDOLON_PROVIDER_CONFIG_VERSION 1

static void set_error(char *error, size_t capacity, const char *format, ...) {
    if (error == NULL || capacity == 0U) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    SDL_vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
}

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        --end;
    }
    *end = '\0';
    return text;
}

static bool parse_bool(const char *text, bool *value) {
    if (SDL_strcasecmp(text, "true") == 0 || strcmp(text, "1") == 0) {
        *value = true;
        return true;
    }
    if (SDL_strcasecmp(text, "false") == 0 || strcmp(text, "0") == 0) {
        *value = false;
        return true;
    }
    return false;
}

void eidolon_provider_config_defaults(EidolonProviderConfig *config) {
    SDL_zero(*config);
    SDL_strlcpy(config->codex.url, "ws://127.0.0.1:4500", sizeof(config->codex.url));
    SDL_strlcpy(config->codex_relay.listen_url, "ws://127.0.0.1:4500",
                sizeof(config->codex_relay.listen_url));
    SDL_strlcpy(config->codex_relay.executable, "codex",
                sizeof(config->codex_relay.executable));
    SDL_strlcpy(config->opencode.url, "http://127.0.0.1:4096/event",
                sizeof(config->opencode.url));
    config->legacy_codex_transcripts = true;
    config->legacy_hooks = true;
}

static bool assign_value(EidolonProviderConfig *config, const char *key, const char *value) {
    if (strcmp(key, "codex.live.enabled") == 0) {
        return parse_bool(value, &config->codex.enabled);
    }
    if (strcmp(key, "codex.live.url") == 0) {
        SDL_strlcpy(config->codex.url, value, sizeof(config->codex.url));
        return value[0] != '\0';
    }
    if (strcmp(key, "codex.relay.enabled") == 0) {
        return parse_bool(value, &config->codex_relay.enabled);
    }
    if (strcmp(key, "codex.relay.listen") == 0) {
        SDL_strlcpy(config->codex_relay.listen_url, value,
                    sizeof(config->codex_relay.listen_url));
        return value[0] != '\0';
    }
    if (strcmp(key, "codex.relay.executable") == 0) {
        SDL_strlcpy(config->codex_relay.executable, value,
                    sizeof(config->codex_relay.executable));
        return value[0] != '\0';
    }
    if (strcmp(key, "opencode.live.enabled") == 0) {
        return parse_bool(value, &config->opencode.enabled);
    }
    if (strcmp(key, "opencode.live.url") == 0) {
        SDL_strlcpy(config->opencode.url, value, sizeof(config->opencode.url));
        return value[0] != '\0';
    }
    if (strcmp(key, "chatgpt.live.enabled") == 0) {
        return parse_bool(value, &config->chatgpt.enabled);
    }
    if (strcmp(key, "chatgpt.live.url") == 0) {
        SDL_strlcpy(config->chatgpt.url, value, sizeof(config->chatgpt.url));
        return value[0] != '\0';
    }
    if (strcmp(key, "zcode.live.enabled") == 0) {
        return parse_bool(value, &config->zcode.enabled);
    }
    if (strcmp(key, "zcode.live.url") == 0) {
        SDL_strlcpy(config->zcode.url, value, sizeof(config->zcode.url));
        return value[0] != '\0';
    }
    if (strcmp(key, "legacy.codex_transcripts.enabled") == 0) {
        return parse_bool(value, &config->legacy_codex_transcripts);
    }
    if (strcmp(key, "legacy.hooks.enabled") == 0) {
        return parse_bool(value, &config->legacy_hooks);
    }
    return true;
}

bool eidolon_provider_config_parse(const char *text, size_t length, EidolonProviderConfig *config,
                                   char *error, size_t error_capacity) {
    if (text == NULL || config == NULL) {
        set_error(error, error_capacity, "provider configuration input is null");
        return false;
    }
    char *copy = SDL_malloc(length + 1U);
    if (copy == NULL) {
        set_error(error, error_capacity, "out of memory");
        return false;
    }
    SDL_memcpy(copy, text, length);
    copy[length] = '\0';
    EidolonProviderConfig candidate = *config;
    bool valid = true;
    bool version_seen = false;
    size_t line_number = 0U;
    char *cursor = copy;
    while (*cursor != '\0') {
        ++line_number;
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }
        line = trim(line);
        if (*line == '\0' || *line == '#') {
            continue;
        }
        char *separator = strchr(line, '=');
        if (separator == NULL) {
            set_error(error, error_capacity, "line %zu: expected key = value", line_number);
            valid = false;
            break;
        }
        *separator = '\0';
        const char *key = trim(line);
        const char *value = trim(separator + 1);
        if (strcmp(key, "version") == 0) {
            version_seen = strcmp(value, "1") == 0;
            if (!version_seen) {
                set_error(error, error_capacity, "line %zu: unsupported version", line_number);
                valid = false;
                break;
            }
        } else if (!assign_value(&candidate, key, value)) {
            set_error(error, error_capacity, "line %zu: invalid value for %s", line_number, key);
            valid = false;
            break;
        }
    }
    if (valid && !version_seen) {
        set_error(error, error_capacity, "missing provider configuration version");
        valid = false;
    }
    if (valid) {
        *config = candidate;
        if (error != NULL && error_capacity > 0U) {
            error[0] = '\0';
        }
    }
    SDL_free(copy);
    return valid;
}

bool eidolon_provider_config_load(const char *path, EidolonProviderConfig *config, char *error,
                                  size_t error_capacity) {
    size_t length = 0U;
    char *text = SDL_LoadFile(path, &length);
    if (text == NULL) {
        set_error(error, error_capacity, "could not load %s: %s", path, SDL_GetError());
        return false;
    }
    const bool valid = eidolon_provider_config_parse(text, length, config, error, error_capacity);
    SDL_free(text);
    return valid;
}
