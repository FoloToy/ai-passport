#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_orchestrator.h"
#include "official_reference_fixtures.h"

typedef struct {
    char order[32];
    char sent[BUDDY_PROTOCOL_TX_MAX];
    esp_err_t commit_result;
    bool secure;
    unsigned permissions_recorded;
    bool ble_enabled;
} fake_runtime_t;

static void record(fake_runtime_t *fake, char operation)
{
    size_t length = strlen(fake->order);

    assert(length + 1U < sizeof(fake->order));
    fake->order[length] = operation;
    fake->order[length + 1U] = '\0';
}

static bool fake_secure(void *context, uint32_t generation)
{
    fake_runtime_t *fake = context;
    return fake->secure && generation == 7U;
}

static esp_err_t fake_send(void *context, const char *data, size_t length,
                           uint32_t generation)
{
    fake_runtime_t *fake = context;

    assert(generation == 7U);
    assert(length < sizeof(fake->sent));
    record(fake, 'S');
    memcpy(fake->sent, data, length);
    fake->sent[length] = '\0';
    return ESP_OK;
}

static esp_err_t fake_commit_setting(void *context, const char *value)
{
    fake_runtime_t *fake = context;

    assert(value[0] != '\0');
    record(fake, 'C');
    return fake->commit_result;
}

static esp_err_t fake_status(void *context, const buddy_state_t *state,
                             buddy_status_report_t *report)
{
    fake_runtime_t *fake = context;

    (void)state;
    record(fake, 'R');
    memset(report, 0, sizeof(*report));
    snprintf(report->name, sizeof(report->name), "%s", "Clawd");
    report->encrypted = true;
    report->battery_available = true;
    report->battery_percent = 87;
    report->battery_mv = 4012;
    report->uptime_ms = 8412000;
    report->free_heap = 84200;
    report->approval_count = 42;
    report->denial_count = 3;
    report->highest_celebrated_level = 5;
    return ESP_OK;
}

static void fake_record_permission(void *context, buddy_permission_decision_t decision)
{
    fake_runtime_t *fake = context;

    assert(decision == BUDDY_PERMISSION_ONCE);
    record(fake, 'P');
    ++fake->permissions_recorded;
}

static esp_err_t fake_unpair(void *context)
{
    record(context, 'U');
    return ESP_OK;
}

static esp_err_t fake_factory_reset(void *context)
{
    record(context, 'F');
    return ESP_OK;
}

static esp_err_t fake_set_ble(void *context, bool enabled)
{
    fake_runtime_t *fake = context;

    record(fake, 'B');
    fake->ble_enabled = enabled;
    return ESP_OK;
}

static esp_err_t fake_persist_level(void *context, uint64_t level)
{
    (void)level;
    record(context, 'L');
    return ESP_OK;
}

static buddy_orchestrator_ops_t fake_ops(fake_runtime_t *fake)
{
    const buddy_orchestrator_ops_t ops = {
        .context = fake,
        .generation_secure = fake_secure,
        .send = fake_send,
        .commit_name = fake_commit_setting,
        .commit_owner = fake_commit_setting,
        .status_report = fake_status,
        .record_permission = fake_record_permission,
        .unpair = fake_unpair,
        .factory_reset = fake_factory_reset,
        .set_ble_enabled = fake_set_ble,
        .persist_level = fake_persist_level,
    };
    return ops;
}

