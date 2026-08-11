#include "buddy_protocol.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

typedef struct {
    char *data;
    size_t size;
    size_t length;
    bool valid;
} buddy_json_writer_t;

static size_t buddy_bounded_length(const char *value, size_t limit)
{
    size_t length;

    if (value == NULL) {
        return 0;
    }
    for (length = 0; length < limit && value[length] != '\0'; ++length) {
    }
    return length;
}

static bool buddy_utf8_next(const unsigned char *data, size_t length, size_t *width)
{
    unsigned char first;

    if (length == 0) {
        return false;
    }
    first = data[0];
    if (first <= 0x7f) {
        *width = 1;
        return true;
    }
    if (first >= 0xc2 && first <= 0xdf && length >= 2 &&
        data[1] >= 0x80 && data[1] <= 0xbf) {
        *width = 2;
        return true;
    }
    if (first == 0xe0 && length >= 3 && data[1] >= 0xa0 && data[1] <= 0xbf &&
        data[2] >= 0x80 && data[2] <= 0xbf) {
        *width = 3;
        return true;
    }
    if (first >= 0xe1 && first <= 0xec && length >= 3 &&
        data[1] >= 0x80 && data[1] <= 0xbf && data[2] >= 0x80 && data[2] <= 0xbf) {
        *width = 3;
        return true;
    }
    if (first == 0xed && length >= 3 && data[1] >= 0x80 && data[1] <= 0x9f &&
        data[2] >= 0x80 && data[2] <= 0xbf) {
        *width = 3;
        return true;
    }
    if (first >= 0xee && first <= 0xef && length >= 3 &&
        data[1] >= 0x80 && data[1] <= 0xbf && data[2] >= 0x80 && data[2] <= 0xbf) {
        *width = 3;
        return true;
    }
    if (first == 0xf0 && length >= 4 && data[1] >= 0x90 && data[1] <= 0xbf &&
        data[2] >= 0x80 && data[2] <= 0xbf && data[3] >= 0x80 && data[3] <= 0xbf) {
        *width = 4;
        return true;
    }
    if (first >= 0xf1 && first <= 0xf3 && length >= 4 &&
        data[1] >= 0x80 && data[1] <= 0xbf && data[2] >= 0x80 && data[2] <= 0xbf &&
        data[3] >= 0x80 && data[3] <= 0xbf) {
        *width = 4;
        return true;
    }
    if (first == 0xf4 && length >= 4 && data[1] >= 0x80 && data[1] <= 0x8f &&
        data[2] >= 0x80 && data[2] <= 0xbf && data[3] >= 0x80 && data[3] <= 0xbf) {
        *width = 4;
        return true;
    }
    return false;
}

static bool buddy_utf8_valid(const char *value, size_t length)
{
    size_t offset = 0;

    while (offset < length) {
        size_t width;

        if (!buddy_utf8_next((const unsigned char *)value + offset, length - offset, &width)) {
            return false;
        }
        offset += width;
    }
    return true;
}

static bool buddy_copy_utf8(char *destination, size_t destination_size, const char *source,
                            size_t source_length, bool *truncated)
{
    size_t offset = 0;
    size_t copied = 0;

    if (destination_size == 0 || (source != NULL && !buddy_utf8_valid(source, source_length))) {
        return false;
    }
    while (source != NULL && offset < source_length) {
        size_t width;

        (void)buddy_utf8_next((const unsigned char *)source + offset, source_length - offset, &width);
        if (width > destination_size - 1 - copied) {
            break;
        }
        memcpy(destination + copied, source + offset, width);
        copied += width;
        offset += width;
    }
    destination[copied] = '\0';
    if (truncated != NULL) {
        *truncated = offset != source_length;
    }
    return true;
}

static bool buddy_json_optional_string(const cJSON *object, const char *name,
                                       const char **value, size_t *length)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);

    *value = NULL;
    *length = 0;
    if (item == NULL) {
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    *value = item->valuestring;
    *length = buddy_bounded_length(*value, BUDDY_JSON_LINE_MAX + 1);
    return *length <= BUDDY_JSON_LINE_MAX && buddy_utf8_valid(*value, *length);
}

static bool buddy_json_unsigned(const cJSON *object, const char *name, unsigned *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);

    if (item == NULL) {
        return true;
    }
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble > UINT_MAX) {
        return false;
    }
    *value = (unsigned)item->valuedouble;
    return (double)*value == item->valuedouble;
}

static bool buddy_json_u64(const cJSON *object, const char *name, uint64_t *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);

    if (item == NULL) {
        return true;
    }
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 ||
        item->valuedouble > (double)UINT64_C(9007199254740991)) {
        return false;
    }
    *value = (uint64_t)item->valuedouble;
    return (double)*value == item->valuedouble;
}

