#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "buddy_ble.h"
#include "buddy_protocol.h"
#include "buddy_settings.h"
#include "buddy_state.h"
#include "buddy_ui.h"

#define BUDDY_CONTROL_QUEUE_DEPTH 8U
#define BUDDY_BUTTON_QUEUE_DEPTH 4U
#define BUDDY_RX_QUEUE_DEPTH 3U
#define BUDDY_APP_STACK_SIZE 12288U
#define BUDDY_APP_PRIORITY 5U
#define BUDDY_APP_TICK_MS 100U
#define BUDDY_BATTERY_SAMPLE_MS 10000ULL
#define BUDDY_SETTINGS_SERVICE_MS 1000ULL

typedef enum {
    BUDDY_CONTROL_KEY,
    BUDDY_CONTROL_BLE_CONNECTED,
    BUDDY_CONTROL_BLE_DISCONNECTED,
    BUDDY_CONTROL_BLE_PASSKEY,
    BUDDY_CONTROL_BLE_ENCRYPTION,
    BUDDY_CONTROL_BOND_DELETE_RESULT,
} buddy_control_type_t;

typedef struct {
    buddy_control_type_t type;
    union {
        struct {
            bsp_btn_t button;
            bsp_btn_ev_t event;
            uint32_t view_generation;
            buddy_page_t page;
            buddy_confirmation_t confirmation;
            buddy_settings_item_t settings_selection;
            bool approval_visible;
            bool passkey_visible;
            bool ble_enabled;
            uint32_t sensitive_connection_generation;
            char prompt_id[BUDDY_PROMPT_ID_MAX];
        } key;
        struct {
            uint32_t passkey;
            int status;
            bool secure;
            bool success;
        } ble;
    } data;
} buddy_control_event_t;

typedef struct {
    uint32_t generation;
    buddy_page_t page;
    buddy_confirmation_t confirmation;
    buddy_settings_item_t settings_selection;
    bool approval_visible;
    bool passkey_visible;
    bool ble_enabled;
    uint32_t sensitive_connection_generation;
    char prompt_id[BUDDY_PROMPT_ID_MAX];
    char prompt_tool[BUDDY_TOOL_MAX];
    char prompt_hint[BUDDY_HINT_MAX];
} buddy_rendered_view_t;

typedef struct {
    char data[BUDDY_JSON_LINE_MAX + 1U];
    size_t length;
    uint32_t connection_generation;
    bool in_use;
} buddy_rx_slot_t;

static const char *const TAG = "buddy_app";
static QueueHandle_t s_control_queue;
static QueueHandle_t s_button_queue;
static QueueHandle_t s_rx_queue;
static QueueSetHandle_t s_queue_set;
static TaskHandle_t s_app_task_handle;
static buddy_rx_slot_t s_rx_slots[BUDDY_RX_QUEUE_DEPTH];
static portMUX_TYPE s_rx_pool_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_view_lock = portMUX_INITIALIZER_UNLOCKED;
static buddy_rendered_view_t s_rendered_view;
static buddy_settings_snapshot_t s_initial_settings;
static bool s_initial_battery_available;
static atomic_bool s_ble_initialized;
static char s_tx_buffer[BUDDY_PROTOCOL_TX_MAX];

