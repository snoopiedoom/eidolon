#include "providers/codex_stream.h"

#include "json_scan.h"

#include <SDL3/SDL.h>

#include <string.h>

static void initialize_event(EidolonConversationEvent *event,
                             EidolonConversationEventType type) {
    SDL_zero(*event);
    event->type = type;
    SDL_strlcpy(event->provider, "codex", sizeof(event->provider));
}

static EidolonConversationPhase parse_phase(const char *frame) {
    char phase[24];
    if (!eidolon_json_get_string_after(frame, "\"item\"", "phase", phase, sizeof(phase))) {
        return EIDOLON_CONVERSATION_PHASE_UNKNOWN;
    }
    if (strcmp(phase, "commentary") == 0) {
        return EIDOLON_CONVERSATION_PHASE_COMMENTARY;
    }
    if (strcmp(phase, "final_answer") == 0) {
        return EIDOLON_CONVERSATION_PHASE_FINAL;
    }
    return EIDOLON_CONVERSATION_PHASE_UNKNOWN;
}

bool eidolon_codex_stream_parse(const char *frame, EidolonConversationEvent *event) {
    char method[64];
    if (frame == NULL || event == NULL ||
        !eidolon_json_get_string(frame, "method", method, sizeof(method))) {
        return false;
    }
    if (strcmp(method, "item/agentMessage/delta") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_TEXT_DELTA);
        return eidolon_json_get_string(frame, "threadId", event->session_id,
                                       sizeof(event->session_id)) &&
               eidolon_json_get_string(frame, "turnId", event->turn_id,
                                       sizeof(event->turn_id)) &&
               eidolon_json_get_string(frame, "itemId", event->message_id,
                                       sizeof(event->message_id)) &&
               eidolon_json_get_string(frame, "delta", event->text, sizeof(event->text));
    }
    if (strcmp(method, "turn/started") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_TURN_STARTED);
        return eidolon_json_get_string(frame, "threadId", event->session_id,
                                       sizeof(event->session_id)) &&
               eidolon_json_get_string_after(frame, "\"turn\"", "id", event->turn_id,
                                             sizeof(event->turn_id));
    }
    if (strcmp(method, "turn/completed") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_TURN_COMPLETED);
        return eidolon_json_get_string(frame, "threadId", event->session_id,
                                       sizeof(event->session_id)) &&
               eidolon_json_get_string_after(frame, "\"turn\"", "id", event->turn_id,
                                             sizeof(event->turn_id));
    }
    char item_type[32];
    if (strcmp(method, "item/completed") == 0 &&
        eidolon_json_get_string_after(frame, "\"item\"", "type", item_type,
                                      sizeof(item_type)) &&
        strcmp(item_type, "agentMessage") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_MESSAGE_COMPLETED);
        event->phase = parse_phase(frame);
        return eidolon_json_get_string(frame, "threadId", event->session_id,
                                       sizeof(event->session_id)) &&
               eidolon_json_get_string(frame, "turnId", event->turn_id,
                                       sizeof(event->turn_id)) &&
               eidolon_json_get_string_after(frame, "\"item\"", "id", event->message_id,
                                             sizeof(event->message_id)) &&
               eidolon_json_get_string_after(frame, "\"item\"", "text", event->text,
                                             sizeof(event->text));
    }
    if (strcmp(method, "thread/name/updated") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_SESSION_UPDATED);
        return eidolon_json_get_string(frame, "threadId", event->session_id,
                                       sizeof(event->session_id)) &&
               eidolon_json_get_string(frame, "threadName", event->title,
                                       sizeof(event->title));
    }
    return false;
}
