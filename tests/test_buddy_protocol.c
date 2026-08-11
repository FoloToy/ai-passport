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

static void test_heartbeat_optional_prompt_stays_in_heartbeat_snapshot(void)
{
    buddy_event_t event = {0};
    const char *json =
        "{\"cmd\":\"heartbeat\",\"running\":1,\"prompt\":{\"id\":\"req_abc123\","
        "\"tool\":\"Bash\",\"hint\":\"git status\"}}";

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.type == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.prompt.connected);
    assert(event.heartbeat.prompt.running == 1);
    assert(event.heartbeat.prompt.id_length == strlen("req_abc123"));
    assert(!event.heartbeat.prompt.id_truncated);
    assert(strcmp(event.heartbeat.prompt.id, "req_abc123") == 0);
    assert(strcmp(event.heartbeat.prompt.tool, "Bash") == 0);
    assert(strcmp(event.heartbeat.prompt.hint, "git status") == 0);
}

static void test_unpair_maps_confirmation_event(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"unpair\"}", &event) == BUDDY_EVENT_UNPAIR_CONFIRMATION);
    assert(event.type == BUDDY_EVENT_UNPAIR_CONFIRMATION);
}

static void test_file_transfer_commands_are_unsupported(void)
{
    static const char *const commands[] = {
        "char_begin", "file", "chunk", "file_end", "char_end", "folder_begin",
    };
    buddy_event_t event = {0};
    size_t index;

    for (index = 0; index < sizeof(commands) / sizeof(commands[0]); ++index) {
        char json[64];

        snprintf(json, sizeof(json), "{\"cmd\":\"%s\"}", commands[index]);
        assert(parse(json, &event) == BUDDY_EVENT_UNSUPPORTED_COMMAND);
        assert(event.type == BUDDY_EVENT_NONE);
    }
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

static void test_oversized_prompt_id_is_rejected(void)
{
    buddy_event_t event = {0};
    char id[BUDDY_PROMPT_ID_MAX + 1];
    char json[BUDDY_PROMPT_ID_MAX + 64];

    memset(id, 'x', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    snprintf(json, sizeof(json), "{\"cmd\":\"prompt\",\"id\":\"%s\"}", id);

    assert(parse(json, &event) == BUDDY_EVENT_MALFORMED);
    assert(event.type == BUDDY_EVENT_NONE);
}

static void test_oversized_nested_prompt_id_is_rejected(void)
{
    buddy_event_t event = {0};
    char id[BUDDY_PROMPT_ID_MAX + 1];
    char json[BUDDY_PROMPT_ID_MAX + 96];

    memset(id, 'x', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    snprintf(json, sizeof(json), "{\"cmd\":\"heartbeat\",\"prompt\":{\"id\":\"%s\"}}", id);

    assert(parse(json, &event) == BUDDY_EVENT_MALFORMED);
    assert(event.type == BUDDY_EVENT_NONE);
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
    char status[BUDDY_MESSAGE_MAX + 4];
    char json[BUDDY_MESSAGE_MAX + 80];

    memset(status, 's', BUDDY_MESSAGE_MAX - 2);
    memcpy(status + BUDDY_MESSAGE_MAX - 2, "\xe4\xb8\xad", 3);
    status[BUDDY_MESSAGE_MAX + 1] = '\0';
    snprintf(json, sizeof(json), "{\"cmd\":\"heartbeat\",\"status\":\"%s\"}", status);

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.message[sizeof(event.heartbeat.message) - 1] == '\0');
    assert(event.heartbeat.message_truncated);
    assert(strlen(event.heartbeat.message) == BUDDY_MESSAGE_MAX - 2);
    assert(event.heartbeat.message[BUDDY_MESSAGE_MAX - 2] == '\0');
}

static void test_documented_command_events_have_bounded_payloads(void)
{
    struct command_case {
        const char *json;
        buddy_event_type_t type;
        const char *value;
    } cases[] = {
        {"{\"cmd\":\"time\",\"time\":\"12:34\"}", BUDDY_EVENT_TIME, "12:34"},
        {"{\"cmd\":\"name\",\"name\":\"Buddy\"}", BUDDY_EVENT_NAME, "Buddy"},
        {"{\"cmd\":\"owner\",\"owner\":\"Claude\"}", BUDDY_EVENT_OWNER, "Claude"},
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        buddy_event_t event = {0};

        assert(parse(cases[index].json, &event) == (int)cases[index].type);
        assert(event.type == cases[index].type);
        assert(strcmp(event.command.value, cases[index].value) == 0);
        assert(!event.command.value_truncated);
    }
}

static void test_status_is_a_no_payload_request(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"status\"}", &event) == BUDDY_EVENT_STATUS_REQUEST);
    assert(event.type == BUDDY_EVENT_STATUS_REQUEST);
    assert(event.command.value[0] == '\0');
    assert(parse("{\"cmd\":\"status\",\"status\":\"Ready\"}", &event) == BUDDY_EVENT_MALFORMED);
}