static bool buddy_json_required_unsigned(const cJSON *object, const char *name,
                                         unsigned *value)
{
    return cJSON_GetObjectItemCaseSensitive(object, name) != NULL &&
           buddy_json_unsigned(object, name, value);
}

static bool buddy_json_required_u64(const cJSON *object, const char *name, uint64_t *value)
{
    return cJSON_GetObjectItemCaseSensitive(object, name) != NULL &&
           buddy_json_u64(object, name, value);
}

static bool buddy_parse_prompt(const cJSON *prompt_json, unsigned running, buddy_prompt_t *prompt)
{
    const char *id;
    const char *tool;
    const char *hint;
    size_t id_length;
    size_t tool_length;
    size_t hint_length;

    if (!buddy_json_optional_string(prompt_json, "id", &id, &id_length) || id == NULL ||
        id_length == 0 || id_length >= sizeof(prompt->id) ||
        !buddy_json_optional_string(prompt_json, "tool", &tool, &tool_length) ||
        !buddy_json_optional_string(prompt_json, "hint", &hint, &hint_length)) {
        return false;
    }
    prompt->connected = true;
    prompt->running = running;
    prompt->id_length = id_length;
    prompt->id_truncated = false;
    return buddy_copy_utf8(prompt->id, sizeof(prompt->id), id, id_length,
                           &prompt->id_truncated) &&
           buddy_copy_utf8(prompt->tool, sizeof(prompt->tool), tool, tool_length,
                           &prompt->tool_truncated) &&
           buddy_copy_utf8(prompt->hint, sizeof(prompt->hint), hint, hint_length,
                           &prompt->hint_truncated);
}

static bool buddy_parse_heartbeat(const cJSON *object, buddy_event_t *event)
{
    const cJSON *entries;
    const cJSON *prompt;
    const char *message;
    size_t message_length;
    unsigned index;

    if (!buddy_json_required_unsigned(object, "total", &event->heartbeat.total) ||
        !buddy_json_required_unsigned(object, "running", &event->heartbeat.running) ||
        !buddy_json_required_unsigned(object, "waiting", &event->heartbeat.waiting) ||
        !buddy_json_required_u64(object, "tokens", &event->heartbeat.tokens) ||
        !buddy_json_required_u64(object, "tokens_today", &event->heartbeat.tokens_today) ||
        !buddy_json_optional_string(object, "msg", &message, &message_length) ||
        message == NULL ||
        !buddy_copy_utf8(event->heartbeat.message, sizeof(event->heartbeat.message), message,
                         message_length, &event->heartbeat.message_truncated)) {
        return false;
    }
    event->heartbeat.connected = true;

    entries = cJSON_GetObjectItemCaseSensitive(object, "entries");
    if (entries == NULL || !cJSON_IsArray(entries)) {
        return false;
    }
    for (index = 0; index < BUDDY_ENTRY_COUNT; ++index) {
        const cJSON *entry = cJSON_GetArrayItem(entries, (int)index);
        const char *entry_value;
        size_t entry_length;

        if (entry == NULL) {
            break;
        }
        if (!cJSON_IsString(entry) || entry->valuestring == NULL) {
            return false;
        }
        entry_value = entry->valuestring;
        entry_length = buddy_bounded_length(entry_value, BUDDY_JSON_LINE_MAX + 1);
        if (entry_length > BUDDY_JSON_LINE_MAX ||
            !buddy_copy_utf8(event->heartbeat.entries[index],
                             sizeof(event->heartbeat.entries[index]), entry_value, entry_length,
                             &event->heartbeat.entries_truncated[index])) {
            return false;
        }
    }

    prompt = cJSON_GetObjectItemCaseSensitive(object, "prompt");
    if (prompt != NULL) {
        if (!cJSON_IsObject(prompt) ||
            !buddy_parse_prompt(prompt, event->heartbeat.running, &event->heartbeat.prompt)) {
            return false;
        }
    }
    event->type = BUDDY_EVENT_HEARTBEAT;
    return true;
}

static bool buddy_parse_command(const cJSON *object, const char *field, buddy_event_type_t type,
                                buddy_event_t *event)
{
    const char *value;
    size_t length;
    size_t value_size = sizeof(event->command.value);

    if (type == BUDDY_EVENT_NAME) {
        value_size = BUDDY_NAME_MAX;
    } else if (type == BUDDY_EVENT_OWNER) {
        value_size = BUDDY_OWNER_MAX;
    }

    if (!buddy_json_optional_string(object, field, &value, &length)) {
        return false;
    }
    if (value == NULL ||
        !buddy_copy_utf8(event->command.value, value_size, value, length,
                         &event->command.value_truncated)) {
        return false;
    }
    event->type = type;
    return true;
}

