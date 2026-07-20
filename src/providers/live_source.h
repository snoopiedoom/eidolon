#ifndef EIDOLON_LIVE_SOURCE_H
#define EIDOLON_LIVE_SOURCE_H

#include "conversation.h"

typedef struct EidolonLiveSource EidolonLiveSource;

EidolonLiveSource *eidolon_codex_live_source_start(const char *url,
                                                   EidolonConversationBus *bus);
EidolonLiveSource *eidolon_opencode_live_source_start(const char *url,
                                                      EidolonConversationBus *bus);
void eidolon_live_source_stop(EidolonLiveSource *source);

#endif
