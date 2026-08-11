#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_protocol.h"

static int parse(const char *json, buddy_event_t *event)
{
    return buddy_protocol_parse(json, strlen(json), event);
}

static void test_heartbeat_maps_documented_fields(void)
{
    buddy_event_t event = {0};
    const char *json =
        "{\"cmd\":\"heartbeat\",\"time\":1234,\"owner\":\"Claude\","
        "\"name\":\"Buddy\",\"status\":\"Working\",\"running\":2,"
        "\"tokens\":50001,\"entries\":[\"one\",\"two\"]}";

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.type == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.connected);
    assert(strcmp(event.heartbeat.owner, "Claude") == 0);
    assert(strcmp(event.heartbeat.name, "Buddy") == 0);
    assert(strcmp(event.heartbeat.message, "Working") == 0);
    assert(event.heartbeat.running == 2);
    assert(event.heartbeat.tokens == 50001);
    assert(strcmp(event.heartbeat.entries[0], "one") == 0);
    assert(strcmp(event.heartbeat.entries[1], "two") == 0);
}

static void test_heartbeat_optional_prompt_maps_prompt_event(void)
{
    buddy_event_t event = {0};
    const char *json =
        "{\"cmd\":\"heartbeat\",\"running\":1,\"prompt\":{\"id\":\"req_abc123\","
        "\"tool\":\"Bash\",\"hint\":\"git status\"}}";

    assert(parse(json, &event) == BUDDY_EVENT_PROMPT);
    assert(event.type == BUDDY_EVENT_PROMPT);
    assert(event.prompt.connected);
    assert(event.prompt.running == 1);
    assert(event.prompt.id_length == strlen("req_abc123"));
    assert(!event.prompt.id_truncated);
    assert(strcmp(event.prompt.id, "req_abc123") == 0);
    assert(strcmp(event.prompt.tool, "Bash") == 0);
    assert(strcmp(event.prompt.hint, "git status") == 0);
}

static void test_unpair_maps_offline_heartbeat(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"unpair\"}", &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.type == BUDDY_EVENT_HEARTBEAT);
    assert(!event.heartbeat.connected);
}

static void test_folder_push_command_is_unsupported(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"char_begin\"}", &event) == BUDDY_EVENT_UNSUPPORTED_COMMAND);
    assert(event.type == BUDDY_EVENT_NONE);
}

static void test_unknown_command_is_rejected(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"mystery\"}", &event) == BUDDY_EVENT_UNKNOWN_COMMAND);
    assert(event.type == BUDDY_EVENT_NONE);
}

static void test_malformed_or_nonobject_json_is_rejected(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":", &event) == BUDDY_EVENT_MALFORMED);
    assert(parse("[]", &event) == BUDDY_EVENT_MALFORMED);
}

static void test_oversized_prompt_id_preserves_the_safety_contract(void)
{
    buddy_event_t event = {0};
    char id[BUDDY_PROMPT_ID_MAX + 1];
    char json[BUDDY_PROMPT_ID_MAX + 64];

    memset(id, 'x', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    snprintf(json, sizeof(json), "{\"cmd\":\"prompt\",\"id\":\"%s\"}", id);

    assert(parse(json, &event) == BUDDY_EVENT_PROMPT);
    assert(event.prompt.id_length == sizeof(id) - 1);
    assert(event.prompt.id_truncated);
    assert(event.prompt.id[sizeof(event.prompt.id) - 1] == '\0');
}

static void test_prompt_id_must_be_a_nonempty_string(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"prompt\"}", &event) == BUDDY_EVENT_MALFORMED);
    assert(parse("{\"cmd\":\"prompt\",\"id\":\"\"}", &event) == BUDDY_EVENT_MALFORMED);
    assert(parse("{\"cmd\":\"prompt\",\"id\":7}", &event) == BUDDY_EVENT_MALFORMED);
}

static void test_prompt_id_rejects_an_embedded_nul(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"prompt\",\"id\":\"req\\u0000other\"}", &event) ==
           BUDDY_EVENT_MALFORMED);
}

static void test_parser_rejects_trailing_bytes(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"unpair\"} trailing", &event) == BUDDY_EVENT_MALFORMED);
}

static void test_heartbeat_rejects_nonintegral_counters(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"heartbeat\",\"running\":1.5}", &event) ==
           BUDDY_EVENT_MALFORMED);
    assert(parse("{\"cmd\":\"heartbeat\",\"tokens\":1.5}", &event) ==
           BUDDY_EVENT_MALFORMED);
}

static void test_display_strings_are_bounded(void)
{
    buddy_event_t event = {0};
    char status[BUDDY_MESSAGE_MAX + 32];
    char json[BUDDY_MESSAGE_MAX + 80];

    memset(status, 's', sizeof(status) - 1);
    status[sizeof(status) - 1] = '\0';
    snprintf(json, sizeof(json), "{\"cmd\":\"heartbeat\",\"status\":\"%s\"}", status);

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.message[sizeof(event.heartbeat.message) - 1] == '\0');
    assert(strlen(event.heartbeat.message) == sizeof(event.heartbeat.message) - 1);
}

static void test_serializers_write_documented_json(void)
{
    char json[192];
    buddy_heartbeat_t heartbeat = {
        .connected = true,
        .running = 3,
        .tokens = 42,
    };

    snprintf(heartbeat.name, sizeof(heartbeat.name), "%s", "Buddy");
    snprintf(heartbeat.owner, sizeof(heartbeat.owner), "%s", "Claude");
    snprintf(heartbeat.message, sizeof(heartbeat.message), "%s", "Ready");
    assert(buddy_protocol_permission_json(json, sizeof(json), "req_abc123",
                                          BUDDY_PERMISSION_ONCE) > 0);
    assert(strcmp(json,
                  "{\"cmd\":\"permission\",\"id\":\"req_abc123\",\"decision\":\"once\"}\n") ==
           0);
    assert(buddy_protocol_ack_json(json, sizeof(json), false) > 0);
    assert(strcmp(json, "{\"cmd\":\"ack\",\"ok\":false}\n") == 0);
    assert(buddy_protocol_status_json(json, sizeof(json), &heartbeat) > 0);
    assert(strcmp(json,
                  "{\"cmd\":\"status\",\"name\":\"Buddy\",\"owner\":\"Claude\","
                  "\"status\":\"Ready\",\"running\":3,\"tokens\":42}\n") == 0);
}

static void test_serializers_fail_without_writing_past_the_output_bound(void)
{
    struct {
        char json[8];
        char guard;
    } output;

    memset(&output, 'x', sizeof(output));
    output.guard = 'g';
    assert(buddy_protocol_permission_json(output.json, sizeof(output.json), "req_abc123",
                                          BUDDY_PERMISSION_ONCE) == 0);
    assert(output.json[sizeof(output.json) - 1] == '\0');
    assert(output.guard == 'g');
}

int main(void)
{
    test_heartbeat_maps_documented_fields();
    test_heartbeat_optional_prompt_maps_prompt_event();
    test_unpair_maps_offline_heartbeat();
    test_folder_push_command_is_unsupported();
    test_unknown_command_is_rejected();
    test_malformed_or_nonobject_json_is_rejected();
    test_oversized_prompt_id_preserves_the_safety_contract();
    test_prompt_id_must_be_a_nonempty_string();
    test_prompt_id_rejects_an_embedded_nul();
    test_parser_rejects_trailing_bytes();
    test_heartbeat_rejects_nonintegral_counters();
    test_display_strings_are_bounded();
    test_serializers_write_documented_json();
    test_serializers_fail_without_writing_past_the_output_bound();
    return 0;
}