static uint64_t buddy_now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static void buddy_copy_text(char *destination, size_t destination_size, const char *source)
{
    size_t length = 0;

    if (destination_size == 0U) {
        return;
    }
    if (source != NULL) {
        while (length + 1U < destination_size && source[length] != '\0') {
            ++length;
        }
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

static void buddy_default_name(char name[BUDDY_NAME_MAX])
{
    uint8_t mac[6];

    if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
        (void)snprintf(name, BUDDY_NAME_MAX, "Claude-%02X%02X%02X", mac[3], mac[4], mac[5]);
    } else {
        buddy_copy_text(name, BUDDY_NAME_MAX, "Claude-Buddy");
    }
}

static buddy_rx_slot_t *buddy_rx_slot_acquire(void)
{
    buddy_rx_slot_t *slot = NULL;
    unsigned index;

    taskENTER_CRITICAL(&s_rx_pool_lock);
    for (index = 0; index < BUDDY_RX_QUEUE_DEPTH; ++index) {
        if (!s_rx_slots[index].in_use) {
            s_rx_slots[index].in_use = true;
            slot = &s_rx_slots[index];
            break;
        }
    }
    taskEXIT_CRITICAL(&s_rx_pool_lock);
    return slot;
}

static void buddy_rx_slot_release(buddy_rx_slot_t *slot)
{
    if (slot == NULL) {
        return;
    }
    taskENTER_CRITICAL(&s_rx_pool_lock);
    slot->length = 0;
    slot->connection_generation = 0;
    slot->data[0] = '\0';
    slot->in_use = false;
    taskEXIT_CRITICAL(&s_rx_pool_lock);
}

static void buddy_queue_control(const buddy_control_event_t *event)
{
    if (s_control_queue == NULL || event == NULL) {
        return;
    }
    (void)xQueueSend(s_control_queue, event, 0);
}

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *context)
{
    buddy_control_event_t control = {0};

    (void)context;
    if ((event != BSP_BTN_CLICK && event != BSP_BTN_LONG) || s_button_queue == NULL) {
        return;
    }
    control.type = BUDDY_CONTROL_KEY;
    control.data.key.button = button;
    control.data.key.event = event;
    taskENTER_CRITICAL(&s_view_lock);
    control.data.key.view_generation = s_rendered_view.generation;
    control.data.key.page = s_rendered_view.page;
    control.data.key.confirmation = s_rendered_view.confirmation;
    control.data.key.settings_selection = s_rendered_view.settings_selection;
    control.data.key.approval_visible = s_rendered_view.approval_visible;
    control.data.key.passkey_visible = s_rendered_view.passkey_visible;
    control.data.key.ble_enabled = s_rendered_view.ble_enabled;
    control.data.key.sensitive_connection_generation =
        s_rendered_view.sensitive_connection_generation;
    memcpy(control.data.key.prompt_id, s_rendered_view.prompt_id,
           sizeof(control.data.key.prompt_id));
    taskEXIT_CRITICAL(&s_view_lock);
    (void)xQueueSend(s_button_queue, &control, 0);
}

static void on_ble_event(const buddy_ble_event_t *event, void *context)
{
    buddy_control_event_t control = {0};

    (void)context;
    if (event == NULL) {
        return;
    }
    if (event->type == BUDDY_BLE_EVENT_RX_LINE) {
        buddy_rx_slot_t *slot;

        if (event->data.rx_line.length > BUDDY_JSON_LINE_MAX) {
            return;
        }
        slot = buddy_rx_slot_acquire();
        if (slot == NULL) {
            return;
        }
        memcpy(slot->data, event->data.rx_line.data, event->data.rx_line.length);
        slot->data[event->data.rx_line.length] = '\0';
        slot->length = event->data.rx_line.length;
        slot->connection_generation = event->data.rx_line.connection_generation;
        if (xQueueSend(s_rx_queue, &slot, 0) != pdTRUE) {
            buddy_rx_slot_release(slot);
        }
        return;
    }

    switch (event->type) {
    case BUDDY_BLE_EVENT_CONNECTED:
        control.type = BUDDY_CONTROL_BLE_CONNECTED;
        break;
    case BUDDY_BLE_EVENT_DISCONNECTED:
        control.type = BUDDY_CONTROL_BLE_DISCONNECTED;
        control.data.ble.status = event->data.disconnected.reason;
        break;
    case BUDDY_BLE_EVENT_PASSKEY:
        control.type = BUDDY_CONTROL_BLE_PASSKEY;
        control.data.ble.passkey = event->data.passkey.value;
        break;
    case BUDDY_BLE_EVENT_ENCRYPTION:
        control.type = BUDDY_CONTROL_BLE_ENCRYPTION;
        control.data.ble.status = event->data.encryption.status;
        control.data.ble.secure = event->data.encryption.status == 0 &&
                                  event->data.encryption.encrypted &&
                                  event->data.encryption.authenticated &&
                                  event->data.encryption.bonded;
        break;
    case BUDDY_BLE_EVENT_BOND_DELETE_RESULT:
        control.type = BUDDY_CONTROL_BOND_DELETE_RESULT;
        control.data.ble.status = event->data.bond_delete_result.status;
        control.data.ble.success = event->data.bond_delete_result.success;
        break;
    case BUDDY_BLE_EVENT_RX_LINE:
        return;
    }
    buddy_queue_control(&control);
}