static bool buddy_parse_time(const cJSON *object, buddy_event_t *event)
{
    const cJSON *time = cJSON_GetObjectItemCaseSensitive(object, "time");
    const cJSON *epoch;
    const cJSON *offset;
    int64_t epoch_value;
    int32_t offset_value;

    if (!cJSON_IsArray(time) || cJSON_GetArraySize(time) != 2) {
        return false;
    }
    epoch = cJSON_GetArrayItem(time, 0);
    offset = cJSON_GetArrayItem(time, 1);
    if (!cJSON_IsNumber(epoch) || epoch->valuedouble < 0 ||
        epoch->valuedouble > 9007199254740991.0 || !cJSON_IsNumber(offset) ||
        offset->valuedouble < INT32_MIN || offset->valuedouble > INT32_MAX) {
        return false;
    }
    epoch_value = (int64_t)epoch->valuedouble;
    offset_value = (int32_t)offset->valuedouble;
    if ((double)epoch_value != epoch->valuedouble ||
        (double)offset_value != offset->valuedouble) {
        return false;
    }
    event->time.epoch_seconds = epoch_value;
    event->time.timezone_offset_seconds = offset_value;
    event->type = BUDDY_EVENT_TIME;
    return true;
}

static bool buddy_is_unsupported_folder_command(const char *command)
{
    return strcmp(command, "char_begin") == 0 || strcmp(command, "file") == 0 ||
           strcmp(command, "chunk") == 0 || strcmp(command, "file_end") == 0 ||
           strcmp(command, "char_end") == 0 || strncmp(command, "folder_", 7) == 0;
}

static bool buddy_json_contains_nul_escape(const char *json, size_t length)
{
    size_t index;

    for (index = 0; index + 5 < length; ++index) {
        if (json[index] == '\\' && json[index + 1] == 'u' && json[index + 2] == '0' &&
            json[index + 3] == '0' && json[index + 4] == '0' && json[index + 5] == '0') {
            return true;
        }
    }
    return false;
}

static bool buddy_json_raw_controls_valid(const char *json, size_t length)
{
    bool in_string = false;
    bool escaped = false;
    size_t index;

    for (index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char)json[index];

        if (byte == 0) {
            return false;
        }
        if (in_string) {
            if (byte < 0x20) {
                return false;
            }
            if (escaped) {
                escaped = false;
            } else if (byte == '\\') {
                escaped = true;
            } else if (byte == '"') {
                in_string = false;
            }
        } else {
            if (byte == '"') {
                in_string = true;
            } else if (byte < 0x20 && byte != '\t' && byte != '\n' && byte != '\r') {
                return false;
            }
        }
    }
    return true;
}

static bool buddy_json_depth_valid(const char *json, size_t length)
{
    bool in_string = false;
    bool escaped = false;
    unsigned depth = 0;
    size_t index;

    for (index = 0; index < length; ++index) {
        char byte = json[index];

        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (byte == '\\') {
                escaped = true;
            } else if (byte == '"') {
                in_string = false;
            }
        } else if (byte == '"') {
            in_string = true;
        } else if (byte == '{' || byte == '[') {
            if (depth >= BUDDY_JSON_MAX_DEPTH) {
                return false;
            }
            ++depth;
        } else if ((byte == '}' || byte == ']') && depth > 0U) {
            --depth;
        }
    }
    return true;
}

