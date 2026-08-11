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

static void buddy_copy_string(char *destination, size_t destination_size, const char *source,
                              size_t *source_length, bool *truncated)
{
    size_t length = 0;

    if (source != NULL) {
        length = strlen(source);
    }
    if (source_length != NULL) {
        *source_length = length;
    }
    if (truncated != NULL) {
        *truncated = length >= destination_size;
    }
    if (destination_size == 0) {
        return;
    }
    if (source != NULL) {
        size_t copied = length;

        if (copied >= destination_size) {
            copied = destination_size - 1;
        }
        memcpy(destination, source, copied);
        destination[copied] = '\0';
        return;
    }
    destination[0] = '\0';
}

static const char *buddy_json_string(const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);

    return cJSON_IsString(item) && item->valuestring != NULL ? item->valuestring : NULL;
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
    const char *id = buddy_json_string(prompt_json, "id");
    const char *tool = buddy_json_string(prompt_json, "tool");
    const char *hint = buddy_json_string(prompt_json, "hint");

    if (id == NULL || id[0] == '\0') {
        return false;
    }
    event->type = BUDDY_EVENT_PROMPT;
    event->prompt.connected = true;
    event->prompt.running = running;
    buddy_copy_string(event->prompt.id, sizeof(event->prompt.id), id, &event->prompt.id_length,
                      &event->prompt.id_truncated);
    buddy_copy_string(event->prompt.tool, sizeof(event->prompt.tool), tool, NULL, NULL);
    buddy_copy_string(event->prompt.hint, sizeof(event->prompt.hint), hint, NULL, NULL);
    return true;
}

static bool buddy_parse_heartbeat(const cJSON *object, buddy_event_t *event)
{
    const cJSON *entries;
    const cJSON *prompt;
    const char *name;
    const char *owner;
    const char *status;
    unsigned running = 0;
    unsigned index;

    if (!buddy_json_unsigned(object, "running", &running) ||
        !buddy_json_u64(object, "tokens", &event->heartbeat.tokens)) {
        return false;
    }
    event->heartbeat.connected = true;
    event->heartbeat.running = running;
    name = buddy_json_string(object, "name");
    owner = buddy_json_string(object, "owner");
    status = buddy_json_string(object, "status");
    buddy_copy_string(event->heartbeat.name, sizeof(event->heartbeat.name), name, NULL, NULL);
    buddy_copy_string(event->heartbeat.owner, sizeof(event->heartbeat.owner), owner, NULL, NULL);
    buddy_copy_string(event->heartbeat.message, sizeof(event->heartbeat.message), status, NULL, NULL);

    entries = cJSON_GetObjectItemCaseSensitive(object, "entries");
    if (entries != NULL) {
        if (!cJSON_IsArray(entries)) {
            return false;
        }
        for (index = 0; index < BUDDY_ENTRY_COUNT; ++index) {
            const cJSON *entry = cJSON_GetArrayItem(entries, (int)index);

            if (entry == NULL) {
                break;
            }
            if (!cJSON_IsString(entry) || entry->valuestring == NULL) {
                return false;
            }
            buddy_copy_string(event->heartbeat.entries[index],
                              sizeof(event->heartbeat.entries[index]), entry->valuestring, NULL, NULL);
        }
    }

    prompt = cJSON_GetObjectItemCaseSensitive(object, "prompt");
    if (prompt != NULL) {
        return cJSON_IsObject(prompt) && buddy_parse_prompt(prompt, running, event);
    }
    event->type = BUDDY_EVENT_HEARTBEAT;
    return true;
}

static bool buddy_is_unsupported_folder_command(const char *command)
{
    return strcmp(command, "char_begin") == 0 || strncmp(command, "char_", 5) == 0 ||
           strncmp(command, "folder_", 7) == 0;
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

int buddy_protocol_parse(const char *json, size_t length, buddy_event_t *event)
{
    cJSON *root;
    const char *end = NULL;
    const char *command;
    unsigned running = 0;
    int result = BUDDY_EVENT_MALFORMED;

    if (json == NULL || event == NULL || buddy_json_contains_nul_escape(json, length)) {
        return BUDDY_EVENT_MALFORMED;
    }
    memset(event, 0, sizeof(*event));
    root = cJSON_ParseWithLengthOpts(json, length, &end, 0);
    if (root == NULL || end != json + length || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return BUDDY_EVENT_MALFORMED;
    }
    command = buddy_json_string(root, "cmd");
    if (command == NULL) {
        cJSON_Delete(root);
        return BUDDY_EVENT_MALFORMED;
    }
    if (strcmp(command, "heartbeat") == 0) {
        if (buddy_parse_heartbeat(root, event)) {
            result = (int)event->type;
        }
    } else if (strcmp(command, "prompt") == 0) {
        if (buddy_json_unsigned(root, "running", &running) && buddy_parse_prompt(root, running, event)) {
            result = event->type;
        }
    } else if (strcmp(command, "unpair") == 0) {
        event->type = BUDDY_EVENT_HEARTBEAT;
        event->heartbeat.connected = false;
        result = event->type;
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
    while (*value != '\0') {
        buddy_writer_char(writer, *value++);
    }
}

static void buddy_writer_json_string(buddy_json_writer_t *writer, const char *value)
{
    static const char digits[] = "0123456789abcdef";

    buddy_writer_char(writer, '"');
    while (value != NULL && *value != '\0') {
        unsigned char byte = (unsigned char)*value++;

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

    if (id == NULL || id[0] == '\0' || decision_name == NULL) {
        return 0;
    }
    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, "{\"cmd\":\"permission\",\"id\":");
    buddy_writer_json_string(&writer, id);
    buddy_writer_literal(&writer, ",\"decision\":");
    buddy_writer_json_string(&writer, decision_name);
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

    if (heartbeat == NULL) {
        return 0;
    }
    buddy_writer_init(&writer, json, size);
    buddy_writer_literal(&writer, "{\"cmd\":\"status\",\"name\":");
    buddy_writer_json_string(&writer, heartbeat->name);
    buddy_writer_literal(&writer, ",\"owner\":");
    buddy_writer_json_string(&writer, heartbeat->owner);
    buddy_writer_literal(&writer, ",\"status\":");
    buddy_writer_json_string(&writer, heartbeat->message);
    buddy_writer_literal(&writer, ",\"running\":");
    buddy_writer_u64(&writer, heartbeat->running);
    buddy_writer_literal(&writer, ",\"tokens\":");
    buddy_writer_u64(&writer, heartbeat->tokens);
    buddy_writer_literal(&writer, "}\n");
    return buddy_writer_finish(&writer);
}