static void test_official_rx_fixtures_reach_state_and_responses(void)
{
    fake_runtime_t fake = {.commit_result = ESP_OK, .secure = true};
    buddy_orchestrator_ops_t ops = fake_ops(&fake);
    buddy_settings_snapshot_t settings = {0};
    buddy_state_t state;
    buddy_action_t action;

    snprintf(settings.name, sizeof(settings.name), "%s", "Persisted");
    buddy_state_init(&state, &settings);
    assert(buddy_orchestrator_process_rx(&state, &ops, OFFICIAL_HEARTBEAT_JSON,
                                         strlen(OFFICIAL_HEARTBEAT_JSON), 7, 1000, &action));
    assert(state.total == 3U && state.running == 1U && state.waiting == 1U);
    assert(strcmp(state.name, "Persisted") == 0);
    assert(strcmp(state.prompt.id, "req_abc123") == 0);

    fake.order[0] = '\0';
    assert(buddy_orchestrator_process_rx(&state, &ops, OFFICIAL_OWNER_JSON,
                                         strlen(OFFICIAL_OWNER_JSON), 7, 1001, &action));
    assert(strcmp(fake.order, "CS") == 0);
    assert(strcmp(fake.sent, "{\"ack\":\"owner\",\"ok\":true}\n") == 0);
    assert(strcmp(state.owner, "Felix") == 0);

    fake.order[0] = '\0';
    assert(buddy_orchestrator_process_rx(&state, &ops, OFFICIAL_TIME_JSON,
                                         strlen(OFFICIAL_TIME_JSON), 7, 1002, &action));
    assert(fake.order[0] == '\0');
    assert(state.epoch_seconds == 1775731234);
    assert(state.timezone_offset_seconds == -25200);

    assert(buddy_orchestrator_process_rx(&state, &ops, OFFICIAL_STATUS_REQUEST_JSON,
                                         strlen(OFFICIAL_STATUS_REQUEST_JSON), 7, 1003,
                                         &action));
    fake.order[0] = '\0';
    assert(buddy_orchestrator_execute_action(&state, &ops, &action, NULL));
    assert(strcmp(fake.order, "RS") == 0);
    assert(strstr(fake.sent, "{\"ack\":\"status\",\"ok\":true,\"data\":{") ==
           fake.sent);
    assert(strstr(fake.sent, "\"bat\":{\"pct\":87,\"mV\":4012}") != NULL);
}

static void test_failed_setting_commit_sends_error_ack_without_state_change(void)
{
    fake_runtime_t fake = {.commit_result = ESP_ERR_INVALID_STATE, .secure = true};
    buddy_orchestrator_ops_t ops = fake_ops(&fake);
    buddy_settings_snapshot_t settings = {0};
    buddy_state_t state;
    buddy_action_t action;

    snprintf(settings.name, sizeof(settings.name), "%s", "Old");
    buddy_state_init(&state, &settings);
    assert(!buddy_orchestrator_process_rx(&state, &ops, OFFICIAL_NAME_JSON,
                                          strlen(OFFICIAL_NAME_JSON), 7, 1000, &action));
    assert(strcmp(fake.order, "CS") == 0);
    assert(strcmp(fake.sent,
                  "{\"ack\":\"name\",\"ok\":false,\"error\":\"persist failed\"}\n") == 0);
    assert(strcmp(state.name, "Old") == 0);
}

static void test_executor_orders_sensitive_side_effects(void)
{
    fake_runtime_t fake = {.commit_result = ESP_OK, .secure = true};
    buddy_orchestrator_ops_t ops = fake_ops(&fake);
    buddy_state_t state;
    buddy_action_t action = {.type = BUDDY_ACTION_PERMISSION};
    buddy_event_t result;

    buddy_state_init(&state, NULL);
    snprintf(action.permission.id, sizeof(action.permission.id), "%s", "req_abc123");
    action.permission.decision = BUDDY_PERMISSION_ONCE;
    action.permission.connection_generation = 7;
    assert(buddy_orchestrator_execute_action(&state, &ops, &action, &result));
    assert(strcmp(fake.order, "SP") == 0);
    assert(result.type == BUDDY_EVENT_PERMISSION_SEND_RESULT);
    assert(result.permission_result.success);

    fake.order[0] = '\0';
    action.type = BUDDY_ACTION_UNPAIR_CONFIRMED;
    action.confirmation_acknowledge = true;
    action.connection_generation = 7;
    assert(buddy_orchestrator_execute_action(&state, &ops, &action, &result));
    assert(strcmp(fake.order, "SU") == 0);

    fake.order[0] = '\0';
    action.type = BUDDY_ACTION_BLE_TOGGLE;
    action.ble_enabled = false;
    assert(buddy_orchestrator_execute_action(&state, &ops, &action, &result));
    assert(strcmp(fake.order, "B") == 0);
    assert(!fake.ble_enabled);
}

int main(void)
{
    test_official_rx_fixtures_reach_state_and_responses();
    test_failed_setting_commit_sends_error_ack_without_state_change();
    test_executor_orders_sensitive_side_effects();
    puts("buddy orchestrator tests passed");
    return 0;
}
