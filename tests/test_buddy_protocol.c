#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_protocol.h"
#include "official_reference_fixtures.h"

static int parse(const char *json, buddy_event_t *event)
{
    return buddy_protocol_parse(json, strlen(json), event);
}

static void test_official_heartbeat_maps_documented_fields(void)
{
    buddy_event_t event = {0};
    const char *json = OFFICIAL_HEARTBEAT_JSON;

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.type == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.connected);
    assert(event.heartbeat.total == 3);
    assert(event.heartbeat.running == 1);
    assert(event.heartbeat.waiting == 1);
    assert(strcmp(event.heartbeat.message, "approve: Bash") == 0);
    assert(event.heartbeat.tokens == 184502);
    assert(event.heartbeat.tokens_today == 31200);
    assert(strcmp(event.heartbeat.entries[0], "10:42 git push") == 0);
    assert(strcmp(event.heartbeat.entries[1], "10:41 yarn test") == 0);
    assert(strcmp(event.heartbeat.prompt.id, "req_abc123") == 0);
}

static void test_heartbeat_optional_prompt_stays_in_heartbeat_snapshot(void)
{
    buddy_event_t event = {0};
    const char *json = OFFICIAL_HEARTBEAT_JSON;

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.type == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.prompt.connected);
    assert(event.heartbeat.prompt.running == 1);
    assert(event.heartbeat.prompt.id_length == strlen("req_abc123"));
    assert(!event.heartbeat.prompt.id_truncated);
    assert(strcmp(event.heartbeat.prompt.id, "req_abc123") == 0);
    assert(strcmp(event.heartbeat.prompt.tool, "Bash") == 0);
    assert(strcmp(event.heartbeat.prompt.hint, "rm -rf /tmp/foo") == 0);
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
    char json[BUDDY_JSON_LINE_MAX];

    memset(id, 'x', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    snprintf(json, sizeof(json),
             "{\"total\":1,\"running\":0,\"waiting\":1,\"msg\":\"approve\","
             "\"entries\":[],\"tokens\":0,\"tokens_today\":0,"
             "\"prompt\":{\"id\":\"%s\"}}", id);

    assert(parse(json, &event) == BUDDY_EVENT_MALFORMED);
    assert(event.type == BUDDY_EVENT_NONE);
}

static void test_oversized_nested_prompt_id_is_rejected(void)
{
    buddy_event_t event = {0};
    char id[BUDDY_PROMPT_ID_MAX + 1];
    char json[BUDDY_JSON_LINE_MAX];

    memset(id, 'x', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    snprintf(json, sizeof(json),
             "{\"total\":1,\"running\":0,\"waiting\":1,\"msg\":\"approve\","
             "\"entries\":[],\"tokens\":0,\"tokens_today\":0,"
             "\"prompt\":{\"id\":\"%s\"}}", id);

    assert(parse(json, &event) == BUDDY_EVENT_MALFORMED);
    assert(event.type == BUDDY_EVENT_NONE);
}

static void test_prompt_id_must_be_a_nonempty_string(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"total\":1,\"running\":0,\"waiting\":1,\"msg\":\"x\","
                 "\"entries\":[],\"tokens\":0,\"tokens_today\":0,\"prompt\":{}}",
                 &event) == BUDDY_EVENT_MALFORMED);
    assert(parse("{\"total\":1,\"running\":0,\"waiting\":1,\"msg\":\"x\","
                 "\"entries\":[],\"tokens\":0,\"tokens_today\":0,"
                 "\"prompt\":{\"id\":\"\"}}", &event) == BUDDY_EVENT_MALFORMED);
    assert(parse("{\"total\":1,\"running\":0,\"waiting\":1,\"msg\":\"x\","
                 "\"entries\":[],\"tokens\":0,\"tokens_today\":0,"
                 "\"prompt\":{\"id\":7}}", &event) == BUDDY_EVENT_MALFORMED);
}