static bool buddy_key_matches_rendered_view(const buddy_control_event_t *control)
{
    bool matches;

    taskENTER_CRITICAL(&s_view_lock);
    matches = control->data.key.view_generation != 0U &&
              control->data.key.view_generation == s_rendered_view.generation;
    taskEXIT_CRITICAL(&s_view_lock);
    return matches;
}

static bool buddy_key_matches_state(const buddy_control_event_t *control,
                                    const buddy_state_t *state)
{
    if (control->data.key.passkey_visible) {
        return false;
    }
    if (control->data.key.confirmation != BUDDY_CONFIRM_NONE) {
        return state->confirmation == control->data.key.confirmation &&
               state->confirmation_connection_generation ==
                   control->data.key.sensitive_connection_generation;
    }
    if (control->data.key.approval_visible) {
        return state->confirmation == BUDDY_CONFIRM_NONE && !state->passkey_visible &&
               strcmp(state->prompt.id, control->data.key.prompt_id) == 0 &&
               state->prompt_connection_generation ==
                   control->data.key.sensitive_connection_generation;
    }
    return state->confirmation == BUDDY_CONFIRM_NONE && !state->passkey_visible &&
           state->prompt.id[0] == '\0' && state->page == control->data.key.page &&
           (state->page != BUDDY_PAGE_SETTINGS ||
            (state->settings_selection == control->data.key.settings_selection &&
             (state->settings_selection != BUDDY_SETTINGS_BLE ||
              state->settings.ble_enabled == control->data.key.ble_enabled)));
}

static bool buddy_translate_key(const buddy_control_event_t *control, buddy_event_t *event)
{
    if (control == NULL || event == NULL || control->type != BUDDY_CONTROL_KEY) {
        return false;
    }
    memset(event, 0, sizeof(*event));
    switch (control->data.key.button) {
    case BSP_BTN_UP:
        event->key = BUDDY_KEY_UP;
        break;
    case BSP_BTN_DOWN:
        event->key = BUDDY_KEY_DOWN;
        break;
    case BSP_BTN_OK:
        event->key = BUDDY_KEY_OK;
        break;
    default:
        return false;
    }
    if (control->data.key.event == BSP_BTN_CLICK) {
        event->type = BUDDY_EVENT_KEY_CLICK;
    } else if (control->data.key.event == BSP_BTN_LONG) {
        event->type = BUDDY_EVENT_KEY_LONG;
    } else {
        return false;
    }

    if (control->data.key.approval_visible && control->data.key.prompt_id[0] != '\0') {
        event->has_observed_prompt_id = true;
        buddy_copy_text(event->observed_prompt_id, sizeof(event->observed_prompt_id),
                        control->data.key.prompt_id);
        event->observed_prompt_id_length = strlen(event->observed_prompt_id);
    } else if (event->type == BUDDY_EVENT_KEY_CLICK && event->key == BUDDY_KEY_OK) {
        event->has_observed_prompt_id = true;
    }
    return true;
}

static bool buddy_control_to_event(const buddy_control_event_t *control,
                                   const buddy_state_t *state, buddy_event_t *event)
{
    memset(event, 0, sizeof(*event));
    if (control->type == BUDDY_CONTROL_KEY) {
        return buddy_key_matches_rendered_view(control) && buddy_key_matches_state(control, state) &&
               buddy_translate_key(control, event);
    }
    switch (control->type) {
    case BUDDY_CONTROL_BLE_CONNECTED:
        event->type = BUDDY_EVENT_BLE_CONNECTED;
        break;
    case BUDDY_CONTROL_BLE_DISCONNECTED:
        event->type = BUDDY_EVENT_BLE_DISCONNECTED;
        event->ble.status = control->data.ble.status;
        break;
    case BUDDY_CONTROL_BLE_PASSKEY:
        event->type = BUDDY_EVENT_BLE_PASSKEY;
        event->ble.passkey = control->data.ble.passkey;
        break;
    case BUDDY_CONTROL_BLE_ENCRYPTION:
        event->type = BUDDY_EVENT_BLE_ENCRYPTION;
        event->ble.status = control->data.ble.status;
        event->ble.secure = control->data.ble.secure;
        break;
    case BUDDY_CONTROL_BOND_DELETE_RESULT:
        event->type = BUDDY_EVENT_BOND_DELETE_RESULT;
        event->ble.status = control->data.ble.status;
        event->ble.success = control->data.ble.success;
        break;
    case BUDDY_CONTROL_KEY:
        return false;
    }
    return true;
}

