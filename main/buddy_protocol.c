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

static bool buddy_parse_prompt(const cJSON *prompt_json, unsigned running, buddy_event_t *event)
{
    const char *id;
    const char *tool;
    const char *hint;
    size_t id_length;
    size_t tool_length;
    size_t hint_length;

    if (!buddy_json_optional_string(prompt_json, "id", &id, &id_length) || id == NULL ||
        id_length == 0 || id_length >= sizeof(event->prompt.id) ||
        !buddy_json_optional_string(prompt_json, "tool", &tool, &tool_length) ||
        !buddy_json_optional_string(prompt_json, "hint", &hint, &hint_length)) {
        return false;
    }
    event->type = BUDDY_EVENT_PROMPT;
    event->prompt.connected = true;
    event->prompt.running = running;
    event->prompt.id_length = id_length;
    event->prompt.id_truncated = false;
    return buddy_copy_utf8(event->prompt.id, sizeof(event->prompt.id), id, id_length,
                           &event->prompt.id_truncated) &&
           buddy_copy_utf8(event->prompt.tool, sizeof(event->prompt.tool), tool, tool_length,
                           &event->prompt.tool_truncated) &&
           buddy_copy_utf8(event->prompt.hint, sizeof(event->prompt.hint), hint, hint_length,
                           &event->prompt.hint_truncated);
}

static bool buddy_parse_heartbeat(const cJSON *object, buddy_event_t *event)
{
    const cJSON *entries;
    const cJSON *prompt;
    const char *name;
    const char *owner;
    const char *status;
    size_t name_length;
    size_t owner_length;
    size_t status_length;
    unsigned running = 0;
    unsigned index;

    if (!buddy_json_unsigned(object, "running", &running) ||
        !buddy_json_u64(object, "tokens", &event->heartbeat.tokens) ||
        !buddy_json_optional_string(object, "name", &name, &name_length) ||
        !buddy_json_optional_string(object, "owner", &owner, &owner_length) ||
        !buddy_json_optional_string(object, "status", &status, &status_length) ||
        !buddy_copy_utf8(event->heartbeat.name, sizeof(event->heartbeat.name), name, name_length,
                         &event->heartbeat.name_truncated) ||
        !buddy_copy_utf8(event->heartbeat.owner, sizeof(event->heartbeat.owner), owner, owner_length,
                         &event->heartbeat.owner_truncated) ||
        !buddy_copy_utf8(event->heartbeat.message, sizeof(event->heartbeat.message), status,
                         status_length, &event->heartbeat.message_truncated)) {
        return false;
    }
    event->heartbeat.connected = true;
    event->heartbeat.running = running;

    entries = cJSON_GetObjectItemCaseSensitive(object, "entries");
    if (entries != NULL) {
        if (!cJSON_IsArray(entries)) {
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
    }

    prompt = cJSON_GetObjectItemCaseSensitive(object, "prompt");
    if (prompt != NULL) {
        return cJSON_IsObject(prompt) && buddy_parse_prompt(prompt, running, event);
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
    if (value == NULL && !buddy_json_optional_string(object, "value", &value, &length)) {
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

int buddy_protocol_parse(const char *json, size_t length, buddy_event_t *event)
{
    cJSON *root;
    const char *end = NULL;
    const char *command;
    size_t command_length;
    unsigned running = 0;
    int result = BUDDY_EVENT_MALFORMED;

    if (json == NULL || event == NULL || length > BUDDY_JSON_LINE_MAX ||
        !buddy_utf8_valid(json, length) || !buddy_json_raw_controls_valid(json, length) ||
        buddy_json_contains_nul_escape(json, length)) {
        return BUDDY_EVENT_MALFORMED;
    }
    memset(event, 0, sizeof(*event));
    root = cJSON_ParseWithLengthOpts(json, length, &end, 0);
    if (root == NULL || end != json + length || !cJSON_IsObject(root) ||
        !buddy_json_optional_string(root, "cmd", &command, &command_length) || command == NULL ||
        command_length == 0) {
        cJSON_Delete(root);
        return BUDDY_EVENT_MALFORMED;
    }
    if (strcmp(command, "heartbeat") == 0) {
        if (buddy_parse_heartbeat(root, event)) {
            result = (int)event->type;
        }
    } else if (strcmp(command, "prompt") == 0) {
        if (buddy_json_unsigned(root, "running", &running) && buddy_parse_prompt(root, running, event)) {
            result = (int)event->type;
        }
    } else if (strcmp(command, "time") == 0) {
        result = buddy_parse_command(root, "time", BUDDY_EVENT_TIME, event) ? (int)event->type : result;
    } else if (strcmp(command, "name") == 0) {
        result = buddy_parse_command(root, "name", BUDDY_EVENT_NAME, event) ? (int)event->type : result;
    } else if (strcmp(command, "owner") == 0) {
        result = buddy_parse_command(root, "owner", BUDDY_EVENT_OWNER, event) ? (int)event->type : result;
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
        memset(event, 0, sizeof(*event));
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

int buddy_protocol_ack_json(char *json, size_t size, bool ok)
{
    buddy_json_writer_t writer;

    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, ok ? "{\"cmd\":\"ack\",\"ok\":true}\n"
                                    : "{\"cmd\":\"ack\",\"ok\":false}\n");
    return buddy_writer_finish(&writer);
}

int buddy_protocol_status_json(char *json, size_t size, const buddy_heartbeat_t *heartbeat)
{
    buddy_json_writer_t writer;
    size_t name_length;
    size_t owner_length;
    size_t message_length;

    if (heartbeat == NULL) {
        return 0;
    }
    name_length = buddy_bounded_length(heartbeat->name, sizeof(heartbeat->name));
    owner_length = buddy_bounded_length(heartbeat->owner, sizeof(heartbeat->owner));
    message_length = buddy_bounded_length(heartbeat->message, sizeof(heartbeat->message));
    if (name_length == sizeof(heartbeat->name) || owner_length == sizeof(heartbeat->owner) ||
        message_length == sizeof(heartbeat->message) || !buddy_utf8_valid(heartbeat->name, name_length) ||
        !buddy_utf8_valid(heartbeat->owner, owner_length) ||
        !buddy_utf8_valid(heartbeat->message, message_length)) {
        return 0;
    }
    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, "{\"cmd\":\"status\",\"name\":");
    buddy_writer_json_string(&writer, heartbeat->name, name_length);
    buddy_writer_literal(&writer, ",\"owner\":");
    buddy_writer_json_string(&writer, heartbeat->owner, owner_length);
    buddy_writer_literal(&writer, ",\"status\":");
    buddy_writer_json_string(&writer, heartbeat->message, message_length);
    buddy_writer_literal(&writer, ",\"running\":");
    buddy_writer_u64(&writer, heartbeat->running);
    buddy_writer_literal(&writer, ",\"tokens\":");
    buddy_writer_u64(&writer, heartbeat->tokens);
    buddy_writer_literal(&writer, "}\n");
    return buddy_writer_finish(&writer);
}
