#include "providers/opencode_stream.h"

#include "json_scan.h"

#include <SDL3/SDL.h>

#include <string.h>

static void initialize_event(EidolonConversationEvent *event,
                             EidolonConversationEventType type) {
    SDL_zero(*event);
    event->type = type;
    SDL_strlcpy(event->provider, "opencode", sizeof(event->provider));
}

static bool parse_text_delta(const char *data, EidolonConversationEvent *event) {
    initialize_event(event, EIDOLON_CONVERSATION_TEXT_DELTA);
    return eidolon_json_get_string(data, "sessionID", event->session_id,
                                   sizeof(event->session_id)) &&
           (eidolon_json_get_string(data, "assistantMessageID", event->message_id,
                                    sizeof(event->message_id)) ||
            eidolon_json_get_string(data, "messageID", event->message_id,
                                    sizeof(event->message_id))) &&
           eidolon_json_get_string(data, "delta", event->text, sizeof(event->text));
}

bool eidolon_opencode_stream_parse(const char *data, EidolonConversationEvent *event) {
    char type[64];
    if (data == NULL || event == NULL ||
        !eidolon_json_get_string(data, "type", type, sizeof(type))) {
        return false;
    }
    if (strcmp(type, "session.next.text.delta") == 0 ||
        strcmp(type, "message.part.delta") == 0) {
        return parse_text_delta(data, event);
    }
    if (strcmp(type, "session.next.text.ended") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_MESSAGE_COMPLETED);
        return eidolon_json_get_string(data, "sessionID", event->session_id,
                                       sizeof(event->session_id)) &&
               (eidolon_json_get_string(data, "assistantMessageID", event->message_id,
                                        sizeof(event->message_id)) ||
                eidolon_json_get_string(data, "messageID", event->message_id,
                                        sizeof(event->message_id))) &&
               eidolon_json_get_string(data, "text", event->text, sizeof(event->text));
    }
    if (strcmp(type, "session.created") == 0 || strcmp(type, "session.updated") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_SESSION_UPDATED);
        return (eidolon_json_get_string(data, "sessionID", event->session_id,
                                        sizeof(event->session_id)) ||
                eidolon_json_get_string_after(data, "\"info\"", "id", event->session_id,
                                              sizeof(event->session_id))) &&
               eidolon_json_get_string_after(data, "\"info\"", "title", event->title,
                                             sizeof(event->title));
    }
    if (strcmp(type, "session.status") == 0) {
        char status[24];
        if (!eidolon_json_get_string_after(data, "\"status\"", "type", status,
                                           sizeof(status))) {
            return false;
        }
        initialize_event(event, strcmp(status, "idle") == 0
                                    ? EIDOLON_CONVERSATION_TURN_COMPLETED
                                    : EIDOLON_CONVERSATION_TURN_STARTED);
        return eidolon_json_get_string(data, "sessionID", event->session_id,
                                       sizeof(event->session_id));
    }
    if (strcmp(type, "session.idle") == 0) {
        initialize_event(event, EIDOLON_CONVERSATION_TURN_COMPLETED);
        return eidolon_json_get_string(data, "sessionID", event->session_id,
                                       sizeof(event->session_id));
    }
    return false;
}
