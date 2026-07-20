#include "conversation.h"
#include "json_scan.h"
#include "provider_config.h"
#include "providers/codex_stream.h"
#include "providers/opencode_stream.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <string.h>

static void test_json_unicode(void) {
    char text[64];
    assert(
        eidolon_json_get_string("{\"text\":\"živjo \\ud83d\\udc96\"}", "text", text, sizeof(text)));
    assert(strcmp(text, "živjo 💖") == 0);
    int64_t identifier = -1;
    assert(eidolon_json_get_integer("{\"id\": 42}", "id", &identifier));
    assert(identifier == 42);
}

static void test_codex(void) {
    const char *delta =
        "{\"method\":\"item/agentMessage/delta\",\"params\":{\"threadId\":\"thread-1\","
        "\"turnId\":\"turn-1\",\"itemId\":\"item-1\",\"delta\":\"hello \\ud83d\\udc96\"}}";
    EidolonConversationEvent event;
    assert(eidolon_codex_stream_parse(delta, &event));
    assert(event.type == EIDOLON_CONVERSATION_TEXT_DELTA);
    assert(strcmp(event.provider, "codex") == 0);
    assert(strcmp(event.session_id, "thread-1") == 0);
    assert(strcmp(event.turn_id, "turn-1") == 0);
    assert(strcmp(event.message_id, "item-1") == 0);
    assert(strcmp(event.text, "hello 💖") == 0);

    assert(eidolon_codex_stream_parse(
        "{\"method\":\"turn/completed\",\"params\":{\"threadId\":\"thread-1\","
        "\"turn\":{\"id\":\"turn-1\",\"status\":\"completed\"}}}",
        &event));
    assert(event.type == EIDOLON_CONVERSATION_TURN_COMPLETED);
    assert(strcmp(event.turn_id, "turn-1") == 0);

    assert(eidolon_codex_stream_parse(
        "{\"method\":\"item/completed\",\"params\":{\"threadId\":\"thread-1\","
        "\"turnId\":\"turn-1\",\"item\":{\"id\":\"item-1\","
        "\"type\":\"agentMessage\",\"text\":\"complete\","
        "\"phase\":\"final_answer\"}}}",
        &event));
    assert(event.type == EIDOLON_CONVERSATION_MESSAGE_COMPLETED);
    assert(event.phase == EIDOLON_CONVERSATION_PHASE_FINAL);
    assert(strcmp(event.text, "complete") == 0);
}

static void test_opencode(void) {
    EidolonConversationEvent event;
    assert(eidolon_opencode_stream_parse("{\"type\":\"session.next.text.delta\",\"properties\":{"
                                         "\"sessionID\":\"ses_1\",\"assistantMessageID\":\"msg_1\","
                                         "\"textID\":\"txt_1\",\"delta\":\"streaming\"}}",
                                         &event));
    assert(event.type == EIDOLON_CONVERSATION_TEXT_DELTA);
    assert(strcmp(event.provider, "opencode") == 0);
    assert(strcmp(event.session_id, "ses_1") == 0);
    assert(strcmp(event.message_id, "msg_1") == 0);
    assert(strcmp(event.text, "streaming") == 0);

    assert(eidolon_opencode_stream_parse(
        "{\"type\":\"session.status\",\"properties\":{\"sessionID\":\"ses_1\","
        "\"status\":{\"type\":\"idle\"}}}",
        &event));
    assert(event.type == EIDOLON_CONVERSATION_TURN_COMPLETED);

    assert(eidolon_opencode_stream_parse("{\"type\":\"session.created\",\"properties\":{\"info\":{"
                                         "\"id\":\"ses_2\",\"title\":\"portable session\"}}}",
                                         &event));
    assert(event.type == EIDOLON_CONVERSATION_SESSION_UPDATED);
    assert(strcmp(event.session_id, "ses_2") == 0);
    assert(strcmp(event.title, "portable session") == 0);
}

static void test_bus(void) {
    EidolonConversationBus *bus = eidolon_conversation_bus_create();
    assert(bus != NULL);
    EidolonConversationEvent input = {.type = EIDOLON_CONVERSATION_TEXT_DELTA};
    SDL_strlcpy(input.provider, "test", sizeof(input.provider));
    SDL_strlcpy(input.text, "one", sizeof(input.text));
    assert(eidolon_conversation_bus_push(bus, &input));
    EidolonConversationEvent output;
    assert(eidolon_conversation_bus_poll(bus, &output));
    assert(strcmp(output.text, "one") == 0);
    assert(!eidolon_conversation_bus_poll(bus, &output));
    assert(eidolon_conversation_bus_dropped(bus) == 0U);
    eidolon_conversation_bus_destroy(bus);
}

static void test_provider_config(void) {
    static const char config_text[] = "version = 1\n"
                                      "codex.live.enabled = true\n"
                                      "codex.live.url = ws://localhost:5555\n"
                                      "codex.relay.enabled = true\n"
                                      "codex.relay.listen = ws://127.0.0.1:4555\n"
                                      "opencode.live.enabled = true\n"
                                      "legacy.codex_transcripts.enabled = false\n"
                                      "future.provider.option = ignored\n";
    EidolonProviderConfig config;
    eidolon_provider_config_defaults(&config);
    char error[EIDOLON_PROVIDER_CONFIG_ERROR_CAPACITY];
    assert(eidolon_provider_config_parse(config_text, strlen(config_text), &config, error,
                                         sizeof(error)));
    assert(config.codex.enabled);
    assert(strcmp(config.codex.url, "ws://localhost:5555") == 0);
    assert(config.codex_relay.enabled);
    assert(strcmp(config.codex_relay.listen_url, "ws://127.0.0.1:4555") == 0);
    assert(config.opencode.enabled);
    assert(!config.legacy_codex_transcripts);
    assert(config.legacy_hooks);

    static const char invalid[] = "version = 1\ncodex.live.enabled = perhaps\n";
    assert(!eidolon_provider_config_parse(invalid, strlen(invalid), &config, error, sizeof(error)));
}

int main(void) {
    assert(SDL_Init(0));
    test_json_unicode();
    test_codex();
    test_opencode();
    test_bus();
    test_provider_config();
    SDL_Quit();
    return 0;
}
