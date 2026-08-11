#include <assert.h>
#include <string.h>

#include "buddy_app_logic.h"
#include "official_reference_fixtures.h"

typedef struct {
    esp_err_t start_result;
    esp_err_t stop_result;
    unsigned starts;
    unsigned stops;
} fake_transport_t;

static esp_err_t fake_start(void *context)
{
    fake_transport_t *fake = context;

    ++fake->starts;
    return fake->start_result;
}

static esp_err_t fake_stop(void *context)
{
    fake_transport_t *fake = context;

    ++fake->stops;
    return fake->stop_result;
}

static void test_only_official_plain_heartbeat_is_normal(void)
{
    static const char invalid_utf8[] = {
        '{', '"', 't', 'o', 't', 'a', 'l', '"', ':', '0', ',',
        '"', 'm', 's', 'g', '"', ':', '"', (char)0xc0, (char)0xaf, '"', '}',
    };
    static const char *const normal[] = {
        OFFICIAL_HEARTBEAT_NO_PROMPT_JSON,
        " { \"total\" : 0, \"running\" : 0, \"waiting\" : 0, "
        "\"msg\" : \"idle\", \"entries\" : [], \"tokens\" : 0, "
        "\"tokens_today\" : 0 } ",
    };
    static const char *const priority[] = {
        OFFICIAL_HEARTBEAT_JSON,
        OFFICIAL_TIME_JSON,
        OFFICIAL_OWNER_JSON,
        OFFICIAL_STATUS_REQUEST_JSON,
        "{\"total\":0,\"running\":0,\"waiting\":0,\"msg\":\"idle\","
        "\"entries\":[],\"tokens\":0,\"tokens_today\":0,\"pr\\u006fmpt\":{}}",
        "{\"total\":0,}",
        "{\"total\":0} trailing",
        "not json",
    };
    size_t index;

    for (index = 0; index < sizeof(normal) / sizeof(normal[0]); ++index) {
        assert(buddy_app_classify_rx(normal[index], strlen(normal[index])) ==
               BUDDY_APP_RX_NORMAL_HEARTBEAT);
    }
    for (index = 0; index < sizeof(priority) / sizeof(priority[0]); ++index) {
        assert(buddy_app_classify_rx(priority[index], strlen(priority[index])) ==
               BUDDY_APP_RX_PRIORITY);
    }
    assert(buddy_app_classify_rx(invalid_utf8, sizeof(invalid_utf8)) ==
           BUDDY_APP_RX_PRIORITY);
}

static void test_priority_evicts_stale_normal_before_queued_priority(void)
{
    assert(buddy_app_rx_overflow_policy(BUDDY_APP_RX_PRIORITY, false, true, true, false) ==
           BUDDY_APP_RX_REPLACE_NORMAL);
    assert(buddy_app_rx_overflow_policy(BUDDY_APP_RX_PRIORITY, false, false, true, true) ==
           BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY);
}

static void test_normal_heartbeat_coalesces_or_drops_without_priority_capacity(void)
{
    assert(buddy_app_rx_overflow_policy(BUDDY_APP_RX_NORMAL_HEARTBEAT, false, true,
                                        true, true) == BUDDY_APP_RX_REPLACE_NORMAL);
    assert(buddy_app_rx_overflow_policy(BUDDY_APP_RX_NORMAL_HEARTBEAT, false, false,
                                        true, true) == BUDDY_APP_RX_DROP);
}

static void test_priority_retry_falls_back_after_a_race(void)
{
    buddy_app_rx_retry_state_t retry;
    buddy_app_rx_overflow_action_t action;

    buddy_app_rx_retry_init(&retry, BUDDY_APP_RX_PRIORITY);
    action = buddy_app_rx_retry_next(&retry, false, true, true, false);
    assert(action == BUDDY_APP_RX_REPLACE_NORMAL);
    buddy_app_rx_retry_record_eviction(&retry, action, false);
    action = buddy_app_rx_retry_next(&retry, false, false, true, false);
    assert(action == BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY);
    buddy_app_rx_retry_record_eviction(&retry, action, true);
    assert(buddy_app_rx_retry_next(&retry, true, false, true, false) ==
           BUDDY_APP_RX_ENQUEUE);
    assert(buddy_app_rx_retry_overflow_count(&retry) == 1U);
}

static void test_status_identity_comes_from_settings_snapshot(void)
{
    buddy_settings_snapshot_t settings = {0};
    buddy_app_status_runtime_t runtime = {
        .encrypted = true,
        .uptime_ms = 123,
        .free_heap = 456,
        .queue_overflow_count = 7,
    };
    buddy_status_report_t report = {0};

    strcpy(settings.name, "Persisted Buddy");
    strcpy(settings.owner, "Persisted Owner");
    settings.approval_count = 8;
    settings.denial_count = 3;
    assert(buddy_app_build_status(&report, &settings, &runtime));
    assert(strcmp(report.name, "Persisted Buddy") == 0);
    assert(report.approval_count == 8);
    assert(report.denial_count == 3);
    assert(report.uptime_ms == 123);
}

static void test_failed_stop_restarts_transport_and_reports_rollback(void)
{
    fake_transport_t fake = {
        .start_result = ESP_OK,
        .stop_result = ESP_ERR_INVALID_STATE,
    };
    buddy_app_ble_transport_ops_t ops = {
        .context = &fake,
        .start = fake_start,
        .stop = fake_stop,
    };
    buddy_app_ble_transport_result_t result = buddy_app_set_ble_transport(&ops, false);

    assert(result.request_status == ESP_ERR_INVALID_STATE);
    assert(result.effective_enabled);
    assert(result.recovery_attempted);
    assert(fake.stops == 1);
    assert(fake.starts == 1);
}

int main(void)
{
    test_only_official_plain_heartbeat_is_normal();
    test_priority_evicts_stale_normal_before_queued_priority();
    test_normal_heartbeat_coalesces_or_drops_without_priority_capacity();
    test_priority_retry_falls_back_after_a_race();
    test_status_identity_comes_from_settings_snapshot();
    test_failed_stop_restarts_transport_and_reports_rollback();
    return 0;
}