static esp_err_t buddy_send_command_ack(const char *command, bool ok, const char *error,
                                        uint32_t connection_generation)
{
    int length = buddy_protocol_command_ack_json(s_tx_buffer, sizeof(s_tx_buffer),
                                                 command, ok, error);

    return length > 0
               ? buddy_ble_send_for_generation(s_tx_buffer, (size_t)length,
                                               connection_generation)
               : ESP_ERR_INVALID_SIZE;
}

static void buddy_sample_battery(buddy_state_t *state)
{
    int percent;
    int millivolts;

    if (!s_initial_battery_available) {
        state->battery_available = false;
        return;
    }
    percent = bsp_battery_soc();
    millivolts = bsp_battery_mv();
    if (percent < 0 || percent > 100 || millivolts < 0 || millivolts > UINT16_MAX) {
        state->battery_available = false;
        return;
    }
    state->battery_available = true;
    state->battery_percent = (uint8_t)percent;
    state->battery_mv = (uint16_t)millivolts;
}

static void buddy_reset_transient_state(buddy_state_t *state, const char *message)
{
    buddy_settings_snapshot_t settings;
    bool battery_available = state->battery_available;
    uint8_t battery_percent = state->battery_percent;
    uint16_t battery_mv = state->battery_mv;

    if (buddy_settings_load(&settings) != ESP_OK) {
        settings = state->settings;
    }
    if (settings.name[0] == '\0') {
        buddy_default_name(settings.name);
    }
    buddy_state_init(state, &settings);
    state->battery_available = battery_available;
    state->battery_percent = battery_percent;
    state->battery_mv = battery_mv;
    state->ble_connected = buddy_ble_is_connected();
    state->ble_encrypted = buddy_ble_is_encrypted();
    buddy_copy_text(state->message, sizeof(state->message), message);
}

static esp_err_t buddy_ble_ensure_initialized(void)
{
    const buddy_ble_config_t config = {
        .event_cb = on_ble_event,
        .event_context = NULL,
    };
    esp_err_t err;

    if (atomic_load(&s_ble_initialized)) {
        return ESP_OK;
    }
    err = buddy_ble_init(&config);
    if (err == ESP_OK) {
        atomic_store(&s_ble_initialized, true);
    }
    return err;
}

static void buddy_send_status(const buddy_state_t *state, uint32_t connection_generation)
{
    buddy_settings_snapshot_t settings;
    buddy_status_report_t status = {0};
    int length;

    if (buddy_settings_load(&settings) != ESP_OK) {
        settings = state->settings;
    }
    buddy_copy_text(status.name, sizeof(status.name), state->name);
    buddy_copy_text(status.owner, sizeof(status.owner), state->owner);
    status.encrypted = atomic_load(&s_ble_initialized) && buddy_ble_is_encrypted();
    status.battery_available = state->battery_available;
    status.battery_percent = state->battery_percent;
    status.battery_mv = state->battery_mv;
    status.uptime_ms = buddy_now_ms();
    status.free_heap = esp_get_free_heap_size();
    status.approval_count = settings.approval_count;
    status.denial_count = settings.denial_count;
    length = buddy_protocol_device_status_json(s_tx_buffer, sizeof(s_tx_buffer), &status);
    if (length == 0 || buddy_ble_send_for_generation(s_tx_buffer, (size_t)length,
                                                     connection_generation) != ESP_OK) {
        ESP_LOGW(TAG, "status response failed");
    }
}

static void buddy_set_ble_enabled(buddy_state_t *state, bool enabled)
{
    bool old_enabled = !enabled;
    esp_err_t err;

    if (buddy_settings_set_ble_enabled(enabled) != ESP_OK) {
        state->settings.ble_enabled = old_enabled;
        buddy_copy_text(state->message, sizeof(state->message), "BLE setting failed");
        return;
    }
    if (enabled) {
        err = buddy_ble_ensure_initialized();
        if (err == ESP_OK) {
            err = buddy_ble_start();
        }
    } else {
        err = atomic_load(&s_ble_initialized) ? buddy_ble_stop() : ESP_OK;
    }
    if (err != ESP_OK) {
        (void)buddy_settings_set_ble_enabled(old_enabled);
        state->settings.ble_enabled = old_enabled;
        buddy_copy_text(state->message, sizeof(state->message), "BLE update failed");
        return;
    }
    state->settings.ble_enabled = enabled;
    (void)buddy_settings_flush(true);
}