static void test_prompt_id_rejects_an_embedded_nul(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"total\":1,\"running\":0,\"waiting\":1,\"msg\":\"x\","
                 "\"entries\":[],\"tokens\":0,\"tokens_today\":0,"
                 "\"prompt\":{\"id\":\"req\\u0000other\"}}", &event) ==
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

    assert(parse("{\"total\":1,\"running\":1.5,\"waiting\":0,\"msg\":\"x\","
                 "\"entries\":[],\"tokens\":0,\"tokens_today\":0}", &event) ==
           BUDDY_EVENT_MALFORMED);
    assert(parse("{\"total\":1,\"running\":0,\"waiting\":0,\"msg\":\"x\","
                 "\"entries\":[],\"tokens\":1.5,\"tokens_today\":0}", &event) ==
           BUDDY_EVENT_MALFORMED);
}

static void test_display_strings_are_bounded(void)
{
    buddy_event_t event = {0};
    char status[BUDDY_MESSAGE_MAX + 4];
    char json[BUDDY_JSON_LINE_MAX];

    memset(status, 's', BUDDY_MESSAGE_MAX - 2);
    memcpy(status + BUDDY_MESSAGE_MAX - 2, "\xe4\xb8\xad", 3);
    status[BUDDY_MESSAGE_MAX + 1] = '\0';
    snprintf(json, sizeof(json),
             "{\"total\":0,\"running\":0,\"waiting\":0,\"msg\":\"%s\","
             "\"entries\":[],\"tokens\":0,\"tokens_today\":0}", status);

    assert(parse(json, &event) == BUDDY_EVENT_HEARTBEAT);
    assert(event.heartbeat.message[sizeof(event.heartbeat.message) - 1] == '\0');
    assert(event.heartbeat.message_truncated);
    assert(strlen(event.heartbeat.message) == BUDDY_MESSAGE_MAX - 2);
    assert(event.heartbeat.message[BUDDY_MESSAGE_MAX - 2] == '\0');
}

