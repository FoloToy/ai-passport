#include "buddy_app_logic.h"

#include <string.h>

#define BUDDY_APP_CLASSIFIER_DEPTH_MAX 16U
#define BUDDY_APP_HEARTBEAT_TOTAL (1U << 0)
#define BUDDY_APP_HEARTBEAT_RUNNING (1U << 1)
#define BUDDY_APP_HEARTBEAT_WAITING (1U << 2)
#define BUDDY_APP_HEARTBEAT_MSG (1U << 3)
#define BUDDY_APP_HEARTBEAT_ENTRIES (1U << 4)
#define BUDDY_APP_HEARTBEAT_TOKENS (1U << 5)
#define BUDDY_APP_HEARTBEAT_TOKENS_TODAY (1U << 6)
#define BUDDY_APP_HEARTBEAT_REQUIRED ((1U << 7) - 1U)

typedef struct {
    const char *cursor;
    const char *end;
    unsigned command_keys;
    unsigned heartbeat_keys;
    bool prompt_key_seen;
} buddy_app_json_scan_t;

static void buddy_app_skip_space(buddy_app_json_scan_t *scan)
{
    while (scan->cursor < scan->end &&
           (*scan->cursor == ' ' || *scan->cursor == '\t' ||
            *scan->cursor == '\r' || *scan->cursor == '\n')) {
        ++scan->cursor;
    }
}

static bool buddy_app_scan_utf8_codepoint(buddy_app_json_scan_t *scan)
{
    const unsigned char *cursor = (const unsigned char *)scan->cursor;
    size_t remaining = (size_t)(scan->end - scan->cursor);
    size_t length;

    if (remaining == 0U) {
        return false;
    }
    if (cursor[0] < 0x80U) {
        ++scan->cursor;
        return true;
    }
    if (cursor[0] >= 0xc2U && cursor[0] <= 0xdfU) {
        length = 2U;
    } else if (cursor[0] == 0xe0U) {
        if (remaining < 2U || cursor[1] < 0xa0U || cursor[1] > 0xbfU) {
            return false;
        }
        length = 3U;
    } else if ((cursor[0] >= 0xe1U && cursor[0] <= 0xecU) ||
               (cursor[0] >= 0xeeU && cursor[0] <= 0xefU)) {
        length = 3U;
    } else if (cursor[0] == 0xedU) {
        if (remaining < 2U || cursor[1] < 0x80U || cursor[1] > 0x9fU) {
            return false;
        }
        length = 3U;
    } else if (cursor[0] == 0xf0U) {
        if (remaining < 2U || cursor[1] < 0x90U || cursor[1] > 0xbfU) {
            return false;
        }
        length = 4U;
    } else if (cursor[0] >= 0xf1U && cursor[0] <= 0xf3U) {
        length = 4U;
    } else if (cursor[0] == 0xf4U) {
        if (remaining < 2U || cursor[1] < 0x80U || cursor[1] > 0x8fU) {
            return false;
        }
        length = 4U;
    } else {
        return false;
    }
    if (remaining < length) {
        return false;
    }
    for (size_t index = 1U; index < length; ++index) {
        if (cursor[index] < 0x80U || cursor[index] > 0xbfU) {
            return false;
        }
    }
    scan->cursor += length;
    return true;
}

static bool buddy_app_scan_string(buddy_app_json_scan_t *scan,
                                  const char **value, size_t *length)
{
    const char *start;

    if (scan->cursor >= scan->end || *scan->cursor != '"') {
        return false;
    }
    start = ++scan->cursor;
    while (scan->cursor < scan->end && *scan->cursor != '"') {
        unsigned char byte = (unsigned char)*scan->cursor;

        /* Any escape is deliberately ambiguous and therefore priority. */
        if (byte == '\\' || byte < 0x20U) {
            return false;
        }
        if (!buddy_app_scan_utf8_codepoint(scan)) {
            return false;
        }
    }
    if (scan->cursor >= scan->end) {
        return false;
    }
    *value = start;
    *length = (size_t)(scan->cursor - start);
    ++scan->cursor;
    return true;
}