static void buddy_factory_reset(buddy_state_t *state)
{
    buddy_settings_snapshot_t defaults = {0};

    if (buddy_settings_factory_reset() != ESP_OK) {
        buddy_copy_text(state->message, sizeof(state->message), "Factory reset failed");
        return;
    }
    defaults.ble_enabled = true;
    buddy_default_name(defaults.name);
    buddy_state_init(state, &defaults);
    buddy_sample_battery(state);
    buddy_copy_text(state->message, sizeof(state->message), "Factory reset complete");
    if (!atomic_load(&s_ble_initialized)) {
        buddy_set_ble_enabled(state, true);
    } else if (buddy_ble_start() != ESP_OK) {
        buddy_copy_text(state->message, sizeof(state->message), "BLE restart failed");
    }
}

static void buddy_execute_action(buddy_state_t *state, const buddy_action_t *action)
{
    int length;

    switch (action->type) {
    case BUDDY_ACTION_PERMISSION:
        length = buddy_protocol_permission_json(s_tx_buffer, sizeof(s_tx_buffer),
                                                action->permission.id,
                                                action->permission.decision);
        if (length == 0 ||
            buddy_ble_send_for_generation(s_tx_buffer, (size_t)length,
                                          action->permission.connection_generation) != ESP_OK) {
            buddy_copy_text(state->message, sizeof(state->message), "Permission send failed");
            ESP_LOGW(TAG, "permission response failed");
        } else {
            buddy_settings_record_permission(action->permission.decision);
        }
        break;
    case BUDDY_ACTION_SETTINGS:
        (void)buddy_settings_set_highest_celebrated_level(
            action->settings.highest_celebrated_level);
        break;
    case BUDDY_ACTION_STATUS:
        buddy_send_status(state, action->connection_generation);
        break;
    case BUDDY_ACTION_UNPAIR_CONFIRMED:
        if (action->confirmation_acknowledge &&
            buddy_send_command_ack("unpair", true, NULL,
                                   action->connection_generation) != ESP_OK) {
            buddy_copy_text(state->message, sizeof(state->message),
                            "Unpair confirmation expired");
            break;
        }
        if (buddy_ble_ensure_initialized() != ESP_OK || buddy_ble_delete_bonds() != ESP_OK) {
            buddy_copy_text(state->message, sizeof(state->message), "Unpair failed");
        } else {
            buddy_reset_transient_state(state, "Unpairing");
        }
        break;
    case BUDDY_ACTION_FACTORY_RESET_CONFIRMED:
        buddy_factory_reset(state);
        break;
    case BUDDY_ACTION_BLE_TOGGLE:
        buddy_set_ble_enabled(state, action->ble_enabled);
        break;
    case BUDDY_ACTION_NONE:
    case BUDDY_ACTION_UI_REFRESH:
    case BUDDY_ACTION_UI_SCROLL:
        break;
    }
}

static bool buddy_rendered_view_same(const buddy_rendered_view_t *left,
                                     const buddy_rendered_view_t *right)
{
    return left->page == right->page && left->confirmation == right->confirmation &&
           left->settings_selection == right->settings_selection &&
           left->approval_visible == right->approval_visible &&
           left->passkey_visible == right->passkey_visible &&
           left->ble_enabled == right->ble_enabled &&
           left->sensitive_connection_generation ==
               right->sensitive_connection_generation &&
           strcmp(left->prompt_id, right->prompt_id) == 0 &&
           strcmp(left->prompt_tool, right->prompt_tool) == 0 &&
           strcmp(left->prompt_hint, right->prompt_hint) == 0;
}

static void buddy_publish_rendered_view(const buddy_ui_snapshot_t *snapshot)
{
    buddy_rendered_view_t next = {
        .page = snapshot->page,
        .confirmation = snapshot->confirmation,
        .settings_selection = snapshot->settings_selection,
        .approval_visible = !snapshot->confirmation_pending && !snapshot->passkey_visible &&
                            !snapshot->approval_locked && snapshot->prompt_id[0] != '\0',
        .passkey_visible = snapshot->passkey_visible,
        .ble_enabled = snapshot->ble_enabled,
        .sensitive_connection_generation = snapshot->confirmation_pending
                                               ? snapshot->confirmation_connection_generation
                                               : snapshot->prompt_connection_generation,
    };

    if (next.approval_visible) {
        buddy_copy_text(next.prompt_id, sizeof(next.prompt_id), snapshot->prompt_id);
        buddy_copy_text(next.prompt_tool, sizeof(next.prompt_tool), snapshot->prompt_tool);
        buddy_copy_text(next.prompt_hint, sizeof(next.prompt_hint), snapshot->prompt_hint);
    }
    taskENTER_CRITICAL(&s_view_lock);
    if (!buddy_rendered_view_same(&next, &s_rendered_view)) {
        next.generation = s_rendered_view.generation + 1U;
        if (next.generation == 0U) {
            next.generation = 1U;
        }
    } else {
        next.generation = s_rendered_view.generation != 0U ? s_rendered_view.generation : 1U;
    }
    s_rendered_view = next;
    taskEXIT_CRITICAL(&s_view_lock);
}

