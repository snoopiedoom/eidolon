#ifndef EIDOLON_CODEX_RELAY_H
#define EIDOLON_CODEX_RELAY_H

#include "conversation.h"

typedef struct EidolonCodexRelay EidolonCodexRelay;

EidolonCodexRelay *eidolon_codex_relay_start(const char *listen_url, const char *executable,
                                             EidolonConversationBus *bus);
void eidolon_codex_relay_stop(EidolonCodexRelay *relay);

#endif
