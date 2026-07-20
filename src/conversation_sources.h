#ifndef EIDOLON_CONVERSATION_SOURCES_H
#define EIDOLON_CONVERSATION_SOURCES_H

#include "conversation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIDOLON_CONVERSATION_PROVIDER_COUNT 4U

typedef enum EidolonProviderConnectionState {
    EIDOLON_PROVIDER_DISABLED,
    EIDOLON_PROVIDER_CONNECTING,
    EIDOLON_PROVIDER_CONNECTED,
    EIDOLON_PROVIDER_LEGACY_ONLY,
    EIDOLON_PROVIDER_UNAVAILABLE,
} EidolonProviderConnectionState;

typedef struct EidolonProviderStatus {
    const char *id;
    const char *name;
    uint32_t capabilities;
    EidolonProviderConnectionState state;
    const char *detail;
} EidolonProviderStatus;

typedef struct EidolonConversationSources EidolonConversationSources;

EidolonConversationSources *eidolon_conversation_sources_create(const char *config_path);
void eidolon_conversation_sources_destroy(EidolonConversationSources *sources);
bool eidolon_conversation_sources_poll(EidolonConversationSources *sources,
                                       EidolonConversationEvent *event);
bool eidolon_conversation_sources_legacy_transcripts(
    const EidolonConversationSources *sources);
bool eidolon_conversation_sources_legacy_hooks(const EidolonConversationSources *sources);
const EidolonProviderStatus *eidolon_conversation_sources_status(
    const EidolonConversationSources *sources, size_t index);

#endif
