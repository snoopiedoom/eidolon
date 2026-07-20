#ifndef EIDOLON_CODEX_STREAM_H
#define EIDOLON_CODEX_STREAM_H

#include "conversation.h"

bool eidolon_codex_stream_parse(const char *frame, EidolonConversationEvent *event);

#endif
