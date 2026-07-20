#include "conversation_sources.h"

#include "log.h"
#include "provider_config.h"
#include "providers/codex_relay.h"
#include "providers/live_source.h"

#include <SDL3/SDL.h>

#include <string.h>

struct EidolonConversationSources {
    EidolonConversationBus *bus;
    EidolonLiveSource *live[EIDOLON_CONVERSATION_PROVIDER_COUNT];
    EidolonCodexRelay *codex_relay;
    EidolonProviderConfig config;
    EidolonProviderStatus providers[EIDOLON_CONVERSATION_PROVIDER_COUNT];
};

typedef EidolonLiveSource *(*ProviderStart)(const char *, EidolonConversationBus *);

typedef struct ProviderDefinition {
    const char *id;
    const char *name;
    uint32_t capabilities;
    const char *detail;
    ProviderStart start;
} ProviderDefinition;

static const ProviderDefinition provider_definitions[EIDOLON_CONVERSATION_PROVIDER_COUNT] = {
    {
        .id = "codex",
        .name = "Codex",
        .capabilities = EIDOLON_CONVERSATION_CAP_LIFECYCLE |
                        EIDOLON_CONVERSATION_CAP_TEXT_DELTA |
                        EIDOLON_CONVERSATION_CAP_COMPLETION |
                        EIDOLON_CONVERSATION_CAP_TITLES |
                        EIDOLON_CONVERSATION_CAP_MULTI_SESSION |
                        EIDOLON_CONVERSATION_CAP_HISTORY,
        .detail = "app-server WebSocket; peer observation unsupported",
        .start = eidolon_codex_live_source_start,
    },
    {
        .id = "opencode",
        .name = "OpenCode",
        .capabilities = EIDOLON_CONVERSATION_CAP_LIFECYCLE |
                        EIDOLON_CONVERSATION_CAP_TEXT_DELTA |
                        EIDOLON_CONVERSATION_CAP_COMPLETION |
                        EIDOLON_CONVERSATION_CAP_TITLES |
                        EIDOLON_CONVERSATION_CAP_MULTI_SESSION,
        .detail = "server-sent events",
        .start = eidolon_opencode_live_source_start,
    },
    {
        .id = "chatgpt",
        .name = "ChatGPT Desktop",
        .capabilities = 0U,
        .detail = "no documented local stream transport",
        .start = NULL,
    },
    {
        .id = "zcode",
        .name = "ZCode",
        .capabilities = 0U,
        .detail = "adapter reserved; stream protocol unavailable",
        .start = NULL,
    },
};

static const EidolonLiveProviderConfig *provider_config_at(const EidolonProviderConfig *config,
                                                           size_t index) {
    switch (index) {
    case 0U:
        return &config->codex;
    case 1U:
        return &config->opencode;
    case 2U:
        return &config->chatgpt;
    case 3U:
        return &config->zcode;
    default:
        return NULL;
    }
}

static void configure_statuses(EidolonConversationSources *sources) {
    for (size_t index = 0U; index < EIDOLON_CONVERSATION_PROVIDER_COUNT; ++index) {
        const ProviderDefinition *definition = &provider_definitions[index];
        const EidolonLiveProviderConfig *config = provider_config_at(&sources->config, index);
        EidolonProviderConnectionState state = EIDOLON_PROVIDER_DISABLED;
        const char *detail = definition->detail;
        if (index == 0U && sources->config.codex_relay.enabled) {
            state = EIDOLON_PROVIDER_CONNECTING;
            detail = "in-path stdio/WebSocket relay";
        } else if (config != NULL && config->enabled) {
            state = definition->start != NULL ? EIDOLON_PROVIDER_CONNECTING
                                              : EIDOLON_PROVIDER_UNAVAILABLE;
        } else if (index == 0U && sources->config.legacy_codex_transcripts) {
            state = EIDOLON_PROVIDER_LEGACY_ONLY;
            detail = "optional transcript fallback";
        }
        sources->providers[index] = (EidolonProviderStatus){
            .id = definition->id,
            .name = definition->name,
            .capabilities = definition->capabilities,
            .state = state,
            .detail = detail,
        };
    }
}