static void buddy_render(buddy_state_t *state, const buddy_action_t *action, uint64_t now_ms)
{
    static buddy_ui_snapshot_t snapshot;

    buddy_state_snapshot(state, &snapshot);
    if (!bsp_lvgl_lock(1000)) {
        return;
    }
    buddy_ui_render(&snapshot);
    if (action->type == BUDDY_ACTION_UI_SCROLL) {
        buddy_ui_scroll(action->scroll_delta);
    }
    buddy_ui_tick(now_ms);
    bsp_lvgl_unlock();
    buddy_publish_rendered_view(&snapshot);
}

static bool buddy_handle_parsed_event(buddy_state_t *state, buddy_event_t *event,
                                      uint32_t connection_generation, uint64_t now_ms,
                                      buddy_action_t *action)
{
    esp_err_t err = ESP_OK;
    bool acknowledge = false;

    event->ble.connection_generation = connection_generation;

    if (event->type == BUDDY_EVENT_NAME) {
        acknowledge = true;
        err = event->command.value_truncated ? ESP_ERR_INVALID_ARG
                                             : buddy_settings_set_name(event->command.value);
    } else if (event->type == BUDDY_EVENT_OWNER) {
        acknowledge = true;
        err = event->command.value_truncated ? ESP_ERR_INVALID_ARG
                                             : buddy_settings_set_owner(event->command.value);
    } else if (event->type == BUDDY_EVENT_TIME) {
        acknowledge = true;
        err = event->command.value_truncated ? ESP_ERR_INVALID_ARG : ESP_OK;
    }

    if (err == ESP_OK) {
        buddy_state_reduce(state, event, now_ms, action);
        if (event->type == BUDDY_EVENT_NAME) {
            buddy_copy_text(state->settings.name, sizeof(state->settings.name),
                            event->command.value);
        } else if (event->type == BUDDY_EVENT_OWNER) {
            buddy_copy_text(state->settings.owner, sizeof(state->settings.owner),
                            event->command.value);
        }
    }
    if (acknowledge) {
        (void)buddy_send_command_ack(event->command.name, err == ESP_OK,
                                     err == ESP_OK ? NULL : "invalid value",
                                     connection_generation);
    }
    return err == ESP_OK;
}

static bool buddy_handle_rx(buddy_state_t *state, buddy_rx_slot_t *slot,
                            buddy_event_t *event, uint64_t now_ms,
                            buddy_action_t *action)
{
    int result;

    if (!buddy_ble_is_generation_secure(slot->connection_generation)) {
        return false;
    }
    result = buddy_protocol_parse(slot->data, slot->length, event);

    if (result >= BUDDY_EVENT_NONE) {
        return buddy_handle_parsed_event(state, event, slot->connection_generation,
                                         now_ms, action);
    } else if (event->command.name[0] != '\0') {
        const char *error = result == BUDDY_EVENT_UNSUPPORTED_COMMAND
                                ? "unsupported in phase 1"
                                : (result == BUDDY_EVENT_UNKNOWN_COMMAND ? "unknown command"
                                                                          : "invalid request");
        (void)buddy_send_command_ack(event->command.name, false, error,
                                     slot->connection_generation);
    } else {
        ESP_LOGW(TAG, "malformed BLE JSON line (%u bytes)", (unsigned)slot->length);
    }
    return false;
}