static bool buddy_app_key_equals(const char *key, size_t key_length,
                                 const char *expected)
{
    size_t expected_length = strlen(expected);

    return key_length == expected_length && memcmp(key, expected, key_length) == 0;
}

static bool buddy_app_scan_value(buddy_app_json_scan_t *scan, unsigned depth,
                                 bool root_command_value);

static void buddy_app_record_heartbeat_key(buddy_app_json_scan_t *scan,
                                           const char *key, size_t key_length)
{
    static const struct {
        const char *name;
        unsigned bit;
    } fields[] = {
        {"total", BUDDY_APP_HEARTBEAT_TOTAL},
        {"running", BUDDY_APP_HEARTBEAT_RUNNING},
        {"waiting", BUDDY_APP_HEARTBEAT_WAITING},
        {"msg", BUDDY_APP_HEARTBEAT_MSG},
        {"entries", BUDDY_APP_HEARTBEAT_ENTRIES},
        {"tokens", BUDDY_APP_HEARTBEAT_TOKENS},
        {"tokens_today", BUDDY_APP_HEARTBEAT_TOKENS_TODAY},
    };
    size_t index;

    for (index = 0; index < sizeof(fields) / sizeof(fields[0]); ++index) {
        if (buddy_app_key_equals(key, key_length, fields[index].name)) {
            scan->heartbeat_keys |= fields[index].bit;
            return;
        }
    }
}

static bool buddy_app_scan_object(buddy_app_json_scan_t *scan, unsigned depth,
                                  bool root_object)
{
    if (depth >= BUDDY_APP_CLASSIFIER_DEPTH_MAX || scan->cursor >= scan->end ||
        *scan->cursor != '{') {
        return false;
    }
    ++scan->cursor;
    buddy_app_skip_space(scan);
    if (scan->cursor < scan->end && *scan->cursor == '}') {
        ++scan->cursor;
        return true;
    }
    for (;;) {
        const char *key;
        size_t key_length;
        bool command_key;

        if (!buddy_app_scan_string(scan, &key, &key_length)) {
            return false;
        }
        command_key = buddy_app_key_equals(key, key_length, "cmd");
        if (command_key) {
            ++scan->command_keys;
            if (!root_object || scan->command_keys != 1U) {
                return false;
            }
        }
        if (root_object) {
            buddy_app_record_heartbeat_key(scan, key, key_length);
        }
        if (root_object && buddy_app_key_equals(key, key_length, "prompt")) {
            scan->prompt_key_seen = true;
        }
        buddy_app_skip_space(scan);
        if (scan->cursor >= scan->end || *scan->cursor != ':') {
            return false;
        }
        ++scan->cursor;
        buddy_app_skip_space(scan);
        if (!buddy_app_scan_value(scan, depth + 1U, command_key)) {
            return false;
        }
        buddy_app_skip_space(scan);
        if (scan->cursor >= scan->end) {
            return false;
        }
        if (*scan->cursor == '}') {
            ++scan->cursor;
            return true;
        }
        if (*scan->cursor != ',') {
            return false;
        }
        ++scan->cursor;
        buddy_app_skip_space(scan);
    }
}

static bool buddy_app_scan_array(buddy_app_json_scan_t *scan, unsigned depth)
{
    if (depth >= BUDDY_APP_CLASSIFIER_DEPTH_MAX || scan->cursor >= scan->end ||
        *scan->cursor != '[') {
        return false;
    }
    ++scan->cursor;
    buddy_app_skip_space(scan);
    if (scan->cursor < scan->end && *scan->cursor == ']') {
        ++scan->cursor;
        return true;
    }
    for (;;) {
        if (!buddy_app_scan_value(scan, depth + 1U, false)) {
            return false;
        }
        buddy_app_skip_space(scan);
        if (scan->cursor >= scan->end) {
            return false;
        }
        if (*scan->cursor == ']') {
            ++scan->cursor;
            return true;
        }
        if (*scan->cursor != ',') {
            return false;
        }
        ++scan->cursor;
        buddy_app_skip_space(scan);
    }
}