EidolonConversationSources *eidolon_conversation_sources_create(const char *config_path) {
    EidolonConversationSources *sources = SDL_calloc(1U, sizeof(*sources));
    if (sources == NULL) {
        return NULL;
    }
    eidolon_provider_config_defaults(&sources->config);
    char error[EIDOLON_PROVIDER_CONFIG_ERROR_CAPACITY];
    if (config_path != NULL &&
        !eidolon_provider_config_load(config_path, &sources->config, error, sizeof(error))) {
        eidolon_log_write("provider", "using built-in defaults: %s", error);
    }
    sources->bus = eidolon_conversation_bus_create();
    if (sources->bus == NULL) {
        SDL_free(sources);
        return NULL;
    }
    configure_statuses(sources);
    if (sources->config.codex_relay.enabled) {
        if (sources->config.codex.enabled) {
            eidolon_log_write("provider", "codex relay enabled; passive live client ignored");
        }
        sources->codex_relay = eidolon_codex_relay_start(
            sources->config.codex_relay.listen_url,
            sources->config.codex_relay.executable, sources->bus);
        if (sources->codex_relay == NULL) {
            sources->providers[0].state = EIDOLON_PROVIDER_UNAVAILABLE;
        }
    }
    for (size_t index = 0U; index < EIDOLON_CONVERSATION_PROVIDER_COUNT; ++index) {
        const ProviderDefinition *definition = &provider_definitions[index];
        const EidolonLiveProviderConfig *config = provider_config_at(&sources->config, index);
        if (config != NULL && config->enabled && definition->start != NULL &&
            !(index == 0U && sources->config.codex_relay.enabled)) {
            sources->live[index] = definition->start(config->url, sources->bus);
            if (sources->live[index] == NULL) {
                sources->providers[index].state = EIDOLON_PROVIDER_UNAVAILABLE;
            }
        }
    }
    return sources;
}

void eidolon_conversation_sources_destroy(EidolonConversationSources *sources) {
    if (sources == NULL) {
        return;
    }
    eidolon_codex_relay_stop(sources->codex_relay);
    for (size_t index = 0U; index < EIDOLON_CONVERSATION_PROVIDER_COUNT; ++index) {
        eidolon_live_source_stop(sources->live[index]);
    }
    eidolon_conversation_bus_destroy(sources->bus);
    SDL_free(sources);
}

bool eidolon_conversation_sources_poll(EidolonConversationSources *sources,
                                       EidolonConversationEvent *event) {
    if (sources == NULL || !eidolon_conversation_bus_poll(sources->bus, event)) {
        return false;
    }
    if (event->type == EIDOLON_CONVERSATION_SOURCE_CONNECTED ||
        event->type == EIDOLON_CONVERSATION_SOURCE_DISCONNECTED) {
        for (size_t index = 0U; index < EIDOLON_CONVERSATION_PROVIDER_COUNT; ++index) {
            if (strcmp(sources->providers[index].id, event->provider) == 0) {
                sources->providers[index].state =
                    event->type == EIDOLON_CONVERSATION_SOURCE_CONNECTED
                        ? EIDOLON_PROVIDER_CONNECTED
                        : EIDOLON_PROVIDER_CONNECTING;
                break;
            }
        }
    }
    return true;
}

bool eidolon_conversation_sources_legacy_transcripts(
    const EidolonConversationSources *sources) {
    return sources != NULL && sources->config.legacy_codex_transcripts;
}

bool eidolon_conversation_sources_legacy_hooks(const EidolonConversationSources *sources) {
    return sources != NULL && sources->config.legacy_hooks;
}

const EidolonProviderStatus *eidolon_conversation_sources_status(
    const EidolonConversationSources *sources, size_t index) {
    return sources != NULL && index < EIDOLON_CONVERSATION_PROVIDER_COUNT
               ? &sources->providers[index]
               : NULL;
}