static void buddy_app_task(void *context)
{
    static buddy_state_t state;
    static buddy_action_t action;
    static buddy_event_t event;
    uint64_t last_battery_ms = 0;
    uint64_t last_settings_ms = 0;

    (void)context;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    buddy_state_init(&state, &s_initial_settings);
    buddy_sample_battery(&state);

    for (;;) {
        QueueSetMemberHandle_t ready =
            xQueueSelectFromSet(s_queue_set, pdMS_TO_TICKS(BUDDY_APP_TICK_MS));
        uint64_t now_ms = buddy_now_ms();
        bool reduced = false;

        memset(&action, 0, sizeof(action));

        if (ready == s_control_queue || ready == s_button_queue) {
            buddy_control_event_t control;
            QueueHandle_t queue = ready == s_control_queue ? s_control_queue : s_button_queue;

            if (xQueueReceive(queue, &control, 0) == pdTRUE &&
                buddy_control_to_event(&control, &state, &event)) {
                buddy_state_reduce(&state, &event, now_ms, &action);
                reduced = true;
            }
        } else if (ready == s_rx_queue) {
            buddy_rx_slot_t *slot = NULL;

            if (xQueueReceive(s_rx_queue, &slot, 0) == pdTRUE && slot != NULL) {
                reduced = buddy_handle_rx(&state, slot, &event, now_ms, &action);
                buddy_rx_slot_release(slot);
            }
        }
        if (!reduced) {
            const buddy_event_t tick = {.type = BUDDY_EVENT_TICK};

            buddy_state_reduce(&state, &tick, now_ms, &action);
        }

        buddy_execute_action(&state, &action);
        if (now_ms - last_battery_ms >= BUDDY_BATTERY_SAMPLE_MS) {
            buddy_sample_battery(&state);
            last_battery_ms = now_ms;
        }
        if (now_ms - last_settings_ms >= BUDDY_SETTINGS_SERVICE_MS) {
            if (buddy_settings_flush(false) != ESP_OK) {
                ESP_LOGW(TAG, "settings flush failed");
            }
            last_settings_ms = now_ms;
        }
        state.ble_connected = atomic_load(&s_ble_initialized) && buddy_ble_is_connected();
        state.ble_encrypted = atomic_load(&s_ble_initialized) && buddy_ble_is_encrypted();
        buddy_render(&state, &action, now_ms);
    }
}

static esp_err_t buddy_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Claude Desktop Buddy starting");
    if (buddy_nvs_init() != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed");
        return;
    }
    err = bsp_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C initialization failed: %s", esp_err_to_name(err));
    }
    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG,
                 "Display/LVGL initialization failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);
    s_initial_battery_available = bsp_battery_init() == ESP_OK;

    if (buddy_settings_init() != ESP_OK || buddy_settings_load(&s_initial_settings) != ESP_OK) {
        ESP_LOGE(TAG, "settings initialization failed");
        return;
    }
    if (s_initial_settings.name[0] == '\0') {
        buddy_default_name(s_initial_settings.name);
    }

    s_control_queue = xQueueCreate(BUDDY_CONTROL_QUEUE_DEPTH, sizeof(buddy_control_event_t));
    s_button_queue = xQueueCreate(BUDDY_BUTTON_QUEUE_DEPTH, sizeof(buddy_control_event_t));
    s_rx_queue = xQueueCreate(BUDDY_RX_QUEUE_DEPTH, sizeof(buddy_rx_slot_t *));
    s_queue_set = xQueueCreateSet(BUDDY_CONTROL_QUEUE_DEPTH + BUDDY_BUTTON_QUEUE_DEPTH +
                                  BUDDY_RX_QUEUE_DEPTH);
    if (s_control_queue == NULL || s_button_queue == NULL || s_rx_queue == NULL ||
        s_queue_set == NULL ||
        xQueueAddToSet(s_control_queue, s_queue_set) != pdPASS ||
        xQueueAddToSet(s_button_queue, s_queue_set) != pdPASS ||
        xQueueAddToSet(s_rx_queue, s_queue_set) != pdPASS ||
        xTaskCreate(buddy_app_task, "buddy_app", BUDDY_APP_STACK_SIZE, NULL,
                    BUDDY_APP_PRIORITY, &s_app_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "application queue/task initialization failed");
        return;
    }
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "button initialization failed; approvals remain fail-closed");
    }
    if (s_initial_settings.ble_enabled) {
        err = buddy_ble_ensure_initialized();
        if (err == ESP_OK) {
            err = buddy_ble_start();
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "BLE initialization failed: %s", esp_err_to_name(err));
        }
    }
    xTaskNotifyGive(s_app_task_handle);
}