int buddy_protocol_parse(const char *json, size_t length, buddy_event_t *event)
{
    cJSON *root;
    const char *end = NULL;
    const char *command;
    size_t command_length;
    int result = BUDDY_EVENT_MALFORMED;

    if (event == NULL) {
        return BUDDY_EVENT_MALFORMED;
    }
    memset(event, 0, sizeof(*event));
    if (json == NULL || length > BUDDY_JSON_LINE_MAX ||
        !buddy_utf8_valid(json, length) || !buddy_json_raw_controls_valid(json, length) ||
        !buddy_json_depth_valid(json, length) ||
        buddy_json_contains_nul_escape(json, length)) {
        return BUDDY_EVENT_MALFORMED;
    }
    root = cJSON_ParseWithLengthOpts(json, length, &end, 0);
    if (root == NULL || end != json + length || !cJSON_IsObject(root) ||
        !buddy_json_optional_string(root, "cmd", &command, &command_length)) {
        cJSON_Delete(root);
        return BUDDY_EVENT_MALFORMED;
    }
    if (command == NULL) {
        if (cJSON_GetObjectItemCaseSensitive(root, "time") != NULL) {
            result = buddy_parse_time(root, event) ? (int)event->type : result;
        } else if (buddy_parse_heartbeat(root, event)) {
            result = (int)event->type;
        }
        cJSON_Delete(root);
        return result;
    }
    if (command_length == 0) {
        cJSON_Delete(root);
        return BUDDY_EVENT_MALFORMED;
    }
    if (command_length >= sizeof(event->command.name) ||
        !buddy_copy_utf8(event->command.name, sizeof(event->command.name), command,
                         command_length, NULL)) {
        cJSON_Delete(root);
        memset(event, 0, sizeof(*event));
        return BUDDY_EVENT_MALFORMED;
    }
    if (strcmp(command, "name") == 0) {
        result = buddy_parse_command(root, "name", BUDDY_EVENT_NAME, event) ? (int)event->type : result;
    } else if (strcmp(command, "owner") == 0) {
        result = buddy_parse_command(root, "name", BUDDY_EVENT_OWNER, event) ? (int)event->type : result;
    } else if (strcmp(command, "status") == 0) {
        if (cJSON_GetObjectItemCaseSensitive(root, "status") == NULL &&
            cJSON_GetObjectItemCaseSensitive(root, "value") == NULL) {
            event->type = BUDDY_EVENT_STATUS_REQUEST;
            result = (int)event->type;
        }
    } else if (strcmp(command, "unpair") == 0) {
        event->type = BUDDY_EVENT_UNPAIR_CONFIRMATION;
        result = (int)event->type;
    } else if (buddy_is_unsupported_folder_command(command)) {
        result = BUDDY_EVENT_UNSUPPORTED_COMMAND;
    } else {
        result = BUDDY_EVENT_UNKNOWN_COMMAND;
    }
    if (result < BUDDY_EVENT_NONE) {
        char parsed_command[BUDDY_COMMAND_MAX];

        memcpy(parsed_command, event->command.name, sizeof(parsed_command));
        memset(event, 0, sizeof(*event));
        memcpy(event->command.name, parsed_command, sizeof(event->command.name));
    }
    cJSON_Delete(root);
    return result;
}

static void buddy_writer_init(buddy_json_writer_t *writer, char *data, size_t size)
{
    writer->data = data;
    writer->size = size;
    writer->length = 0;
    writer->valid = data != NULL && size > 0;
    if (writer->valid) {
        data[0] = '\0';
    }
}

static void buddy_writer_char(buddy_json_writer_t *writer, char value)
{
    if (!writer->valid || writer->length + 1 >= writer->size) {
        writer->valid = false;
        return;
    }
    writer->data[writer->length++] = value;
    writer->data[writer->length] = '\0';
}

static void buddy_writer_literal(buddy_json_writer_t *writer, const char *value)
{
    while (writer->valid && *value != '\0') {
        buddy_writer_char(writer, *value++);
    }
}

static void buddy_writer_json_string(buddy_json_writer_t *writer, const char *value, size_t length)
{
    static const char digits[] = "0123456789abcdef";
    size_t index;

    if (!writer->valid || !buddy_utf8_valid(value, length)) {
        writer->valid = false;
        return;
    }
    buddy_writer_char(writer, '"');
    for (index = 0; writer->valid && index < length; ++index) {
        unsigned char byte = (unsigned char)value[index];

        switch (byte) {
        case '"':
            buddy_writer_literal(writer, "\\\"");
            break;
        case '\\':
            buddy_writer_literal(writer, "\\\\");
            break;
        case '\b':
            buddy_writer_literal(writer, "\\b");
            break;
        case '\f':
            buddy_writer_literal(writer, "\\f");
            break;
        case '\n':
            buddy_writer_literal(writer, "\\n");
            break;
        case '\r':
            buddy_writer_literal(writer, "\\r");
            break;
        case '\t':
            buddy_writer_literal(writer, "\\t");
            break;
        default:
            if (byte < 0x20) {
                buddy_writer_literal(writer, "\\u00");
                buddy_writer_char(writer, digits[byte >> 4]);
                buddy_writer_char(writer, digits[byte & 0x0f]);
            } else {
                buddy_writer_char(writer, (char)byte);
            }
            break;
        }
    }
    buddy_writer_char(writer, '"');
}

static void buddy_writer_u64(buddy_json_writer_t *writer, uint64_t value)
{
    char number[32];
    int length = snprintf(number, sizeof(number), "%" PRIu64 "", value);

    if (length < 0 || (size_t)length >= sizeof(number)) {
        writer->valid = false;
        return;
    }
    buddy_writer_literal(writer, number);
}