static bool buddy_app_scan_number(buddy_app_json_scan_t *scan)
{
    const char *cursor = scan->cursor;

    if (cursor < scan->end && *cursor == '-') {
        ++cursor;
    }
    if (cursor >= scan->end) {
        return false;
    }
    if (*cursor == '0') {
        ++cursor;
    } else if (*cursor >= '1' && *cursor <= '9') {
        do {
            ++cursor;
        } while (cursor < scan->end && *cursor >= '0' && *cursor <= '9');
    } else {
        return false;
    }
    if (cursor < scan->end && *cursor == '.') {
        ++cursor;
        if (cursor >= scan->end || *cursor < '0' || *cursor > '9') {
            return false;
        }
        do {
            ++cursor;
        } while (cursor < scan->end && *cursor >= '0' && *cursor <= '9');
    }
    if (cursor < scan->end && (*cursor == 'e' || *cursor == 'E')) {
        ++cursor;
        if (cursor < scan->end && (*cursor == '+' || *cursor == '-')) {
            ++cursor;
        }
        if (cursor >= scan->end || *cursor < '0' || *cursor > '9') {
            return false;
        }
        do {
            ++cursor;
        } while (cursor < scan->end && *cursor >= '0' && *cursor <= '9');
    }
    scan->cursor = cursor;
    return true;
}

static bool buddy_app_scan_literal(buddy_app_json_scan_t *scan, const char *literal)
{
    size_t length = strlen(literal);

    if ((size_t)(scan->end - scan->cursor) < length ||
        memcmp(scan->cursor, literal, length) != 0) {
        return false;
    }
    scan->cursor += length;
    return true;
}

static bool buddy_app_scan_value(buddy_app_json_scan_t *scan, unsigned depth,
                                 bool root_command_value)
{
    if (scan->cursor >= scan->end) {
        return false;
    }
    if (root_command_value) {
        const char *value;
        size_t length;

        if (!buddy_app_scan_string(scan, &value, &length)) {
            return false;
        }
        return true;
    }
    switch (*scan->cursor) {
    case '{':
        return buddy_app_scan_object(scan, depth, false);
    case '[':
        return buddy_app_scan_array(scan, depth);
    case '"': {
        const char *value;
        size_t length;

        return buddy_app_scan_string(scan, &value, &length);
    }
    case 't':
        return buddy_app_scan_literal(scan, "true");
    case 'f':
        return buddy_app_scan_literal(scan, "false");
    case 'n':
        return buddy_app_scan_literal(scan, "null");
    default:
        return buddy_app_scan_number(scan);
    }
}

buddy_app_rx_class_t buddy_app_classify_rx(const char *data, size_t length)
{
    buddy_app_json_scan_t scan = {
        .cursor = data,
        .end = data != NULL ? data + length : NULL,
    };

    if (data == NULL || length == 0U) {
        return BUDDY_APP_RX_PRIORITY;
    }
    buddy_app_skip_space(&scan);
    if (!buddy_app_scan_object(&scan, 0, true)) {
        return BUDDY_APP_RX_PRIORITY;
    }
    buddy_app_skip_space(&scan);
    return scan.cursor == scan.end && scan.command_keys == 0U &&
                   scan.heartbeat_keys == BUDDY_APP_HEARTBEAT_REQUIRED &&
                   !scan.prompt_key_seen
               ? BUDDY_APP_RX_NORMAL_HEARTBEAT
               : BUDDY_APP_RX_PRIORITY;
}

buddy_app_rx_overflow_action_t buddy_app_rx_overflow_policy(
    buddy_app_rx_class_t incoming, bool slot_available, bool normal_pending,
    bool priority_pending, bool priority_full)
{
    if (incoming == BUDDY_APP_RX_NORMAL_HEARTBEAT) {
        if (normal_pending) {
            return BUDDY_APP_RX_REPLACE_NORMAL;
        }
        return slot_available ? BUDDY_APP_RX_ENQUEUE : BUDDY_APP_RX_DROP;
    }
    if (priority_full) {
        return BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY;
    }
    if (slot_available) {
        return BUDDY_APP_RX_ENQUEUE;
    }
    if (normal_pending) {
        return BUDDY_APP_RX_REPLACE_NORMAL;
    }
    return priority_pending ? BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY
                            : BUDDY_APP_RX_DROP;
}

void buddy_app_rx_retry_init(buddy_app_rx_retry_state_t *state,
                             buddy_app_rx_class_t incoming)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->incoming = incoming;
}

