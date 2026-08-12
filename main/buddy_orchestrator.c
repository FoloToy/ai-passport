#include "buddy_orchestrator.h"

#include <string.h>

static void buddy_orchestrator_copy(char *destination, size_t destination_size,
                                    const char *source)
{
    size_t length = 0;

    if (destination_size == 0U) {
        return;
    }
    while (source != NULL && length + 1U < destination_size && source[length] != '\0') {
        ++length;
    }
    if (length != 0U) {
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

static esp_err_t buddy_orchestrator_send_json(const buddy_orchestrator_ops_t *ops,
                                              const char *json, int length,
                                              uint32_t generation)
{
    if (ops == NULL || ops->send == NULL || length <= 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ops->send(ops->context, json, (size_t)length, generation);
}

static esp_err_t buddy_orchestrator_send_ack(const buddy_orchestrator_ops_t *ops,
                                             const char *command, bool ok,
                                             const char *error, uint32_t generation)
{
    char json[BUDDY_PROTOCOL_TX_MAX];
    int length = buddy_protocol_command_ack_json(json, sizeof(json), command, ok, error);

    return buddy_orchestrator_send_json(ops, json, length, generation);
}

bool buddy_orchestrator_process_rx(buddy_state_t *state,
                                   const buddy_orchestrator_ops_t *ops,
                                   const char *json, size_t length,
                                   uint32_t connection_generation,
                                   uint64_t now_ms, buddy_action_t *action)
{
    buddy_event_t event;
    esp_err_t err = ESP_OK;
    bool setting_command = false;
    int parsed;

    if (state == NULL || ops == NULL || ops->generation_secure == NULL ||
        !ops->generation_secure(ops->context, connection_generation)) {
        return false;
    }
    parsed = buddy_protocol_parse(json, length, &event);
    if (parsed < BUDDY_EVENT_NONE) {
        if (event.command.name[0] != '\0') {
            const char *error = parsed == BUDDY_EVENT_UNSUPPORTED_COMMAND
                                    ? "unsupported in phase 1"
                                    : (parsed == BUDDY_EVENT_UNKNOWN_COMMAND
                                           ? "unknown command"
                                           : "invalid request");
            (void)buddy_orchestrator_send_ack(ops, event.command.name, false, error,
                                              connection_generation);
        }
        return false;
    }

    event.ble.connection_generation = connection_generation;
    if (event.type == BUDDY_EVENT_NAME) {
        setting_command = true;
        err = event.command.value_truncated || ops->commit_name == NULL
                  ? ESP_ERR_INVALID_ARG
                  : ops->commit_name(ops->context, event.command.value);
    } else if (event.type == BUDDY_EVENT_OWNER) {
        setting_command = true;
        err = event.command.value_truncated || ops->commit_owner == NULL
                  ? ESP_ERR_INVALID_ARG
                  : ops->commit_owner(ops->context, event.command.value);
    }

    if (err == ESP_OK) {
        buddy_state_reduce(state, &event, now_ms, action);
        if (event.type == BUDDY_EVENT_NAME) {
            buddy_orchestrator_copy(state->settings.name, sizeof(state->settings.name),
                                    event.command.value);
        } else if (event.type == BUDDY_EVENT_OWNER) {
            buddy_orchestrator_copy(state->settings.owner, sizeof(state->settings.owner),
                                    event.command.value);
        }
    }
    if (setting_command) {
        const char *error = event.command.value_truncated ? "invalid value" : "persist failed";
        esp_err_t send_err = buddy_orchestrator_send_ack(
            ops, event.command.name, err == ESP_OK, err == ESP_OK ? NULL : error,
            connection_generation);
        return err == ESP_OK && send_err == ESP_OK;
    }
    return err == ESP_OK;
}

bool buddy_orchestrator_execute_action(buddy_state_t *state,
                                       const buddy_orchestrator_ops_t *ops,
                                       const buddy_action_t *action,
                                       buddy_event_t *result_event)
{
    char json[BUDDY_PROTOCOL_TX_MAX];
    int length;
    esp_err_t err;

    if (state == NULL || ops == NULL || action == NULL) {
        return false;
    }
    if (result_event != NULL) {
        memset(result_event, 0, sizeof(*result_event));
    }
    switch (action->type) {
    case BUDDY_ACTION_PERMISSION:
        if (result_event == NULL) {
            return false;
        }
        result_event->type = BUDDY_EVENT_PERMISSION_SEND_RESULT;
        buddy_orchestrator_copy(result_event->permission_result.id,
                                sizeof(result_event->permission_result.id),
                                action->permission.id);
        result_event->permission_result.id_length =
            strlen(result_event->permission_result.id);
        result_event->permission_result.decision = action->permission.decision;
        length = buddy_protocol_permission_json(json, sizeof(json), action->permission.id,
                                                action->permission.decision);
        err = buddy_orchestrator_send_json(ops, json, length,
                                           action->permission.connection_generation);
        result_event->permission_result.success = err == ESP_OK;
        if (err == ESP_OK && ops->record_permission != NULL) {
            ops->record_permission(ops->context, action->permission.decision);
        }
        return true;
    case BUDDY_ACTION_SETTINGS:
        return ops->persist_level != NULL &&
               ops->persist_level(ops->context,
                                  action->settings.highest_celebrated_level) == ESP_OK;
    case BUDDY_ACTION_STATUS: {
        buddy_status_report_t report;

        if (ops->status_report == NULL ||
            ops->status_report(ops->context, state, &report) != ESP_OK) {
            return false;
        }
        length = buddy_protocol_device_status_json(json, sizeof(json), &report);
        return buddy_orchestrator_send_json(ops, json, length,
                                            action->connection_generation) == ESP_OK;
    }
    case BUDDY_ACTION_UNPAIR_CONFIRMED:
        if (ops->unpair == NULL) {
            return false;
        }
        if (action->confirmation_acknowledge &&
            buddy_orchestrator_send_ack(ops, "unpair", true, NULL,
                                        action->connection_generation) != ESP_OK) {
            return false;
        }
        return ops->unpair(ops->context) == ESP_OK;
    case BUDDY_ACTION_FACTORY_RESET_CONFIRMED:
        return ops->factory_reset != NULL && ops->factory_reset(ops->context) == ESP_OK;
    case BUDDY_ACTION_BLE_TOGGLE:
        return ops->set_ble_enabled != NULL &&
               ops->set_ble_enabled(ops->context, action->ble_enabled) == ESP_OK;
    case BUDDY_ACTION_NONE:
    case BUDDY_ACTION_UI_REFRESH:
    case BUDDY_ACTION_UI_SCROLL:
    case BUDDY_ACTION_DISPLAY_BACKLIGHT:
    case BUDDY_ACTION_SCREEN_OFF:
        return true;
    }
    return false;
}