static void buddy_writer_bool(buddy_json_writer_t *writer, bool value)
{
    buddy_writer_literal(writer, value ? "true" : "false");
}

static int buddy_writer_finish(const buddy_json_writer_t *writer)
{
    return writer->valid && writer->length <= INT_MAX ? (int)writer->length : 0;
}

static const char *buddy_decision_name(buddy_permission_decision_t decision)
{
    switch (decision) {
    case BUDDY_PERMISSION_ONCE:
        return "once";
    case BUDDY_PERMISSION_ALWAYS:
        return "always";
    case BUDDY_PERMISSION_DENY:
        return "deny";
    case BUDDY_PERMISSION_NONE:
    default:
        return NULL;
    }
}

int buddy_protocol_permission_json(char *json, size_t size, const char *id,
                                   buddy_permission_decision_t decision)
{
    buddy_json_writer_t writer;
    const char *decision_name = buddy_decision_name(decision);
    size_t id_length = buddy_bounded_length(id, BUDDY_PROMPT_ID_MAX);

    if (id == NULL || id_length == 0 || id_length == BUDDY_PROMPT_ID_MAX ||
        !buddy_utf8_valid(id, id_length) || decision_name == NULL) {
        return 0;
    }
    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, "{\"cmd\":\"permission\",\"id\":");
    buddy_writer_json_string(&writer, id, id_length);
    buddy_writer_literal(&writer, ",\"decision\":");
    buddy_writer_json_string(&writer, decision_name, strlen(decision_name));
    buddy_writer_literal(&writer, "}\n");
    return buddy_writer_finish(&writer);
}

int buddy_protocol_command_ack_json(char *json, size_t size, const char *command,
                                    bool ok, const char *error)
{
    buddy_json_writer_t writer;
    size_t command_length = buddy_bounded_length(command, BUDDY_COMMAND_MAX);
    size_t error_length = buddy_bounded_length(error, BUDDY_MESSAGE_MAX);

    if (command == NULL || command_length == 0 || command_length == BUDDY_COMMAND_MAX ||
        !buddy_utf8_valid(command, command_length) ||
        (!ok && (error == NULL || error_length == 0 || error_length == BUDDY_MESSAGE_MAX ||
                 !buddy_utf8_valid(error, error_length)))) {
        return 0;
    }
    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, "{\"ack\":");
    buddy_writer_json_string(&writer, command, command_length);
    buddy_writer_literal(&writer, ",\"ok\":");
    buddy_writer_bool(&writer, ok);
    if (!ok) {
        buddy_writer_literal(&writer, ",\"error\":");
        buddy_writer_json_string(&writer, error, error_length);
    }
    buddy_writer_literal(&writer, "}\n");
    return buddy_writer_finish(&writer);
}

int buddy_protocol_device_status_json(char *json, size_t size,
                                      const buddy_status_report_t *status)
{
    buddy_json_writer_t writer;
    size_t name_length;

    if (status == NULL) {
        return 0;
    }
    name_length = buddy_bounded_length(status->name, sizeof(status->name));
    if (name_length == sizeof(status->name) ||
        !buddy_utf8_valid(status->name, name_length)) {
        return 0;
    }

    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, "{\"ack\":\"status\",\"ok\":true,\"data\":{\"name\":");
    buddy_writer_json_string(&writer, status->name, name_length);
    buddy_writer_literal(&writer, ",\"sec\":");
    buddy_writer_bool(&writer, status->encrypted);
    if (status->battery_available) {
        buddy_writer_literal(&writer, ",\"bat\":{\"pct\":");
        buddy_writer_u64(&writer, status->battery_percent);
        buddy_writer_literal(&writer, ",\"mV\":");
        buddy_writer_u64(&writer, status->battery_mv);
        buddy_writer_literal(&writer, "}");
    }
    buddy_writer_literal(&writer, ",\"sys\":{\"up\":");
    buddy_writer_u64(&writer, status->uptime_ms / 1000U);
    buddy_writer_literal(&writer, ",\"heap\":");
    buddy_writer_u64(&writer, status->free_heap);
    buddy_writer_literal(&writer, "},\"stats\":{\"appr\":");
    buddy_writer_u64(&writer, status->approval_count);
    buddy_writer_literal(&writer, ",\"deny\":");
    buddy_writer_u64(&writer, status->denial_count);
    buddy_writer_literal(&writer, ",\"lvl\":");
    buddy_writer_u64(&writer, status->highest_celebrated_level);
    buddy_writer_literal(&writer, "}}}\n");
    return buddy_writer_finish(&writer);
}