static void test_name_command_truncates_at_a_utf8_codepoint_boundary(void)
{
    buddy_event_t event = {0};
    char name[BUDDY_NAME_MAX + 4];
    char json[BUDDY_NAME_MAX + 40];

    memset(name, 'n', BUDDY_NAME_MAX - 2);
    memcpy(name + BUDDY_NAME_MAX - 2, "\xe4\xb8\xad", 3);
    name[BUDDY_NAME_MAX + 1] = '\0';
    snprintf(json, sizeof(json), "{\"cmd\":\"name\",\"name\":\"%s\"}", name);

    assert(parse(json, &event) == BUDDY_EVENT_NAME);
    assert(event.command.value_truncated);
    assert(strlen(event.command.value) == BUDDY_NAME_MAX - 2);
    assert(event.command.value[BUDDY_NAME_MAX - 2] == '\0');
}

static void test_parser_rejects_oversized_or_invalid_utf8_lines(void)
{
    buddy_event_t event = {0};
    char oversized[BUDDY_JSON_LINE_MAX + 1];
    char invalid_utf8[] = "{\"cmd\":\"name\",\"name\":\"\xc0\xaf\"}";

    memset(oversized, 'x', sizeof(oversized));
    assert(buddy_protocol_parse(oversized, sizeof(oversized), &event) == BUDDY_EVENT_MALFORMED);
    assert(parse(invalid_utf8, &event) == BUDDY_EVENT_MALFORMED);
}

static void test_parser_rejects_raw_nul_and_control_bytes(void)
{
    buddy_event_t event = {0};
    static const char raw_nul[] = {
        '{', '"', 'c', 'm', 'd', '"', ':', '"', 'p', 'r', 'o', 'm', 'p', 't', '"', ',',
        '"', 'i', 'd', '"', ':', '"', 'r', 'e', 'q', '\0', 's', 'u', 'f', 'f', 'i', 'x',
        '"', '}',
    };
    static const char string_control[] = {
        '{', '"', 'c', 'm', 'd', '"', ':', '"', 'n', 'a', 'm', 'e', '"', ',',
        '"', 'n', 'a', 'm', 'e', '"', ':', '"', 'a', '\x1f', 'b', '"', '}',
    };
    static const char structural_control[] = {
        '{', '\x01', '"', 'c', 'm', 'd', '"', ':', '"', 's', 't', 'a', 't', 'u', 's', '"', '}',
    };

    assert(buddy_protocol_parse(raw_nul, sizeof(raw_nul), &event) == BUDDY_EVENT_MALFORMED);
    assert(buddy_protocol_parse(string_control, sizeof(string_control), &event) ==
           BUDDY_EVENT_MALFORMED);
    assert(buddy_protocol_parse(structural_control, sizeof(structural_control), &event) ==
           BUDDY_EVENT_MALFORMED);
    assert(parse("{\n\"cmd\":\"status\"\n}", &event) == BUDDY_EVENT_STATUS_REQUEST);
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

static void test_serializers_reject_invalid_or_unterminated_inputs(void)
{
    char json[192];
    char id[BUDDY_PROMPT_ID_MAX];
    buddy_heartbeat_t heartbeat = {0};

    memset(id, 'x', sizeof(id));
    assert(buddy_protocol_permission_json(json, sizeof(json), id, BUDDY_PERMISSION_ONCE) == 0);
    assert(buddy_protocol_permission_json(json, sizeof(json), "\xc0\xaf", BUDDY_PERMISSION_ONCE) == 0);
    memset(heartbeat.name, 'x', sizeof(heartbeat.name));
    assert(buddy_protocol_status_json(json, sizeof(json), &heartbeat) == 0);
}

int main(void)
{
    test_heartbeat_maps_documented_fields();
    test_heartbeat_optional_prompt_stays_in_heartbeat_snapshot();
    test_unpair_maps_confirmation_event();
    test_file_transfer_commands_are_unsupported();
    test_unknown_command_is_rejected();
    test_malformed_or_nonobject_json_is_rejected();
    test_oversized_prompt_id_is_rejected();
    test_oversized_nested_prompt_id_is_rejected();
    test_prompt_id_must_be_a_nonempty_string();
    test_prompt_id_rejects_an_embedded_nul();
    test_parser_rejects_trailing_bytes();
    test_heartbeat_rejects_nonintegral_counters();
    test_display_strings_are_bounded();
    test_documented_command_events_have_bounded_payloads();
    test_status_is_a_no_payload_request();
    test_name_command_truncates_at_a_utf8_codepoint_boundary();
    test_parser_rejects_oversized_or_invalid_utf8_lines();
    test_parser_rejects_raw_nul_and_control_bytes();
    test_serializers_write_documented_json();
    test_serializers_fail_without_writing_past_the_output_bound();
    test_serializers_reject_invalid_or_unterminated_inputs();
    return 0;
}