static void test_official_time_owner_and_name_payloads(void)
{
    struct command_case {
        const char *json;
        buddy_event_type_t type;
        const char *value;
    } cases[] = {
        {OFFICIAL_NAME_JSON, BUDDY_EVENT_NAME, "Clawd"},
        {OFFICIAL_OWNER_JSON, BUDDY_EVENT_OWNER, "Felix"},
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        buddy_event_t event = {0};

        assert(parse(cases[index].json, &event) == (int)cases[index].type);
        assert(event.type == cases[index].type);
        assert(strcmp(event.command.value, cases[index].value) == 0);
        assert(!event.command.value_truncated);
    }
    buddy_event_t time_event = {0};
    assert(parse(OFFICIAL_TIME_JSON, &time_event) == BUDDY_EVENT_TIME);
    assert(time_event.time.epoch_seconds == 1775731234);
    assert(time_event.time.timezone_offset_seconds == -25200);
    assert(time_event.command.name[0] == '\0');
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

static void test_parser_rejects_excessive_json_nesting(void)
{
    buddy_event_t event = {0};
    char json[256];
    size_t length = 0;
    unsigned depth;

    for (depth = 0; depth < BUDDY_JSON_MAX_DEPTH + 1U; ++depth) {
        length += (size_t)snprintf(json + length, sizeof(json) - length, "{\"x\":");
    }
    length += (size_t)snprintf(json + length, sizeof(json) - length, "0");
    for (depth = 0; depth < BUDDY_JSON_MAX_DEPTH + 1U; ++depth) {
        length += (size_t)snprintf(json + length, sizeof(json) - length, "}");
    }

    assert(length < sizeof(json));
    assert(buddy_protocol_parse(json, length, &event) == BUDDY_EVENT_MALFORMED);
}

static void test_preflight_failure_clears_a_reused_event(void)
{
    buddy_event_t event = {0};

    assert(parse("{\"cmd\":\"status\"}", &event) == BUDDY_EVENT_STATUS_REQUEST);
    assert(event.command.name[0] != '\0');
    assert(parse("{\"cmd\":\"status\"}\x01", &event) == BUDDY_EVENT_MALFORMED);
    assert(event.type == BUDDY_EVENT_NONE);
    assert(event.command.name[0] == '\0');
}

static void test_serializers_write_documented_json(void)
{
    char json[192];

    assert(buddy_protocol_permission_json(json, sizeof(json), "req_abc123",
                                          BUDDY_PERMISSION_ONCE) > 0);
    assert(strcmp(json,
                  "{\"cmd\":\"permission\",\"id\":\"req_abc123\",\"decision\":\"once\"}\n") ==
           0);
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

    memset(id, 'x', sizeof(id));
    assert(buddy_protocol_permission_json(json, sizeof(json), id, BUDDY_PERMISSION_ONCE) == 0);
    assert(buddy_protocol_permission_json(json, sizeof(json), "\xc0\xaf", BUDDY_PERMISSION_ONCE) == 0);
}

static void test_command_ack_names_the_request_and_error(void)
{
    buddy_event_t event = {0};
    char json[160];

    assert(parse("{\"cmd\":\"char_begin\"}", &event) == BUDDY_EVENT_UNSUPPORTED_COMMAND);
    assert(strcmp(event.command.name, "char_begin") == 0);
    assert(buddy_protocol_command_ack_json(json, sizeof(json), event.command.name, false,
                                           "unsupported in phase 1") > 0);
    assert(strcmp(json,
                  "{\"ack\":\"char_begin\",\"ok\":false,"
                  "\"error\":\"unsupported in phase 1\"}\n") == 0);
}

static void test_device_status_omits_unavailable_battery_fields(void)
{
    buddy_status_report_t status = {
        .encrypted = true,
        .uptime_ms = 123456,
        .free_heap = 32000,
        .approval_count = 7,
        .denial_count = 2,
        .queue_overflow_count = 4,
        .battery_available = false,
    };
    char json[320];

    snprintf(status.name, sizeof(status.name), "%s", "Buddy");
    assert(buddy_protocol_device_status_json(json, sizeof(json), &status) > 0);
    assert(strcmp(json,
                  "{\"ack\":\"status\",\"ok\":true,\"data\":{"
                  "\"name\":\"Buddy\",\"sec\":true,"
                  "\"sys\":{\"up\":123,\"heap\":32000},"
                  "\"stats\":{\"appr\":7,\"deny\":2,\"lvl\":0}}}\n") == 0);
    assert(strstr(json, "\"bat\"") == NULL);

    status.battery_available = true;
    status.battery_percent = 73;
    status.battery_mv = 3875;
    assert(buddy_protocol_device_status_json(json, sizeof(json), &status) > 0);
    assert(strstr(json, "\"bat\":{\"pct\":73,\"mV\":3875}") != NULL);
    assert(strstr(json, "\"sys\":{\"up\":123,\"heap\":32000}") != NULL);
}

static void test_task_tx_capacity_handles_worst_case_escaping(void)
{
    char id[BUDDY_PROMPT_ID_MAX];
    buddy_status_report_t status = {0};
    char json[BUDDY_PROTOCOL_TX_MAX];
    size_t index;

    for (index = 0; index + 1U < sizeof(id); ++index) {
        id[index] = '\x01';
    }
    id[sizeof(id) - 1U] = '\0';
    memset(status.name, '\x01', sizeof(status.name) - 1U);
    status.encrypted = true;
    status.battery_available = true;
    status.battery_percent = 100;
    status.battery_mv = UINT16_MAX;
    status.uptime_ms = UINT64_MAX;
    status.free_heap = UINT64_MAX;
    status.approval_count = UINT64_MAX;
    status.denial_count = UINT64_MAX;
    status.queue_overflow_count = UINT64_MAX;

    assert(buddy_protocol_permission_json(json, sizeof(json), id, BUDDY_PERMISSION_ONCE) > 0);
    assert(buddy_protocol_device_status_json(json, sizeof(json), &status) > 0);
}

int main(void)
{
    test_official_heartbeat_maps_documented_fields();
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
    test_official_time_owner_and_name_payloads();
    test_status_is_a_no_payload_request();
    test_name_command_truncates_at_a_utf8_codepoint_boundary();
    test_parser_rejects_oversized_or_invalid_utf8_lines();
    test_parser_rejects_raw_nul_and_control_bytes();
    test_parser_rejects_excessive_json_nesting();
    test_preflight_failure_clears_a_reused_event();
    test_serializers_write_documented_json();
    test_serializers_fail_without_writing_past_the_output_bound();
    test_serializers_reject_invalid_or_unterminated_inputs();
    test_command_ack_names_the_request_and_error();
    test_device_status_omits_unavailable_battery_fields();
    test_task_tx_capacity_handles_worst_case_escaping();
    return 0;
}