buddy_app_rx_overflow_action_t buddy_app_rx_retry_next(
    const buddy_app_rx_retry_state_t *state, bool slot_available,
    bool normal_pending, bool priority_pending, bool priority_full)
{
    if (state == NULL) {
        return BUDDY_APP_RX_DROP;
    }
    if (state->incoming == BUDDY_APP_RX_NORMAL_HEARTBEAT) {
        if (normal_pending && !state->normal_attempted) {
            return BUDDY_APP_RX_REPLACE_NORMAL;
        }
        return slot_available ? BUDDY_APP_RX_ENQUEUE : BUDDY_APP_RX_DROP;
    }
    if (priority_full && !state->priority_failed) {
        return BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY;
    }
    if (slot_available) {
        return BUDDY_APP_RX_ENQUEUE;
    }
    if (normal_pending && !state->normal_attempted) {
        return BUDDY_APP_RX_REPLACE_NORMAL;
    }
    if (priority_pending && !state->priority_failed) {
        return BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY;
    }
    return BUDDY_APP_RX_DROP;
}

void buddy_app_rx_retry_record_eviction(buddy_app_rx_retry_state_t *state,
                                        buddy_app_rx_overflow_action_t action,
                                        bool succeeded)
{
    if (state == NULL) {
        return;
    }
    if (action == BUDDY_APP_RX_REPLACE_NORMAL) {
        state->normal_attempted = true;
        if (succeeded && state->normal_evictions < UINT32_MAX) {
            ++state->normal_evictions;
        }
    } else if (action == BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY) {
        state->priority_failed = !succeeded;
        if (succeeded && state->priority_evictions < UINT32_MAX) {
            ++state->priority_evictions;
        }
    }
}

uint64_t buddy_app_rx_retry_overflow_count(const buddy_app_rx_retry_state_t *state)
{
    return state == NULL ? 0U
                         : (uint64_t)state->normal_evictions +
                               (uint64_t)state->priority_evictions;
}

static bool buddy_app_copy_bounded(char *destination, size_t destination_size,
                                   const char *source, size_t source_size)
{
    size_t length = 0;

    while (length < source_size && source[length] != '\0') {
        ++length;
    }
    if (length == source_size || length >= destination_size) {
        return false;
    }
    memcpy(destination, source, length + 1U);
    return true;
}

bool buddy_app_build_status(buddy_status_report_t *report,
                            const buddy_settings_snapshot_t *settings,
                            const buddy_app_status_runtime_t *runtime)
{
    if (report == NULL || settings == NULL || runtime == NULL) {
        return false;
    }
    memset(report, 0, sizeof(*report));
    if (!buddy_app_copy_bounded(report->name, sizeof(report->name),
                                settings->name, sizeof(settings->name))) {
        return false;
    }
    report->approval_count = settings->approval_count;
    report->denial_count = settings->denial_count;
    report->encrypted = runtime->encrypted;
    report->battery_available = runtime->battery_available;
    report->battery_percent = runtime->battery_percent;
    report->battery_mv = runtime->battery_mv;
    report->uptime_ms = runtime->uptime_ms;
    report->free_heap = runtime->free_heap;
    report->queue_overflow_count = runtime->queue_overflow_count;
    report->highest_celebrated_level = settings->highest_celebrated_level;
    return true;
}

buddy_app_ble_transport_result_t buddy_app_set_ble_transport(
    const buddy_app_ble_transport_ops_t *ops, bool enabled)
{
    buddy_app_ble_transport_result_t result = {
        .request_status = ESP_ERR_INVALID_ARG,
        .effective_enabled = !enabled,
    };

    if (ops == NULL || ops->start == NULL || ops->stop == NULL) {
        return result;
    }
    if (enabled) {
        result.request_status = ops->start(ops->context);
        result.effective_enabled = result.request_status == ESP_OK;
        return result;
    }
    result.request_status = ops->stop(ops->context);
    if (result.request_status == ESP_OK) {
        result.effective_enabled = false;
        return result;
    }
    result.recovery_attempted = true;
    result.effective_enabled = ops->start(ops->context) == ESP_OK;
    return result;
}
