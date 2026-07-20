#ifndef EIDOLON_PROVIDER_CONFIG_H
#define EIDOLON_PROVIDER_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define EIDOLON_PROVIDER_URL_CAPACITY 512U
#define EIDOLON_PROVIDER_CONFIG_ERROR_CAPACITY 256U

typedef struct EidolonLiveProviderConfig {
    bool enabled;
    char url[EIDOLON_PROVIDER_URL_CAPACITY];
} EidolonLiveProviderConfig;

typedef struct EidolonCodexRelayConfig {
    bool enabled;
    char listen_url[EIDOLON_PROVIDER_URL_CAPACITY];
    char executable[EIDOLON_PROVIDER_URL_CAPACITY];
} EidolonCodexRelayConfig;

typedef struct EidolonProviderConfig {
    EidolonLiveProviderConfig codex;
    EidolonCodexRelayConfig codex_relay;
    EidolonLiveProviderConfig opencode;
    EidolonLiveProviderConfig chatgpt;
    EidolonLiveProviderConfig zcode;
    bool legacy_codex_transcripts;
    bool legacy_hooks;
} EidolonProviderConfig;

void eidolon_provider_config_defaults(EidolonProviderConfig *config);
bool eidolon_provider_config_parse(const char *text, size_t length, EidolonProviderConfig *config,
                                   char *error, size_t error_capacity);
bool eidolon_provider_config_load(const char *path, EidolonProviderConfig *config, char *error,
                                  size_t error_capacity);

#endif
